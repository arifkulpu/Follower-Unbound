#pragma once
#include "PCH.h"
#include "Core/FollowerProfile.h"
#include "Core/ClassInterface.h"

namespace FollowerUnbound
{
    class FollowerManager
    {
    public:
        static FollowerManager* GetSingleton()
        {
            static FollowerManager instance;
            return &instance;
        }

        void Initialize();
        void Update(float deltaGameHours);
        void ScanForActiveFollowers();
        void InitGameTime();          // Oyun yüklenince _lastGameTimeDays'i başlatır
        void UpdateGameTimeNeeds();
        void TriggerFollowerConsumption(); // İhtiyaç seansındaki takipçilerin tüketimlerini tetikler
        void CheckAutonomousCrafting(RE::Actor* actor, FollowerProfile& profile); // Ok ve arbalet oku üretimi
        void OnSleepStop(); // Oyuncu uyudu — tum takipciler dinlendirilir

        bool RegisterFollower(RE::Actor* actor);
        void RestoreSavedProfile(FollowerProfile profile);
        void UnregisterFollower(RE::FormID formID);
        bool IsFollower(RE::FormID formID) const;
        void EnforceAssignedLoadout(RE::Actor* actor, FollowerProfile& profile, bool forcePrimary = false);
        void UpdateCombatWeaponSwitching(RE::Actor* actor, FollowerProfile& profile);

        // Protect-Equipped-Clothing: captures everything the actor is currently
        // wearing/holding (right hand, left hand, worn armor/clothes) into the
        // profile's personal protectedEquipment set. Those items are never sold.
        static void CaptureEquippedProtection(RE::Actor* actor, FollowerProfile& profile);

        static bool IsAnimalActor(RE::Actor* actor);
        static bool IsHumanoidRace(RE::Actor* actor);

        std::shared_ptr<FollowerProfile> GetProfile(RE::FormID formID);
        std::unordered_map<RE::FormID, FollowerProfile> GetProfilesSnapshot() const;

        std::vector<RE::Actor*> GetActiveFollowers();
        void RefreshFollowerRoles();

        void Clear();

    private:
        FollowerManager() = default;
        ~FollowerManager() = default;
        FollowerManager(const FollowerManager&) = delete;
        FollowerManager& operator=(const FollowerManager&) = delete;

        mutable std::shared_mutex _mutex;
        std::unordered_map<RE::FormID, std::shared_ptr<FollowerProfile>> _profiles;
        float _timeSinceLastScan{ 0.0f };
        float _lastGameTimeDays{ 0.0f };
    };
}
