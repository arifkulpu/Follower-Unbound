#include "UI/NotificationManager.h"
#include "UI/PrismaUIManager.h"
#include "Core/Settings.h"

namespace FollowerUnbound
{
    namespace
    {
        // İki HUD mesajı arasındaki minimum süre (saniye) — ard arda spam ve çökme önlenir
        constexpr float kMinMessageInterval = 2.5f;
        // Kuyruk büyüklük sınırı — eski mesajlar düşürülür (yığılma engellenir)
        constexpr std::size_t kMaxQueueSize = 6;
    }

    void NotificationManager::ShowHUDMessage(std::string_view message)
    {
        if (message.empty()) return;
        if (!Settings::GetSingleton()->enableNotifications) return;

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
        if (!Settings::GetSingleton()->enableNotifications) {
            std::lock_guard lock(_mutex);
            if (!_queue.empty()) _queue.clear();
            return;
        }

        std::string next;
        {
            std::lock_guard lock(_mutex);
            if (_queue.empty()) return;

            auto now = std::chrono::steady_clock::now();
            if (_lastShown.time_since_epoch().count() > 0 &&
                std::chrono::duration<float>(now - _lastShown).count() < kMinMessageInterval) {
                return; // hız sınırı: en fazla 1 mesaj / 2.5 saniye
            }

            next = std::move(_queue.front());
            _queue.pop_front();
            _lastShown = now;
        }

        // Öncelik PrismaUI bildirim katmanı; PrismaUI yoksa veya hazır değilse
        // güvenli bir şekilde standart Skyrim bildirimine düşülür. Asla kuyruğa geri
        // atılıp per-frame döngüye sokulmaz (oyun çökmesini önler).
        if (!PrismaUIManager::GetSingleton()->ShowNotification(next)) {
            RE::DebugNotification(next.c_str());
        }
    }

    void NotificationManager::QueueLootNotification(std::string_view followerName, std::string_view itemName, uint32_t count)
    {
        if (!Settings::GetSingleton()->enableNotifications) return;
        std::lock_guard lock(_mutex);
        _pendingLoot[std::string(followerName)].emplace_back(std::string(itemName), count);
    }

    void NotificationManager::FlushLootNotifications()
    {
        if (!Settings::GetSingleton()->enableNotifications) {
            std::lock_guard lock(_mutex);
            _pendingLoot.clear();
            return;
        }

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

    void NotificationManager::Clear()
    {
        std::lock_guard lock(_mutex);
        _queue.clear();
        _pendingLoot.clear();
        _lastShown = {};
    }
}
