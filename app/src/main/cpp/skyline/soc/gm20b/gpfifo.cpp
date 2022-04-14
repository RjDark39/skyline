// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/signal.h>
#include <loader/loader.h>
#include <kernel/types/KProcess.h>
#include <soc.h>
#include <os.h>
#include "engines/maxwell_3d.h"

namespace skyline::soc::gm20b {
    /**
     * @brief A single pushbuffer method header that describes a compressed method sequence
     * @url https://github.com/NVIDIA/open-gpu-doc/blob/ab27fc22db5de0d02a4cabe08e555663b62db4d4/manuals/volta/gv100/dev_ram.ref.txt#L850
     * @url https://github.com/NVIDIA/open-gpu-doc/blob/ab27fc22db5de0d02a4cabe08e555663b62db4d4/classes/host/clb06f.h#L179
     */
    union PushBufferMethodHeader {
        u32 raw;

        enum class TertOp : u8 {
            Grp0IncMethod = 0,
            Grp0SetSubDevMask = 1,
            Grp0StoreSubDevMask = 2,
            Grp0UseSubDevMask = 3,
            Grp2NonIncMethod = 0,
        };

        enum class SecOp : u8 {
            Grp0UseTert = 0,
            IncMethod = 1,
            Grp2UseTert = 2,
            NonIncMethod = 3,
            ImmdDataMethod = 4,
            OneInc = 5,
            Reserved6 = 6,
            EndPbSegment = 7,
        };

        u16 methodAddress : 12;
        struct {
            u8 _pad0_ : 4;
            u16 subDeviceMask : 12;
        };

        struct {
            u16 _pad1_ : 13;
            SubchannelId methodSubChannel : 3;
            union {
                TertOp tertOp : 3;
                u16 methodCount : 13;
                u16 immdData : 13;
            };
        };

        struct {
            u32 _pad2_ : 29;
            SecOp secOp : 3;
        };
    };
    static_assert(sizeof(PushBufferMethodHeader) == sizeof(u32));

    ChannelGpfifo::ChannelGpfifo(const DeviceState &state, ChannelContext &channelCtx, size_t numEntries) :
        state(state),
        gpfifoEngine(state.soc->host1x.syncpoints, channelCtx),
        channelCtx(channelCtx),
        gpEntries(numEntries),
        thread(std::thread(&ChannelGpfifo::Run, this)) {}

    void ChannelGpfifo::Send(u32 method, u32 argument, SubchannelId subChannel, bool lastCall) {
        Logger::Debug("Called GPU method - method: 0x{:X} argument: 0x{:X} subchannel: 0x{:X} last: {}", method, argument, subChannel, lastCall);

        if (method < engine::GPFIFO::RegisterCount) {
            gpfifoEngine.CallMethod(method, argument);
        } else if (method < engine::EngineMethodsEnd) { [[likely]]
            switch (subChannel) {
                case SubchannelId::ThreeD:
                    channelCtx.maxwell3D->CallMethod(method, argument);
                    break;
                default:
                    Logger::Warn("Called method 0x{:X} in unimplemented engine 0x{:X}, args: 0x{:X}", method, subChannel, argument);
                    break;
            }
        } else {
            switch (subChannel) {
                case SubchannelId::ThreeD:
                    channelCtx.maxwell3D->HandleMacroCall(method - engine::EngineMethodsEnd, argument, lastCall);
                    break;
                case SubchannelId::TwoD:
                    // TODO: Fix this when we implement the 2D Engine
                    Logger::Warn("Calling macros in the 2D engine is unimplemented!");
                    break;
                default:
                    Logger::Warn("Called method 0x{:X} out of bounds for engine 0x{:X}, args: 0x{:X}", method, subChannel, argument);
                    break;
            }
        }
    }

    void ChannelGpfifo::Process(GpEntry gpEntry) {
        if (!gpEntry.size) {
            // This is a GPFIFO control entry, all control entries have a zero length and contain no pushbuffers
            switch (gpEntry.opcode) {
                case GpEntry::Opcode::Nop:
                    return;
                default:
                    Logger::Warn("Unsupported GpEntry control opcode used: {}", static_cast<u8>(gpEntry.opcode));
                    return;
            }
        }

        pushBufferData.resize(gpEntry.size);
        channelCtx.asCtx->gmmu.Read<u32>(pushBufferData, gpEntry.Address());

        // There will be at least one entry here
        auto entry{pushBufferData.begin()};

        // Executes the current split method, returning once execution is finished or the current GpEntry has reached its end
        auto resumeSplitMethod{[&](){
            switch (resumeState.state) {
                case MethodResumeState::State::Inc:
                    while (entry != pushBufferData.end() && resumeState.remaining)
                        Send(resumeState.address++, *(entry++), resumeState.subChannel, --resumeState.remaining == 0);

                    break;
                case MethodResumeState::State::OneInc:
                    Send(resumeState.address++, *(entry++), resumeState.subChannel, --resumeState.remaining == 0);

                    // After the first increment OneInc methods work the same as a NonInc method, this is needed so they can resume correctly if they are broken up by multiple GpEntries
                    resumeState.state = MethodResumeState::State::NonInc;
                    [[fallthrough]];
                case MethodResumeState::State::NonInc:
                    while (entry != pushBufferData.end() && resumeState.remaining)
                        Send(resumeState.address, *(entry++), resumeState.subChannel, --resumeState.remaining == 0);

                    break;
            }
        }};

        // We've a method from a previous GpEntry that needs resuming
        if (resumeState.remaining)
            resumeSplitMethod();

        // Process more methods if the entries are still not all used up after handling resuming
        for (; entry != pushBufferData.end(); entry++) {
            // An entry containing all zeroes is a NOP, skip over it
            if (*entry == 0)
                continue;

            PushBufferMethodHeader methodHeader{.raw = *entry};

            // Needed in order to check for methods split across multiple GpEntries
            auto remainingEntries{std::distance(entry, pushBufferData.end()) - 1};

            // Handles storing state and initial execution for methods that are split across multiple GpEntries
            auto startSplitMethod{[&](auto methodState) {
                resumeState = {
                    .remaining = methodHeader.methodCount,
                    .address = methodHeader.methodAddress,
                    .subChannel = methodHeader.methodSubChannel,
                    .state = methodState
                };

                // Skip over method header as `resumeSplitMethod` doesn't expect it to be there
                entry++;

                resumeSplitMethod();
            }};

            switch (methodHeader.secOp) {
                case PushBufferMethodHeader::SecOp::IncMethod:
                    if (remainingEntries >= methodHeader.methodCount) {
                        for (u32 i{}; i < methodHeader.methodCount; i++)
                            Send(methodHeader.methodAddress + i, *++entry, methodHeader.methodSubChannel, i == methodHeader.methodCount - 1);

                        break;
                    } else {
                        startSplitMethod(MethodResumeState::State::Inc);
                        return;
                    }
                case PushBufferMethodHeader::SecOp::NonIncMethod:
                    if (remainingEntries >= methodHeader.methodCount) {
                        for (u32 i{}; i < methodHeader.methodCount; i++)
                            Send(methodHeader.methodAddress, *++entry, methodHeader.methodSubChannel, i == methodHeader.methodCount - 1);

                        break;
                    } else {
                        startSplitMethod(MethodResumeState::State::NonInc);
                        return;
                    }
                case PushBufferMethodHeader::SecOp::OneInc:
                    if (remainingEntries >= methodHeader.methodCount) {
                        for (u32 i{}; i < methodHeader.methodCount; i++)
                            Send(methodHeader.methodAddress + (i ? 1 : 0), *++entry, methodHeader.methodSubChannel, i == methodHeader.methodCount - 1);

                        break;
                    } else {
                        startSplitMethod(MethodResumeState::State::OneInc);
                        return;
                    }
                case PushBufferMethodHeader::SecOp::ImmdDataMethod:
                    Send(methodHeader.methodAddress, methodHeader.immdData, methodHeader.methodSubChannel, true);
                    break;

                case PushBufferMethodHeader::SecOp::EndPbSegment:
                    return;

                default:
                    throw exception("Unsupported pushbuffer method SecOp: {}", static_cast<u8>(methodHeader.secOp));
            }
        }
    }

    void ChannelGpfifo::Run() {
        pthread_setname_np(pthread_self(), "GPFIFO");
        try {
            signal::SetSignalHandler({SIGINT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV}, signal::ExceptionalSignalHandler);

            gpEntries.Process([this](GpEntry gpEntry) {
                Logger::Debug("Processing pushbuffer: 0x{:X}, Size: 0x{:X}", gpEntry.Address(), +gpEntry.size);
                Process(gpEntry);
            });
        } catch (const signal::SignalException &e) {
            if (e.signal != SIGINT) {
                Logger::Error("{}\nStack Trace:{}", e.what(), state.loader->GetStackTrace(e.frames));
                signal::BlockSignal({SIGINT});
                state.process->Kill(false);
            }
        } catch (const std::exception &e) {
            Logger::Error(e.what());
            signal::BlockSignal({SIGINT});
            state.process->Kill(false);
        }
    }

    void ChannelGpfifo::Push(span<GpEntry> entries) {
        gpEntries.Append(entries);
    }

    void ChannelGpfifo::Push(GpEntry entry) {
        gpEntries.Push(entry);
    }

    ChannelGpfifo::~ChannelGpfifo() {
        if (thread.joinable()) {
            pthread_kill(thread.native_handle(), SIGINT);
            thread.join();
        }
    }
}
