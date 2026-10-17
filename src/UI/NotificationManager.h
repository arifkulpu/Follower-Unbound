#pragma once
#include "PCH.h"

namespace FollowerUnbound
{
    class NotificationManager
    {
    public:
        static NotificationManager* GetSingleton()
        {
            static NotificationManager instance;
            return &instance;
        }

        // HUD mesajını DOĞRUDAN göstermez — kuyruğa ekler. Böylece bekleme/
        // uyuma sonrası biriken mesajlar üst üste binmez; saniyede en fazla
        // bir mesaj ekranda gösterilir.
        void ShowHUDMessage(std::string_view message);

        // Her frame çağrılır (RenderBackgroundTick) — kuyruktan hız sınırlı
        // şekilde mesaj gösterir.
        void Update();

        void QueueLootNotification(std::string_view followerName, std::string_view itemName, uint32_t count);
        void FlushLootNotifications();
        void Clear();

    private:
        NotificationManager() = default;

        std::mutex _mutex;
        std::map<std::string, std::vector<std::pair<std::string, uint32_t>>> _pendingLoot;

        std::deque<std::string> _queue;
        std::chrono::steady_clock::time_point _lastShown{};
    };
}
