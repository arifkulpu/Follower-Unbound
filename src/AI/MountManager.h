#pragma once
#include "PCH.h"
#include <chrono>
#include <unordered_map>

namespace FollowerUnbound
{
    // Follower Mount Management (A Horse's Life / Vanilla / Custom Follower Mounts)
    // - Follower mounts when player mounts.
    // - Follower stays mounted during combat if player is mounted (Mounted Combat).
    // - Follower dismounts when player dismounts.
    class MountManager
    {
    public:
        static MountManager* GetSingleton()
        {
            static MountManager instance;
            return &instance;
        }

        // Periodic update called from game thread tick
        void Update();

        // Finds the assigned horse for a follower (Checks A Horse's Life ownership, faction ownership, and last ridden)
        RE::Actor* FindAssignedHorse(RE::Actor* follower);

        // Mounts the follower to the specified horse
        bool MountHorse(RE::Actor* follower, RE::Actor* horse);

        // Dismounts the follower
        bool DismountHorse(RE::Actor* follower);

    private:
        MountManager() = default;

        std::chrono::steady_clock::time_point _lastUpdate{};
        bool _playerMountedLastTick{ false };

        // Throttle mounting attempts so we don't spam activate
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastMountAttempt;
    };
}
