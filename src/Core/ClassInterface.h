#pragma once
#include "PCH.h"
#include "KingCore/KingCoreAPI.h"
#include "LifeAgainAPI.h"
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <mutex>

namespace FollowerUnbound
{
    // High-level role archetype used by AI evaluation
    enum class FollowerRole : uint32_t
    {
        None = 0,
        Warrior,
        Archer,
        Assassin,
        Thief,
        Mage,
        Necromancer,
        Knight,
        Paladin,
        Druid,
        Crossbowman,
        Alchemist,
        Bard,
        Merchant,
        Custom
    };

    /**
     * Seamless King Core & ClassBound Integration.
     * When King Core / ClassBound is active, queries pull directly from the high-performance
     * cached KingCore::BehaviorProfile (event-driven, zero polling).
     * If King Core or ClassBound is not installed, seamlessly falls back to Skyrim native heuristics.
     */
    class ClassInterface : public KingCore::IClassEventListener
    {
    public:
        static ClassInterface* GetSingleton()
        {
            static ClassInterface instance;
            return &instance;
        }

        // Set whether external King Core / Classbound handler is plugged in
        void SetExternalClassProviderActive(bool active) { _externalProviderActive = active; }
        bool IsExternalClassProviderActive() const { return _externalProviderActive; }

        void SetClassBoundAPI(KingCore::IClassBoundAPI* api);
        [[nodiscard]] KingCore::IClassBoundAPI* GetClassBoundAPI() const { return _classBoundAPI; }

        void SetTreatmentItems(const KingCore::TreatmentRegistration* reg);
        [[nodiscard]] bool IsTreatmentItem(RE::FormID formID) const;
        void OnLifeAgainInjuryState(const LifeAgain::API::FollowerInjuryStateEvent& event);
        [[nodiscard]] uint32_t GetTreatmentPotionDemand(RE::FormID followerFormID) const;

        // IClassEventListener callback (Event-driven dirty flag invalidation)
        void OnClassEvent(const KingCore::ClassEvent& ev) override;

        // Resolves the role of an actor
        FollowerRole ResolveRole(RE::Actor* actor);

        // Resolves or queries full BehaviorProfile (cached, dirty-flag evaluated)
        bool GetBehaviorProfile(RE::Actor* actor, KingCore::BehaviorProfile& outProfile);

        // Helper conversion
        static FollowerRole FromKingCoreClass(KingCore::ClassID id);
        static const char* GetRoleName(FollowerRole role);
        std::string GetClassDisplayName(RE::Actor* actor, FollowerRole role);

    private:
        ClassInterface() = default;
        ~ClassInterface() override = default;

        bool _externalProviderActive{ false };
        KingCore::IClassBoundAPI* _classBoundAPI{ nullptr };

        mutable std::mutex _treatmentMutex;
        std::unordered_set<uint32_t> _treatmentItems;
        mutable std::mutex _injuryMutex;
        std::unordered_map<RE::FormID, std::array<uint16_t, 8>> _lifeAgainInjuries;

        // Guards _profileCache against concurrent OnClassEvent (invalidation) and
        // GetBehaviorProfile (read/write) calls from different threads.
        mutable std::mutex _cacheMutex;

        // Cache for BehaviorProfiles to prevent frame-by-frame overhead
        std::unordered_map<uint32_t, KingCore::BehaviorProfile> _profileCache;

        FollowerRole FallbackDetectRole(RE::Actor* actor);
        void GenerateFallbackProfile(RE::Actor* actor, KingCore::BehaviorProfile& outProfile);
    };
}
