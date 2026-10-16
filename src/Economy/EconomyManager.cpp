#include "Economy/EconomyManager.h"
#include "UI/NotificationManager.h"

namespace FollowerUnbound
{
    bool EconomyManager::AddGold(RE::FormID followerID, int32_t amount)
    {
        if (amount <= 0) return false;
        auto profile = FollowerManager::GetSingleton()->GetProfile(followerID);
        if (!profile) return false;

        const auto credit = static_cast<uint32_t>(amount);
        profile->gold = std::min<uint32_t>(std::numeric_limits<uint32_t>::max() - credit, profile->gold) + credit;
        auto& pref = profile->preferences;
        if (pref.savingsEnabled && pref.savingsTarget > pref.savedGold && pref.savingsPercent > 0) {
            const uint64_t progress = static_cast<uint64_t>(credit) * pref.savingsPercent + pref.savingsRemainder;
            const uint64_t planned = progress / 100u;
            pref.savingsRemainder = static_cast<uint32_t>(progress % 100u);
            const uint32_t room = pref.savingsTarget - pref.savedGold;
            const auto saved = static_cast<uint32_t>(std::min<uint64_t>(planned, room));
            pref.savedGold += saved;
            if (pref.savedGold >= pref.savingsTarget) pref.savingsRemainder = 0;
        }
        return true;
    }

    bool EconomyManager::RemoveGold(RE::FormID followerID, int32_t amount)
    {
        if (amount <= 0) return false;
        auto profile = FollowerManager::GetSingleton()->GetProfile(followerID);
        if (!profile) return false;

        if (profile->gold < static_cast<uint32_t>(amount)) {
            return false;
        }

        profile->gold -= static_cast<uint32_t>(amount);
        return true;
    }

    int32_t EconomyManager::GetGold(RE::FormID followerID)
    {
        auto profile = FollowerManager::GetSingleton()->GetProfile(followerID);
        return profile ? static_cast<int32_t>(profile->gold) : 0;
    }

    bool EconomyManager::PlayerGiveGold(RE::Actor* follower, int32_t amount)
    {
        if (!follower || amount <= 0) return false;
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;

        auto goldItem = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F); // Gold001
        if (!goldItem) return false;

        int32_t playerGold = player->GetItemCount(goldItem);
        if (playerGold < amount) {
            NotificationManager::GetSingleton()->ShowHUDMessage("You don't have enough gold.");
            return false;
        }

        // Remove from player inventory
        player->RemoveItem(goldItem, amount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

        // Add to follower economy (virtual counter)
        AddGold(follower->GetFormID(), amount);

        // Add to follower's PHYSICAL inventory so it shows in menu
        follower->AddObjectToContainer(goldItem, nullptr, amount, nullptr);

        std::string msg = std::format("Gave {} gold to {}.", amount, follower->GetName());
        NotificationManager::GetSingleton()->ShowHUDMessage(msg);
        logger::info("Player gave {} gold to {:08X}", amount, follower->GetFormID());
        return true;
    }

    void EconomyManager::DistributeReward(int32_t totalReward, float followerSharePercentage)
    {
        if (totalReward <= 0) return;
        auto followers = FollowerManager::GetSingleton()->GetActiveFollowers();
        if (followers.empty()) return;

        float totalShareRatio = std::clamp(followerSharePercentage / 100.0f, 0.0f, 1.0f);
        int32_t totalFollowerGold = static_cast<int32_t>(totalReward * totalShareRatio);
        if (totalFollowerGold <= 0) return;

        int32_t sharePerFollower = totalFollowerGold / static_cast<int32_t>(followers.size());
        if (sharePerFollower <= 0) return;

        for (auto follower : followers) {
            AddGold(follower->GetFormID(), sharePerFollower);
            if (auto goldItem = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F)) {
                follower->AddObjectToContainer(goldItem, nullptr, sharePerFollower, nullptr);
            }
            std::string msg = std::format("{} received {} gold as their quest share.", follower->GetName(), sharePerFollower);
            NotificationManager::GetSingleton()->ShowHUDMessage(msg);
        }
    }
}
