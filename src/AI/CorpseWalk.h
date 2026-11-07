#pragma once
#include "PCH.h"
#include <chrono>
#include <unordered_map>

namespace FollowerUnbound
{
    // A follower looting a dead actor walks up to the body first (Actor.KeepOffsetFromActor, no ESP needed), then the
    // existing LootManager transfer + loot animation run at the body. LootManager asks Check() before it moves an item:
    //   Go   = the follower is at the body (or the walk is not used / failed: the old remote transfer is the fallback)
    //   Wait = still walking, try again on the next tick
    // The 10 s wait before the walk is applied where LootManager queues the corpse (see DelayMs()).
    //
    // Settings: Data/SKSE/Plugins/FollowerUnboundLootWalk.json (changed from PrismaUI: lootWalk, lootWalkDelay).
    // All methods run on the game/UI task thread.
    class CorpseWalk
    {
    public:
        static CorpseWalk* GetSingleton()
        {
            static CorpseWalk instance;
            return &instance;
        }

        struct Config
        {
            bool  enabled{ true };
            float delaySeconds{ 10.0f };       // after a corpse is queued, followers set off this late
            float maxStartDistance{ 2500.0f }; // further off (or another cell): old remote transfer
            float walkLimitSeconds{ 8.0f };    // not there by then: takes it from where it stands
            float arrivedReach{ 90.0f };       // this near the body's edge counts as there
        };
        Config& GetConfig() { return _config; }
        void    LoadConfig();
        void    SaveConfig() const;

        // Only dead actors are walked to (a chest has no body to stand at).
        bool WantsWalk(RE::TESObjectREFR* source) const;
        std::uint32_t DelayMs() const { return static_cast<std::uint32_t>(_config.delaySeconds * 1000.0f); }

        enum class Gate { Go, Wait };
        Gate Check(RE::Actor* follower, RE::TESObjectREFR* source);

        void Update();   // ends walks that were left without anyone asking (timeouts), forgets old entries
        void Reset();    // the loot round is over: all walks end

    private:
        CorpseWalk();

        enum class State { Walking, Done };
        struct Walk
        {
            RE::FormID body{ 0 };
            State      state{ State::Walking };
            std::chrono::steady_clock::time_point started{};
            std::chrono::steady_clock::time_point doneAt{};
        };

        bool CanWalk(RE::Actor* follower, RE::Actor* body) const;
        bool StartWalk(RE::Actor* follower, RE::Actor* body);
        bool Evaluate(RE::Actor* follower, RE::Actor* body, Walk& walk);   // true when the walk is over
        void Stop(RE::Actor* follower);

        Config _config;
        std::unordered_map<RE::FormID, Walk> _walks;   // follower -> its walk
    };
}
