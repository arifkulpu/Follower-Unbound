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
        void CheckSpellTomeLearning(RE::Actor* actor, FollowerProfile& profile);   // Büyü kitaplarını okuma ve öğrenme
        void OnSleepStop(); // Oyuncu uyudu — tum takipciler dinlendirilir

        bool RegisterFollower(RE::Actor* actor);
        void RestoreSavedProfile(FollowerProfile profile);
        void UnregisterFollower(RE::FormID formID);
        bool IsFollower(RE::FormID formID) const;
        // ignoreCooldown: kullanıcı atama yaptığında hemen uygula. allowInCombat: savaş içi düzeltme.
        void EnforceAssignedLoadout(RE::Actor* actor, FollowerProfile& profile,
            bool ignoreCooldown = false, bool allowInCombat = false);

        // Savaşta AI'nın atanmamış bir silah seçmesi durumunda (TESEquipEvent) atanan silaha döner.
        void EnforceCombatWeapons(RE::Actor* actor);
        void NoteOwnEquip(RE::FormID actorID);
        bool WasOwnEquipRecent(RE::FormID actorID) const;

        // Atama (assigned*) yardımcıları: silah/kalkan/zırh atamalarının tek doğru yolu.
        static bool IsTwoHandedWeapon(RE::TESForm* form);
        static void AssignLoadoutItem(FollowerProfile& profile, RE::TESBoundObject* item,
            bool left = false, bool secondary = false);
        static void ClearAssignment(FollowerProfile& profile, RE::FormID itemID);
        // oldItemID atanmışsa yerine newItem'ı atar (yoksa hiçbir şey yapmaz).
        static void ReplaceAssignment(FollowerProfile& profile, RE::FormID oldItemID, RE::TESBoundObject* newItem);

        // ── Hayali çanta ──
        // Atanmamış silah/zırh/kalkanları takipçinin gerçek envanterinden hayali çantaya (profile.backpackItems)
        // taşır; böylece savaş/outfit AI'sı yalnızca oyuncunun seçtiği ekipmanı görür. Savaşta, ata binerken,
        // bekleyen yükseltme teklifi olan, kuşanılı, büyülü/temperli (ekstra veri) eşyalara dokunmaz.
        void StashUnassignedGear(RE::Actor* actor, FollowerProfile& profile);
        // Oyuncu çantadan bir eşyayı geri aldığında: eşya kısa süre yeniden çantaya konmaz.
        void NoteBackpackRetrieval(RE::FormID actorID, RE::FormID itemID);

        // Çantaya alma / geri verme: büyülü, temperli, şarjlı ve özel adlı eşyaların ekstra verisini korur.
        // wanted = -1: tüm yığın (kuşanılı/zehirli/görev eşyası ise 0 döner). allowWorn: UI'dan elle taşıma.
        int32_t StashItem(RE::Actor* actor, FollowerProfile& profile, RE::TESBoundObject* obj,
            int32_t wanted = -1, bool allowWorn = false);
        // customIndex >= 0: o özel kaydı geri ver; -1: önce sade, yoksa ilk özel kayıt.
        int32_t RestoreFromBackpack(RE::TESObjectREFR* dest, FollowerProfile& profile, RE::FormID itemID,
            int32_t customIndex = -1, int32_t count = 1);
        // Takipçi bırakıldığında (ölüm hariç) çanta içeriğini oyuncuya geri verir.
        void ReturnBackpackToPlayer(FollowerProfile& profile);
        void ProcessDormantBackpacks();

        // Protect-Equipped-Clothing: captures everything the actor is currently
        // wearing/holding (right hand, left hand, worn armor/clothes) into the
        // profile's personal protectedEquipment set. Those items are never sold.
        static void CaptureEquippedProtection(RE::Actor* actor, FollowerProfile& profile);

        static bool IsAnimalActor(RE::Actor* actor);
        static bool IsHumanoidRace(RE::Actor* actor);

        std::shared_ptr<FollowerProfile> GetProfile(RE::FormID formID);
        // includeDormant: pasif (takipçi olmayan) profiller de dahil — co-save için true kullan.
        std::unordered_map<RE::FormID, FollowerProfile> GetProfilesSnapshot(bool includeDormant = false) const;

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
        // Takipçilikten çıkan/ölen aktörlerin profilleri silinmez; yeniden takipçi olunca geri yüklenir.
        std::unordered_map<RE::FormID, std::shared_ptr<FollowerProfile>> _dormantProfiles;
        mutable std::mutex _equipStateMutex;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _enforceLast;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _ownEquipTime;
        struct CombatFixState { std::chrono::steady_clock::time_point windowStart{}; int count{ 0 }; };
        std::unordered_map<RE::FormID, CombatFixState> _combatFix;
        std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> _backpackGrace;
        struct DormantMeta { float sinceDays{ 0.0f }; bool dead{ false }; };
        std::unordered_map<RE::FormID, DormantMeta> _dormantMeta;   // pasif profilin ne zaman/neden pasifleştiği   // (actor<<32|item) -> geri alma zamanı
        float _timeSinceLastScan{ 0.0f };
        float _lastGameTimeDays{ 0.0f };
        mutable std::mutex _readMutex;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _readingActiveFollowers;
    };
}
