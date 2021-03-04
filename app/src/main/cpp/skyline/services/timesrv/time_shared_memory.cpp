// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "core.h"
#include "time_manager_server.h"

namespace skyline::service::timesrv::core {
    struct __attribute__((packed)) TimeSharedMemoryLayout {
        template<typename T>
        struct ClockContextEntry {
            u32 updateCount;
            u32 _pad_;
            std::array<T, 2> context;
        };

        ClockContextEntry<SteadyClockTimePoint> standardSteadyClockContextEntry;
        ClockContextEntry<SystemClockContext> localSystemClockContextEntry;
        ClockContextEntry<SystemClockContext> networkSystemClockContextEntry;
        struct __attribute__((packed)) {
            u32 updateCount;
            std::array<u8, 2> enabled;
        } standardUserSystemClockAutomaticCorrectionEnabledEntry;
    };
    static_assert(offsetof(TimeSharedMemoryLayout, localSystemClockContextEntry) == 0x38);
    static_assert(offsetof(TimeSharedMemoryLayout, networkSystemClockContextEntry) == 0x80);
    static_assert(offsetof(TimeSharedMemoryLayout, standardUserSystemClockAutomaticCorrectionEnabledEntry) == 0xC8);

    /**
     * @brief Time Shared Memory uses a double buffered format that alternates writes context data, this is a helper to simplify that
     */
    template<typename T>
    static void UpdateTimeSharedMemoryItem(u32 &updateCount, std::array<T, 2> &item, const T &newValue) {
        u32 newCount{updateCount + 1};
        item[newCount & 1] = newValue;
        asm volatile("DMB ISHST"); // 0xA
        updateCount = newCount;
    }

    /**
     * @brief Waits for Time Shared Memory to settle then returns the latest version of the requested value
     */
    template<typename T>
    static T ReadTimeSharedMemoryItem(u32 &updateCount, std::array<T, 2> &item) {
        u32 checkUpdateCount{};
        T out{};

        do {
            checkUpdateCount = updateCount;
            out = item[updateCount & 1];
            asm volatile("DMB ISHLD"); // 0x9
        } while (checkUpdateCount != updateCount);

        return out;
    }

    constexpr size_t TimeSharedMemorySize{0x1000}; //!< The size of the time shared memory region

    TimeSharedMemory::TimeSharedMemory(const DeviceState &state) : kTimeSharedMemory(std::make_shared<kernel::type::KSharedMemory>(state, TimeSharedMemorySize)), timeSharedMemory(reinterpret_cast<TimeSharedMemoryLayout *>(kTimeSharedMemory->kernel.ptr)) {}

    void TimeSharedMemory::SetupStandardSteadyClock(UUID rtcId, TimeSpanType baseTimePoint) {
        SteadyClockTimePoint context{
            .timePoint = baseTimePoint.Nanoseconds() - static_cast<i64>(util::GetTimeNs()),
            .clockSourceId = rtcId
        };

        UpdateTimeSharedMemoryItem(timeSharedMemory->standardSteadyClockContextEntry.updateCount, timeSharedMemory->standardSteadyClockContextEntry.context, context);
    }

    void TimeSharedMemory::SetSteadyClockRawTimePoint(TimeSpanType timePoint) {
        auto context{ReadTimeSharedMemoryItem(timeSharedMemory->standardSteadyClockContextEntry.updateCount, timeSharedMemory->standardSteadyClockContextEntry.context)};
        context.timePoint = timePoint.Nanoseconds() - static_cast<i64>(util::GetTimeNs());

        UpdateTimeSharedMemoryItem(timeSharedMemory->standardSteadyClockContextEntry.updateCount, timeSharedMemory->standardSteadyClockContextEntry.context, context);
    }

    void TimeSharedMemory::UpdateLocalSystemClockContext(const SystemClockContext &context) {
        UpdateTimeSharedMemoryItem(timeSharedMemory->localSystemClockContextEntry.updateCount, timeSharedMemory->localSystemClockContextEntry.context, context);
    }

    void TimeSharedMemory::UpdateNetworkSystemClockContext(const SystemClockContext &context) {
        UpdateTimeSharedMemoryItem(timeSharedMemory->networkSystemClockContextEntry.updateCount, timeSharedMemory->networkSystemClockContextEntry.context, context);
    }

    void TimeSharedMemory::SetStandardUserSystemClockAutomaticCorrectionEnabled(bool enabled) {
        UpdateTimeSharedMemoryItem(timeSharedMemory->standardUserSystemClockAutomaticCorrectionEnabledEntry.updateCount, timeSharedMemory->standardUserSystemClockAutomaticCorrectionEnabledEntry.enabled, static_cast<u8>(enabled));
    }

    bool SystemClockContextUpdateCallback::UpdateBaseContext(const SystemClockContext &newContext) {
        if (context && context == newContext)
            return false;

        context = newContext;
        return true;

    }

    void SystemClockContextUpdateCallback::SignalOperationEvent() {
        std::lock_guard lock(mutex);

        for (const auto &event : operationEventList)
            event->Signal();
    }

    void SystemClockContextUpdateCallback::AddOperationEvent(const std::shared_ptr<kernel::type::KEvent> &event) {
        std::lock_guard lock(mutex);

        operationEventList.push_back(event);
    }

    LocalSystemClockUpdateCallback::LocalSystemClockUpdateCallback(TimeSharedMemory &timeSharedMemory) : timeSharedMemory(timeSharedMemory) {}

    Result LocalSystemClockUpdateCallback::UpdateContext(const SystemClockContext &newContext) {
        // No need to update shmem state redundantly
        if (!UpdateBaseContext(newContext))
            return {};

        timeSharedMemory.UpdateLocalSystemClockContext(newContext);

        SignalOperationEvent();
        return {};
    }

    NetworkSystemClockUpdateCallback::NetworkSystemClockUpdateCallback(TimeSharedMemory &timeSharedMemory) : timeSharedMemory(timeSharedMemory) {}

    Result NetworkSystemClockUpdateCallback::UpdateContext(const SystemClockContext &newContext) {
        // No need to update shmem state redundantly
        if (!UpdateBaseContext(newContext))
            return {};

        timeSharedMemory.UpdateNetworkSystemClockContext(newContext);

        SignalOperationEvent();
        return {};
    }

    Result EphemeralNetworkSystemClockUpdateCallback::UpdateContext(const SystemClockContext &newContext) {
        // Avoid signalling the event when there is no change in context
        if (!UpdateBaseContext(newContext))
            return {};

        SignalOperationEvent();
        return {};
    }
}