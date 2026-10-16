#include "AI/CombatTracker.h"
#include "Core/FollowerManager.h"
#include "Core/Settings.h"

namespace FollowerUnbound
{
    bool CombatTracker::IsTrackedFriendly(RE::Actor* actor)
    {
        if (!actor || actor->IsPlayerRef()) return actor != nullptr;
        return FollowerManager::GetSingleton()->IsFollower(actor->GetFormID());
    }

    bool CombatTracker::NotifyCombatStart(RE::Actor* actor, RE::Actor* target)
    {
        if (!actor) return false;

        // Düşman kaydı: friend'ler arasındaki savaşlar (takipçi↔takipçi, oyuncu↔takipçi)
        // yağma yüzünden işaretlenmez; yalnızca friend ↔ dış aktör çatışmaları işaretlenir.
        const bool actorFriendly = IsTrackedFriendly(actor);
        const bool targetFriendly = target && IsTrackedFriendly(target);
        const bool involvesFriendly = actorFriendly || targetFriendly;
        std::lock_guard lock(_mutex);
        const auto now = std::chrono::steady_clock::now();
        const bool newEncounter = involvesFriendly &&
            (_lastCombatActivity.time_since_epoch().count() == 0 ||
             now - _lastCombatActivity >= std::chrono::seconds(8));
        if (involvesFriendly) {
            _lastCombatActivity = now;
        }

        if (actorFriendly && target && !targetFriendly) {
            _hostileParticipants.insert(target->GetFormID());
        } else if (!actorFriendly && targetFriendly) {
            _hostileParticipants.insert(actor->GetFormID());
        }

        if (_hostileParticipants.size() > kMaxCasualties) {
            _hostileParticipants.clear();
        }
        return newEncounter;
    }

    void CombatTracker::NotifyCombatEnd(RE::Actor* actor)
    {
        // Kimse (oyuncu/takipçi) savaşta değilse participant listesini boşalt.
        // Casualty listesi (cesetler) korunur — yağma savaş bittikten SONRA yapılır.
        if (!actor) return;

        NotifyCombatActivity(actor);

        auto player = RE::PlayerCharacter::GetSingleton();
        if (player && player->IsInCombat()) return;

        for (auto* follower : FollowerManager::GetSingleton()->GetActiveFollowers()) {
            if (follower && follower->IsInCombat()) return;
        }

        std::lock_guard lock(_mutex);
        _hostileParticipants.clear();
    }

    void CombatTracker::NotifyCombatActivity(RE::Actor* actor, RE::Actor* target)
    {
        if (!actor) return;
        if (!IsTrackedFriendly(actor) && (!target || !IsTrackedFriendly(target))) return;
        std::lock_guard lock(_mutex);
        _lastCombatActivity = std::chrono::steady_clock::now();
    }

    void CombatTracker::NotifyDeath(RE::Actor* deadActor)
    {
        if (!deadActor) return;

        RE::FormID formID = deadActor->GetFormID();
        if (!formID) return;

        std::lock_guard lock(_mutex);
        _lastCombatActivity = std::chrono::steady_clock::now();
        if (_hostileParticipants.contains(formID)) {
            _casualties.insert(formID);
        }
        if (_casualties.size() > kMaxCasualties) {
            _casualties.clear();
        }
    }

    bool CombatTracker::ShouldAutoLoot(RE::Actor* deadActor) const
    {
        auto settings = Settings::GetSingleton();
        if (!settings->lootHostilesOnly) return true;
        if (!deadActor) return false;

        RE::FormID formID = deadActor->GetFormID();

        {
            std::lock_guard lock(_mutex);
            if (_casualties.contains(formID) || _hostileParticipants.contains(formID)) {
                return true;
            }
        }

        // Event kaçırma toleransı: ölüm anında oyuncuya hostile olan aktörler
        // (agresyon ölümle silinmez) yağma adayı sayılır.
        auto player = RE::PlayerCharacter::GetSingleton();
        if (player && deadActor->IsHostileToActor(player)) {
            return true;
        }

        return false;
    }

    bool CombatTracker::IsLootSafe() const
    {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player || player->IsInCombat()) return false;
        for (auto* follower : FollowerManager::GetSingleton()->GetActiveFollowers()) {
            if (follower && follower->IsInCombat()) return false;
        }
        std::lock_guard lock(_mutex);
        return std::chrono::steady_clock::now() - _lastCombatActivity >= std::chrono::seconds(8);
    }
}
