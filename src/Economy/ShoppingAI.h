#pragma once
#include "PCH.h"
#include "Core/FollowerProfile.h"

namespace FollowerUnbound
{
    struct SupplyCounts
    {
        int32_t health{ 0 }, magicka{ 0 }, stamina{ 0 }, poison{ 0 }, invisibilityPotions{ 0 };
        int32_t poisonHealth{ 0 }, poisonMagicka{ 0 }, poisonStamina{ 0 }, poisonOther{ 0 };
        int32_t foodL{ 0 }, foodM{ 0 }, foodS{ 0 };
        int32_t drinkN{ 0 }, drinkA{ 0 };
        int32_t arrows{ 0 }, bolts{ 0 }, treatmentPotions{ 0 }, curePoisonPotions{ 0 };
        int32_t soulGems{ 0 };
        int32_t pettySoulGems{ 0 }, lesserSoulGems{ 0 }, commonSoulGems{ 0 };
        int32_t greaterSoulGems{ 0 }, grandSoulGems{ 0 }, blackSoulGems{ 0 };
        int32_t blueMountainFlowers{ 0 }, wheat{ 0 }, redMountainFlowers{ 0 };
        int32_t whiteCaps{ 0 }, purpleMountainFlowers{ 0 }, pineThrushEggs{ 0 };
        int32_t healthMinor{ 0 }, healthStandard{ 0 }, healthPlentiful{ 0 }, healthVigorous{ 0 }, healthExtreme{ 0 }, healthUltimate{ 0 };
        int32_t magickaMinor{ 0 }, magickaStandard{ 0 }, magickaPlentiful{ 0 }, magickaVigorous{ 0 }, magickaExtreme{ 0 }, magickaUltimate{ 0 };
        int32_t staminaMinor{ 0 }, staminaStandard{ 0 }, staminaPlentiful{ 0 }, staminaVigorous{ 0 }, staminaExtreme{ 0 }, staminaUltimate{ 0 };
        int32_t torches{ 0 }, lanternOil{ 0 }, soap{ 0 };
    };

    class ShoppingAI
    {
    public:
        static ShoppingAI* GetSingleton()
        {
            static ShoppingAI instance;
            return &instance;
        }

        // When in town/inn/market, followers can scan nearby vendors and purchase necessities
        void ProcessShopping(RE::Actor* follower, FollowerProfile& profile);
        static SupplyCounts GetSupplyCounts(RE::Actor* follower);

        // Followers can sell excess loot/weapons/armor/gems to appropriate vendors
        void ProcessSelling(RE::Actor* follower, FollowerProfile& profile);

    private:
        ShoppingAI() = default;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastShopTime;
        std::unordered_map<RE::FormID, size_t> _purchaseCursor;
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastSellTime;
        // Takipçi + eşya -> son satın alma zamanı (yeni alınan eşyanın hemen
        // geri satılmasını önleyen alış-satış döngüsü koruması)
        std::unordered_map<RE::FormID, std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point>> _lastPurchaseTime;
    };
}
