#pragma once
#include "PCH.h"

namespace FollowerUnbound
{
    class DroppedItemRecovery
    {
    public:
        static DroppedItemRecovery* GetSingleton()
        {
            static DroppedItemRecovery instance;
            return &instance;
        }

        // Event sinks only record handles and IDs; engine inventory work is
        // deferred to the UI task queue after combat has ended.
        void RecordDrop(RE::FormID followerID, RE::FormID itemID, std::int32_t count, RE::ObjectRefHandle droppedRef);
        void RecordWorldLoot(RE::FormID followerID, RE::FormID itemID, RE::ObjectRefHandle droppedRef,
            float maxDistance = 1400.0f);
        void ScheduleProcessing();

    private:
        struct DroppedItem
        {
            RE::FormID followerID{ 0 };
            RE::FormID itemID{ 0 };
            std::int32_t count{ 0 };
            RE::ObjectRefHandle droppedRef;
            std::chrono::steady_clock::time_point recordedAt;
            bool lootAnimationPlayed{ false };
            std::chrono::steady_clock::time_point pickupReadyAt{};
            float maxDistance{ 3500.0f };
        };

        DroppedItemRecovery() = default;
        void ProcessOne();

        std::mutex _mutex;
        std::deque<DroppedItem> _droppedItems;
        std::atomic_bool _processingScheduled{ false };
    };
}
