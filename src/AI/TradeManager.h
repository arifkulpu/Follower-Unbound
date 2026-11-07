#pragma once
#include "PCH.h"
#include "Core/FollowerProfile.h"
#include "AI/TradeVoice.h"
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

namespace FollowerUnbound
{
    // Follower -> merchant walk + short "talk" scene, then the EXISTING ShoppingAI does the actual selling/buying.
    // Needs FollowerUnboundTrade.esp (10 quest/scene/package/marker slots). Without the plugin, HandleTradeTick()
    // returns false and the old instant trading keeps working.
    //
    // Threading: every method must run on the game/UI task thread (same place LootManager::ScheduleProcessing
    // runs its work). TradeManager::ScheduleUpdate() queues Update() through SKSE's UI task queue.
    class TradeManager
    {
    public:
        static TradeManager* GetSingleton()
        {
            static TradeManager instance;
            return &instance;
        }

        struct Config
        {
            bool  enabled{ true };
            // WaitingForPlayer=1 while walking, so follower mods do not teleport it back. It is NOT saved with the game
            // (a save made mid-walk would leave the follower waiting), and the scene's packages already outrank follow
            // packages, so it is off by default.
            bool  holdWhileWalking{ false };
            bool  voiceEnabled{ true };        // greet/thank with the game's own voice lines (topics: FollowerUnboundTradeVoice.json)
            float noticeDistance{ 1000.0f };   // a merchant this near is "about"
            float nearbyStaySeconds{ 6.0f };   // stays this long near a merchant before it goes over
            float reach{ 220.0f };             // close enough to trade (across a counter)
            float reachArrived{ 350.0f };      // ...or as close as the path went
            float floorHeight{ 150.0f };       // more height apart = another floor
            float spotAhead{ 150.0f };         // marker this far in front of the merchant
            float farSpotAhead{ 260.0f };      // second try after it ended up behind a stall
            float merchantGone{ 1500.0f };     // merchant this far off: it walked away
            float playerLeash{ 2000.0f };      // does not go if you are further than this
            float walkTimeoutSeconds{ 25.0f };
            float stuckSeconds{ 7.0f };        // no nearer for this long
            float stallSeconds{ 4.0f };        // near but no nearer for this long: trades from there
            float lingerSeconds{ 3.5f };       // the "talk" after the trade
            float retrySeconds{ 120.0f };      // after a failed walk, per follower+merchant
            float merchantRestSeconds{ 5.0f };
            float cooldownGameDays{ 0.25f };   // after a real sale/purchase: not again with the same merchant for this long (6 game hours)
            float noDealCooldownGameDays{ 0.25f };   // arrived but nothing changed hands (6 game hours)
        };
        Config& GetConfig() { return _config; }
        // Settings file: Data/SKSE/Plugins/FollowerUnboundTrade.json (loaded on first use; PrismaUI calls SaveConfig()).
        void LoadConfig();
        void SaveConfig() const;

        // Whether FollowerUnboundTrade.esp is loaded (and its forms resolve).
        bool PluginAvailable();
        // Walking is on: ShoppingAI must not trade by itself from a distance any more (see TradeVendorFilter).
        bool WalkModeOn() { return _config.enabled && PluginAvailable(); }

        // Replaces the direct ProcessSelling()/ProcessShopping() calls (every ~2 s, per follower).
        // true  = TradeManager owns trading for this follower now: the caller must NOT run the instant path.
        // false = walking is off or the plugin is missing: the caller runs the old instant path.
        bool HandleTradeTick(RE::Actor* actor, FollowerProfile& profile);

        // Advances walks/talks. Throttled internally (~400 ms). Game thread only.
        void Update();
        // Queues Update() on the UI task queue (call this from the place that calls LootManager::ScheduleProcessing).
        void ScheduleUpdate();

        bool IsBusy(RE::FormID followerID) const;   // walking to / talking with a merchant

        // On game load / new game / settings off: stops the quests, clears factions and holds.
        void AbortAll(const char* why);

        // Is the trip worth it? Default: something to sell (loose item with value, or backpack content) or gold to spend.
        // Replace with a ShoppingAI dry-run when you have one.
        using BusinessCheck = std::function<bool(RE::Actor*, FollowerProfile&, RE::Actor*)>;
        void SetBusinessCheck(BusinessCheck check) { _businessCheck = std::move(check); }

        // Optional: the follower's/merchant's lines (a Say call, a bark). bought: gold went down.
        using TalkHook = std::function<void(RE::Actor* follower, RE::Actor* merchant, bool sold, bool bought)>;
        void SetTalkHook(TalkHook hook) { _talkHook = std::move(hook); }

    private:
        TradeManager();   // loads the settings file

        static constexpr int kSlots = 10;
        enum class State { Free, Walking, Talking };

        struct SlotForms
        {
            RE::TESQuest*      quest{ nullptr };
            RE::TESFaction*    followerFaction{ nullptr };
            RE::TESFaction*    merchantFaction{ nullptr };
            RE::BGSScene*      scene{ nullptr };
            RE::TESObjectREFR* spot{ nullptr };
        };

        struct Slot
        {
            State           state{ State::Free };
            RE::FormID      follower{ 0 };
            RE::FormID      merchantID{ 0 };
            RE::ActorHandle merchant;
            RE::ActorHandle followerHandle;
            std::chrono::steady_clock::time_point started{};
            std::chrono::steady_clock::time_point closerAt{};
            std::chrono::steady_clock::time_point talkStart{};
            std::chrono::steady_clock::time_point calmUntil{};
            float           closest{ (std::numeric_limits<float>::max)() };
            float           playerApart{ 0.0f };
            RE::NiPoint3    spotPos{};
            bool            gestured{ false };
            bool            sold{ false };
            bool            bought{ false };
            std::vector<TradeVoice::Line> lines;   // the spoken lines of this visit
            float           talkSeconds{ 3.5f };   // how long the talk lasts (longer when lines are spoken)
        };

        using PairKey = std::pair<RE::FormID, RE::FormID>;   // follower, merchant
        struct NearEntry
        {
            std::chrono::steady_clock::time_point since{};   // first seen near
            std::chrono::steady_clock::time_point last{};    // last tick it was near
        };

        // forms / slots
        const SlotForms& Forms(int slot);
        int  SlotOf(RE::FormID follower) const;
        int  FreeSlot();
        bool MerchantBusy(RE::Actor* merchant);

        // merchants
        static bool OpenNow(RE::TESFaction* faction);
        static bool VendorOpen(RE::Actor* merchant);   // shop hours (no vendor faction: open)
        static bool BehindMerchant(const Config& c, RE::Actor* follower, RE::Actor* merchant);
        static RE::NiPoint3 CustomerSide(RE::Actor* merchant, RE::Actor* follower, float ahead);
        bool InReach(RE::Actor* follower, RE::Actor* merchant, float reach) const;
        bool DefaultBusiness(RE::Actor* actor, FollowerProfile& profile) const;

        // walk / talk
        bool StartWalk(RE::Actor* follower, RE::Actor* merchant);
        void AdvanceWalk(int slot);
        void OnArrive(int slot, RE::Actor* follower, RE::Actor* merchant);
        void AdvanceTalk(int slot);
        void EndSlot(int slot, bool failed, const char* why);

        // small helpers
        void HoldFollower(RE::Actor* actor, bool hold);
        void QueueRepack(RE::ActorHandle a, RE::ActorHandle b);
        void ProcessRepacks();
        void StopIdleQuests();

        Config _config;
        std::array<Slot, kSlots>        _slots;
        std::array<SlotForms, kSlots>   _forms;
        bool                            _formsLoaded{ false };
        bool                            _formsOk{ false };

        std::map<PairKey, NearEntry>                                  _nearSince;
        std::map<PairKey, std::chrono::steady_clock::time_point> _failedAt;
        std::map<PairKey, float>                                 _tradeAllowedDay;   // game day the pair may trade again
        std::set<PairKey>                                        _farSpot;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _merchantRest;
        std::unordered_map<RE::FormID, float>                    _heldBefore;   // follower -> WaitingForPlayer before

        struct Repack { RE::ActorHandle a, b; std::chrono::steady_clock::time_point at; };
        std::vector<Repack> _repacks;

        std::chrono::steady_clock::time_point _lastSlotStart{};
        std::chrono::steady_clock::time_point _lastUpdate{};
        std::chrono::steady_clock::time_point _idleQuestSince{};
        std::atomic_bool _updateScheduled{ false };

        BusinessCheck _businessCheck;
        TalkHook      _talkHook;
    };

    // ShoppingAI picks its vendors itself (name/faction roster, anything within 1500 units). To make it trade with the
    // merchant the follower actually walked to, TradeManager sets a filter around its ProcessSelling/ProcessShopping calls.
    namespace TradeVendorFilter
    {
        void       Set(RE::FormID vendorID);   // 0 = no filter
        RE::FormID Get();
        // The vendors ShoppingAI would consider near the follower (nearest first). Defined in ShoppingAI.cpp.
        std::vector<RE::Actor*> Nearby(RE::Actor* follower, float maxDistance);

        struct Scope
        {
            explicit Scope(RE::FormID id) { Set(id); }
            ~Scope() { Set(0); }
            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;
        };
    }
}
