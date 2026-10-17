#pragma once
#include "PCH.h"
#include "Core/FollowerManager.h"

namespace FollowerUnbound
{
    class LootManager
    {
    public:
        static LootManager* GetSingleton()
        {
            static LootManager instance;
            return &instance;
        }

        // Reads a container's remaining inventory and queues balanced transfers.
        // Returns true if processed/looted (or skipped/empty), false if followers are not currently eligible (retry later).
        bool ProcessContainerLoot(RE::TESObjectREFR* containerRef);

        // Called only after the player closes a corpse's container menu.
        void QueuePlayerLootDistribution(RE::FormID corpseID);
        void ProcessPlayerLootDistributions();

        // True while a player-requested distribution or item transfer remains.
        bool HasPendingLootWork();

        // Flushes one pending item move per update to keep engine inventory
        // mutations isolated to a single UI task.
        void FlushPendingTransfers();

        // RenderBackgroundTick only requests work. Engine operations are performed
        // later through SKSE's UI task queue.
        void ScheduleProcessing();

    private:
        // Batched transfer queue.
        struct PendingTransfer
        {
            uint64_t    transferID{ 0 };
            RE::FormID  itemFormID{ 0 };
            RE::FormID  actorFormID{ 0 };   // receiving follower
            RE::FormID  sourceFormID{ 0 };  // corpse/container
            int32_t     count{ 1 };
            std::chrono::steady_clock::time_point readyAt{};
            bool        isGold{ false };
            bool        autoEquip{ false };
            std::string followerName;
            std::string itemName;
        };
        static constexpr int kMaxTransfersPerTick = 1;

        LootManager() = default;

        static bool IsLootableItem(RE::TESBoundObject* item);
        static bool IsQuestItem(RE::TESObjectREFR* ref);

        struct QueuedCorpse
        {
            RE::FormID formID{ 0 };
            std::chrono::steady_clock::time_point availableAt{};
        };
        std::deque<QueuedCorpse> _playerLootRequests;
        size_t _nextRecipientOffset{ 0 };
        std::mutex _lootMutex;

        // Batched item transfer queue
        std::vector<PendingTransfer> _pendingTransfers;
        std::mutex _transferMutex;
        std::atomic_bool _processingScheduled{ false };
        std::atomic_uint64_t _nextTransferID{ 1 };
    };
}
