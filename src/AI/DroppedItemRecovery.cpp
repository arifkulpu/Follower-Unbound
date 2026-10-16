#include "AI/DroppedItemRecovery.h"
#include "Core/FollowerManager.h"
#include "AI/AnimationManager.h"
#include "AI/LootManager.h"
#include "UI/EquipPrompt.h"
#include "UI/NotificationManager.h"

namespace FollowerUnbound
{
    void DroppedItemRecovery::RecordDrop(RE::FormID followerID, RE::FormID itemID,
        std::int32_t count, RE::ObjectRefHandle droppedRef)
    {
        if (!followerID || !itemID || count <= 0 || !droppedRef) return;
        std::lock_guard lock(_mutex);
        if (_droppedItems.size() >= 64) {
            logger::warn("DroppedItemRecovery: queue limit reached; oldest dropped-item record discarded.");
            _droppedItems.pop_front();
        }
        DroppedItem record{ followerID, itemID, count, droppedRef, std::chrono::steady_clock::now() };
        record.maxDistance = 3500.0f;
        _droppedItems.push_back(std::move(record));
        logger::info("DroppedItemRecovery: recorded follower={:08X} item={:08X} count={}; will recover after combat.",
            followerID, itemID, count);
    }

    void DroppedItemRecovery::RecordWorldLoot(RE::FormID followerID, RE::FormID itemID,
        RE::ObjectRefHandle droppedRef, float maxDistance)
    {
        if (!followerID || !itemID || !droppedRef || maxDistance <= 0.0f) return;
        std::lock_guard lock(_mutex);
        for (const auto& queued : _droppedItems) {
            if (queued.droppedRef.native_handle() == droppedRef.native_handle()) return;
        }
        if (_droppedItems.size() >= 64) {
            logger::warn("DroppedItemRecovery: queue limit reached; oldest dropped-item record discarded.");
            _droppedItems.pop_front();
        }
        DroppedItem record{ followerID, itemID, 1, droppedRef, std::chrono::steady_clock::now() };
        record.maxDistance = maxDistance;
        _droppedItems.push_back(std::move(record));
        logger::info("DroppedItemRecovery: queued nearby world loot follower={:08X} item={:08X}; radius={:.0f}.",
            followerID, itemID, maxDistance);
    }

    void DroppedItemRecovery::ScheduleProcessing()
    {
        bool expected = false;
        if (!_processingScheduled.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            _processingScheduled.store(false, std::memory_order_release);
            return;
        }
        tasks->AddUITask([this]() {
            ProcessOne();
            _processingScheduled.store(false, std::memory_order_release);
        });
    }

    void DroppedItemRecovery::ProcessOne()
    {
        if (LootManager::GetSingleton()->HasPendingLootWork() || EquipPrompt::HasPendingPrompts()) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || player->IsInCombat()) return;
        auto followers = FollowerManager::GetSingleton()->GetActiveFollowers();
        for (auto* follower : followers) {
            if (follower && !follower->IsDead() && follower->IsInCombat()) return;
        }

        DroppedItem dropped;
        {
            std::lock_guard lock(_mutex);
            if (_droppedItems.empty()) return;
            dropped = _droppedItems.front();
        }

        const auto age = std::chrono::steady_clock::now() - dropped.recordedAt;
        if (age > std::chrono::minutes(3)) {
            std::lock_guard lock(_mutex);
            if (!_droppedItems.empty() && _droppedItems.front().followerID == dropped.followerID &&
                _droppedItems.front().itemID == dropped.itemID) _droppedItems.pop_front();
            logger::info("DroppedItemRecovery: discarded expired record follower={:08X} item={:08X}.",
                dropped.followerID, dropped.itemID);
            return;
        }

        auto* follower = RE::TESForm::LookupByID<RE::Actor>(dropped.followerID);
        auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(dropped.itemID);
        auto groundRef = dropped.droppedRef.get();
        if (!follower || !item || !groundRef || follower->IsDead()) {
            std::lock_guard lock(_mutex);
            if (!_droppedItems.empty() && _droppedItems.front().followerID == dropped.followerID &&
                _droppedItems.front().itemID == dropped.itemID) _droppedItems.pop_front();
            logger::info("DroppedItemRecovery: reference unavailable; discarded follower={:08X} item={:08X}.",
                dropped.followerID, dropped.itemID);
            return;
        }
        if (groundRef->GetParentCell() != follower->GetParentCell()) {
            std::lock_guard lock(_mutex);
            if (!_droppedItems.empty() && _droppedItems.front().followerID == dropped.followerID &&
                _droppedItems.front().itemID == dropped.itemID) _droppedItems.pop_front();
            logger::info("DroppedItemRecovery: skipped item outside follower's current cell; follower={:08X} item={:08X}.",
                dropped.followerID, dropped.itemID);
            return;
        }
        if (follower->IsInCombat()) return;
        if (!follower->Is3DLoaded() || groundRef->GetBaseObject() != item) return;
        if (follower->GetPosition().GetDistance(groundRef->GetPosition()) > dropped.maxDistance) return;

        if (dropped.count <= 0) {
            std::lock_guard lock(_mutex);
            if (!_droppedItems.empty() && _droppedItems.front().followerID == dropped.followerID &&
                _droppedItems.front().itemID == dropped.itemID) _droppedItems.pop_front();
            return;
        }

        // Play the same corpse-search/pickup animation used by the loot feature,
        // then let it finish before the engine transfers the dropped item.
        if (!dropped.lootAnimationPlayed) {
            const bool played = AnimationManager::GetSingleton()->PlayLootAnimation(follower, groundRef.get());
            if (played) {
                std::lock_guard lock(_mutex);
                if (!_droppedItems.empty() && _droppedItems.front().followerID == dropped.followerID &&
                    _droppedItems.front().itemID == dropped.itemID &&
                    _droppedItems.front().recordedAt == dropped.recordedAt) {
                    _droppedItems.front().lootAnimationPlayed = true;
                    _droppedItems.front().pickupReadyAt = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                }
                logger::info("DroppedItemRecovery: loot animation started for follower={:08X} item={:08X}.",
                    dropped.followerID, dropped.itemID);
                return;
            }
            logger::debug("DroppedItemRecovery: loot animation unavailable; using immediate pickup for follower={:08X} item={:08X}.",
                dropped.followerID, dropped.itemID);
        }
        if (dropped.lootAnimationPlayed && std::chrono::steady_clock::now() < dropped.pickupReadyAt) return;

        logger::info("DroppedItemRecovery: picking up {} x{} for follower={:08X}.", item->GetName(), dropped.count, dropped.followerID);
        follower->PickUpObject(groundRef.get(), dropped.count, false, false);
        {
            std::lock_guard lock(_mutex);
            if (!_droppedItems.empty() && _droppedItems.front().followerID == dropped.followerID &&
                _droppedItems.front().itemID == dropped.itemID) _droppedItems.pop_front();
        }
        NotificationManager::GetSingleton()->ShowHUDMessage(
            std::format("{} recovered: {}{}.", follower->GetName(), item->GetName(),
                dropped.count > 1 ? std::format(" x{}", dropped.count) : ""));
    }
}
