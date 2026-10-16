#include "UI/NotificationManager.h"

namespace FollowerUnbound
{
    namespace
    {
        // İki HUD mesajı arasındaki minimum süre (saniye)
        constexpr float kMinMessageInterval = 2.0f;
        // Kuyruk büyüklük sınırı — eski mesajlar düşürülür (spam birikmesi engellenir)
        constexpr std::size_t kMaxQueueSize = 8;
    }

    void NotificationManager::ShowHUDMessage(std::string_view message)
    {
        std::lock_guard lock(_mutex);

        // Aynı mesaj zaten kuyrukta ise tekrar ekleme (birleştirme)
        for (const auto& m : _queue) {
            if (m == message) return;
        }

        if (_queue.size() >= kMaxQueueSize) {
            _queue.pop_front(); // en eski mesajı düşür
        }
        _queue.emplace_back(message);
    }

    void NotificationManager::Update()
    {
        std::string next;
        {
            std::lock_guard lock(_mutex);
            if (_queue.empty()) return;

            auto now = std::chrono::steady_clock::now();
            if (_lastShown.time_since_epoch().count() > 0 &&
                std::chrono::duration<float>(now - _lastShown).count() < kMinMessageInterval) {
                return; // hız sınırı: en fazla 1 mesaj / 2 saniye
            }

            next = std::move(_queue.front());
            _queue.pop_front();
            _lastShown = now;
        }

        RE::DebugNotification(next.c_str());
    }

    void NotificationManager::QueueLootNotification(std::string_view followerName, std::string_view itemName, uint32_t count)
    {
        std::lock_guard lock(_mutex);
        _pendingLoot[std::string(followerName)].emplace_back(std::string(itemName), count);
    }

    void NotificationManager::FlushLootNotifications()
    {
        std::lock_guard lock(_mutex);
        if (_pendingLoot.empty()) return;

        for (const auto& [follower, items] : _pendingLoot) {
            std::string summary = follower + " looted: ";
            bool first = true;
            for (const auto& [item, count] : items) {
                if (!first) summary += ", ";
                summary += item;
                if (count > 1) {
                    summary += " x" + std::to_string(count);
                }
                first = false;
            }
            // Kuyruğa eklenir — doğrudan gösterilmez (yığılma önlenir)
            if (_queue.size() >= kMaxQueueSize) {
                _queue.pop_front();
            }
            _queue.emplace_back(std::move(summary));
        }

        _pendingLoot.clear();
    }
}
