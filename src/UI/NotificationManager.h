#pragma once
#include "PCH.h"
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <mutex>
#include <chrono>

namespace FollowerUnbound
{
    enum class NotificationCategory
    {
        General,     // Normal message
        Critical,    // Health potions in combat, encumbrance full, warnings (always shown immediately)
        Loot,        // Item/gold looting
        Needs,       // Eating / drinking / hunger
        Trade,       // Buying / selling
        Resource     // Mining, woodchopping, shared gold bags
    };

    class NotificationManager
    {
    public:
        static NotificationManager* GetSingleton()
        {
            static NotificationManager instance;
            return &instance;
        }

        // Enqueues a notification with optional category classification.
        void ShowHUDMessage(std::string_view message, NotificationCategory category = NotificationCategory::General);

        // Called on background render tick to flush rate-limited notifications and batched digests.
        void Update();

        void QueueLootNotification(std::string_view followerName, std::string_view itemName, uint32_t count);
        void FlushLootNotifications();

        void Clear();

    private:
        NotificationManager() = default;

        NotificationCategory ClassifyMessage(std::string_view message) const;
        void FlushBatchedDigests(std::chrono::steady_clock::time_point now);
        void EnqueueFinalMessage(std::string message);

        std::mutex _mutex;

        // Pending loot raw records: followerName -> vector of (itemName, count)
        std::map<std::string, std::vector<std::pair<std::string, uint32_t>>> _pendingLoot;

        // Needs digest buffer
        struct NeedsDigest
        {
            std::set<std::string> ateFollowers;
            std::set<std::string> drankFollowers;
            std::chrono::steady_clock::time_point firstSeen{};
            bool active{ false };
        } _needsDigest;

        // Trade digest buffer: followerName -> TradeRecord
        struct TradeRecord
        {
            std::string vendorName;
            uint32_t itemsSold{ 0 };
            uint32_t goldEarned{ 0 };
            uint32_t itemsBought{ 0 };
            uint32_t goldSpent{ 0 };
            std::string lastSoldItem;
            std::string lastBoughtItem;
            std::chrono::steady_clock::time_point firstSeen{};
        };
        std::map<std::string, TradeRecord> _tradeDigests;

        // Resource digest buffer
        struct ResourceDigest
        {
            std::map<std::string, uint32_t> resources; // resourceName -> count
            uint32_t bonusGold{ 0 };
            std::chrono::steady_clock::time_point firstSeen{};
            bool active{ false };
        } _resourceDigest;

        // Output queue of ready-to-display HUD strings
        std::deque<std::string> _queue;
        std::chrono::steady_clock::time_point _lastShown{};
    };
}
