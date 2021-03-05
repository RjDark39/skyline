// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <unistd.h>
#include <common/signal.h>
#include "types/KThread.h"
#include "scheduler.h"

namespace skyline::kernel {
    Scheduler::CoreContext::CoreContext(u8 id, u8 preemptionPriority) : id(id), preemptionPriority(preemptionPriority) {}

    Scheduler::Scheduler(const DeviceState &state) : state(state) {}

    void Scheduler::SignalHandler(int signal, siginfo *info, ucontext *ctx, void **tls) {
        if (*tls) {
            const auto &state{*reinterpret_cast<nce::ThreadContext *>(*tls)->state};
            state.scheduler->Rotate(false);
            YieldPending = false;
            state.scheduler->WaitSchedule();
        } else {
            YieldPending = true;
        }
    }

    Scheduler::CoreContext &Scheduler::LoadBalance(const std::shared_ptr<type::KThread> &thread, bool alwaysInsert) {
        std::lock_guard migrationLock(thread->coreMigrationMutex);
        auto *currentCore{&cores.at(thread->coreId)};

        if (!currentCore->queue.empty() && thread->affinityMask.count() != 1) {
            // Select core where the current thread will be scheduled the earliest based off average timeslice durations for resident threads
            // There's a preference for the current core as migration isn't free
            size_t minTimeslice{};
            CoreContext *optimalCore{};
            for (auto &candidateCore : cores) {
                if (thread->affinityMask.test(candidateCore.id)) {
                    u64 timeslice{};

                    if (!candidateCore.queue.empty()) {
                        std::unique_lock coreLock(candidateCore.mutex);

                        auto threadIterator{candidateCore.queue.cbegin()};
                        if (threadIterator != candidateCore.queue.cend()) {
                            const auto &runningThread{*threadIterator};
                            timeslice += runningThread->averageTimeslice ? std::min(runningThread->averageTimeslice - (util::GetTimeTicks() - runningThread->timesliceStart), 1UL) : runningThread->timesliceStart ? util::GetTimeTicks() - runningThread->timesliceStart : 1UL;

                            while (++threadIterator != candidateCore.queue.cend()) {
                                const auto &residentThread{*threadIterator};
                                if (residentThread->priority <= thread->priority)
                                    timeslice += residentThread->averageTimeslice ? residentThread->averageTimeslice : 1UL;
                            }
                        }
                    }

                    if (!optimalCore || timeslice < minTimeslice || (timeslice == minTimeslice && &candidateCore == currentCore)) {
                        optimalCore = &candidateCore;
                        minTimeslice = timeslice;
                    }
                }
            }

            if (optimalCore != currentCore) {
                if (!alwaysInsert && thread == state.thread)
                    RemoveThread();
                else if (!alwaysInsert && thread != state.thread) [[unlikely]]
                    throw exception("Migrating an external thread (T{}) without 'alwaysInsert' isn't supported", thread->id);
                thread->coreId = optimalCore->id;
                InsertThread(thread);
                state.logger->Debug("Load Balancing T{}: C{} -> C{}", thread->id, currentCore->id, optimalCore->id);
            } else {
                if (alwaysInsert)
                    InsertThread(thread);
                state.logger->Debug("Load Balancing T{}: C{} (Late)", thread->id, currentCore->id);
            }

            return *optimalCore;
        }

        if (alwaysInsert)
            InsertThread(thread);
        state.logger->Debug("Load Balancing T{}: C{} (Early)", thread->id, currentCore->id);

        return *currentCore;
    }

    void Scheduler::InsertThread(const std::shared_ptr<type::KThread> &thread) {
        auto &core{cores.at(thread->coreId)};
        std::unique_lock lock(core.mutex);
        auto nextThread{std::upper_bound(core.queue.begin(), core.queue.end(), thread->priority.load(), type::KThread::IsHigherPriority)};
        if (nextThread == core.queue.begin()) {
            if (nextThread != core.queue.end()) {
                // If the inserted thread has a higher priority than the currently running thread (and the queue isn't empty)
                // We can yield the thread which is currently scheduled on the core by sending it a signal
                // It is optimized to avoid waiting for the thread to yield on receiving the signal which serializes the entire pipeline
                auto front{core.queue.front()};
                front->forceYield = true;
                core.queue.splice(std::upper_bound(core.queue.begin(), core.queue.end(), front->priority.load(), type::KThread::IsHigherPriority), core.queue, core.queue.begin());
                core.queue.push_front(thread);

                if (state.thread != front) {
                    // If the inserting thread isn't at the front, we need to send it an OS signal to yield
                    if (!front->pendingYield) {
                        // We only want to yield the thread if it hasn't already been sent a signal to yield in the past
                        // Not doing this can lead to races and deadlocks but is also slower as it prevents redundant signals
                        front->SendSignal(YieldSignal);
                        front->pendingYield = true;
                    }
                } else {
                    // If the thread at the front is being yielded, we can just set the YieldPending flag
                    // This avoids an OS signal and would cause a deadlock otherwise as the core lock would be relocked
                    YieldPending = true;
                }
            } else {
                core.queue.push_front(thread);
            }
            if (thread != state.thread)
                core.frontCondition.notify_all(); // We only want to trigger the conditional variable if the current thread isn't inserting itself
        } else {
            core.queue.insert(nextThread, thread);
        }
    }

    void Scheduler::WaitSchedule(bool loadBalance) {
        auto &thread{state.thread};
        auto *core{&cores.at(thread->coreId)};

        std::unique_lock lock(core->mutex);
        if (loadBalance && thread->affinityMask.count() > 1) {
            std::chrono::milliseconds loadBalanceThreshold{PreemptiveTimeslice * 2}; //!< The amount of time that needs to pass unscheduled for a thread to attempt load balancing
            while (!core->frontCondition.wait_for(lock, loadBalanceThreshold, [&]() { return !core->queue.empty() && core->queue.front() == thread; })) {
                lock.unlock();
                LoadBalance(state.thread);
                if (thread->coreId == core->id) {
                    lock.lock();
                } else {
                    core = &cores.at(thread->coreId);
                    lock = std::unique_lock(core->mutex);
                }

                loadBalanceThreshold *= 2; // We double the duration required for future load balancing for this invocation to minimize pointless load balancing
            }
        } else {
            core->frontCondition.wait(lock, [&]() { return !core->queue.empty() && core->queue.front() == thread; });
        }

        if (thread->priority == core->preemptionPriority) {
            struct itimerspec spec{.it_value = {.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(PreemptiveTimeslice).count()}};
            timer_settime(thread->preemptionTimer, 0, &spec, nullptr);
            thread->isPreempted = true;
        }

        thread->timesliceStart = util::GetTimeTicks();
    }

    bool Scheduler::TimedWaitSchedule(std::chrono::nanoseconds timeout) {
        auto &thread{state.thread};
        auto *core{&cores.at(thread->coreId)};

        std::unique_lock lock(core->mutex);
        if (core->frontCondition.wait_for(lock, timeout, [&]() { return !core->queue.empty() && core->queue.front() == thread; })) {
            if (thread->priority == core->preemptionPriority) {
                struct itimerspec spec{.it_value = {.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(PreemptiveTimeslice).count()}};
                timer_settime(thread->preemptionTimer, 0, &spec, nullptr);
                thread->isPreempted = true;
            }

            thread->timesliceStart = util::GetTimeTicks();

            return true;
        } else {
            return false;
        }
    }

    void Scheduler::Rotate(bool cooperative) {
        auto &thread{state.thread};
        auto &core{cores.at(thread->coreId)};

        std::unique_lock lock(core.mutex);
        if (core.queue.front() == thread) {
            // If this thread is at the front of the thread queue then we need to rotate the thread
            // In the case where this thread was forcefully yielded, we don't need to do this as it's done by the thread which yielded us
            // Splice the linked element from the beginning of the queue to where it's priority is present
            core.queue.splice(std::upper_bound(core.queue.begin(), core.queue.end(), thread->priority.load(), type::KThread::IsHigherPriority), core.queue, core.queue.begin());

            if (core.queue.front() != thread)
                core.frontCondition.notify_all(); // If we aren't at the front of the queue, only then should we wake the thread at the front up
        } else if (!thread->forceYield) { [[unlikely]]
            throw exception("T{} called Rotate while not being in C{}'s queue", thread->id, thread->coreId);
        }

        thread->averageTimeslice = (thread->averageTimeslice / 4) + (3 * (util::GetTimeTicks() - thread->timesliceStart / 4));

        if (cooperative && thread->isPreempted) {
            // If a preemptive thread did a cooperative yield then we need to disarm the preemptive timer
            struct itimerspec spec{};
            timer_settime(thread->preemptionTimer, 0, &spec, nullptr);
        }

        thread->isPreempted = false;
        thread->pendingYield = false;
        thread->forceYield = false;
    }

    void Scheduler::UpdatePriority(const std::shared_ptr<type::KThread> &thread) {
        std::lock_guard migrationLock(thread->coreMigrationMutex);
        auto *core{&cores.at(thread->coreId)};
        std::unique_lock coreLock(core->mutex);

        auto currentIt{std::find(core->queue.begin(), core->queue.end(), thread)};
        if (currentIt == core->queue.end() || currentIt == core->queue.begin())
            // If the thread isn't in the queue then the new priority will be handled automatically on insertion
            return;
        if (currentIt == core->queue.begin()) {
            // Alternatively, if it's currently running then we'd just want to cause it to yield if it's priority is lower than the the thread behind it
            auto nextIt{std::next(currentIt)};
            if (nextIt != core->queue.end() && (*nextIt)->priority < thread->priority) {
                if (!thread->pendingYield) {
                    thread->SendSignal(YieldSignal);
                    thread->pendingYield = true;
                }
            } else if (!thread->isPreempted && thread->priority == core->preemptionPriority) {
                // If the thread needs to be preempted due to the new priority then arm it's preemption timer
                struct itimerspec spec{.it_value = {.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(PreemptiveTimeslice).count()}};
                timer_settime(thread->preemptionTimer, 0, &spec, nullptr);
                thread->isPreempted = true;
            }
            return;
        }

        auto targetIt{std::upper_bound(core->queue.begin(), core->queue.end(), thread->priority.load(), type::KThread::IsHigherPriority)};
        if (currentIt == targetIt)
            // If this thread's position isn't affected by the priority change then we have nothing to do
            return;

        core->queue.erase(currentIt);

        if (thread->isPreempted && thread->priority != core->preemptionPriority) {
            struct itimerspec spec{};
            timer_settime(thread->preemptionTimer, 0, &spec, nullptr);
            thread->isPreempted = false;
        }

        targetIt = std::upper_bound(core->queue.begin(), core->queue.end(), thread->priority.load(), type::KThread::IsHigherPriority); // Iterator invalidation
        if (targetIt == core->queue.begin() && targetIt != core->queue.end()) {
            core->queue.insert(std::next(core->queue.begin()), thread);
            auto front{core->queue.front()};
            if (!front->pendingYield) {
                front->SendSignal(YieldSignal);
                front->pendingYield = true;
            }
        } else {
            core->queue.insert(targetIt, thread);
        }
    }

    void Scheduler::ParkThread() {
        auto &thread{state.thread};
        std::lock_guard migrationLock(thread->coreMigrationMutex);
        RemoveThread();

        auto originalCoreId{thread->coreId};
        thread->coreId = constant::ParkedCoreId;
        for (auto &core : cores)
            if (originalCoreId != core.id && thread->affinityMask.test(core.id) && (core.queue.empty() || core.queue.front()->priority > thread->priority))
                thread->coreId = core.id;

        if (thread->coreId == constant::ParkedCoreId) {
            std::unique_lock lock(parkedMutex);
            parkedQueue.insert(std::upper_bound(parkedQueue.begin(), parkedQueue.end(), thread->priority.load(), type::KThread::IsHigherPriority), thread);
            parkedFrontCondition.wait(lock, [&]() { return parkedQueue.front() == thread && thread->coreId != constant::ParkedCoreId; });
        }

        InsertThread(thread);
    }

    void Scheduler::WakeParkedThread() {
        std::unique_lock parkedLock(parkedMutex);
        if (!parkedQueue.empty()) {
            auto &thread{state.thread};
            auto &core{cores.at(thread->coreId)};
            std::unique_lock coreLock(core.mutex);
            auto nextThread{core.queue.size() > 1 ? *std::next(core.queue.begin()) : nullptr};
            nextThread = nextThread->priority == thread->priority ? nextThread : nullptr; // If the next thread doesn't have the same priority then it won't be scheduled next
            auto parkedThread{parkedQueue.front()};

            // We need to be conservative about waking up a parked thread, it should only be done if it's priority is higher than the current thread
            // Alternatively, it should be done if it's priority is equivalent to the current thread's priority but the next thread had been scheduled prior or if there is no next thread (Current thread would be rescheduled)
            if (parkedThread->priority < thread->priority || (parkedThread->priority == thread->priority && (!nextThread || parkedThread->timesliceStart < nextThread->timesliceStart))) {
                parkedThread->coreId = thread->coreId;
                parkedLock.unlock();
                parkedFrontCondition.notify_all();
            }
        }
    }

    void Scheduler::RemoveThread() {
        auto &thread{state.thread};
        auto &core{cores.at(thread->coreId)};
        {
            std::unique_lock lock(core.mutex);
            auto it{std::find(core.queue.begin(), core.queue.end(), thread)};
            if (it != core.queue.end()) {
                it = core.queue.erase(it);
                if (it == core.queue.begin()) {
                    // We need to update the averageTimeslice accordingly, if we've been unscheduled by this
                    if (thread->timesliceStart)
                        thread->averageTimeslice = (thread->averageTimeslice / 4) + (3 * (util::GetTimeTicks() - thread->timesliceStart / 4));

                    if (it != core.queue.end())
                        core.frontCondition.notify_all(); // We need to wake the thread at the front of the queue, if we were at the front previously
                }
            }
        }

        if (thread->isPreempted) {
            struct itimerspec spec{};
            timer_settime(thread->preemptionTimer, 0, &spec, nullptr);
            thread->isPreempted = false;
        }

        YieldPending = false;
    }
}
