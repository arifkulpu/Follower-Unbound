#include "Core/Settings.h"
#include <Windows.h>

namespace FollowerUnbound
{
    std::filesystem::path Settings::GetINIPath()
    {
        auto path = std::filesystem::path{ "Data/SKSE/Plugins/FollowerUnbound.ini" };
        return path;
    }

    void Settings::Load()
    {
        auto iniPath = GetINIPath();
        if (!std::filesystem::exists(iniPath)) {
            logger::info("Settings INI not found, using defaults. Will save on first write.");
            Save();
            return;
        }

        char buf[64];
        auto pathStr = iniPath.string();
        const char* path = pathStr.c_str();

        auto readFloat = [&](const char* section, const char* key, float defaultVal) -> float {
            GetPrivateProfileStringA(section, key, std::to_string(defaultVal).c_str(), buf, sizeof(buf), path);
            try { return std::stof(buf); } catch (...) { return defaultVal; }
        };

        auto readBool = [&](const char* section, const char* key, bool defaultVal) -> bool {
            GetPrivateProfileStringA(section, key, defaultVal ? "1" : "0", buf, sizeof(buf), path);
            return std::string(buf) != "0";
        };

        auto readInt = [&](const char* section, const char* key, int defaultVal) -> int {
            GetPrivateProfileStringA(section, key, std::to_string(defaultVal).c_str(), buf, sizeof(buf), path);
            try { return std::stoi(buf); } catch (...) { return defaultVal; }
        };

        // ── NEEDS RATES ──
        hungerDepletionHours  = readFloat("NeedsRates", "HungerDepletionHours",  8.0f);
        thirstDepletionHours  = readFloat("NeedsRates", "ThirstDepletionHours",  6.0f);
        fatigueDepletionHours = readFloat("NeedsRates", "FatigueDepletionHours", 10.0f);
        hungerDepletionHours  = std::clamp(hungerDepletionHours,  1.0f, 72.0f);
        thirstDepletionHours  = std::clamp(thirstDepletionHours,  1.0f, 72.0f);
        fatigueDepletionHours = std::clamp(fatigueDepletionHours, 1.0f, 72.0f);

        // ── THRESHOLDS ──
        eatThreshold          = readFloat("Thresholds", "EatThreshold",           50.0f);
        drinkThreshold        = readFloat("Thresholds", "DrinkThreshold",         50.0f);
        consumeDelaySeconds   = readFloat("Thresholds", "ConsumeDelaySeconds",    3.0f);
        consumeIntervalSeconds = readFloat("Thresholds", "ConsumeIntervalSeconds", 10.0f);
        postCombatConsumeDelaySeconds = readFloat("Thresholds", "PostCombatConsumeDelaySeconds", 6.0f);
        healthPotionThreshold = readFloat("Thresholds", "HealthPotionThreshold",  40.0f);
        magickaPotionThreshold= readFloat("Thresholds", "MagickaPotionThreshold", 30.0f);
        staminaPotionThreshold= readFloat("Thresholds", "StaminaPotionThreshold", 30.0f);

        eatThreshold          = std::clamp(eatThreshold,           0.0f, 100.0f);
        drinkThreshold        = std::clamp(drinkThreshold,         0.0f, 100.0f);
        consumeDelaySeconds   = std::clamp(consumeDelaySeconds,    1.0f, 30.0f);
        consumeIntervalSeconds = std::clamp(consumeIntervalSeconds, 2.0f, 60.0f);
        postCombatConsumeDelaySeconds = std::clamp(postCombatConsumeDelaySeconds, 0.0f, 60.0f);
        healthPotionThreshold = std::clamp(healthPotionThreshold,  5.0f, 95.0f);
        magickaPotionThreshold= std::clamp(magickaPotionThreshold, 5.0f, 95.0f);
        staminaPotionThreshold= std::clamp(staminaPotionThreshold, 5.0f, 95.0f);

        emergencyHealthPct  = std::clamp(readFloat("Thresholds", "EmergencyHealthPct",  15.0f),  1.0f, 95.0f);
        emergencyMagickaPct = std::clamp(readFloat("Thresholds", "EmergencyMagickaPct",  5.0f),  1.0f, 95.0f);
        emergencyStaminaPct = std::clamp(readFloat("Thresholds", "EmergencyStaminaPct",  5.0f),  1.0f, 95.0f);

        // ── SYSTEM TOGGLES ──
        enableCombatPotions  = readBool("Systems", "EnableCombatPotions",  true);
        enableNeedsSystem    = readBool("Systems", "EnableNeedsSystem",    true);
        enableAutoEquip      = readBool("Systems", "EnableAutoEquip",      true);
        enableShoppingSystem = readBool("Systems", "EnableShoppingSystem", true);
        enableSellingSystem  = readBool("Systems", "EnableSellingSystem",  true);
        enablePoisonSystem   = readBool("Systems", "EnablePoisonSystem",   true);
        enablePetSystem      = readBool("Systems", "EnablePetSystem",      true);
        protectEquippedClothing = readBool("Systems", "ProtectEquippedClothing", true);
        promptOnBetterEquipment = readBool("Systems", "PromptOnBetterEquipment", true);
        equipmentUpgradeChance  = static_cast<uint32_t>(std::clamp(readInt("Systems", "EquipmentUpgradeChance", 35), 1, 100));
        enableEquipmentPurchaseOffers = readBool("Systems", "EnableEquipmentPurchaseOffers", true);
        magePreferRobes         = readBool("Systems", "MagePreferRobes",         true);

        // ── LOOTING ──
        autoLootCorpses    = readBool("Looting", "AutoLootCorpses",    true);
        lootDelaySeconds   = std::clamp(readFloat("Looting", "LootDelaySeconds", 10.0f), 1.0f, 120.0f);
        lootMaxSpareWeapons = static_cast<uint32_t>(std::clamp(readInt("Looting", "LootMaxSpareWeapons", 3), 0, 10));
        lootMaxSpareArmor   = static_cast<uint32_t>(std::clamp(readInt("Looting", "LootMaxSpareArmor", 2), 0, 10));
        lootHostilesOnly   = readBool("Looting", "LootHostilesOnly",   true);

        lootGold        = readBool("Looting", "LootGold",        true);
        lootWeapons     = readBool("Looting", "LootWeapons",     true);
        lootArmor       = readBool("Looting", "LootArmor",       true);
        lootArrows      = readBool("Looting", "LootArrows",      true);
        lootPotions     = readBool("Looting", "LootPotions",     true);
        lootFood        = readBool("Looting", "LootFood",        true);
        lootIngredients = readBool("Looting", "LootIngredients", false);
        lootSoulGems    = readBool("Looting", "LootSoulGems",    false);
        lootMisc        = readBool("Looting", "LootMisc",        true);

        // ── SELLING CATEGORY FILTERS ──
        sellWeapons                = readBool("Selling", "SellWeapons",                true);
        sellArmor                  = readBool("Selling", "SellArmor",                  true);
        sellAmmo                   = readBool("Selling", "SellAmmo",                   true);
        sellPotions                = readBool("Selling", "SellPotions",                true);
        sellFood                   = readBool("Selling", "SellFood",                   true);
        sellGems                   = readBool("Selling", "SellGems",                   true);
        protectSoulGems            = readBool("Selling", "ProtectSoulGems",            true);
        transactionBatchMultiplier = static_cast<uint32_t>(std::clamp(readInt("Selling", "TransactionBatchMultiplier", 5), 1, 20));

        // ── NOTIFICATION TOGGLES ──
        enableNotifications       = readBool("Notifications", "EnableNotifications",       true);
        notifyCombatHealthPotion  = readBool("Notifications", "NotifyCombatHealthPotion",  true);
        notifyCombatMagickaPotion = readBool("Notifications", "NotifyCombatMagickaPotion", true);
        notifyCombatStaminaPotion = readBool("Notifications", "NotifyCombatStaminaPotion", true);
        notifyPoisonApplied       = readBool("Notifications", "NotifyPoisonApplied",       true);
        notifyNeedsFood           = readBool("Notifications", "NotifyNeedsFood",           true);
        notifyNeedsDrink          = readBool("Notifications", "NotifyNeedsDrink",          true);
        notifyNeedsHungryWarning  = readBool("Notifications", "NotifyNeedsHungryWarning",  true);
        notifyNeedsThirstyWarning = readBool("Notifications", "NotifyNeedsThirstyWarning", true);
        notifyLoot                = readBool("Notifications", "NotifyLoot",                true);
        notifyShopping            = readBool("Notifications", "NotifyShopping",            true);
        notifySelling             = readBool("Notifications", "NotifySelling",             true);
        notifyAutoEquip           = readBool("Notifications", "NotifyAutoEquip",           true);

        logger::info("Settings loaded: hunger={:.1f}h thirst={:.1f}h fatigue={:.1f}h "
            "eat<{:.0f}% drink<{:.0f}% delay={:.1f}s potions H<{:.0f}% M<{:.0f}% S<{:.0f}% "
            "systems=[combat:{} needs:{} equip:{} shop:{} sell:{} poison:{}]",
            hungerDepletionHours, thirstDepletionHours, fatigueDepletionHours,
            eatThreshold, drinkThreshold, consumeDelaySeconds,
            healthPotionThreshold, magickaPotionThreshold, staminaPotionThreshold,
            enableCombatPotions, enableNeedsSystem, enableAutoEquip,
            enableShoppingSystem, enableSellingSystem, enablePoisonSystem);
    }

    void Settings::Save()
    {
        auto iniPath = GetINIPath();
        std::error_code ec;
        std::filesystem::create_directories(iniPath.parent_path(), ec);

        auto pathStr = iniPath.string();
        const char* path = pathStr.c_str();

        auto writeFloat = [&](const char* section, const char* key, float val) {
            WritePrivateProfileStringA(section, key, std::format("{:.2f}", val).c_str(), path);
        };
        auto writeBool = [&](const char* section, const char* key, bool val) {
            WritePrivateProfileStringA(section, key, val ? "1" : "0", path);
        };
        auto writeInt = [&](const char* section, const char* key, int val) {
            WritePrivateProfileStringA(section, key, std::to_string(val).c_str(), path);
        };

        // ── NEEDS RATES ──
        writeFloat("NeedsRates", "HungerDepletionHours",  hungerDepletionHours);
        writeFloat("NeedsRates", "ThirstDepletionHours",  thirstDepletionHours);
        writeFloat("NeedsRates", "FatigueDepletionHours", fatigueDepletionHours);

        // ── THRESHOLDS ──
        writeFloat("Thresholds", "EatThreshold",           eatThreshold);
        writeFloat("Thresholds", "DrinkThreshold",         drinkThreshold);
        writeFloat("Thresholds", "ConsumeDelaySeconds",    consumeDelaySeconds);
        writeFloat("Thresholds", "ConsumeIntervalSeconds", consumeIntervalSeconds);
        writeFloat("Thresholds", "PostCombatConsumeDelaySeconds", postCombatConsumeDelaySeconds);
        writeFloat("Thresholds", "HealthPotionThreshold",  healthPotionThreshold);
        writeFloat("Thresholds", "MagickaPotionThreshold", magickaPotionThreshold);
        writeFloat("Thresholds", "StaminaPotionThreshold", staminaPotionThreshold);
        writeFloat("Thresholds", "EmergencyHealthPct",  emergencyHealthPct);
        writeFloat("Thresholds", "EmergencyMagickaPct", emergencyMagickaPct);
        writeFloat("Thresholds", "EmergencyStaminaPct", emergencyStaminaPct);

        // ── SYSTEM TOGGLES ──
        writeBool("Systems", "EnableCombatPotions",  enableCombatPotions);
        writeBool("Systems", "EnableNeedsSystem",    enableNeedsSystem);
        writeBool("Systems", "EnableAutoEquip",      enableAutoEquip);
        writeBool("Systems", "EnableShoppingSystem", enableShoppingSystem);
        writeBool("Systems", "EnableSellingSystem",  enableSellingSystem);
        writeBool("Systems", "EnablePoisonSystem",   enablePoisonSystem);
        writeBool("Systems", "EnablePetSystem",      enablePetSystem);
        writeBool("Systems", "ProtectEquippedClothing", protectEquippedClothing);
        writeBool("Systems", "PromptOnBetterEquipment", promptOnBetterEquipment);
        writeInt("Systems",  "EquipmentUpgradeChance",  static_cast<int>(equipmentUpgradeChance));
        writeBool("Systems", "EnableEquipmentPurchaseOffers", enableEquipmentPurchaseOffers);
        writeBool("Systems", "MagePreferRobes",         magePreferRobes);

        // ── LOOTING ──
        writeBool("Looting", "AutoLootCorpses",      autoLootCorpses);
        writeFloat("Looting", "LootDelaySeconds",    lootDelaySeconds);
        writeInt("Looting", "LootMaxSpareWeapons",   static_cast<int>(lootMaxSpareWeapons));
        writeInt("Looting", "LootMaxSpareArmor",     static_cast<int>(lootMaxSpareArmor));
        writeBool("Looting", "LootHostilesOnly",     lootHostilesOnly);

        writeBool("Looting", "LootGold",        lootGold);
        writeBool("Looting", "LootWeapons",     lootWeapons);
        writeBool("Looting", "LootArmor",       lootArmor);
        writeBool("Looting", "LootArrows",      lootArrows);
        writeBool("Looting", "LootPotions",     lootPotions);
        writeBool("Looting", "LootFood",        lootFood);
        writeBool("Looting", "LootIngredients", lootIngredients);
        writeBool("Looting", "LootSoulGems",    lootSoulGems);
        writeBool("Looting", "LootMisc",        lootMisc);

        // ── SELLING CATEGORY FILTERS ──
        writeBool("Selling", "SellWeapons",                sellWeapons);
        writeBool("Selling", "SellArmor",                  sellArmor);
        writeBool("Selling", "SellAmmo",                   sellAmmo);
        writeBool("Selling", "SellPotions",                sellPotions);
        writeBool("Selling", "SellFood",                   sellFood);
        writeBool("Selling", "SellGems",                   sellGems);
        writeBool("Selling", "ProtectSoulGems",            protectSoulGems);
        writeInt("Selling",  "TransactionBatchMultiplier", static_cast<int>(transactionBatchMultiplier));

        // ── NOTIFICATION TOGGLES ──
        writeBool("Notifications", "EnableNotifications",       enableNotifications);
        writeBool("Notifications", "NotifyCombatHealthPotion",  notifyCombatHealthPotion);
        writeBool("Notifications", "NotifyCombatMagickaPotion", notifyCombatMagickaPotion);
        writeBool("Notifications", "NotifyCombatStaminaPotion", notifyCombatStaminaPotion);
        writeBool("Notifications", "NotifyPoisonApplied",       notifyPoisonApplied);
        writeBool("Notifications", "NotifyNeedsFood",           notifyNeedsFood);
        writeBool("Notifications", "NotifyNeedsDrink",          notifyNeedsDrink);
        writeBool("Notifications", "NotifyNeedsHungryWarning",  notifyNeedsHungryWarning);
        writeBool("Notifications", "NotifyNeedsThirstyWarning", notifyNeedsThirstyWarning);
        writeBool("Notifications", "NotifyLoot",                notifyLoot);
        writeBool("Notifications", "NotifyShopping",            notifyShopping);
        writeBool("Notifications", "NotifySelling",             notifySelling);
        writeBool("Notifications", "NotifyAutoEquip",           notifyAutoEquip);

        logger::info("Settings saved to {}", iniPath.string());
    }
}
