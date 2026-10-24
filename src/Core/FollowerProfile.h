#pragma once
#include "PCH.h"
#include "Core/ClassInterface.h"

namespace FollowerUnbound
{
    enum class FollowerPersonality : uint32_t
    {
        Balanced = 0,
        Frugal,
        Generous,
        Warrior,
        Adventurer,
        Alchemist,
        Blacksmith,
        Drunkard,
        Survivalist
    };

    enum class FollowerState : uint32_t
    {
        Idle = 0,
        Traveling,
        Combat,
        PostCombat,
        Looting,
        NeedsCheck,
        Shopping,
        Crafting,
        Resting,
        FollowPlayer
    };

    struct FollowerNeeds
    {
        float hunger{ 100.0f };   // 100 = tam dolu, 0 = aç
        float thirst{ 100.0f };   // 100 = susuzluk yok, 0 = dehidre
        float fatigue{ 100.0f };  // 100 = dinlenmiş, 0 = bitik
        float alcohol{ 0.0f };    // sarhoşluk seviyesi

        // deltaHours: geçen oyun saati
        // hungerRate, thirstRate, fatigueRate: saatte kaç puan düşeceği (Settings'den gelir)
        void Tick(float deltaHours, float hungerRate, float thirstRate, float fatigueRate)
        {
            hunger  = std::max(0.0f, hunger  - (deltaHours * hungerRate));
            thirst  = std::max(0.0f, thirst  - (deltaHours * thirstRate));
            fatigue = std::max(0.0f, fatigue - (deltaHours * fatigueRate));
            alcohol = std::max(0.0f, alcohol - (deltaHours * 10.0f));
        }

        // Eski imza — Settings olmadan çağrılanlar için geriye dönük uyumluluk
        void Tick(float deltaHours)
        {
            Tick(deltaHours, 12.5f, 16.67f, 10.0f); // varsayılan: 8h, 6h, 10h
        }

        // Oyuncu uyuduğunda çağrılır — takipçi de uyudu sayılır
        void RestoreFromSleep()
        {
            fatigue = 100.0f;
        }
    };

    struct FollowerPreferences
    {
        // Loot settings (Items to be looted)
        bool allowLoot{ true };
        bool allowLootGold{ true };
        bool allowLootWeapons{ true };
        bool allowLootArmor{ true };
        bool allowLootClothing{ true };
        bool allowLootJewelry{ true };
        bool allowLootPotions{ true };
        bool allowLootFood{ true };
        bool allowLootDrinks{ true };
        bool allowLootArrows{ true };
        bool allowLootIngredients{ true };
        bool allowLootSoulGems{ true };
        bool allowLootGems{ true };
        bool allowLootBooks{ true };
        bool allowLootScrolls{ true };

        // Sell settings (Items to be sold)
        bool allowSellWeapons{ true };
        bool allowSellArmor{ true };
        bool allowSellClothing{ true };
        bool allowSellJewelry{ true };
        bool allowSellAmmo{ true };
        bool allowSellPotions{ true };
        bool allowSellFood{ true };
        bool allowSellDrinks{ true };
        bool allowSellIngredients{ true };
        bool allowSellSoulGems{ true };
        bool allowSellGems{ true };
        bool allowSellBooks{ true };
        bool allowSellScrolls{ true };

        // Legacy / General Shopping settings
        bool allowBuyFood{ true };
        bool allowBuyPotions{ true };
        bool allowBuyEquipment{ true };
        bool allowBuyAmmunition{ true };

        // Potions (3 types, limits 0-25)
        bool buyHealthPotions{ false }; // Legacy, kept for save compatibility.
        uint32_t maxHealthPotions{ 5 };
        bool buyMagickaPotions{ false }; // Legacy, kept for save compatibility.
        uint32_t maxMagickaPotions{ 5 };
        bool buyStaminaPotions{ false }; // Legacy, kept for save compatibility.
        uint32_t maxStaminaPotions{ 5 };
        uint32_t maxPoisons{ 5 };

        // Food (3 sizes, limits 0-25)
        bool buyFoodLarge{ true };
        uint32_t maxFoodLarge{ 3 };
        bool buyFoodMedium{ true };
        uint32_t maxFoodMedium{ 5 };
        bool buyFoodSmall{ true };
        uint32_t maxFoodSmall{ 5 };

        // Drinks (2 types, limits 0-25)
        bool buyDrinkNormal{ true };
        uint32_t maxDrinkNormal{ 5 };
        bool buyDrinkAlcohol{ false };
        uint32_t maxDrinkAlcohol{ 2 };

        // Ammunition (Arrows and Bolts, limits 0-250)
        bool buyArrows{ false };
        uint32_t maxArrows{ 50 };
        uint32_t minArrows{ 20 };
        bool buyBolts{ false };
        uint32_t maxBolts{ 50 };

        // Legacy aggregate stock retained for loot/sale compatibility.
        bool buySoulGems{ true };
        uint32_t maxSoulGems{ 5 };

        // Extra Utilities: Meşale, Fener Yağı, Sabun (1-20, default 5)
        bool buyTorches{ true };
        uint32_t maxTorches{ 5 };
        bool buyLanternOil{ true };
        uint32_t maxLanternOil{ 5 };
        bool buySoap{ true };
        uint32_t maxSoap{ 5 };

        // Crafting settings
        bool allowCraftArrows{ false };
        bool allowCraftPotions{ true };
        bool allowUpgradeEquipment{ true };

        // Economy
        bool receiveQuestRewards{ true };
        float rewardSharePercent{ 25.0f };

        // Appended in serialization version 6 for backward-compatible saves.
        bool buyTreatmentPotions{ true };
        uint32_t maxTreatmentPotions{ 3 };
        bool buyCurePoisonPotions{ true };
        uint32_t maxCurePoisonPotions{ 3 };

        // Independently enable/disable routine poison purchases.
        bool buyPoisons{ true };

        // Bolt production is separate from arrow production and shopping.
        bool allowCraftBolts{ false };

        // Torch production is independently configurable per follower.
        bool allowCraftTorches{ true };

        // Personal gold savings: earnings are reserved until this target is met.
        bool savingsEnabled{ true };
        uint32_t savingsTarget{ 1000 };
        uint32_t savingsPercent{ 30 };
        uint32_t savedGold{ 0 };
        uint32_t savingsRemainder{ 0 }; // Fractional percentage carry, 0-99.

        // Per-capacity soul gem purchasing; all types start enabled.
        bool buyPettySoulGems{ true }; uint32_t maxPettySoulGems{ 5 };
        bool buyLesserSoulGems{ true }; uint32_t maxLesserSoulGems{ 1 };
        bool buyCommonSoulGems{ true }; uint32_t maxCommonSoulGems{ 1 };
        bool buyGreaterSoulGems{ true }; uint32_t maxGreaterSoulGems{ 1 };
        bool buyGrandSoulGems{ true }; uint32_t maxGrandSoulGems{ 1 };
        bool buyBlackSoulGems{ true }; uint32_t maxBlackSoulGems{ 1 };

        // Per-follower cap for the three restorative flowers used in potion recipes.
        uint32_t maxPotionFlowers{ 30 };

        // Per-tier potion purchasing. Minor is the default; higher tiers are opt-in.
        bool buyHealthMinor{ true }, buyHealthStandard{ false }, buyHealthPlentiful{ false };
        bool buyHealthVigorous{ false }, buyHealthExtreme{ false }, buyHealthUltimate{ false };
        bool buyMagickaMinor{ true }, buyMagickaStandard{ false }, buyMagickaPlentiful{ false };
        bool buyMagickaVigorous{ false }, buyMagickaExtreme{ false }, buyMagickaUltimate{ false };
        bool buyStaminaMinor{ true }, buyStaminaStandard{ false }, buyStaminaPlentiful{ false };
        bool buyStaminaVigorous{ false }, buyStaminaExtreme{ false }, buyStaminaUltimate{ false };

        // Per-effect poison purchasing and combat invisibility stock.
        bool buyPoisonHealth{ true }, buyPoisonMagicka{ true }, buyPoisonStamina{ true }, buyPoisonOther{ true };
        bool buyInvisibilityPotions{ true };
        uint32_t maxInvisibilityPotions{ 3 };

        // Independent target stock for each purchasable restorative potion tier.
        uint32_t maxHealthMinor{ 5 }, maxHealthStandard{ 5 }, maxHealthPlentiful{ 5 };
        uint32_t maxHealthVigorous{ 5 }, maxHealthExtreme{ 5 }, maxHealthUltimate{ 5 };
        uint32_t maxMagickaMinor{ 5 }, maxMagickaStandard{ 5 }, maxMagickaPlentiful{ 5 };
        uint32_t maxMagickaVigorous{ 5 }, maxMagickaExtreme{ 5 }, maxMagickaUltimate{ 5 };
        uint32_t maxStaminaMinor{ 5 }, maxStaminaStandard{ 5 }, maxStaminaPlentiful{ 5 };
        uint32_t maxStaminaVigorous{ 5 }, maxStaminaExtreme{ 5 }, maxStaminaUltimate{ 5 };
        uint32_t maxPoisonHealth{ 5 }, maxPoisonMagicka{ 5 }, maxPoisonStamina{ 5 }, maxPoisonOther{ 5 };
        uint32_t maxBuyBlueMountainFlowers{ 5 }, maxBuyWheat{ 5 };
        uint32_t maxBuyRedMountainFlowers{ 5 }, maxBuyWhiteCaps{ 5 };
        uint32_t maxBuyPurpleMountainFlowers{ 5 }, maxBuyPineThrushEggs{ 5 };

        // Soul Gem fill preference: 0 = Any (Her İkisi), 1 = Empty (Boş), 2 = Full (Dolu)
        uint32_t soulGemFillPetty{ 0 };
        uint32_t soulGemFillLesser{ 0 };
        uint32_t soulGemFillCommon{ 0 };
        uint32_t soulGemFillGreater{ 0 };
        uint32_t soulGemFillGrand{ 0 };
        uint32_t soulGemFillBlack{ 0 };

        // ── SİLAH VE ZIRH KUŞANMA TERCİHLERİ ──
        // Zırh Tercihi (Kıyafet Bütünlüğü): 0=Hepsi/Serbest, 1=Ağır Zırh, 2=Hafif Zırh, 3=Kıyafet/Cübbe
        uint32_t preferredArmorType{ 0 };

        // Asli Silah Tercihi (Primary Weapon Style):
        // 0=Serbest, 1=Tek Elli, 2=Çift Elli, 3=Yay, 4=Arbalet, 5=Asa
        uint32_t primaryWeaponStyle{ 0 };
        // Tek Elli / Asa ise El Seçimleri:
        // Sağ El: 0=Herhangi, 1=Kılıç, 2=Hançer, 3=Balta, 4=Gürz, 5=Asa
        uint32_t primaryRightHand{ 0 };
        // Sol El: 0=Herhangi, 1=Kalkan, 2=Kılıç, 3=Hançer, 4=Balta, 5=Gürz, 6=Asa, 7=Büyü/Boş
        uint32_t primaryLeftHand{ 0 };
        // Çift Elli ise Tür: 0=Herhangi, 1=Büyük Kılıç, 2=Büyük Balta, 3=Savaş Çekici
        uint32_t primaryTwoHandType{ 0 };

        // Yardımcı Silah Tercihi (Secondary Weapon Style):
        // 0=Yok, 1=Tek Elli, 2=Çift Elli, 3=Yay, 4=Arbalet, 5=Asa
        uint32_t secondaryWeaponStyle{ 0 };
        uint32_t secondaryRightHand{ 0 };
        uint32_t secondaryLeftHand{ 0 };
        uint32_t secondaryTwoHandType{ 0 };
    };

    class FollowerProfile
    {
    public:
        RE::FormID formID{ 0 };
        std::string name{ "Follower" };
        uint32_t gold{ 250 };
        FollowerRole role{ FollowerRole::None }; // Managed or overridden by King Core / Classbound
        std::string classBoundDisplayName;        // ClassBound/KingCore'un atadığı orijinal sınıf adı (UI için)
        bool isAnimal{ false };                    // Evcil hayvan (kedi, köpek, at vb.) — class ve ekonomi yok, sadece ihtiyaçlar
        FollowerPersonality personality{ FollowerPersonality::Balanced };
        FollowerState currentState{ FollowerState::Idle };

        FollowerNeeds needs;
        FollowerPreferences preferences;

        // Protected equipment designated by player
        std::unordered_set<RE::FormID> protectedEquipment;

        // Vendor gear offers declined by this follower. Saved per profile so
        // the same item is never offered again after loading the save.
        std::unordered_set<RE::FormID> declinedEquipmentPurchases;

        // Oyuncu tarafindan secilen kalici loadout (Asli & Yardimci Silahlar ve Zirhlar)
        // Oyun yuklendiginde (load game) Skyrim'in vanilla ekipman secimi yerine bu esyalar oncelikli kusanilir.
        RE::FormID assignedPrimaryRight{ 0 };
        RE::FormID assignedPrimaryLeft{ 0 };
        RE::FormID assignedSecondaryRight{ 0 };
        RE::FormID assignedSecondaryLeft{ 0 };
        RE::FormID assignedHead{ 0 };
        RE::FormID assignedBody{ 0 };
        RE::FormID assignedHands{ 0 };
        RE::FormID assignedFeet{ 0 };

        FollowerProfile() = default;
        explicit FollowerProfile(RE::FormID a_formID, std::string_view a_name = "")
            : formID(a_formID), name(a_name) {}

        bool IsProtected(RE::FormID itemID) const
        {
            return protectedEquipment.contains(itemID);
        }

        void ProtectItem(RE::FormID itemID, bool protect = true)
        {
            if (protect) {
                protectedEquipment.insert(itemID);
            } else {
                protectedEquipment.erase(itemID);
            }
        }
    };
}
