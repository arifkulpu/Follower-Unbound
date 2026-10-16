#pragma once
#include "PCH.h"
#include "Core/FollowerManager.h"

namespace FollowerUnbound
{
    class EconomyManager
    {
    public:
        static EconomyManager* GetSingleton()
        {
            static EconomyManager instance;
            return &instance;
        }

        // Add or remove gold from follower
        bool AddGold(RE::FormID followerID, int32_t amount);
        bool RemoveGold(RE::FormID followerID, int32_t amount);
        int32_t GetGold(RE::FormID followerID);

        // Player gives gold to follower directly
        bool PlayerGiveGold(RE::Actor* follower, int32_t amount);

        // Split quest or bounty reward with all active followers
        void DistributeReward(int32_t totalReward, float followerSharePercentage = 30.0f);

    private:
        EconomyManager() = default;
    };
}
