#include "KProcess.h"
#include <nce.h>
#include <os.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <asm/unistd.h>
#include <nce/guest.h>

namespace skyline::kernel::type {
    KProcess::TlsPage::TlsPage(u64 address) : address(address) {}

    u64 KProcess::TlsPage::ReserveSlot() {
        if (Full())
            throw exception("Trying to get TLS slot from full page");
        slot[index] = true;
        return Get(index++); // ++ on right will cause increment after evaluation of expression
    }

    u64 KProcess::TlsPage::Get(u8 slotNo) {
        if (slotNo >= constant::TlsSlots)
            throw exception("TLS slot is out of range");
        return address + (constant::TlsSlotSize * slotNo);
    }

    bool KProcess::TlsPage::Full() {
        return slot[constant::TlsSlots - 1];
    }

    u64 KProcess::GetTlsSlot() {
        for (auto &tlsPage: tlsPages)
            if (!tlsPage->Full())
                return tlsPage->ReserveSlot();
        u64 address;
        if (tlsPages.empty()) {
            auto region = state.os->memory.GetRegion(memory::Regions::TlsIo);
            address = region.size ? region.address : 0;
        } else
            address = (*(tlsPages.end() - 1))->address + PAGE_SIZE;
        auto tlsMem = NewHandle<KPrivateMemory>(address, PAGE_SIZE, memory::Permission(true, true, false), memory::MemoryStates::ThreadLocal).item;
        tlsPages.push_back(std::make_shared<TlsPage>(tlsMem->address));
        auto &tlsPage = tlsPages.back();
        if (tlsPages.empty())
            tlsPage->ReserveSlot(); // User-mode exception handling
        return tlsPage->ReserveSlot();
    }

    void KProcess::InitializeMemory() {
        heap = NewHandle<KPrivateMemory>(state.os->memory.GetRegion(memory::Regions::Heap).address, constant::DefHeapSize, memory::Permission{true, true, false}, memory::MemoryStates::Heap).item;
        threads[pid]->tls = GetTlsSlot();
    }

    KProcess::KProcess(const DeviceState &state, pid_t pid, u64 entryPoint, u64 stackBase, u64 stackSize, std::shared_ptr<type::KSharedMemory> &tlsMemory) : pid(pid), KSyncObject(state, KType::KProcess) {
        auto thread = NewHandle<KThread>(pid, entryPoint, 0x0, stackBase + stackSize, 0, constant::DefaultPriority, this, tlsMemory).item;
        threads[pid] = thread;
        state.nce->WaitThreadInit(thread);
        memFd = open(fmt::format("/proc/{}/mem", pid).c_str(), O_RDWR | O_CLOEXEC);
        if (memFd == -1)
            throw exception("Cannot open file descriptor to /proc/{}/mem, \"{}\"", pid, strerror(errno));
    }

    KProcess::~KProcess() {
        close(memFd);
        status = Status::Exiting;
    }

    std::shared_ptr<KThread> KProcess::CreateThread(u64 entryPoint, u64 entryArg, u64 stackTop, u8 priority) {
        auto size = (sizeof(ThreadContext) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
        auto tlsMem = std::make_shared<type::KSharedMemory>(state, 0, size, memory::Permission{true, true, false}, memory::MemoryStates::Reserved);
        Registers fregs{};
        fregs.x0 = CLONE_THREAD | CLONE_SIGHAND | CLONE_PTRACE | CLONE_FS | CLONE_VM | CLONE_FILES | CLONE_IO;
        fregs.x1 = stackTop;
        fregs.x3 = tlsMem->Map(0, size, memory::Permission{true, true, false});
        fregs.x8 = __NR_clone;
        fregs.x5 = reinterpret_cast<u64>(&guest::entry);
        fregs.x6 = entryPoint;
        state.nce->ExecuteFunction(ThreadCall::Clone, fregs);
        if (static_cast<int>(fregs.x0) < 0)
            throw exception("Cannot create thread: Address: 0x{:X}, Stack Top: 0x{:X}", entryPoint, stackTop);
        auto pid = static_cast<pid_t>(fregs.x0);
        auto process = NewHandle<KThread>(pid, entryPoint, entryArg, stackTop, GetTlsSlot(), priority, this, tlsMem).item;
        threads[pid] = process;
        return process;
    }

    void KProcess::ReadMemory(void *destination, u64 offset, size_t size) const {
        struct iovec local{
            .iov_base = destination,
            .iov_len = size
        };
        struct iovec remote{
            .iov_base = reinterpret_cast<void *>(offset),
            .iov_len = size
        };

        if (process_vm_readv(pid, &local, 1, &remote, 1, 0) < 0)
            pread64(memFd, destination, size, offset);
    }

    void KProcess::WriteMemory(void *source, u64 offset, size_t size) const {
        struct iovec local{
            .iov_base = source,
            .iov_len = size
        };
        struct iovec remote{
            .iov_base = reinterpret_cast<void *>(offset),
            .iov_len = size
        };

        if (process_vm_writev(pid, &local, 1, &remote, 1, 0) < 0)
            pwrite64(memFd, source, size, offset);
    }

    void KProcess::CopyMemory(u64 source, u64 destination, size_t size) const {
        if (size <= PAGE_SIZE) {
            std::vector<u8> buffer(size);
            state.process->ReadMemory(buffer.data(), source, size);
            state.process->WriteMemory(buffer.data(), destination, size);
        } else {
            Registers fregs{};
            fregs.x0 = source;
            fregs.x1 = destination;
            fregs.x2 = size;
            state.nce->ExecuteFunction(ThreadCall::Memcopy, fregs);
        }
    }

    std::optional<KProcess::HandleOut<KMemory>> KProcess::GetMemoryObject(u64 address) {
        for (auto&[handle, object] : state.process->handles) {
            switch (object->objectType) {
                case type::KType::KPrivateMemory:
                case type::KType::KSharedMemory:
                case type::KType::KTransferMemory: {
                    auto mem = std::static_pointer_cast<type::KMemory>(object);
                    if (mem->IsInside(address))
                        return std::optional<KProcess::HandleOut<KMemory>>({mem, handle});
                }
                default:
                    break;
            }
        }
        return std::nullopt;
    }

    void KProcess::MutexLock(u64 address, handle_t owner, bool alwaysLock) {
        std::unique_lock lock(mutexLock);
        u32 mtxVal = ReadMemory<u32>(address);
        if(alwaysLock) {
            if(!mtxVal) {
                state.logger->Warn("Mutex Value was 0");
                mtxVal = (constant::MtxOwnerMask & state.thread->handle);
                WriteMemory<u32>(mtxVal, address);
                return;
                // TODO: Replace with atomic CAS
            }
        } else {
            if (mtxVal != (owner | ~constant::MtxOwnerMask))
                return;
        }
        auto &mtxWaiters = mutexes[address];
        std::shared_ptr<WaitStatus> status;
        for (auto it = mtxWaiters.begin();;++it) {
            if (it != mtxWaiters.end() && (*it)->priority >= state.thread->priority)
                continue;
            status = std::make_shared<WaitStatus>(state.thread->priority, state.thread->pid);
            mtxWaiters.insert(it, status);
            break;
        }
        lock.unlock();
        while (!status->flag);
        lock.lock();
        for (auto it = mtxWaiters.begin(); it != mtxWaiters.end(); ++it)
            if((*it)->pid == state.thread->pid) {
                mtxWaiters.erase(it);
                break;
            }
        mtxVal = (constant::MtxOwnerMask & state.thread->handle) | (mtxWaiters.empty() ? 0 : ~constant::MtxOwnerMask);
        WriteMemory<u32>(mtxVal, address);
        lock.unlock();
    }

    bool KProcess::MutexUnlock(u64 address) {
        std::lock_guard lock(mutexLock);
        u32 mtxVal = ReadMemory<u32>(address);
        if ((mtxVal & constant::MtxOwnerMask) != state.thread->handle)
            return false;
        auto &mtxWaiters = mutexes[address];
        if (mtxWaiters.empty()) {
            mtxVal = 0;
            WriteMemory<u32>(mtxVal, address);
        } else
            (*mtxWaiters.begin())->flag = true;
        return true;
    }

    bool KProcess::ConditionalVariableWait(u64 address, u64 timeout) {
        std::unique_lock lock(conditionalLock);
        auto &condWaiters = conditionals[address];
        std::shared_ptr<WaitStatus> status;
        for (auto it = condWaiters.begin();;++it) {
            if (it != condWaiters.end() && (*it)->priority >= state.thread->priority)
                continue;
            status = std::make_shared<WaitStatus>(state.thread->priority, state.thread->pid);
            condWaiters.insert(it, status);
            break;
        }
        lock.unlock();
        bool timedOut{};
        auto start = utils::GetCurrTimeNs();
        while (!status->flag) {
            if ((utils::GetCurrTimeNs() - start) >= timeout)
                timedOut = true;
        }
        lock.lock();
        for (auto it = condWaiters.begin(); it != condWaiters.end(); ++it)
            if((*it)->pid == state.thread->pid) {
                condWaiters.erase(it);
                break;
            }
        lock.unlock();
        return !timedOut;
    }

    void KProcess::ConditionalVariableSignal(u64 address, u64 amount) {
        std::lock_guard lock(conditionalLock);
        auto &condWaiters = conditionals[address];
        amount = std::min(condWaiters.size(), amount);
        for (size_t i = 0; i < amount; ++i)
            condWaiters[i]->flag = true;
    }
}
