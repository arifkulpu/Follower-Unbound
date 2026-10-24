#pragma once
#include "PCH.h"

namespace FollowerUnbound
{
    class Settings
    {
    public:
        static Settings* GetSingleton()
        {
            static Settings instance;
            return &instance;
        }

        // ── SYSTEM TOGGLES ──
        // Each flag enables/disables an entire subsystem.
        bool enableCombatPotions{ true };    // Auto-drink potions in combat
        bool enableNeedsSystem{ true };      // Hunger / Thirst / Fatigue system
        bool enableAutoEquip{ true };        // Auto-equip best available gear
        bool enableShoppingSystem{ true };   // Auto-purchase supplies from vendors
        bool enableSellingSystem{ true };    // Auto-sell excess loot/gear to vendors
        bool enablePoisonSystem{ true };     // Assassin auto-apply poison to weapon
        bool enablePetSystem{ true };        // Animals/Pets (cats, dogs, horses) have hunger/thirst/fatigue needs

        // ── HOTKEY SETTINGS ──
        // menuHotkey: DirectX scan code (default: 0x17 = 'I')
        // menuHotkeyModifier: bitmask: 0 = None, 1 = Shift, 2 = Ctrl, 4 = Alt (default: 1 = Shift)
        // menuHotkeyText: human-readable label (default: "Shift + I")
        uint32_t menuHotkey{ 0x17 };
        uint32_t menuHotkeyModifier{ 1 };
        std::string menuHotkeyText{ "Shift + I" };

        // ── LOOTING ──
        bool autoLootCorpses{ true };        // Oyuncu ceset/konteyner menüsünü kapatınca kalan eşyaları takipçilere dağıt
        float lootDelaySeconds{ 10.0f };     // Savaş bittikten kaç saniye sonra cesetler yağmalanır
        uint32_t lootMaxSpareWeapons{ 3 };   // Takipçinin taşıyabileceği yedek silah sayısı (fazlasını yağmalamaz)
        uint32_t lootMaxSpareArmor{ 2 };     // Takipçinin taşıyabileceği yedek zırh parçası sayısı
        bool lootHostilesOnly{ true };       // Sadece savaşta düşman olan (hostile) cesetler yağmalanır; masum/şehir NPC'leri ve pasif hayvanlar dokunulmaz

        // ── LOOT CATEGORIES (global anahtarlar; takipçi başına tercihler bunlarla ÇARPILIR) ──
        bool lootGold{ true };               // Cesetteki altın paylaşılır
        bool lootWeapons{ true };
        bool lootArmor{ true };
        bool lootArrows{ true };
        bool lootPotions{ true };
        bool lootFood{ true };
        bool lootIngredients{ false };       // Bitki/mineral gibi malzemeler (tatlı def. kapalı — takipçilerin işi değil)
        bool lootSoulGems{ false };          // Ruh taşları oyuncuya kalsın (SoulKeeper dostu)
        bool lootMisc{ true };               // Ejderha kemiği/pullanı, mücevher vb. MISC eşyalar (kapalıysa takipçiler dokunmaz)

        // ── EQUIPMENT PROTECTION ──
        // When ON, each follower's currently equipped clothes/armor/weapons are captured
        // into their personal protectedEquipment set and are NEVER sold.
        bool protectEquippedClothing{ true };
        bool promptOnBetterEquipment{ true }; // Yeni daha iyi/büyülü silah veya zırh bulunduğunda oyuncuya onay kutusu sor
        uint32_t equipmentUpgradeChance{ 35 }; // Daha iyi ekipman algılama / sorma şansı (%) - spamı önler
        bool enableEquipmentPurchaseOffers{ true }; // Satıcılardaki daha iyi ekipman için PrismaUI satın alma teklifi göster
        bool magePreferRobes{ true };         // Büyücü takipçiler ağır zırh yerine büyülü cübbeleri tercih etsin

        // ── SELLING CATEGORY FILTERS ──
        bool sellWeapons{ true };            // Sell excess unequipped weapons
        bool sellArmor{ true };              // Sell excess unequipped armor
        bool sellAmmo{ true };               // Sell excess arrows/bolts over limit
        bool sellPotions{ true };            // Sell excess potions/ingredients
        bool sellFood{ true };               // Sell excess food & drinks over personal stock limits
        bool sellGems{ true };               // Sell gems and valuables
        bool protectSoulGems{ true };        // NEVER sell soul gems (SoulKeeper compatibility)
        uint32_t transactionBatchMultiplier{ 5 }; // Sale batch multiplier (1x, 3x, 5x, 10x); purchases fill stock deficits

        // ── NOTIFICATION TOGGLES ──
        // Subsystem runs normally; only the HUD notification is suppressed when false.
        bool enableNotifications{ true };         // Master toggle for all notifications
        uint32_t notificationMode{ 0 };           // 0: Batched / Digest (Toplu Özet), 1: Detailed (Ayrıntılı), 2: Critical Only (Sadece Kritik)
        bool notifyCombatHealthPotion{ true };    // "X drank a Health Potion!"
        bool notifyCombatMagickaPotion{ true };   // "X drank a Magicka Potion!"
        bool notifyCombatStaminaPotion{ true };   // "X drank a Stamina Potion!"
        bool notifyPoisonApplied{ true };         // "X applied poison to their weapon!"
        bool notifyNeedsFood{ true };             // "X consumed Y. (Hunger: Z%)"
        bool notifyNeedsDrink{ true };            // "X consumed Y. (Thirst: Z%)"
        bool notifyNeedsHungryWarning{ true };    // "X is hungry but has nothing to eat!"
        bool notifyNeedsThirstyWarning{ true };   // "X is thirsty but has nothing to drink!"
        bool notifyLoot{ true };                  // "X looted: ..."
        bool notifyShopping{ true };              // "X purchased Y from Z. (-N Gold)"
        bool notifySelling{ true };               // "X sold Y to Z. (+N Gold)"
        bool notifyAutoEquip{ true };             // "X equipped Y"

        // ── NEEDS DEPLETION RATES (game hours) ──
        float hungerDepletionHours{ 8.0f };
        float thirstDepletionHours{ 6.0f };
        float fatigueDepletionHours{ 10.0f };

        // ── BEHAVIOR THRESHOLDS (%) ──
        float eatThreshold{ 50.0f };
        float drinkThreshold{ 50.0f };

        // Delay between consuming items (seconds)
        float consumeDelaySeconds{ 3.0f };

        // Aynı takipçinin iki lokma/yudum arasındaki süre (saniye).
        // Yeme/içme animasyonunun rahatça bitmesi içindir.
        float consumeIntervalSeconds{ 10.0f };

        // Savaş bittikten sonra yemek/içmek için beklenen süre (saniye)
        float postCombatConsumeDelaySeconds{ 6.0f };

        // ── COMBAT POTION THRESHOLDS (%) ──
        float healthPotionThreshold{ 40.0f };
        float magickaPotionThreshold{ 30.0f };
        float staminaPotionThreshold{ 30.0f };

        // ── EMERGENCY POTION THRESHOLDS (%) ──
        // Below these, potions are drunk back-to-back with NO cooldown (critical survival).
        float emergencyHealthPct{ 15.0f };
        float emergencyMagickaPct{ 5.0f };
        float emergencyStaminaPct{ 5.0f };

        float HungerRatePerHour()  const { return 100.0f / std::max(0.1f, hungerDepletionHours); }
        float ThirstRatePerHour()  const { return 100.0f / std::max(0.1f, thirstDepletionHours); }
        float FatigueRatePerHour() const { return 100.0f / std::max(0.1f, fatigueDepletionHours); }

        void Load();
        void Save();

    private:
        Settings() = default;
        ~Settings() = default;
        Settings(const Settings&) = delete;
        Settings& operator=(const Settings&) = delete;

        static std::filesystem::path GetINIPath();
    };
}

