#include "UI/NotificationManager.h"
#include "UI/PrismaUIManager.h"
#include "Core/Settings.h"
#include <algorithm>

namespace FollowerUnbound
{
    namespace
    {
        // Minimum time interval between HUD messages delivered to the screen
        constexpr float kMinMessageInterval = 2.0f;
        // Maximum ready queue size to prevent backlog accumulation
        constexpr std::size_t kMaxQueueSize = 4;
        // Batch window for consolidating related events
        constexpr auto kBatchWindow = std::chrono::milliseconds(2200);

        std::string ToLower(std::string_view s)
        {
            std::string res(s);
            std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return res;
        }

        std::string ExtractFollowerName(std::string_view message)
        {
            const auto colonPos = message.find(':');
            if (colonPos != std::string_view::npos && colonPos < 30) {
                return std::string(message.substr(0, colonPos));
            }
            const auto spacePos = message.find(' ');
            if (spacePos != std::string_view::npos && spacePos < 30) {
                return std::string(message.substr(0, spacePos));
            }
            return {};
        }
    }

    NotificationCategory NotificationManager::ClassifyMessage(std::string_view message) const
    {
        const std::string lower = ToLower(message);

        // 1. Critical alerts — always immediate
        if (lower.find("health potion!") != std::string::npos ||
            lower.find("daha fazla eşya taşıyamam") != std::string::npos ||
            lower.find("can't carry") != std::string::npos ||
            lower.find("cannot carry") != std::string::npos ||
            lower.find("kuşanıldı") != std::string::npos ||
            lower.find("equipped ") != std::string::npos ||
            lower.find("starving") != std::string::npos) {
            return NotificationCategory::Critical;
        }

        // 2. Needs
        if (lower.find("consumed ") != std::string::npos ||
            lower.find("ate ") != std::string::npos ||
            lower.find("drank ") != std::string::npos ||
            lower.find("hunger:") != std::string::npos ||
            lower.find("thirst:") != std::string::npos ||
            lower.find("açlık:") != std::string::npos ||
            lower.find("susuzluk:") != std::string::npos) {
            return NotificationCategory::Needs;
        }

        // 3. Trade
        if (lower.find(" sold ") != std::string::npos ||
            lower.find(" purchased ") != std::string::npos ||
            lower.find(" bought ") != std::string::npos ||
            lower.find(" sattı") != std::string::npos ||
            lower.find(" satın aldı") != std::string::npos) {
            return NotificationCategory::Trade;
        }

        // 4. Resources
        if (lower.find("maden ") != std::string::npos ||
            lower.find("odun ") != std::string::npos ||
            lower.find("mined ") != std::string::npos ||
            lower.find("firewood") != std::string::npos ||
            lower.find("bonus altın") != std::string::npos ||
            lower.find("gold given") != std::string::npos ||
            lower.find("received ") != std::string::npos) {
            return NotificationCategory::Resource;
        }

        return NotificationCategory::General;
    }

    void NotificationManager::ShowHUDMessage(std::string_view message, NotificationCategory category)
    {
        if (message.empty()) return;
        auto* settings = Settings::GetSingleton();
        if (!settings->enableNotifications) return;

        if (category == NotificationCategory::General) {
            category = ClassifyMessage(message);
        }

        const uint32_t mode = settings->notificationMode;

        // Mode 2: Critical Only — completely suppress non-critical notifications
        if (mode == 2) {
            if (category != NotificationCategory::Critical) return;
            std::lock_guard lock(_mutex);
            EnqueueFinalMessage(std::string(message));
            return;
        }

        // Mode 1: Detailed — enqueue all individual messages directly without batching
        if (mode == 1) {
            std::lock_guard lock(_mutex);
            EnqueueFinalMessage(std::string(message));
            return;
        }

        // Mode 0: Batched / Digest (Default)
        std::lock_guard lock(_mutex);
        const auto now = std::chrono::steady_clock::now();

        if (category == NotificationCategory::Critical) {
            EnqueueFinalMessage(std::string(message));
            return;
        }

        if (category == NotificationCategory::Needs) {
            _needsDigest.active = true;
            if (_needsDigest.firstSeen.time_since_epoch().count() == 0) {
                _needsDigest.firstSeen = now;
            }
            std::string name = ExtractFollowerName(message);
            if (name.empty()) name = "Companion";
            const std::string lower = ToLower(message);
            if (lower.find("drank") != std::string::npos || lower.find("thirst") != std::string::npos) {
                _needsDigest.drankFollowers.insert(name);
            }
            if (lower.find("ate") != std::string::npos || lower.find("hunger") != std::string::npos ||
                lower.find("consumed") != std::string::npos) {
                _needsDigest.ateFollowers.insert(name);
            }
            return;
        }

        if (category == NotificationCategory::Trade) {
            std::string name = ExtractFollowerName(message);
            if (name.empty()) name = "Companion";
            auto& rec = _tradeDigests[name];
            if (rec.firstSeen.time_since_epoch().count() == 0) {
                rec.firstSeen = now;
            }
            const std::string lower = ToLower(message);
            if (lower.find("sold") != std::string::npos) {
                rec.itemsSold++;
            } else if (lower.find("purchased") != std::string::npos || lower.find("bought") != std::string::npos) {
                rec.itemsBought++;
            }
            return;
        }

        if (category == NotificationCategory::Resource) {
            _resourceDigest.active = true;
            if (_resourceDigest.firstSeen.time_since_epoch().count() == 0) {
                _resourceDigest.firstSeen = now;
            }
            const std::string lower = ToLower(message);
            if (lower.find("odun") != std::string::npos || lower.find("firewood") != std::string::npos) {
                _resourceDigest.resources["Firewood"]++;
            } else if (lower.find("maden") != std::string::npos || lower.find("mined") != std::string::npos) {
                _resourceDigest.resources["Ore"]++;
            } else if (lower.find("gold") != std::string::npos || lower.find("altın") != std::string::npos) {
                _resourceDigest.bonusGold += 25; // estimated batch representation
            }
            return;
        }

        // Other messages
        EnqueueFinalMessage(std::string(message));
    }

    void NotificationManager::EnqueueFinalMessage(std::string message)
    {
        if (message.empty()) return;

        // Deduplication: Avoid queuing identical message if already present
        for (const auto& m : _queue) {
            if (m == message) return;
        }

        if (_queue.size() >= kMaxQueueSize) {
            _queue.pop_front();
        }
        _queue.emplace_back(std::move(message));
    }

    void NotificationManager::FlushBatchedDigests(std::chrono::steady_clock::time_point now)
    {
        // 1. Needs Digest
        if (_needsDigest.active && (now - _needsDigest.firstSeen >= kBatchWindow)) {
            std::string msg;
            const bool ate = !_needsDigest.ateFollowers.empty();
            const bool drank = !_needsDigest.drankFollowers.empty();
            const size_t total = std::max(_needsDigest.ateFollowers.size(), _needsDigest.drankFollowers.size());

            if (ate && drank) {
                if (total == 1) {
                    const auto& name = !ate ? *_needsDigest.drankFollowers.begin() : *_needsDigest.ateFollowers.begin();
                    msg = std::format("{} rested and had a meal.", name);
                } else {
                    msg = "Companions rested and replenished supplies.";
                }
            } else if (ate) {
                if (total == 1) msg = std::format("{} ate a meal.", *_needsDigest.ateFollowers.begin());
                else msg = "Companions ate a meal.";
            } else if (drank) {
                if (total == 1) msg = std::format("{} had a drink.", *_needsDigest.drankFollowers.begin());
                else msg = "Companions had a drink.";
            }

            if (!msg.empty()) EnqueueFinalMessage(std::move(msg));
            _needsDigest = {};
        }

        // 2. Trade Digest
        for (auto it = _tradeDigests.begin(); it != _tradeDigests.end();) {
            auto& [follower, rec] = *it;
            if (now - rec.firstSeen >= kBatchWindow) {
                std::string msg;
                if (rec.itemsSold > 0 && rec.itemsBought > 0) {
                    msg = std::format("{} finished trading: sold {} item(s), bought {} item(s).",
                        follower, rec.itemsSold, rec.itemsBought);
                } else if (rec.itemsSold > 0) {
                    msg = std::format("{} sold {} item(s) to the merchant.", follower, rec.itemsSold);
                } else if (rec.itemsBought > 0) {
                    msg = std::format("{} purchased supplies from the merchant.", follower);
                }
                if (!msg.empty()) EnqueueFinalMessage(std::move(msg));
                it = _tradeDigests.erase(it);
            } else {
                ++it;
            }
        }

        // 3. Resource Digest
        if (_resourceDigest.active && (now - _resourceDigest.firstSeen >= kBatchWindow)) {
            std::string msg;
            if (_resourceDigest.resources.contains("Ore")) {
                msg = "Ore mined & shared with companions.";
            } else if (_resourceDigest.resources.contains("Firewood")) {
                msg = "Firewood shared with companions.";
            } else if (_resourceDigest.bonusGold > 0) {
                msg = "Bonus gold shared with companions.";
            } else {
                msg = "Resources shared with companions.";
            }
            EnqueueFinalMessage(std::move(msg));
            _resourceDigest = {};
        }
    }

    void NotificationManager::Update()
    {
        auto* settings = Settings::GetSingleton();
        if (!settings->enableNotifications) {
            std::lock_guard lock(_mutex);
            if (!_queue.empty()) _queue.clear();
            _pendingLoot.clear();
            _needsDigest = {};
            _tradeDigests.clear();
            _resourceDigest = {};
            return;
        }

        std::string next;
        {
            std::lock_guard lock(_mutex);
            const auto now = std::chrono::steady_clock::now();

            // Flush time-elapsed batch digests into _queue
            if (settings->notificationMode == 0) {
                FlushBatchedDigests(now);
            }

            if (_queue.empty()) return;

            if (_lastShown.time_since_epoch().count() > 0 &&
                std::chrono::duration<float>(now - _lastShown).count() < kMinMessageInterval) {
                return; // Rate limit between screen alerts
            }

            next = std::move(_queue.front());
            _queue.pop_front();
            _lastShown = now;
        }

        // Deliver to PrismaUI overlay or fallback to engine notification
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
        auto* settings = Settings::GetSingleton();
        if (!settings->enableNotifications || !settings->notifyLoot) {
            std::lock_guard lock(_mutex);
            _pendingLoot.clear();
            return;
        }

        std::lock_guard lock(_mutex);
        if (_pendingLoot.empty()) return;

        const uint32_t mode = settings->notificationMode;

        // Mode 2: Critical Only — skip loot notifications entirely
        if (mode == 2) {
            _pendingLoot.clear();
            return;
        }

        // Mode 1: Detailed — list individual items per follower
        if (mode == 1) {
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
                EnqueueFinalMessage(std::move(summary));
            }
            _pendingLoot.clear();
            return;
        }

        // Mode 0: Batched Digest — consolidate all followers & items into 1 neat summary
        uint32_t totalItems = 0;
        uint32_t totalGold = 0;
        std::set<std::string> followers;

        for (const auto& [follower, items] : _pendingLoot) {
            followers.insert(follower);
            for (const auto& [item, count] : items) {
                if (item == "Gold" || item == "gold") {
                    totalGold += count;
                } else {
                    totalItems += count;
                }
            }
        }

        if (totalItems == 0 && totalGold == 0) {
            _pendingLoot.clear();
            return;
        }

        std::string followerList;
        for (const auto& f : followers) {
            if (!followerList.empty()) followerList += ", ";
            followerList += f;
        }

        std::string summary;
        if (totalItems > 0 && totalGold > 0) {
            summary = std::format("Party Loot: {} items & {} gold collected ({}).", totalItems, totalGold, followerList);
        } else if (totalItems > 0) {
            summary = std::format("Party Loot: {} items collected ({}).", totalItems, followerList);
        } else if (totalGold > 0) {
            summary = std::format("Party Loot: {} gold collected ({}).", totalGold, followerList);
        }

        EnqueueFinalMessage(std::move(summary));
        _pendingLoot.clear();
    }

    void NotificationManager::Clear()
    {
        std::lock_guard lock(_mutex);
        _queue.clear();
        _pendingLoot.clear();
        _needsDigest = {};
        _tradeDigests.clear();
        _resourceDigest = {};
        _lastShown = {};
    }
}
