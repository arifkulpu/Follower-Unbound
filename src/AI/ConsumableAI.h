#pragma once
#include "PCH.h"
#include "Core/FollowerProfile.h"

namespace FollowerUnbound
{
    class ConsumableAI
    {
    public:
        static ConsumableAI* GetSingleton()
        {
            static ConsumableAI instance;
            return &instance;
        }

        void UpdateCombatConsumables(RE::Actor* follower, FollowerProfile& profile);
        void UpdateNeedsConsumption(RE::Actor* follower, FollowerProfile& profile);
        void UpdateOutOfCombatPoison(RE::Actor* follower, FollowerProfile& profile);
        void ApplyNeedsDebuffs(RE::Actor* follower, FollowerProfile& profile);

        // Savaş bitişini kaydet: yeme/içme activity'i birkaç saniye geciktirir
        void NotifyCombatEnded(RE::FormID actorID);

        // Oyun yüklendiğinde en az 10 saniye boyunca zehir veya sarf tüketimini beklet
        void NotifyGameLoaded();

        // Sleep veya Wait bittiğinde 5 saniye boyunca tüketimi duraklat
        void NotifySleepEnded();
        void NotifyWaitEnded();

        // Envanter (veya takas) kapandığında 3 saniye sonra yemeleri için zamanlayıcı kur
        void NotifyInventoryClosed();

        // Takipçi zehir satın aldığında veya verildiğinde 3 saniye sonra sürmesi için bildir
        void NotifyPoisonAcquired(RE::FormID actorID);

    private:
        ConsumableAI() = default;
        bool TryCurePoison(RE::Actor* follower);
        std::unordered_map<RE::FormID, float> _potionCooldowns;
        enum class CombatPotionType { None, Health, Magicka, Stamina, Invisibility };
        std::unordered_map<RE::FormID, CombatPotionType> _lastPotionType;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastConsumeTime;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastPotionTime;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _combatDrinkAnimTime;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastCurePoisonTime;

        // Uyku veya bekleme sonrası 5 saniye bekleme süresi
        std::chrono::steady_clock::time_point _sleepEndTime{};
        std::chrono::steady_clock::time_point _waitEndTime{};
        std::chrono::steady_clock::time_point _gameLoadedTime{};

        // Savaş bitişi: yeme/içme savaştan hemen sonra değil, ayarlanan süre sonra başlar
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastCombatEndTime;

        // Envanter kapandıktan sonra 3 saniye bekleme süresi
        std::chrono::steady_clock::time_point _inventoryClosedTime{};

        // Zehir animasyonu takibi: Animasyon oynarken kına alma çağrılmamalı, animasyon bitince kına alınmalı
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastPoisonAnimTime;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _poisonAcquiredTime;
        std::chrono::steady_clock::time_point _lastLootActiveTime{};

        // ── SIRALI YEME/İÇME KİLİDİ ──
        // Çoklu takipçide animasyon hatasını önler: aynı anda yalnızca BİR takipçi
        // yer/içer. 1. takipçi tamamen bitmeden 2. takipçi başlamaz.
        RE::FormID _activeConsumer{ 0 };
        std::chrono::steady_clock::time_point _activeLastActivity{};

        // Takipçilerin envanterinde yemek/içecek kalmadığında uyarı mesajı spam'ini önleme zamanlayıcısı (en az 60 sn)
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastMissingFoodAlertTime;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastMissingDrinkAlertTime;

        // Histerezis (seans) takibi: Takipçi esik altina inince seans baslar, %100 olunca biter.
        std::unordered_set<RE::FormID> _isDrinkingSession;
        std::unordered_set<RE::FormID> _isEatingSession;
    };
}
