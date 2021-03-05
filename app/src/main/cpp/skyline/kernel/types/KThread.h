// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <csetjmp>
#include <nce/guest.h>
#include <kernel/scheduler.h>
#include <common/signal.h>
#include "KSyncObject.h"
#include "KPrivateMemory.h"
#include "KSharedMemory.h"

namespace skyline {
    namespace kernel::type {
        /**
         * @brief KThread manages a single thread of execution which is responsible for running guest code and kernel code which is invoked by the guest
         */
        class KThread : public KSyncObject, public std::enable_shared_from_this<KThread> {
          private:
            KProcess *parent;
            std::thread thread; //!< If this KThread is backed by a host thread then this'll hold it
            pthread_t pthread{}; //!< The pthread_t for the host thread running this guest thread

            void StartThread();

          public:
            std::mutex statusMutex; //!< Synchronizes all thread state changes, running/ready
            std::condition_variable statusCondition; //!< A conditional variable signalled on the status of the thread changing
            bool running{false}; //!< If the host thread that corresponds to this thread is running, this doesn't reflect guest scheduling changes
            bool killed{false}; //!< If this thread was previously running and has been killed
            bool ready{false}; //!< If this thread is ready to recieve signals or not

            KHandle handle;
            size_t id; //!< Index of thread in parent process's KThread vector

            nce::ThreadContext ctx{}; //!< The context of the guest thread during the last SVC
            jmp_buf originalCtx; //!< The context of the host thread prior to jumping into guest code

            void *entry;
            u64 entryArgument;
            void *stackTop;

            std::atomic<u8> basePriority; //!< The priority of the thread for the scheduler without any priority-inheritance
            std::atomic<u8> priority; //!< The priority of the thread for the scheduler
            i8 idealCore; //!< The ideal CPU core for this thread to run on
            i8 coreId; //!< The CPU core on which this thread is running
            CoreMask affinityMask{}; //!< A mask of CPU cores this thread is allowed to run on
            std::mutex coreMigrationMutex; //!< Synchronizes operations which depend on which core the thread is running on
            u64 timesliceStart{}; //!< Start of the scheduler timeslice
            u64 averageTimeslice{}; //!< A weighted average of the timeslice duration for this thread
            timer_t preemptionTimer{}; //!< A kernel timer used for preemption interrupts
            bool isPreempted{}; //!< If the preemption timer has been armed and will fire
            bool pendingYield{}; //!< If the current thread has been yielded and hasn't been acted upon it yet
            bool forceYield{}; //!< If the thread has been forcefully yielded by another thread
            std::mutex waiterMutex; //!< Synchronizes operations on mutation of the waiter members
            u32* waitKey; //!< The key of the mutex which this thread is waiting on
            KHandle waitTag; //!< The handle of the thread which requested the mutex lock
            std::shared_ptr<KThread> waitThread; //!< The thread which this thread is waiting on
            std::list<std::shared_ptr<type::KThread>> waiters; //!< A queue of threads waiting on this thread sorted by priority
            bool isCancellable{false}; //!< If the thread is currently in a position where it is cancellable
            bool cancelSync{false}; //!< If to cancel a SvcWaitSynchronization call this thread currently is in/the next one it joins
            type::KSyncObject* wakeObject{}; //!< A pointer to the synchronization object responsible for waking this thread up

            KThread(const DeviceState &state, KHandle handle, KProcess *parent, size_t id, void *entry, u64 argument, void *stackTop, u8 priority, i8 idealCore);

            ~KThread();

            /**
             * @param self If the calling thread should jump directly into guest code or if a new thread should be created for it
             * @note If the thread is already running then this does nothing
             * @note 'stack' will be created if it wasn't set prior to calling this
             */
            void Start(bool self = false);

            /**
             * @param join Return after the thread has joined rather than instantly
             */
            void Kill(bool join);

            /**
             * @brief Sends a host OS signal to the thread which is running this KThread
             */
            void SendSignal(int signal);

            /**
             * @return If the supplied priority value is higher than the current thread
             */
            static constexpr bool IsHigherPriority(const i8 priority, const std::shared_ptr<type::KThread> &it) {
                return priority < it->priority;
            }
        };
    }
}
