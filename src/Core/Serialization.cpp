#include "Core/Serialization.h"
#include "Core/FollowerManager.h"
#include "Core/Settings.h"

namespace FollowerUnbound
{
    namespace
    {
        std::mutex g_pendingProfilesMutex;
        std::vector<FollowerProfile> g_pendingProfiles;
    }

    void Serialization::SaveCallback(SKSE::SerializationInterface* a_intfc)
    {
        if (!a_intfc) return;

        auto manager = FollowerManager::GetSingleton();
        // GetProfilesSnapshot: shared_lock ile thread-safe kopya (GetAllProfiles lock almaz)
        const auto profiles = manager->GetProfilesSnapshot();

        if (!a_intfc->OpenRecord(RecordTypeFollowerProfile, SerializationVersion)) {
            logger::error("Failed to open record for Follower Profiles");
            return;
        }

        uint32_t count = static_cast<uint32_t>(profiles.size());
        a_intfc->WriteRecordData(count);

        for (const auto& [formID, profile] : profiles) {
            a_intfc->WriteRecordData(profile.formID);
            a_intfc->WriteRecordData(profile.gold);
            a_intfc->WriteRecordData(profile.role);
            a_intfc->WriteRecordData(profile.personality);
            a_intfc->WriteRecordData(profile.needs);
            a_intfc->WriteRecordData(profile.preferences);

            uint32_t protectedCount = static_cast<uint32_t>(profile.protectedEquipment.size());
            a_intfc->WriteRecordData(protectedCount);
            for (auto itemID : profile.protectedEquipment) {
                a_intfc->WriteRecordData(itemID);
            }

            uint32_t declinedCount = static_cast<uint32_t>(profile.declinedEquipmentPurchases.size());
            a_intfc->WriteRecordData(declinedCount);
            for (auto itemID : profile.declinedEquipmentPurchases) {
                a_intfc->WriteRecordData(itemID);
            }
        }

        logger::info("Saved {} follower profiles to co-save.", count);
    }

    struct FollowerPreferencesV1
    {
        bool allowLoot{ true };
        bool allowLootGold{ true };
        bool allowLootWeapons{ true };
        bool allowLootArmor{ true };
        bool allowLootPotions{ true };
        bool allowLootArrows{ true };
        bool allowLootFood{ true };
        bool allowBuyFood{ true };
        bool allowBuyPotions{ true };
        bool allowBuyEquipment{ true };
        bool allowBuyAmmunition{ true };
        bool allowCraftArrows{ true };
        bool allowCraftPotions{ true };
        bool allowUpgradeEquipment{ true };
        uint32_t maxArrows{ 200 };
        uint32_t minArrows{ 50 };
        uint32_t maxHealthPotions{ 10 };
        uint32_t maxMagickaPotions{ 10 };
        uint32_t maxStaminaPotions{ 10 };
        uint32_t maxPoisons{ 5 };
        bool receiveQuestRewards{ true };
        float rewardSharePercent{ 25.0f };
    };

    // v2 kayıtları için ayna struct — v3'te soul gem alanları eklendi (buySoulGems,
    // maxSoulGems). Bu struct eski binary layout ile birebir aynı olmalı.
    struct FollowerPreferencesV2
    {
        bool allowLoot{ true };
        bool allowLootGold{ true };
        bool allowLootWeapons{ true };
        bool allowLootArmor{ true };
        bool allowLootPotions{ true };
        bool allowLootArrows{ true };
        bool allowLootFood{ true };
        bool allowBuyFood{ true };
        bool allowBuyPotions{ true };
        bool allowBuyEquipment{ true };
        bool allowBuyAmmunition{ true };
        bool buyHealthPotions{ true };
        uint32_t maxHealthPotions{ 5 };
        bool buyMagickaPotions{ false };
        uint32_t maxMagickaPotions{ 5 };
        bool buyStaminaPotions{ false };
        uint32_t maxStaminaPotions{ 5 };
        uint32_t maxPoisons{ 5 };
        bool buyFoodLarge{ true };
        uint32_t maxFoodLarge{ 3 };
        bool buyFoodMedium{ true };
        uint32_t maxFoodMedium{ 5 };
        bool buyFoodSmall{ true };
        uint32_t maxFoodSmall{ 5 };
        bool buyDrinkNormal{ true };
        uint32_t maxDrinkNormal{ 5 };
        bool buyDrinkAlcohol{ false };
        uint32_t maxDrinkAlcohol{ 2 };
        bool buyArrows{ true };
        uint32_t maxArrows{ 50 };
        uint32_t minArrows{ 20 };
        bool buyBolts{ false };
        uint32_t maxBolts{ 50 };
        bool allowCraftArrows{ true };
        bool allowCraftPotions{ true };
        bool allowUpgradeEquipment{ true };
        bool receiveQuestRewards{ true };
        float rewardSharePercent{ 25.0f };
    };

    struct FollowerPreferencesV3
    {
        bool allowLoot{ true };
        bool allowLootGold{ true };
        bool allowLootWeapons{ true };
        bool allowLootArmor{ true };
        bool allowLootPotions{ true };
        bool allowLootArrows{ true };
        bool allowLootFood{ true };
        bool allowBuyFood{ true };
        bool allowBuyPotions{ true };
        bool allowBuyEquipment{ true };
        bool allowBuyAmmunition{ true };
        bool buyHealthPotions{ true };
        uint32_t maxHealthPotions{ 5 };
        bool buyMagickaPotions{ false };
        uint32_t maxMagickaPotions{ 5 };
        bool buyStaminaPotions{ false };
        uint32_t maxStaminaPotions{ 5 };
        uint32_t maxPoisons{ 5 };
        bool buyFoodLarge{ true };
        uint32_t maxFoodLarge{ 3 };
        bool buyFoodMedium{ true };
        uint32_t maxFoodMedium{ 5 };
        bool buyFoodSmall{ true };
        uint32_t maxFoodSmall{ 5 };
        bool buyDrinkNormal{ true };
        uint32_t maxDrinkNormal{ 5 };
        bool buyDrinkAlcohol{ false };
        uint32_t maxDrinkAlcohol{ 2 };
        bool buyArrows{ true };
        uint32_t maxArrows{ 50 };
        uint32_t minArrows{ 20 };
        bool buyBolts{ false };
        uint32_t maxBolts{ 50 };
        bool buySoulGems{ true };
        uint32_t maxSoulGems{ 5 };
        bool allowCraftArrows{ true };
        bool allowCraftPotions{ true };
        bool allowUpgradeEquipment{ true };
        bool receiveQuestRewards{ true };
        float rewardSharePercent{ 25.0f };
    };

    struct FollowerPreferencesV4
    {
        bool allowLoot{ true };
        bool allowLootGold{ true };
        bool allowLootWeapons{ true };
        bool allowLootArmor{ true };
        bool allowLootPotions{ true };
        bool allowLootArrows{ true };
        bool allowLootFood{ true };
        bool allowBuyFood{ true };
        bool allowBuyPotions{ true };
        bool allowBuyEquipment{ true };
        bool allowBuyAmmunition{ true };
        bool buyHealthPotions{ true };
        uint32_t maxHealthPotions{ 5 };
        bool buyMagickaPotions{ false };
        uint32_t maxMagickaPotions{ 5 };
        bool buyStaminaPotions{ false };
        uint32_t maxStaminaPotions{ 5 };
        uint32_t maxPoisons{ 5 };
        bool buyFoodLarge{ true };
        uint32_t maxFoodLarge{ 3 };
        bool buyFoodMedium{ true };
        uint32_t maxFoodMedium{ 5 };
        bool buyFoodSmall{ true };
        uint32_t maxFoodSmall{ 5 };
        bool buyDrinkNormal{ true };
        uint32_t maxDrinkNormal{ 5 };
        bool buyDrinkAlcohol{ false };
        uint32_t maxDrinkAlcohol{ 2 };
        bool buyArrows{ true };
        uint32_t maxArrows{ 50 };
        uint32_t minArrows{ 20 };
        bool buyBolts{ false };
        uint32_t maxBolts{ 50 };
        bool buySoulGems{ true };
        uint32_t maxSoulGems{ 5 };
        bool buyTorches{ true };
        uint32_t maxTorches{ 5 };
        bool buyLanternOil{ true };
        uint32_t maxLanternOil{ 5 };
        bool buySoap{ true };
        uint32_t maxSoap{ 5 };
        bool allowCraftArrows{ true };
        bool allowCraftPotions{ true };
        bool allowUpgradeEquipment{ true };
        bool receiveQuestRewards{ true };
        float rewardSharePercent{ 25.0f };
    };

    void Serialization::LoadCallback(SKSE::SerializationInterface* a_intfc)
    {
        if (!a_intfc) return;

        uint32_t type, version, length;
        {
            std::lock_guard lock(g_pendingProfilesMutex);
            g_pendingProfiles.clear();
        }

        while (a_intfc->GetNextRecordInfo(type, version, length)) {
            uint32_t bytesRead = 0;
            bool recordOK = true;
            auto read = [&](auto& value) {
                const auto expected = static_cast<uint32_t>(sizeof(value));
                const auto actual = a_intfc->ReadRecordData(value);
                bytesRead += actual;
                if (actual != expected) recordOK = false;
                return recordOK;
            };
            if (type == RecordTypeFollowerProfile) {
                if (version < 1 || version > SerializationVersion) {
                    logger::warn("Unsupported follower co-save version {} (record size {}).", version, length);
                    break;
                }
                uint32_t count = 0;
                if (!read(count) || count > 256 || count > length / 36) {
                    logger::warn("Invalid follower count in co-save record (count={}, bytes={}).", count, length);
                    break;
                }

                for (uint32_t i = 0; i < count; ++i) {
                    RE::FormID oldFormID = 0;
                    read(oldFormID);

                    RE::FormID newFormID = 0;
                    bool resolved = a_intfc->ResolveFormID(oldFormID, newFormID);

                    uint32_t gold = 0;
                    FollowerRole role = FollowerRole::Warrior;
                    FollowerPersonality personality = FollowerPersonality::Balanced;
                    FollowerNeeds needs;
                    FollowerPreferences preferences;

                    read(gold);
                    read(role);
                    read(personality);
                    read(needs);

                    if (version == 1) {
                        FollowerPreferencesV1 v1;
                        read(v1);
                        preferences.allowLoot = v1.allowLoot;
                        preferences.allowLootGold = v1.allowLootGold;
                        preferences.allowLootWeapons = v1.allowLootWeapons;
                        preferences.allowLootArmor = v1.allowLootArmor;
                        preferences.allowLootPotions = v1.allowLootPotions;
                        preferences.allowLootArrows = v1.allowLootArrows;
                        preferences.allowLootFood = v1.allowLootFood;
                        preferences.allowBuyFood = v1.allowBuyFood;
                        preferences.allowBuyPotions = v1.allowBuyPotions;
                        preferences.allowBuyEquipment = v1.allowBuyEquipment;
                        preferences.allowBuyAmmunition = v1.allowBuyAmmunition;
                        preferences.maxArrows = v1.maxArrows;
                        preferences.minArrows = v1.minArrows;
                        preferences.maxHealthPotions = v1.maxHealthPotions;
                        preferences.maxMagickaPotions = v1.maxMagickaPotions;
                        preferences.maxStaminaPotions = v1.maxStaminaPotions;
                        preferences.maxPoisons = v1.maxPoisons;
                        preferences.receiveQuestRewards = v1.receiveQuestRewards;
                        preferences.rewardSharePercent = v1.rewardSharePercent;
                    } else if (version == 2) {
                        FollowerPreferencesV2 v2;
                        read(v2);
                        preferences.allowLoot = v2.allowLoot;
                        preferences.allowLootGold = v2.allowLootGold;
                        preferences.allowLootWeapons = v2.allowLootWeapons;
                        preferences.allowLootArmor = v2.allowLootArmor;
                        preferences.allowLootPotions = v2.allowLootPotions;
                        preferences.allowLootArrows = v2.allowLootArrows;
                        preferences.allowLootFood = v2.allowLootFood;
                        preferences.allowBuyFood = v2.allowBuyFood;
                        preferences.allowBuyPotions = v2.allowBuyPotions;
                        preferences.allowBuyEquipment = v2.allowBuyEquipment;
                        preferences.allowBuyAmmunition = v2.allowBuyAmmunition;
                        preferences.buyHealthPotions = v2.buyHealthPotions;
                        preferences.maxHealthPotions = v2.maxHealthPotions;
                        preferences.buyMagickaPotions = v2.buyMagickaPotions;
                        preferences.maxMagickaPotions = v2.maxMagickaPotions;
                        preferences.buyStaminaPotions = v2.buyStaminaPotions;
                        preferences.maxStaminaPotions = v2.maxStaminaPotions;
                        preferences.maxPoisons = v2.maxPoisons;
                        preferences.buyFoodLarge = v2.buyFoodLarge;
                        preferences.maxFoodLarge = v2.maxFoodLarge;
                        preferences.buyFoodMedium = v2.buyFoodMedium;
                        preferences.maxFoodMedium = v2.maxFoodMedium;
                        preferences.buyFoodSmall = v2.buyFoodSmall;
                        preferences.maxFoodSmall = v2.maxFoodSmall;
                        preferences.buyDrinkNormal = v2.buyDrinkNormal;
                        preferences.maxDrinkNormal = v2.maxDrinkNormal;
                        preferences.buyDrinkAlcohol = v2.buyDrinkAlcohol;
                        preferences.maxDrinkAlcohol = v2.maxDrinkAlcohol;
                        preferences.buyArrows = v2.buyArrows;
                        preferences.maxArrows = v2.maxArrows;
                        preferences.minArrows = v2.minArrows;
                        preferences.buyBolts = v2.buyBolts;
                        preferences.maxBolts = v2.maxBolts;
                        preferences.allowCraftArrows = v2.allowCraftArrows;
                        preferences.allowCraftPotions = v2.allowCraftPotions;
                        preferences.allowUpgradeEquipment = v2.allowUpgradeEquipment;
                        preferences.receiveQuestRewards = v2.receiveQuestRewards;
                        preferences.rewardSharePercent = v2.rewardSharePercent;
                    } else if (version == 3) {
                        FollowerPreferencesV3 v3;
                        read(v3);
                        preferences.allowLoot = v3.allowLoot;
                        preferences.allowLootGold = v3.allowLootGold;
                        preferences.allowLootWeapons = v3.allowLootWeapons;
                        preferences.allowLootArmor = v3.allowLootArmor;
                        preferences.allowLootPotions = v3.allowLootPotions;
                        preferences.allowLootArrows = v3.allowLootArrows;
                        preferences.allowLootFood = v3.allowLootFood;
                        preferences.allowBuyFood = v3.allowBuyFood;
                        preferences.allowBuyPotions = v3.allowBuyPotions;
                        preferences.allowBuyEquipment = v3.allowBuyEquipment;
                        preferences.allowBuyAmmunition = v3.allowBuyAmmunition;
                        preferences.buyHealthPotions = v3.buyHealthPotions;
                        preferences.maxHealthPotions = v3.maxHealthPotions;
                        preferences.buyMagickaPotions = v3.buyMagickaPotions;
                        preferences.maxMagickaPotions = v3.maxMagickaPotions;
                        preferences.buyStaminaPotions = v3.buyStaminaPotions;
                        preferences.maxStaminaPotions = v3.maxStaminaPotions;
                        preferences.maxPoisons = v3.maxPoisons;
                        preferences.buyFoodLarge = v3.buyFoodLarge;
                        preferences.maxFoodLarge = v3.maxFoodLarge;
                        preferences.buyFoodMedium = v3.buyFoodMedium;
                        preferences.maxFoodMedium = v3.maxFoodMedium;
                        preferences.buyFoodSmall = v3.buyFoodSmall;
                        preferences.maxFoodSmall = v3.maxFoodSmall;
                        preferences.buyDrinkNormal = v3.buyDrinkNormal;
                        preferences.maxDrinkNormal = v3.maxDrinkNormal;
                        preferences.buyDrinkAlcohol = v3.buyDrinkAlcohol;
                        preferences.maxDrinkAlcohol = v3.maxDrinkAlcohol;
                        preferences.buyArrows = v3.buyArrows;
                        preferences.maxArrows = v3.maxArrows;
                        preferences.minArrows = v3.minArrows;
                        preferences.buyBolts = v3.buyBolts;
                        preferences.maxBolts = v3.maxBolts;
                        preferences.buySoulGems = v3.buySoulGems;
                        preferences.maxSoulGems = v3.maxSoulGems;
                        preferences.allowCraftArrows = v3.allowCraftArrows;
                        preferences.allowCraftPotions = v3.allowCraftPotions;
                        preferences.allowUpgradeEquipment = v3.allowUpgradeEquipment;
                        preferences.receiveQuestRewards = v3.receiveQuestRewards;
                        preferences.rewardSharePercent = v3.rewardSharePercent;
                    } else if (version == 4) {
                        FollowerPreferencesV4 v4;
                        read(v4);
                        preferences.allowLoot = v4.allowLoot;
                        preferences.allowLootGold = v4.allowLootGold;
                        preferences.allowLootWeapons = v4.allowLootWeapons;
                        preferences.allowLootArmor = v4.allowLootArmor;
                        preferences.allowLootPotions = v4.allowLootPotions;
                        preferences.allowLootArrows = v4.allowLootArrows;
                        preferences.allowLootFood = v4.allowLootFood;
                        preferences.allowBuyFood = v4.allowBuyFood;
                        preferences.allowBuyPotions = v4.allowBuyPotions;
                        preferences.allowBuyEquipment = v4.allowBuyEquipment;
                        preferences.allowBuyAmmunition = v4.allowBuyAmmunition;
                        preferences.buyHealthPotions = v4.buyHealthPotions;
                        preferences.maxHealthPotions = v4.maxHealthPotions;
                        preferences.buyMagickaPotions = v4.buyMagickaPotions;
                        preferences.maxMagickaPotions = v4.maxMagickaPotions;
                        preferences.buyStaminaPotions = v4.buyStaminaPotions;
                        preferences.maxStaminaPotions = v4.maxStaminaPotions;
                        preferences.maxPoisons = v4.maxPoisons;
                        preferences.buyFoodLarge = v4.buyFoodLarge;
                        preferences.maxFoodLarge = v4.maxFoodLarge;
                        preferences.buyFoodMedium = v4.buyFoodMedium;
                        preferences.maxFoodMedium = v4.maxFoodMedium;
                        preferences.buyFoodSmall = v4.buyFoodSmall;
                        preferences.maxFoodSmall = v4.maxFoodSmall;
                        preferences.buyDrinkNormal = v4.buyDrinkNormal;
                        preferences.maxDrinkNormal = v4.maxDrinkNormal;
                        preferences.buyDrinkAlcohol = v4.buyDrinkAlcohol;
                        preferences.maxDrinkAlcohol = v4.maxDrinkAlcohol;
                        preferences.buyArrows = v4.buyArrows;
                        preferences.maxArrows = v4.maxArrows;
                        preferences.minArrows = v4.minArrows;
                        preferences.buyBolts = v4.buyBolts;
                        preferences.maxBolts = v4.maxBolts;
                        preferences.buySoulGems = v4.buySoulGems;
                        preferences.maxSoulGems = v4.maxSoulGems;
                        preferences.buyTorches = v4.buyTorches;
                        preferences.maxTorches = v4.maxTorches;
                        preferences.buyLanternOil = v4.buyLanternOil;
                        preferences.maxLanternOil = v4.maxLanternOil;
                        preferences.buySoap = v4.buySoap;
                        preferences.maxSoap = v4.maxSoap;
                        preferences.allowCraftArrows = v4.allowCraftArrows;
                        preferences.allowCraftPotions = v4.allowCraftPotions;
                        preferences.allowUpgradeEquipment = v4.allowUpgradeEquipment;
                        preferences.receiveQuestRewards = v4.receiveQuestRewards;
                        preferences.rewardSharePercent = v4.rewardSharePercent;
                    } else if (version == 5) {
                        // Version 6 appends four treatment stock fields to
                        // FollowerPreferences. Read the unchanged v5 prefix
                        // and keep the new fields' defaults for old saves.
                        constexpr auto legacySize = static_cast<uint32_t>(offsetof(FollowerPreferences, buyTreatmentPotions));
                        const auto actual = a_intfc->ReadRecordData(&preferences, legacySize);
                        bytesRead += actual;
                        if (actual != legacySize) recordOK = false;
                    } else if (version == 6) {
                        // Version 7 appends the independent poison-buy switch.
                        constexpr auto legacySize = static_cast<uint32_t>(offsetof(FollowerPreferences, buyPoisons));
                        const auto actual = a_intfc->ReadRecordData(&preferences, legacySize);
                        bytesRead += actual;
                        if (actual != legacySize) recordOK = false;
                        // v6 defaults had these two purchase toggles disabled;
                        // v7 makes them enabled as the new out-of-box behavior.
                        preferences.buyMagickaPotions = true;
                        preferences.buyStaminaPotions = true;
                        preferences.buyPoisons = true;
                    } else if (version == 7) {
                        // Version 8 appends the separate bolt-crafting toggle.
                        constexpr auto legacySize = static_cast<uint32_t>(offsetof(FollowerPreferences, allowCraftBolts));
                        const auto actual = a_intfc->ReadRecordData(&preferences, legacySize);
                        bytesRead += actual;
                        if (actual != legacySize) recordOK = false;
                    } else if (version == 8) {
                        // Version 9 appends the torch-crafting preference.
                        constexpr auto legacySize = static_cast<uint32_t>(offsetof(FollowerPreferences, allowCraftTorches));
                        const auto actual = a_intfc->ReadRecordData(&preferences, legacySize);
                        bytesRead += actual;
                        if (actual != legacySize) recordOK = false;
                    } else if (version == 9) {
                        // Version 10 appends follower-specific gold savings.
                        constexpr auto legacySize = static_cast<uint32_t>(offsetof(FollowerPreferences, savingsEnabled));
                        const auto actual = a_intfc->ReadRecordData(&preferences, legacySize);
                        bytesRead += actual;
                        if (actual != legacySize) recordOK = false;
                    } else if (version == 10) {
                        // Version 11 appends fractional savings accumulation.
                        constexpr auto legacySize = static_cast<uint32_t>(offsetof(FollowerPreferences, savingsRemainder));
                        const auto actual = a_intfc->ReadRecordData(&preferences, legacySize);
                        bytesRead += actual;
                        if (actual != legacySize) recordOK = false;
                    } else if (version == 11) {
                        // Version 12 appends per-capacity soul gem purchase preferences.
                        constexpr auto legacySize = static_cast<uint32_t>(offsetof(FollowerPreferences, buyPettySoulGems));
                        const auto actual = a_intfc->ReadRecordData(&preferences, legacySize);
                        bytesRead += actual;
                        if (actual != legacySize) recordOK = false;
                    } else if (version == 12 || version == 13) {
                        // Version 14 appends the restorative-flower stock target.
                        constexpr auto legacySize = static_cast<uint32_t>(offsetof(FollowerPreferences, maxPotionFlowers));
                        const auto actual = a_intfc->ReadRecordData(&preferences, legacySize);
                        bytesRead += actual;
                        if (actual != legacySize) recordOK = false;
                    } else if (version == 14) {
                        // Version 15 appends per-tier restorative potion purchase options.
                        constexpr auto legacySize = static_cast<uint32_t>(offsetof(FollowerPreferences, buyHealthMinor));
                        const auto actual = a_intfc->ReadRecordData(&preferences, legacySize);
                        bytesRead += actual;
                        if (actual != legacySize) recordOK = false;
                    } else if (version == 15) {
                        // Version 16 appends poison-type and invisibility purchasing preferences.
                        constexpr auto legacySize = static_cast<uint32_t>(offsetof(FollowerPreferences, buyPoisonHealth));
                        const auto actual = a_intfc->ReadRecordData(&preferences, legacySize);
                        bytesRead += actual;
                        if (actual != legacySize) recordOK = false;
                    } else if (version == 16 || version == 17) {
                        // Version 18 appends independent purchase targets for
                        // potion tiers, poison effects, and potion ingredients.
                        constexpr auto legacySize = static_cast<uint32_t>(offsetof(FollowerPreferences, maxHealthMinor));
                        const auto actual = a_intfc->ReadRecordData(&preferences, legacySize);
                        bytesRead += actual;
                        if (actual != legacySize) recordOK = false;
                    } else {
                        read(preferences);
                    }

                    // Version 13 changes the out-of-box crafting policy. Old saves
                    // inherited enabled arrow crafting from legacy defaults; turn
                    // both ammunition crafting options off once during migration.
                    if (version < 13) {
                        preferences.allowCraftArrows = false;
                        preferences.allowCraftBolts = false;
                    }
                    if (version < 14) preferences.maxPotionFlowers = 30;
                    if (version < 15) {
                        // New policy: only the weakest tier is enabled by default.
                        preferences.buyHealthMinor = true;
                        preferences.buyMagickaMinor = true;
                        preferences.buyStaminaMinor = true;
                        preferences.buyHealthPotions = false;
                        preferences.buyMagickaPotions = false;
                        preferences.buyStaminaPotions = false;
                    }
                    if (version < 16) preferences.buyInvisibilityPotions = true;
                    if (version < 17) {
                        // Enable soul gem purchasing and selling by default in
                        // existing profiles as well as newly registered ones.
                        preferences.allowSellSoulGems = true;
                        preferences.buyPettySoulGems = true;
                        preferences.buyLesserSoulGems = true;
                        preferences.buyCommonSoulGems = true;
                        preferences.buyGreaterSoulGems = true;
                        preferences.buyGrandSoulGems = true;
                        preferences.buyBlackSoulGems = true;
                    }
                    if (version < 18) {
                        // Preserve the old shared targets as the initial value
                        // for every newly independent tier/effect.
                        preferences.maxHealthMinor = preferences.maxHealthPotions;
                        preferences.maxHealthStandard = preferences.maxHealthPotions;
                        preferences.maxHealthPlentiful = preferences.maxHealthPotions;
                        preferences.maxHealthVigorous = preferences.maxHealthPotions;
                        preferences.maxHealthExtreme = preferences.maxHealthPotions;
                        preferences.maxHealthUltimate = preferences.maxHealthPotions;
                        preferences.maxMagickaMinor = preferences.maxMagickaPotions;
                        preferences.maxMagickaStandard = preferences.maxMagickaPotions;
                        preferences.maxMagickaPlentiful = preferences.maxMagickaPotions;
                        preferences.maxMagickaVigorous = preferences.maxMagickaPotions;
                        preferences.maxMagickaExtreme = preferences.maxMagickaPotions;
                        preferences.maxMagickaUltimate = preferences.maxMagickaPotions;
                        preferences.maxStaminaMinor = preferences.maxStaminaPotions;
                        preferences.maxStaminaStandard = preferences.maxStaminaPotions;
                        preferences.maxStaminaPlentiful = preferences.maxStaminaPotions;
                        preferences.maxStaminaVigorous = preferences.maxStaminaPotions;
                        preferences.maxStaminaExtreme = preferences.maxStaminaPotions;
                        preferences.maxStaminaUltimate = preferences.maxStaminaPotions;
                        preferences.maxPoisonHealth = preferences.maxPoisons;
                        preferences.maxPoisonMagicka = preferences.maxPoisons;
                        preferences.maxPoisonStamina = preferences.maxPoisons;
                        preferences.maxPoisonOther = preferences.maxPoisons;
                    }

                    // New defaults: routine arrow/bolt purchases are opt-in;
                    // magicka, stamina and poison supplies are enabled.
                    if (version < 8) {
                        preferences.buyMagickaPotions = true;
                        preferences.buyStaminaPotions = true;
                        preferences.buyPoisons = true;
                        preferences.buyArrows = false;
                        preferences.buyBolts = false;
                    }
                    preferences.savingsTarget = std::clamp(preferences.savingsTarget, 1u, 10000u);
                    preferences.savedGold = std::min(preferences.savedGold, preferences.savingsTarget);
                    preferences.savingsPercent = std::min(preferences.savingsPercent, 100u);

                    uint32_t pCount = 0;
                    if (!read(pCount) || pCount > 512 || bytesRead > length ||
                        pCount > (length - bytesRead) / sizeof(RE::FormID)) {
                        logger::warn("Invalid protected item count for follower {:08X}.", oldFormID);
                        recordOK = false;
                        break;
                    }

                    std::unordered_set<RE::FormID> protectedItems;
                    for (uint32_t p = 0; p < pCount; ++p) {
                        RE::FormID oldItemID = 0;
                        read(oldItemID);
                        RE::FormID newItemID = 0;
                        if (resolved && a_intfc->ResolveFormID(oldItemID, newItemID)) {
                            protectedItems.insert(newItemID);
                        }
                    }

                    std::unordered_set<RE::FormID> declinedEquipmentPurchases;
                    if (version >= 19) {
                        uint32_t declinedCount = 0;
                        if (!read(declinedCount) || declinedCount > 4096 || bytesRead > length ||
                            declinedCount > (length - bytesRead) / sizeof(RE::FormID)) {
                            logger::warn("Invalid declined equipment count for follower {:08X}.", oldFormID);
                            recordOK = false;
                            break;
                        }
                        for (uint32_t p = 0; p < declinedCount; ++p) {
                            RE::FormID oldItemID = 0;
                            read(oldItemID);
                            RE::FormID newItemID = 0;
                            if (resolved && a_intfc->ResolveFormID(oldItemID, newItemID)) {
                                declinedEquipmentPurchases.insert(newItemID);
                            }
                        }
                    }

                    if (!resolved) {
                        logger::warn("Failed to resolve FormID {:08X}", oldFormID);
                        continue;
                    }

                    FollowerProfile profile(newFormID);
                    profile.gold = gold;
                    profile.role = role;
                    profile.personality = personality;
                    profile.needs = needs;
                    profile.preferences = preferences;
                    profile.protectedEquipment = std::move(protectedItems);
                    profile.declinedEquipmentPurchases = std::move(declinedEquipmentPurchases);

                    // Co-save callbacks run while the game is restoring its
                    // world. Defer actor lookup, follower checks, and inventory
                    // scans until kPostLoadGame has completed.
                    {
                        std::lock_guard lock(g_pendingProfilesMutex);
                        g_pendingProfiles.push_back(std::move(profile));
                    }
                }
                if (!recordOK) {
                    logger::warn("Stopped loading malformed follower co-save record after {} of {} bytes.", bytesRead, length);
                    break;
                }
                logger::info("Loaded follower profiles from co-save.");
            }
        }
    }

    void Serialization::ApplyPendingProfiles()
    {
        std::vector<FollowerProfile> pending;
        {
            std::lock_guard lock(g_pendingProfilesMutex);
            pending.swap(g_pendingProfiles);
        }

        auto* manager = FollowerManager::GetSingleton();
        for (auto& savedProfile : pending) {
            manager->RestoreSavedProfile(std::move(savedProfile));
        }

        logger::info("Applied {} saved follower profile(s) after world load.", pending.size());
    }

    void Serialization::RevertCallback(SKSE::SerializationInterface*)
    {
        {
            std::lock_guard lock(g_pendingProfilesMutex);
            g_pendingProfiles.clear();
        }
        FollowerManager::GetSingleton()->Clear();
        logger::info("Reverted follower profiles.");
    }
}

