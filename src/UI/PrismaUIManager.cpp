#include "UI/PrismaUIManager.h"
#include "UI/NotificationManager.h"
#include "UI/EquipPrompt.h"
#include "PrismaUI_API.h"
#include "UI/SKSEMenuFramework.h"
#include "Core/FollowerManager.h"
#include "Core/Settings.h"
#include "Economy/EconomyManager.h"
#include "Economy/ShoppingAI.h"
#include "AI/ItemEvaluator.h"
#include "AI/AnimationManager.h"
#include "Core/TNGCompatibility.h"
#include <cctype>
#include <cmath>

namespace FollowerUnbound
{
    namespace
    {
        std::string GetPortraitRaceKey(RE::Actor* actor)
        {
            auto* race = actor ? actor->GetRace() : nullptr;
            const char* rawName = race ? race->GetName() : nullptr;
            if (!rawName) return {};

            std::string name(rawName);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name.find("khajiit") != std::string::npos) return "khajiit";
            if (name.find("argonian") != std::string::npos) return "argonian";
            if (name.find("orc") != std::string::npos || name.find("orsimer") != std::string::npos) return "orc";
            if (name.find("wood elf") != std::string::npos || name.find("bosmer") != std::string::npos) return "wood_elf";
            if (name.find("dark elf") != std::string::npos || name.find("dunmer") != std::string::npos) return "dark_elf";
            if (name.find("high elf") != std::string::npos || name.find("altmer") != std::string::npos) return "high_elf";
            if (name.find("redguard") != std::string::npos) return "redguard";
            if (name.find("breton") != std::string::npos) return "breton";
            if (name.find("imperial") != std::string::npos) return "imperial";
            if (name.find("nord") != std::string::npos) return "nord";
            return {};
        }

        std::atomic_bool g_leftShiftDown{ false };
        std::atomic_bool g_rightShiftDown{ false };
        std::atomic_bool g_leftCtrlDown{ false };
        std::atomic_bool g_rightCtrlDown{ false };
        std::atomic_bool g_leftAltDown{ false };
        std::atomic_bool g_rightAltDown{ false };

        void PressEscapeForFramework()
        {
            INPUT events[2]{};
            for (auto& event : events) event.type = INPUT_KEYBOARD;
            events[0].ki.wVk = VK_ESCAPE;
            events[1].ki.wVk = VK_ESCAPE;
            events[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, events, sizeof(INPUT));
        }

        void AddPreferences(nlohmann::json& out, const FollowerPreferences& p)
        {
            out = {
                {"allowLoot",p.allowLoot},{"lootWeapons",p.allowLootWeapons},{"lootArmor",p.allowLootArmor},
                {"lootClothing",p.allowLootClothing},{"lootPotions",p.allowLootPotions},{"lootFood",p.allowLootFood},
                {"lootDrinks",p.allowLootDrinks},{"lootGold",p.allowLootGold},{"lootArrows",p.allowLootArrows},
                {"lootIngredients",p.allowLootIngredients},{"lootSoulGems",p.allowLootSoulGems},{"lootGems",p.allowLootGems},
                {"lootBooks",p.allowLootBooks},{"lootScrolls",p.allowLootScrolls},
                {"sellWeapons",p.allowSellWeapons},{"sellArmor",p.allowSellArmor},{"sellClothing",p.allowSellClothing},
                {"sellJewelry",p.allowSellJewelry},{"sellAmmo",p.allowSellAmmo},{"sellPotions",p.allowSellPotions},
                {"sellFood",p.allowSellFood},{"sellDrinks",p.allowSellDrinks},{"sellIngredients",p.allowSellIngredients},
                {"sellSoulGems",p.allowSellSoulGems},{"sellGems",p.allowSellGems},{"sellBooks",p.allowSellBooks},
                {"sellScrolls",p.allowSellScrolls},{"buyHealthPotions",p.buyHealthPotions},
                {"buyMagickaPotions",p.buyMagickaPotions},{"buyStaminaPotions",p.buyStaminaPotions},
                {"maxHealthPotions",p.maxHealthPotions},{"maxMagickaPotions",p.maxMagickaPotions},
                {"maxStaminaPotions",p.maxStaminaPotions},{"buyPoisons",p.buyPoisons},{"maxPoisons",p.maxPoisons},
                {"buyFoodLarge",p.buyFoodLarge},{"buyFoodMedium",p.buyFoodMedium},{"buyFoodSmall",p.buyFoodSmall},
                {"maxFoodLarge",p.maxFoodLarge},{"maxFoodMedium",p.maxFoodMedium},{"maxFoodSmall",p.maxFoodSmall},
                {"buyDrinkNormal",p.buyDrinkNormal},{"buyDrinkAlcohol",p.buyDrinkAlcohol},
                {"maxDrinkNormal",p.maxDrinkNormal},{"maxDrinkAlcohol",p.maxDrinkAlcohol},
                {"buyArrows",p.buyArrows},{"maxArrows",p.maxArrows},{"buyBolts",p.buyBolts},{"maxBolts",p.maxBolts},
                {"buySoulGems",p.buySoulGems},{"maxSoulGems",p.maxSoulGems},{"buyTorches",p.buyTorches},
                {"buyPettySoulGems",p.buyPettySoulGems},{"maxPettySoulGems",p.maxPettySoulGems},{"soulGemFillPetty",p.soulGemFillPetty},
                {"buyLesserSoulGems",p.buyLesserSoulGems},{"maxLesserSoulGems",p.maxLesserSoulGems},{"soulGemFillLesser",p.soulGemFillLesser},
                {"buyCommonSoulGems",p.buyCommonSoulGems},{"maxCommonSoulGems",p.maxCommonSoulGems},{"soulGemFillCommon",p.soulGemFillCommon},
                {"buyGreaterSoulGems",p.buyGreaterSoulGems},{"maxGreaterSoulGems",p.maxGreaterSoulGems},{"soulGemFillGreater",p.soulGemFillGreater},
                {"buyGrandSoulGems",p.buyGrandSoulGems},{"maxGrandSoulGems",p.maxGrandSoulGems},{"soulGemFillGrand",p.soulGemFillGrand},
                {"buyBlackSoulGems",p.buyBlackSoulGems},{"maxBlackSoulGems",p.maxBlackSoulGems},{"soulGemFillBlack",p.soulGemFillBlack},
                {"maxPotionFlowers",p.maxPotionFlowers},
                {"buyHealthMinor",p.buyHealthMinor},{"buyHealthStandard",p.buyHealthStandard},{"buyHealthPlentiful",p.buyHealthPlentiful},
                {"buyHealthVigorous",p.buyHealthVigorous},{"buyHealthExtreme",p.buyHealthExtreme},{"buyHealthUltimate",p.buyHealthUltimate},
                {"maxHealthMinor",p.maxHealthMinor},{"maxHealthStandard",p.maxHealthStandard},{"maxHealthPlentiful",p.maxHealthPlentiful},
                {"maxHealthVigorous",p.maxHealthVigorous},{"maxHealthExtreme",p.maxHealthExtreme},{"maxHealthUltimate",p.maxHealthUltimate},
                {"buyMagickaMinor",p.buyMagickaMinor},{"buyMagickaStandard",p.buyMagickaStandard},{"buyMagickaPlentiful",p.buyMagickaPlentiful},
                {"buyMagickaVigorous",p.buyMagickaVigorous},{"buyMagickaExtreme",p.buyMagickaExtreme},{"buyMagickaUltimate",p.buyMagickaUltimate},
                {"maxMagickaMinor",p.maxMagickaMinor},{"maxMagickaStandard",p.maxMagickaStandard},{"maxMagickaPlentiful",p.maxMagickaPlentiful},
                {"maxMagickaVigorous",p.maxMagickaVigorous},{"maxMagickaExtreme",p.maxMagickaExtreme},{"maxMagickaUltimate",p.maxMagickaUltimate},
                {"buyStaminaMinor",p.buyStaminaMinor},{"buyStaminaStandard",p.buyStaminaStandard},{"buyStaminaPlentiful",p.buyStaminaPlentiful},
                {"buyStaminaVigorous",p.buyStaminaVigorous},{"buyStaminaExtreme",p.buyStaminaExtreme},{"buyStaminaUltimate",p.buyStaminaUltimate},
                {"maxStaminaMinor",p.maxStaminaMinor},{"maxStaminaStandard",p.maxStaminaStandard},{"maxStaminaPlentiful",p.maxStaminaPlentiful},
                {"maxStaminaVigorous",p.maxStaminaVigorous},{"maxStaminaExtreme",p.maxStaminaExtreme},{"maxStaminaUltimate",p.maxStaminaUltimate},
                {"buyPoisonHealth",p.buyPoisonHealth},{"buyPoisonMagicka",p.buyPoisonMagicka},
                {"buyPoisonStamina",p.buyPoisonStamina},{"buyPoisonOther",p.buyPoisonOther},
                {"maxPoisonHealth",p.maxPoisonHealth},{"maxPoisonMagicka",p.maxPoisonMagicka},
                {"maxPoisonStamina",p.maxPoisonStamina},{"maxPoisonOther",p.maxPoisonOther},
                {"maxBuyBlueMountainFlowers",p.maxBuyBlueMountainFlowers},{"maxBuyWheat",p.maxBuyWheat},
                {"maxBuyRedMountainFlowers",p.maxBuyRedMountainFlowers},{"maxBuyWhiteCaps",p.maxBuyWhiteCaps},
                {"maxBuyPurpleMountainFlowers",p.maxBuyPurpleMountainFlowers},{"maxBuyPineThrushEggs",p.maxBuyPineThrushEggs},
                {"buyInvisibilityPotions",p.buyInvisibilityPotions},{"maxInvisibilityPotions",p.maxInvisibilityPotions},
                {"maxTorches",p.maxTorches},{"buyLanternOil",p.buyLanternOil},{"maxLanternOil",p.maxLanternOil},
                {"buySoap",p.buySoap},{"maxSoap",p.maxSoap},
                {"buyTreatmentPotions",p.buyTreatmentPotions},{"maxTreatmentPotions",p.maxTreatmentPotions},
                {"buyCurePoisonPotions",p.buyCurePoisonPotions},{"maxCurePoisonPotions",p.maxCurePoisonPotions},
                {"allowCraftArrows",p.allowCraftArrows},{"allowCraftBolts",p.allowCraftBolts},
                {"allowCraftTorches",p.allowCraftTorches},
                {"savingsEnabled",p.savingsEnabled},{"savingsTarget",p.savingsTarget},
                {"savingsPercent",p.savingsPercent},{"savedGold",p.savedGold},
                {"allowCraftPotions",p.allowCraftPotions},{"allowUpgradeEquipment",p.allowUpgradeEquipment},
                {"receiveQuestRewards",p.receiveQuestRewards},{"rewardSharePercent",p.rewardSharePercent},
                {"preferredArmorType",p.preferredArmorType},
                {"primaryWeaponStyle",p.primaryWeaponStyle},
                {"primaryRightHand",p.primaryRightHand},{"primaryLeftHand",p.primaryLeftHand},
                {"primaryTwoHandType",p.primaryTwoHandType},
                {"secondaryWeaponStyle",p.secondaryWeaponStyle},
                {"secondaryRightHand",p.secondaryRightHand},{"secondaryLeftHand",p.secondaryLeftHand},
                {"secondaryTwoHandType",p.secondaryTwoHandType}
            };
        }

        uint32_t ReadBoundedUInt(const nlohmann::json& value, uint32_t minimum, uint32_t maximum)
        {
            if (!value.is_number()) return minimum;
            const double number = value.get<double>();
            if (!std::isfinite(number)) return minimum;
            return static_cast<uint32_t>(std::clamp(std::trunc(number),
                static_cast<double>(minimum), static_cast<double>(maximum)));
        }

        void SetPreference(FollowerPreferences& p, const std::string& key, const nlohmann::json& value)
        {
#define FU_BOOL_PREF(name, field) if (key == name) { p.field = value.get<bool>(); return; }
#define FU_UINT_PREF(name, field, lo, hi) if (key == name) { p.field = ReadBoundedUInt(value, uint32_t(lo), uint32_t(hi)); return; }
            if (key == "savingsEnabled") {
                p.savingsEnabled = value.get<bool>();
                if (!p.savingsEnabled) { p.savedGold = 0; p.savingsRemainder = 0; } // Turning savings off releases the reserve.
                return;
            }
            if (key == "savingsTarget") {
                p.savingsTarget = ReadBoundedUInt(value, 1u, 10000u);
                p.savedGold = std::min(p.savedGold, p.savingsTarget);
                if (p.savedGold >= p.savingsTarget) p.savingsRemainder = 0;
                return;
            }
            FU_BOOL_PREF("allowLoot", allowLoot) FU_BOOL_PREF("lootWeapons", allowLootWeapons)
            FU_BOOL_PREF("lootArmor", allowLootArmor) FU_BOOL_PREF("lootClothing", allowLootClothing)
            FU_BOOL_PREF("lootPotions", allowLootPotions) FU_BOOL_PREF("lootFood", allowLootFood)
            FU_BOOL_PREF("lootDrinks", allowLootDrinks) FU_BOOL_PREF("lootGold", allowLootGold)
            FU_BOOL_PREF("lootArrows", allowLootArrows) FU_BOOL_PREF("lootIngredients", allowLootIngredients)
            FU_BOOL_PREF("lootSoulGems", allowLootSoulGems) FU_BOOL_PREF("lootGems", allowLootGems)
            FU_BOOL_PREF("lootBooks", allowLootBooks) FU_BOOL_PREF("lootScrolls", allowLootScrolls)
            FU_BOOL_PREF("sellWeapons", allowSellWeapons) FU_BOOL_PREF("sellArmor", allowSellArmor)
            FU_BOOL_PREF("sellClothing", allowSellClothing) FU_BOOL_PREF("sellJewelry", allowSellJewelry)
            FU_BOOL_PREF("sellAmmo", allowSellAmmo) FU_BOOL_PREF("sellPotions", allowSellPotions)
            FU_BOOL_PREF("sellFood", allowSellFood) FU_BOOL_PREF("sellDrinks", allowSellDrinks)
            FU_BOOL_PREF("sellIngredients", allowSellIngredients) FU_BOOL_PREF("sellSoulGems", allowSellSoulGems)
            FU_BOOL_PREF("sellGems", allowSellGems) FU_BOOL_PREF("sellBooks", allowSellBooks)
            FU_BOOL_PREF("sellScrolls", allowSellScrolls) FU_BOOL_PREF("buyHealthPotions", buyHealthPotions)
            FU_BOOL_PREF("buyMagickaPotions", buyMagickaPotions) FU_BOOL_PREF("buyStaminaPotions", buyStaminaPotions)
            FU_UINT_PREF("maxHealthPotions", maxHealthPotions, 0, 25) FU_UINT_PREF("maxMagickaPotions", maxMagickaPotions, 0, 25)
            FU_UINT_PREF("maxStaminaPotions", maxStaminaPotions, 0, 25) FU_BOOL_PREF("buyPoisons", buyPoisons) FU_UINT_PREF("maxPoisons", maxPoisons, 0, 25)
            FU_BOOL_PREF("buyFoodLarge", buyFoodLarge) FU_BOOL_PREF("buyFoodMedium", buyFoodMedium) FU_BOOL_PREF("buyFoodSmall", buyFoodSmall)
            FU_UINT_PREF("maxFoodLarge", maxFoodLarge, 0, 25) FU_UINT_PREF("maxFoodMedium", maxFoodMedium, 0, 25)
            FU_UINT_PREF("maxFoodSmall", maxFoodSmall, 0, 25) FU_BOOL_PREF("buyDrinkNormal", buyDrinkNormal)
            FU_BOOL_PREF("buyDrinkAlcohol", buyDrinkAlcohol) FU_UINT_PREF("maxDrinkNormal", maxDrinkNormal, 0, 25)
            FU_UINT_PREF("maxDrinkAlcohol", maxDrinkAlcohol, 0, 25) FU_BOOL_PREF("buyArrows", buyArrows)
            FU_UINT_PREF("maxArrows", maxArrows, 0, 250) FU_BOOL_PREF("buyBolts", buyBolts)
            FU_UINT_PREF("maxBolts", maxBolts, 0, 250) FU_BOOL_PREF("buySoulGems", buySoulGems)
            FU_UINT_PREF("maxSoulGems", maxSoulGems, 0, 20) FU_BOOL_PREF("buyTorches", buyTorches)
            FU_BOOL_PREF("buyPettySoulGems", buyPettySoulGems) FU_UINT_PREF("maxPettySoulGems", maxPettySoulGems, 1, 20) FU_UINT_PREF("soulGemFillPetty", soulGemFillPetty, 0, 2)
            FU_BOOL_PREF("buyLesserSoulGems", buyLesserSoulGems) FU_UINT_PREF("maxLesserSoulGems", maxLesserSoulGems, 1, 20) FU_UINT_PREF("soulGemFillLesser", soulGemFillLesser, 0, 2)
            FU_BOOL_PREF("buyCommonSoulGems", buyCommonSoulGems) FU_UINT_PREF("maxCommonSoulGems", maxCommonSoulGems, 1, 20) FU_UINT_PREF("soulGemFillCommon", soulGemFillCommon, 0, 2)
            FU_BOOL_PREF("buyGreaterSoulGems", buyGreaterSoulGems) FU_UINT_PREF("maxGreaterSoulGems", maxGreaterSoulGems, 1, 20) FU_UINT_PREF("soulGemFillGreater", soulGemFillGreater, 0, 2)
            FU_BOOL_PREF("buyGrandSoulGems", buyGrandSoulGems) FU_UINT_PREF("maxGrandSoulGems", maxGrandSoulGems, 1, 20) FU_UINT_PREF("soulGemFillGrand", soulGemFillGrand, 0, 2)
            FU_BOOL_PREF("buyBlackSoulGems", buyBlackSoulGems) FU_UINT_PREF("maxBlackSoulGems", maxBlackSoulGems, 1, 20) FU_UINT_PREF("soulGemFillBlack", soulGemFillBlack, 0, 2)
            FU_UINT_PREF("maxPotionFlowers", maxPotionFlowers, 1, 50)
            FU_BOOL_PREF("buyHealthMinor", buyHealthMinor) FU_BOOL_PREF("buyHealthStandard", buyHealthStandard)
            FU_BOOL_PREF("buyHealthPlentiful", buyHealthPlentiful) FU_BOOL_PREF("buyHealthVigorous", buyHealthVigorous)
            FU_BOOL_PREF("buyHealthExtreme", buyHealthExtreme) FU_BOOL_PREF("buyHealthUltimate", buyHealthUltimate)
            FU_UINT_PREF("maxHealthMinor", maxHealthMinor, 0, 25) FU_UINT_PREF("maxHealthStandard", maxHealthStandard, 0, 25)
            FU_UINT_PREF("maxHealthPlentiful", maxHealthPlentiful, 0, 25) FU_UINT_PREF("maxHealthVigorous", maxHealthVigorous, 0, 25)
            FU_UINT_PREF("maxHealthExtreme", maxHealthExtreme, 0, 25) FU_UINT_PREF("maxHealthUltimate", maxHealthUltimate, 0, 25)
            FU_BOOL_PREF("buyMagickaMinor", buyMagickaMinor) FU_BOOL_PREF("buyMagickaStandard", buyMagickaStandard)
            FU_BOOL_PREF("buyMagickaPlentiful", buyMagickaPlentiful) FU_BOOL_PREF("buyMagickaVigorous", buyMagickaVigorous)
            FU_BOOL_PREF("buyMagickaExtreme", buyMagickaExtreme) FU_BOOL_PREF("buyMagickaUltimate", buyMagickaUltimate)
            FU_UINT_PREF("maxMagickaMinor", maxMagickaMinor, 0, 25) FU_UINT_PREF("maxMagickaStandard", maxMagickaStandard, 0, 25)
            FU_UINT_PREF("maxMagickaPlentiful", maxMagickaPlentiful, 0, 25) FU_UINT_PREF("maxMagickaVigorous", maxMagickaVigorous, 0, 25)
            FU_UINT_PREF("maxMagickaExtreme", maxMagickaExtreme, 0, 25) FU_UINT_PREF("maxMagickaUltimate", maxMagickaUltimate, 0, 25)
            FU_BOOL_PREF("buyStaminaMinor", buyStaminaMinor) FU_BOOL_PREF("buyStaminaStandard", buyStaminaStandard)
            FU_BOOL_PREF("buyStaminaPlentiful", buyStaminaPlentiful) FU_BOOL_PREF("buyStaminaVigorous", buyStaminaVigorous)
            FU_BOOL_PREF("buyStaminaExtreme", buyStaminaExtreme) FU_BOOL_PREF("buyStaminaUltimate", buyStaminaUltimate)
            FU_UINT_PREF("maxStaminaMinor", maxStaminaMinor, 0, 25) FU_UINT_PREF("maxStaminaStandard", maxStaminaStandard, 0, 25)
            FU_UINT_PREF("maxStaminaPlentiful", maxStaminaPlentiful, 0, 25) FU_UINT_PREF("maxStaminaVigorous", maxStaminaVigorous, 0, 25)
            FU_UINT_PREF("maxStaminaExtreme", maxStaminaExtreme, 0, 25) FU_UINT_PREF("maxStaminaUltimate", maxStaminaUltimate, 0, 25)
            FU_BOOL_PREF("buyPoisonHealth", buyPoisonHealth) FU_BOOL_PREF("buyPoisonMagicka", buyPoisonMagicka)
            FU_BOOL_PREF("buyPoisonStamina", buyPoisonStamina) FU_BOOL_PREF("buyPoisonOther", buyPoisonOther)
            FU_UINT_PREF("maxPoisonHealth", maxPoisonHealth, 0, 25) FU_UINT_PREF("maxPoisonMagicka", maxPoisonMagicka, 0, 25)
            FU_UINT_PREF("maxPoisonStamina", maxPoisonStamina, 0, 25) FU_UINT_PREF("maxPoisonOther", maxPoisonOther, 0, 25)
            FU_UINT_PREF("maxBuyBlueMountainFlowers", maxBuyBlueMountainFlowers, 0, 50) FU_UINT_PREF("maxBuyWheat", maxBuyWheat, 0, 50)
            FU_UINT_PREF("maxBuyRedMountainFlowers", maxBuyRedMountainFlowers, 0, 50) FU_UINT_PREF("maxBuyWhiteCaps", maxBuyWhiteCaps, 0, 50)
            FU_UINT_PREF("maxBuyPurpleMountainFlowers", maxBuyPurpleMountainFlowers, 0, 50) FU_UINT_PREF("maxBuyPineThrushEggs", maxBuyPineThrushEggs, 0, 50)
            FU_BOOL_PREF("buyInvisibilityPotions", buyInvisibilityPotions) FU_UINT_PREF("maxInvisibilityPotions", maxInvisibilityPotions, 0, 25)
            FU_UINT_PREF("maxTorches", maxTorches, 0, 20) FU_BOOL_PREF("buyLanternOil", buyLanternOil)
            FU_UINT_PREF("maxLanternOil", maxLanternOil, 0, 20) FU_BOOL_PREF("buySoap", buySoap)
            FU_UINT_PREF("maxSoap", maxSoap, 0, 20) FU_BOOL_PREF("allowCraftArrows", allowCraftArrows)
            FU_BOOL_PREF("buyTreatmentPotions", buyTreatmentPotions) FU_UINT_PREF("maxTreatmentPotions", maxTreatmentPotions, 0, 25)
            FU_BOOL_PREF("buyCurePoisonPotions", buyCurePoisonPotions) FU_UINT_PREF("maxCurePoisonPotions", maxCurePoisonPotions, 0, 25)
            FU_BOOL_PREF("allowCraftBolts", allowCraftBolts) FU_BOOL_PREF("allowCraftTorches", allowCraftTorches)
            FU_BOOL_PREF("savingsEnabled", savingsEnabled) FU_UINT_PREF("savingsPercent", savingsPercent, 0, 100)
            FU_BOOL_PREF("allowCraftPotions", allowCraftPotions) FU_BOOL_PREF("allowUpgradeEquipment", allowUpgradeEquipment)
            FU_BOOL_PREF("receiveQuestRewards", receiveQuestRewards)
            FU_UINT_PREF("preferredArmorType", preferredArmorType, 0, 3)
            FU_UINT_PREF("primaryWeaponStyle", primaryWeaponStyle, 0, 5)
            FU_UINT_PREF("primaryRightHand", primaryRightHand, 0, 5)
            FU_UINT_PREF("primaryLeftHand", primaryLeftHand, 0, 7)
            FU_UINT_PREF("primaryTwoHandType", primaryTwoHandType, 0, 3)
            FU_UINT_PREF("secondaryWeaponStyle", secondaryWeaponStyle, 0, 5)
            FU_UINT_PREF("secondaryRightHand", secondaryRightHand, 0, 5)
            FU_UINT_PREF("secondaryLeftHand", secondaryLeftHand, 0, 7)
            FU_UINT_PREF("secondaryTwoHandType", secondaryTwoHandType, 0, 3)
            if (key == "rewardSharePercent") p.rewardSharePercent = std::clamp(value.get<float>(), 0.0f, 100.0f);
#undef FU_BOOL_PREF
#undef FU_UINT_PREF
        }
    }

    bool PrismaUIManager::Initialize()
    {
        if (_api) return true;
        auto* apiV2 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
        _api = apiV2 ? static_cast<PRISMA_UI_API::IVPrismaUI1*>(apiV2) :
            PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
        if (!_api) {
            logger::info("PrismaUI not found; keeping SKSE Menu Framework interface available.");
            return false;
        }
        _view = _api->CreateView("FollowerUnbound/index.html", [](PrismaView view) {
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([view]() {
                auto* self = GetSingleton();
                if (!self->_api || self->_view != view || !self->_api->IsValid(view)) return;
                self->_viewReady = true;
                self->_api->Hide(view);
                self->SendState();
            });
        });
        if (_view == 0 || !_api->IsValid(_view)) {
            logger::error("Could not create PrismaUI Follower Unbound view.");
            _api = nullptr;
            return false;
        }
        _api->RegisterJSListener(_view, "fuAction", OnAction);
        _api->RegisterJSListener(_view, "fuClose", OnClose);
        if (auto* apiV2 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>()) {
            apiV2->RegisterConsoleCallback(_view, [](PrismaView view, PRISMA_UI_API::ConsoleMessageLevel level, const char* msg) {
                if (level == PRISMA_UI_API::ConsoleMessageLevel::Error) logger::error("PrismaUI Follower Unbound {}: {}", view, msg ? msg : "(null)");
                else if (level == PRISMA_UI_API::ConsoleMessageLevel::Warning) logger::warn("PrismaUI Follower Unbound {}: {}", view, msg ? msg : "(null)");
            });
        }
        _api->Hide(_view);

        // Keep a transparent, non-interactive overlay alive independently from
        // the management menu so game notifications can be rendered by PrismaUI.
        _notificationView = _api->CreateView("FollowerUnbound/notifications.html", [](PrismaView view) {
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([view]() {
                auto* self = GetSingleton();
                if (!self->_api || self->_notificationView != view || !self->_api->IsValid(view)) return;
                self->_notificationViewReady = true;
                self->_api->SetOrder(view, 1000);
                self->_api->Show(view);
            });
        });
        if (_notificationView == 0 || !_api->IsValid(_notificationView)) {
            logger::error("Could not create PrismaUI notification overlay; mod notifications will be unavailable.");
            _notificationView = 0;
        } else if (auto* apiV2 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>()) {
            apiV2->RegisterConsoleCallback(_notificationView, [](PrismaView view, PRISMA_UI_API::ConsoleMessageLevel level, const char* msg) {
                if (level == PRISMA_UI_API::ConsoleMessageLevel::Error) logger::error("PrismaUI Notifications {}: {}", view, msg ? msg : "(null)");
                else if (level == PRISMA_UI_API::ConsoleMessageLevel::Warning) logger::warn("PrismaUI Notifications {}: {}", view, msg ? msg : "(null)");
            });
        }
        if (SKSEMenuFramework::IsInstalled()) {
            _hotkeyListener = SKSEMenuFramework::AddInputEvent(OnFrameworkInput);
        }
        logger::info("Registered Follower Unbound PrismaUI; shortcut is Shift+I.");
        return true;
    }

    bool PrismaUIManager::ShowNotification(std::string_view message)
    {
        if (!Settings::GetSingleton()->enableNotifications) return false;
        if (!_api || !_notificationView || !_notificationViewReady || !_api->IsValid(_notificationView)) return false;
        const auto argument = nlohmann::json(std::string(message)).dump();
        const auto script = "window.fuNotify && window.fuNotify(" + argument + ");";
        const auto notificationView = _notificationView;
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([this, notificationView, script]() {
                if (_api && _notificationViewReady && _api->IsValid(notificationView)) {
                    _api->Invoke(notificationView, script.c_str());
                }
            });
            return true;
        }
        return false;
    }

    void PrismaUIManager::ClearNotifications()
    {
        if (!_api || !_notificationView || !_notificationViewReady || !_api->IsValid(_notificationView)) return;
        const auto notificationView = _notificationView;
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([this, notificationView]() {
                if (_api && _notificationViewReady && _api->IsValid(notificationView)) {
                    _api->Invoke(notificationView, "window.fuClearNotifications && window.fuClearNotifications();");
                }
            });
        }
    }

    bool __stdcall PrismaUIManager::OnFrameworkInput(RE::InputEvent* event)
    {
        bool openRequested = false;
        bool closeRequested = false;
        for (auto* current = event; current; current = current->next) {
            if (current->GetDevice() != RE::INPUT_DEVICE::kKeyboard) continue;
            auto* button = current->AsButtonEvent();
            if (!button) continue;

            const auto key = button->GetIDCode();
            auto* self = GetSingleton();
            if (key == RE::BSKeyboardDevice::Keys::kEscape && button->IsDown() &&
                self->IsAvailable() && !self->_api->IsHidden(self->_view)) {
                closeRequested = true;
                continue;
            }
            if (key == RE::BSKeyboardDevice::Keys::kLeftShift) {
                if (button->IsUp()) g_leftShiftDown.store(false, std::memory_order_relaxed);
                else if (button->IsPressed()) g_leftShiftDown.store(true, std::memory_order_relaxed);
                continue;
            }
            if (key == RE::BSKeyboardDevice::Keys::kRightShift) {
                if (button->IsUp()) g_rightShiftDown.store(false, std::memory_order_relaxed);
                else if (button->IsPressed()) g_rightShiftDown.store(true, std::memory_order_relaxed);
                continue;
            }
            if (key == RE::BSKeyboardDevice::Keys::kLeftControl) {
                if (button->IsUp()) g_leftCtrlDown.store(false, std::memory_order_relaxed);
                else if (button->IsPressed()) g_leftCtrlDown.store(true, std::memory_order_relaxed);
                continue;
            }
            if (key == RE::BSKeyboardDevice::Keys::kRightControl) {
                if (button->IsUp()) g_rightCtrlDown.store(false, std::memory_order_relaxed);
                else if (button->IsPressed()) g_rightCtrlDown.store(true, std::memory_order_relaxed);
                continue;
            }
            if (key == RE::BSKeyboardDevice::Keys::kLeftAlt) {
                if (button->IsUp()) g_leftAltDown.store(false, std::memory_order_relaxed);
                else if (button->IsPressed()) g_leftAltDown.store(true, std::memory_order_relaxed);
                continue;
            }
            if (key == RE::BSKeyboardDevice::Keys::kRightAlt) {
                if (button->IsUp()) g_rightAltDown.store(false, std::memory_order_relaxed);
                else if (button->IsPressed()) g_rightAltDown.store(true, std::memory_order_relaxed);
                continue;
            }

            auto* settings = Settings::GetSingleton();
            const uint32_t targetKey = settings->menuHotkey;
            const uint32_t targetMod = settings->menuHotkeyModifier; // 0=None, 1=Shift, 2=Ctrl, 3=Alt

            bool modSatisfied = false;
            if (targetMod == 0) {
                modSatisfied = true;
            } else if (targetMod == 1) {
                modSatisfied = (g_leftShiftDown.load(std::memory_order_relaxed) || g_rightShiftDown.load(std::memory_order_relaxed));
            } else if (targetMod == 2) {
                modSatisfied = (g_leftCtrlDown.load(std::memory_order_relaxed) || g_rightCtrlDown.load(std::memory_order_relaxed));
            } else if (targetMod == 3) {
                modSatisfied = (g_leftAltDown.load(std::memory_order_relaxed) || g_rightAltDown.load(std::memory_order_relaxed));
            }

            if (key == targetKey && button->IsDown() && modSatisfied) {
                openRequested = true;
            }
        }

        if (closeRequested) {
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([]() { OnClose(nullptr); });
                return true;
            }
            return false;
        }
        if (!openRequested) return false;
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([]() { GetSingleton()->Open(); });
            return true;
        }
        return false;
    }

    void PrismaUIManager::Open()
    {
        if (!IsAvailable()) return;
        if (!_api->IsHidden(_view)) {
            if (EquipPrompt::HasActivePrompt()) EquipPrompt::Resolve(false);
            _api->Unfocus(_view);
            _api->Hide(_view);
            return;
        }
        // SKSEMenuFramework açık ise kapat, değilse beklemeden DOĞRUDAN ShowView aç!
        if (SKSEMenuFramework::IsInstalled() && SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([]() { GetSingleton()->BeginFrameworkClose(); });
                return;
            }
        }
        ShowView();
    }

    void PrismaUIManager::BeginFrameworkClose()
    {
        if (auto* window = SKSEMenuFramework::GetMainWindow()) {
            const bool wasOpen = window->IsOpen.exchange(false, std::memory_order_acq_rel);
            if (wasOpen) PressEscapeForFramework();
        } else {
            PressEscapeForFramework();
        }
        WaitForFrameworkClose(120);
    }

    void PrismaUIManager::WaitForFrameworkClose(uint32_t remaining)
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        tasks->AddTask([remaining]() {
            if (SKSEMenuFramework::IsInstalled() && SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
                if (remaining == 0) {
                    GetSingleton()->ShowNotification("Close the SKSE menu before opening Follower Unbound.");
                    return;
                }
                if (remaining % 20 == 0 && !SKSEMenuFramework::GetMainWindow()) PressEscapeForFramework();
                WaitForFrameworkClose(remaining - 1);
                return;
            }
            GetSingleton()->ShowView();
        });
    }

    void PrismaUIManager::ShowView()
    {
        if (!IsAvailable()) return;
        SendState();
        _api->Show(_view);
        _api->Focus(_view, true, false);
    }

    void PrismaUIManager::OnClose(const char*)
    {
        auto* self = GetSingleton();
        if (!self->IsAvailable()) return;
        // Closing the overlay while an equipment decision is active dismisses
        // that decision as declined. Never trap the player behind a modal.
        if (EquipPrompt::HasActivePrompt()) EquipPrompt::Resolve(false);
        if (self->_viewReady) self->_api->Invoke(self->_view, "window.fuHideEquipPrompt && window.fuHideEquipPrompt();");
        self->_api->Unfocus(self->_view);
        self->_api->Hide(self->_view);
    }

    void PrismaUIManager::ShowEquipPrompt(RE::FormID followerID, RE::FormID itemID, RE::FormID oldItemID,
        std::string_view followerName, std::string_view itemName, std::string_view slotLabel,
        std::string_view reason, std::string_view currentStats, std::string_view candidateStats)
    {
        if (!IsAvailable() || !_viewReady) {
            logger::warn("EquipPrompt: PrismaUI is unavailable; skipped prompt for {} / {}.", followerName, itemName);
            EquipPrompt::Resolve(false);
            return;
        }

        ShowView();
        nlohmann::json prompt = {
            {"follower", followerID}, {"item", itemID}, {"oldItem", oldItemID},
            {"followerName", std::string(followerName)}, {"itemName", std::string(itemName)},
            {"slotLabel", std::string(slotLabel)},
            {"reason", std::string(reason)}, {"currentStats", std::string(currentStats)},
            {"candidateStats", std::string(candidateStats)}, {"mode", "loot"}
        };
        const auto script = "window.fuShowEquipPrompt && window.fuShowEquipPrompt(" + prompt.dump() + ");";
        _api->Invoke(_view, script.c_str());
        logger::info("EquipPrompt: opened PrismaUI decision for {} / {} ({}).", followerName, itemName, slotLabel);
    }

    void PrismaUIManager::ShowEquipmentPurchasePrompt(RE::FormID followerID, RE::FormID itemID, RE::FormID oldItemID,
        std::string_view followerName, std::string_view itemName, std::string_view slotLabel,
        std::string_view vendorName, uint32_t price, uint32_t spendableGold,
        std::string_view currentStats, std::string_view candidateStats)
    {
        if (!IsAvailable() || !_viewReady) {
            logger::warn("EquipmentPurchase: PrismaUI is unavailable; skipped offer for {} / {}.", followerName, itemName);
            EquipPrompt::Resolve(false);
            return;
        }
        ShowView();
        nlohmann::json prompt = {
            {"follower", followerID}, {"item", itemID}, {"oldItem", oldItemID},
            {"followerName", std::string(followerName)}, {"itemName", std::string(itemName)},
            {"slotLabel", std::string(slotLabel)},
            {"vendorName", std::string(vendorName)}, {"price", price},
            {"spendableGold", spendableGold},
            {"currentStats", std::string(currentStats)}, {"candidateStats", std::string(candidateStats)},
            {"mode", "purchase"}
        };
        const auto script = "window.fuShowEquipPrompt && window.fuShowEquipPrompt(" + prompt.dump() + ");";
        _api->Invoke(_view, script.c_str());
        logger::info("EquipmentPurchase: opened PrismaUI offer for {} / {} ({}) at {} gold.", followerName, itemName, slotLabel, price);
    }

    void PrismaUIManager::OnAction(const char* data)
    {
        if (!data) return;
        try {
            const auto action = nlohmann::json::parse(data);
            if (action.value("kind", std::string{}) == "close") { OnClose(data); return; }
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([action]() {
                auto* self = GetSingleton();
                self->ApplyAction(action);
                self->SendState();
            });
        } catch (const std::exception& e) {
            logger::warn("Could not parse PrismaUI action: {}", e.what());
        }
    }

    void PrismaUIManager::ApplyAction(const nlohmann::json& a)
    {
        const auto kind = a.value("kind", std::string{});
        auto* manager = FollowerManager::GetSingleton();
        if (kind == "refresh") { manager->ScanForActiveFollowers(); return; }
        if (kind == "notificationsBulk") {
            auto* s = Settings::GetSingleton();
            const bool enable = a.value("enable", false);
            s->enableNotifications = enable;
            s->notifyLoot = enable;
            s->notifyShopping = enable;
            s->notifySelling = enable;
            s->notifyNeedsFood = enable;
            s->notifyNeedsDrink = enable;
            s->notifyNeedsHungryWarning = enable;
            s->notifyNeedsThirstyWarning = enable;
            s->notifyCombatHealthPotion = enable;
            s->notifyCombatMagickaPotion = enable;
            s->notifyCombatStaminaPotion = enable;
            s->notifyPoisonApplied = enable;
            s->notifyAutoEquip = enable;
            if (!enable) {
                NotificationManager::GetSingleton()->Clear();
                ClearNotifications();
            }
            s->Save();
            SendState();
            return;
        }
        if (kind == "setting") {
            auto* s = Settings::GetSingleton();
            const auto key = a.value("key", std::string{});
            const auto& v = a.at("value");
            if (key == "notifications") {
                const bool val = v.get<bool>();
                s->enableNotifications = val;
                if (!val) {
                    NotificationManager::GetSingleton()->Clear();
                    ClearNotifications();
                }
            } else
            if (key == "notifyNeeds") {
                const bool val = v.get<bool>();
                s->notifyNeedsFood = val;
                s->notifyNeedsDrink = val;
                s->notifyNeedsHungryWarning = val;
                s->notifyNeedsThirstyWarning = val;
            } else
            if (key == "notifyCombatPotions") {
                const bool val = v.get<bool>();
                s->notifyCombatHealthPotion = val;
                s->notifyCombatMagickaPotion = val;
                s->notifyCombatStaminaPotion = val;
                s->notifyPoisonApplied = val;
            } else
#define FU_SET_BOOL(name, field) if (key == name) s->field = v.get<bool>(); else
            FU_SET_BOOL("autoLoot", autoLootCorpses) FU_SET_BOOL("autoEquip", enableAutoEquip)
            FU_SET_BOOL("selling", enableSellingSystem) FU_SET_BOOL("shopping", enableShoppingSystem)
            FU_SET_BOOL("combatPotions", enableCombatPotions) FU_SET_BOOL("needs", enableNeedsSystem)
            FU_SET_BOOL("poison", enablePoisonSystem) FU_SET_BOOL("protectOutfit", protectEquippedClothing)
            FU_SET_BOOL("promptBetterGear", promptOnBetterEquipment) FU_SET_BOOL("mageRobes", magePreferRobes)
            FU_SET_BOOL("equipmentPurchaseOffers", enableEquipmentPurchaseOffers)
            FU_SET_BOOL("spellTomes", enableSpellTomeLearning)
            FU_SET_BOOL("notifyLoot", notifyLoot)
            FU_SET_BOOL("notifyShopping", notifyShopping)
            FU_SET_BOOL("notifySelling", notifySelling)
            FU_SET_BOOL("notifyAutoEquip", notifyAutoEquip)
            FU_SET_BOOL("lootHostilesOnly", lootHostilesOnly) FU_SET_BOOL("lootGold", lootGold)
            FU_SET_BOOL("lootWeapons", lootWeapons) FU_SET_BOOL("lootArmor", lootArmor)
            FU_SET_BOOL("lootArrows", lootArrows) FU_SET_BOOL("lootPotions", lootPotions)
            FU_SET_BOOL("lootFood", lootFood) FU_SET_BOOL("lootIngredients", lootIngredients)
            FU_SET_BOOL("lootSoulGems", lootSoulGems) FU_SET_BOOL("lootMisc", lootMisc)
            if (key == "notificationMode") s->notificationMode = std::clamp<uint32_t>(v.get<uint32_t>(), 0, 2);
            if (key == "lootDelay") s->lootDelaySeconds = std::clamp(v.get<float>(), 0.0f, 60.0f);
            if (key == "equipmentUpgradeChance") s->equipmentUpgradeChance = std::clamp<uint32_t>(v.get<uint32_t>(), 1, 100);
            s->Save();
            return;
        }
        if (kind == "gold") {
            const auto id = a.value("follower", uint32_t{ 0 });
            const auto amount = a.value("amount", 0);
            if ((amount == 100 || amount == 500) && id) {
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
                if (actor) EconomyManager::GetSingleton()->PlayerGiveGold(actor, amount);
            }
            return;
        }
        if (kind == "preference") {
            auto profile = manager->GetProfile(a.value("follower", uint32_t{ 0 }));
            if (!profile) return;
            SetPreference(profile->preferences, a.value("key", std::string{}), a.at("value"));
            return;
        }
        if (kind == "equipPromptDecision") {
            EquipPrompt::Resolve(a.value("accept", false));
            return;
        }
        if (kind == "setHotkey") {
            auto* s = Settings::GetSingleton();
            if (a.contains("hotkey")) {
                if (a["hotkey"].is_number()) s->menuHotkey = a["hotkey"].get<uint32_t>();
                else if (a["hotkey"].is_string()) s->menuHotkey = static_cast<uint32_t>(std::stoul(a["hotkey"].get<std::string>(), nullptr, 0));
            }
            if (a.contains("modifier")) {
                if (a["modifier"].is_number()) s->menuHotkeyModifier = a["modifier"].get<uint32_t>();
                else if (a["modifier"].is_string()) s->menuHotkeyModifier = static_cast<uint32_t>(std::stoul(a["modifier"].get<std::string>(), nullptr, 0));
            }
            if (a.contains("text") && a["text"].is_string()) {
                s->menuHotkeyText = a["text"].get<std::string>();
            }
            s->Save();
            logger::info("Hotkey updated to 0x{:X} (modifier={}, text={})", s->menuHotkey, s->menuHotkeyModifier, s->menuHotkeyText);
            return;
        }
        const auto followerID = a.value("follower", uint32_t{ 0 });
        auto profile = manager->GetProfile(followerID);
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(followerID);
        if (!profile || !actor) return;
        if (kind == "protect") {
            const auto itemID = a.value("item", uint32_t{ 0 });
            const bool protect = a.value("value", false);
            profile->ProtectItem(itemID, protect);
            // An item left equipped is intentionally excluded from sales. When the
            // player explicitly removes its protection, unequip it as well so it
            // becomes eligible for the next merchant sale without requiring a
            // second protected item to replace it first.
            if (!protect) {
                auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(itemID);
                if (object && (object->As<RE::TESObjectARMO>() || object->As<RE::TESObjectWEAP>())) {
                    if (auto* equip = RE::ActorEquipManager::GetSingleton()) {
                        equip->UnequipObject(actor, object, nullptr, 1, nullptr, false, false, false, true);
                        logger::info("EquipmentProtection: {} unprotected and unequipped {}; it is now eligible for sale.", actor->GetName(), object->GetName());
                    }
                }
            }
        } else if (kind == "equipSelected") {
            std::vector<uint32_t> ids = a.value("items", std::vector<uint32_t>{});
            auto* equip = RE::ActorEquipManager::GetSingleton();
            if (!equip) return;
            for (auto itemID : ids) {
                auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(itemID);
                if (!armor || IsTngHiddenBodyItem(armor)) continue;
                const auto mask = static_cast<uint32_t>(armor->GetSlotMask());
                std::vector<uint32_t> conflicts;
                for (auto protectedID : profile->protectedEquipment) {
                    if (std::find(ids.begin(), ids.end(), protectedID) != ids.end()) continue;
                    auto* oldArmor = RE::TESForm::LookupByID<RE::TESObjectARMO>(protectedID);
                    if (oldArmor && (static_cast<uint32_t>(oldArmor->GetSlotMask()) & mask)) conflicts.push_back(protectedID);
                }
                for (auto oldID : conflicts) profile->ProtectItem(oldID, false);
                profile->ProtectItem(itemID, true);

                // Zırh slotunu kalıcı loadout'a kaydet
                using Slot = RE::BIPED_MODEL::BipedObjectSlot;
                if (mask & static_cast<uint32_t>(Slot::kHead)) profile->assignedHead = itemID;
                if (mask & static_cast<uint32_t>(Slot::kBody)) profile->assignedBody = itemID;
                if (mask & static_cast<uint32_t>(Slot::kHands)) profile->assignedHands = itemID;
                if (mask & static_cast<uint32_t>(Slot::kFeet)) profile->assignedFeet = itemID;

                equip->EquipObject(actor, armor, nullptr, 1, nullptr, false, false, false, true);
            }
            for (auto itemID : ids) {
                auto* weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(itemID);
                if (!weapon) continue;
                profile->ProtectItem(itemID, true);
                profile->assignedPrimaryRight = itemID;
                equip->EquipObject(actor, weapon, nullptr, 1, nullptr, false, false, false, true);
            }
            AnimationManager::GetSingleton()->SheatheWeapon(actor);
        } else if (kind == "equipHand") {
            const auto itemID = a.value("item", uint32_t{ 0 });
            const bool isLeft = a.value("leftHand", false);
            const bool isSecondary = a.value("secondary", false);
            auto* equip = RE::ActorEquipManager::GetSingleton();
            if (!equip) return;
            if (auto* bound = RE::TESForm::LookupByID<RE::TESBoundObject>(itemID)) {
                profile->ProtectItem(itemID, true);

                if (isSecondary) {
                    if (isLeft) profile->assignedSecondaryLeft = itemID;
                    else profile->assignedSecondaryRight = itemID;
                } else {
                    if (isLeft) profile->assignedPrimaryLeft = itemID;
                    else profile->assignedPrimaryRight = itemID;
                }

                // 0x00013F42 = RightHand, 0x00013F43 = LeftHand
                auto* slot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(isLeft ? 0x00013F43 : 0x00013F42);
                equip->EquipObject(actor, bound, nullptr, 1, slot, false, false, false, true);
                AnimationManager::GetSingleton()->SheatheWeapon(actor);
            }
        }
    }

    void PrismaUIManager::SendState()
    {
        if (!_api || !_view || !_api->IsValid(_view) || !_viewReady) return;
        nlohmann::json state;
        auto* settings = Settings::GetSingleton();
        state["settings"] = {
            {"autoLoot",settings->autoLootCorpses},{"autoEquip",settings->enableAutoEquip},
            {"selling",settings->enableSellingSystem},{"shopping",settings->enableShoppingSystem},
            {"combatPotions",settings->enableCombatPotions},{"needs",settings->enableNeedsSystem},
            {"poison",settings->enablePoisonSystem},{"protectOutfit",settings->protectEquippedClothing},
            {"promptBetterGear",settings->promptOnBetterEquipment},{"mageRobes",settings->magePreferRobes},
            {"equipmentPurchaseOffers",settings->enableEquipmentPurchaseOffers},{"spellTomes",settings->enableSpellTomeLearning},
            {"notifications",settings->enableNotifications},
            {"notifyLoot",settings->notifyLoot},
            {"notifyShopping",settings->notifyShopping},
            {"notifySelling",settings->notifySelling},
            {"notifyNeeds",settings->notifyNeedsFood},
            {"notifyCombatPotions",settings->notifyCombatHealthPotion},
            {"notifyAutoEquip",settings->notifyAutoEquip},
            {"lootHostilesOnly",settings->lootHostilesOnly},{"lootGold",settings->lootGold},
            {"lootWeapons",settings->lootWeapons},{"lootArmor",settings->lootArmor},
            {"lootArrows",settings->lootArrows},{"lootPotions",settings->lootPotions},
            {"lootFood",settings->lootFood},{"lootIngredients",settings->lootIngredients},
            {"lootSoulGems",settings->lootSoulGems},{"lootMisc",settings->lootMisc},
            {"notificationMode",settings->notificationMode},
            {"lootDelay",settings->lootDelaySeconds},{"equipmentUpgradeChance",settings->equipmentUpgradeChance},
            {"menuHotkey",settings->menuHotkey},{"menuHotkeyModifier",settings->menuHotkeyModifier},
            {"menuHotkeyText",settings->menuHotkeyText}
        };
        state["followers"] = nlohmann::json::array();
        auto gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        for (const auto& [id, p] : FollowerManager::GetSingleton()->GetProfilesSnapshot()) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            if (!actor) continue;
            const auto invGold = gold ? ItemEvaluator::GetItemCount(actor, gold) : 0;
            const uint32_t totalGold = static_cast<uint32_t>(std::max(0, invGold));
            const uint32_t savedGold = p.preferences.savingsEnabled ? std::min(p.preferences.savedGold, totalGold) : 0;
            const uint32_t spendableGold = totalGold >= savedGold ? (totalGold - savedGold) : 0;
            nlohmann::json f = {
                {"id",id},{"name",p.name},{"animal",p.isAnimal},{"role",p.classBoundDisplayName},
                {"portraitRace",GetPortraitRaceKey(actor)},
                {"portraitFemale",actor->GetActorBase() && actor->GetActorBase()->GetSex() == RE::SEX::kFemale},
                {"hunger",p.needs.hunger},{"thirst",p.needs.thirst},{"fatigue",p.needs.fatigue},
                {"gold",totalGold},
                {"savedGold",savedGold},
                {"spendableGold",spendableGold},
                {"carry",actor->GetWeightInContainer()},{"capacity",actor->GetTotalCarryWeight()}
            };
            AddPreferences(f["prefs"], p.preferences);
            const auto stock = ShoppingAI::GetSupplyCounts(actor);
            f["stock"] = {
                {"healthPotions",stock.health},{"magickaPotions",stock.magicka},{"staminaPotions",stock.stamina},
                {"poisons",stock.poison},{"foodLarge",stock.foodL},{"foodMedium",stock.foodM},{"foodSmall",stock.foodS},
                {"drinks",stock.drinkN},{"alcohol",stock.drinkA},{"arrows",stock.arrows},{"bolts",stock.bolts},
                {"soulGems",stock.soulGems},{"pettySoulGems",stock.pettySoulGems},{"lesserSoulGems",stock.lesserSoulGems},
                {"commonSoulGems",stock.commonSoulGems},{"greaterSoulGems",stock.greaterSoulGems},
                {"pettySoulGemsEmpty",stock.pettySoulGemsEmpty},{"pettySoulGemsFull",stock.pettySoulGemsFull},
                {"lesserSoulGemsEmpty",stock.lesserSoulGemsEmpty},{"lesserSoulGemsFull",stock.lesserSoulGemsFull},
                {"commonSoulGemsEmpty",stock.commonSoulGemsEmpty},{"commonSoulGemsFull",stock.commonSoulGemsFull},
                {"greaterSoulGemsEmpty",stock.greaterSoulGemsEmpty},{"greaterSoulGemsFull",stock.greaterSoulGemsFull},
                {"grandSoulGemsEmpty",stock.grandSoulGemsEmpty},{"grandSoulGemsFull",stock.grandSoulGemsFull},
                {"blackSoulGemsEmpty",stock.blackSoulGemsEmpty},{"blackSoulGemsFull",stock.blackSoulGemsFull},
                {"poisonHealth",stock.poisonHealth},{"poisonMagicka",stock.poisonMagicka},
                {"poisonStamina",stock.poisonStamina},{"poisonOther",stock.poisonOther},
                {"invisibilityPotions",stock.invisibilityPotions},
                {"healthMinor",stock.healthMinor},{"healthStandard",stock.healthStandard},{"healthPlentiful",stock.healthPlentiful},
                {"healthVigorous",stock.healthVigorous},{"healthExtreme",stock.healthExtreme},{"healthUltimate",stock.healthUltimate},
                {"magickaMinor",stock.magickaMinor},{"magickaStandard",stock.magickaStandard},{"magickaPlentiful",stock.magickaPlentiful},
                {"magickaVigorous",stock.magickaVigorous},{"magickaExtreme",stock.magickaExtreme},{"magickaUltimate",stock.magickaUltimate},
                {"staminaMinor",stock.staminaMinor},{"staminaStandard",stock.staminaStandard},{"staminaPlentiful",stock.staminaPlentiful},
                {"staminaVigorous",stock.staminaVigorous},{"staminaExtreme",stock.staminaExtreme},{"staminaUltimate",stock.staminaUltimate},
                {"grandSoulGems",stock.grandSoulGems},{"blackSoulGems",stock.blackSoulGems},
                {"blueMountainFlowers",stock.blueMountainFlowers},{"wheat",stock.wheat},
                {"redMountainFlowers",stock.redMountainFlowers},{"whiteCaps",stock.whiteCaps},
                {"purpleMountainFlowers",stock.purpleMountainFlowers},{"pineThrushEggs",stock.pineThrushEggs},
                {"torches",stock.torches},{"lanternOil",stock.lanternOil},{"soap",stock.soap},
                {"treatmentPotions",stock.treatmentPotions},{"curePoisonPotions",stock.curePoisonPotions}
            };
            f["weapons"] = nlohmann::json::array();
            f["apparel"] = nlohmann::json::array();
            f["assignedPrimaryRight"] = p.assignedPrimaryRight;
            f["assignedPrimaryLeft"] = p.assignedPrimaryLeft;
            f["assignedSecondaryRight"] = p.assignedSecondaryRight;
            f["assignedSecondaryLeft"] = p.assignedSecondaryLeft;
            f["assignedHead"] = p.assignedHead;
            f["assignedBody"] = p.assignedBody;
            f["assignedHands"] = p.assignedHands;
            f["assignedFeet"] = p.assignedFeet;
            auto* eqRight = actor->GetEquippedObject(false);
            auto* eqLeft = actor->GetEquippedObject(true);
            f["equippedRight"] = eqRight && eqRight->GetName() ? eqRight->GetName() : "Boş";
            f["equippedLeft"] = eqLeft && eqLeft->GetName() ? eqLeft->GetName() : "Boş";

            for (const auto& [item, inv] : actor->GetInventory()) {
                if (!item || inv.first <= 0) continue;
                if (auto* weapon = item->As<RE::TESObjectWEAP>()) {
                    bool worn = false;
                    if (eqRight && eqRight->GetFormID() == weapon->GetFormID()) worn = true;
                    if (eqLeft && eqLeft->GetFormID() == weapon->GetFormID()) worn = true;
                    uint32_t wType = 1;
                    switch (weapon->GetWeaponType()) {
                    case RE::WEAPON_TYPE::kOneHandSword: wType = 1; break;
                    case RE::WEAPON_TYPE::kOneHandDagger: wType = 2; break;
                    case RE::WEAPON_TYPE::kOneHandAxe: wType = 3; break;
                    case RE::WEAPON_TYPE::kOneHandMace: wType = 4; break;
                    case RE::WEAPON_TYPE::kTwoHandSword: wType = 5; break;
                    case RE::WEAPON_TYPE::kTwoHandAxe: wType = 6; break;
                    case RE::WEAPON_TYPE::kBow: wType = 8; break;
                    case RE::WEAPON_TYPE::kCrossbow: wType = 9; break;
                    case RE::WEAPON_TYPE::kStaff: wType = 10; break;
                    default: wType = 1; break;
                    }
                    const bool is2H = (wType >= 5 && wType <= 9);
                    f["weapons"].push_back({{"id",weapon->GetFormID()},{"name",weapon->GetName() ? weapon->GetName() : "Weapon"},
                        {"slot",ItemEvaluator::GetWeaponTypeLabel(weapon)},
                        {"weaponType",wType},{"isTwoHanded",is2H},
                        {"damage",weapon->GetAttackDamage()},{"count",inv.first},{"worn",worn},{"protected",p.IsProtected(weapon->GetFormID())}});
                } else if (auto* armor = item->As<RE::TESObjectARMO>(); armor && !IsTngHiddenBodyItem(armor) &&
                    (armor->GetFormFlags() & RE::TESObjectARMO::RecordFlags::kNonPlayable) == 0) {
                    bool worn = false;
                    if (inv.second && inv.second->extraLists) for (auto* list : *inv.second->extraLists) {
                        if (list && (list->HasType(RE::ExtraDataType::kWorn) || list->HasType(RE::ExtraDataType::kWornLeft))) { worn = true; break; }
                    }
                    using ArmorType = RE::BGSBipedObjectForm::ArmorType;
                    const uint32_t armType = (armor->GetArmorType() == ArmorType::kHeavyArmor) ? 1 :
                                             ((armor->GetArmorType() == ArmorType::kLightArmor) ? 2 : 3);
                    const bool isShield = (static_cast<uint32_t>(armor->GetSlotMask()) & static_cast<uint32_t>(RE::BIPED_MODEL::BipedObjectSlot::kShield)) != 0;
                    f["apparel"].push_back({{"id",armor->GetFormID()},{"name",armor->GetName() ? armor->GetName() : "Apparel"},
                        {"slot",ItemEvaluator::GetArmorSlotLabel(armor)},
                        {"armorType",armType},{"isShield",isShield},
                        {"armor",armor->GetArmorRating()},{"count",inv.first},{"worn",worn},{"protected",p.IsProtected(armor->GetFormID())}});
                }
            }
            state["followers"].push_back(std::move(f));
        }
        const auto script = "window.fuSetState && window.fuSetState(" + state.dump() + ");";
        _api->Invoke(_view, script.c_str());
    }
}
