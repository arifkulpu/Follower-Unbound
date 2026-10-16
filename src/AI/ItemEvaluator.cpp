#include "AI/ItemEvaluator.h"

namespace FollowerUnbound
{
    bool ItemEvaluator::IsHealthPotion(RE::TESBoundObject* item)
    {
        auto alchemy = item ? item->As<RE::AlchemyItem>() : nullptr;
        if (!alchemy || alchemy->IsPoison()) return false;
        // If it's a food/drink item, make sure it's not a pure edible/drinkable food
        if (alchemy->IsFood() && !IsDrink(item)) return false;

        for (auto effect : alchemy->effects) {
            if (effect && effect->baseEffect) {
                if (effect->baseEffect->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kPeakValueModifier ||
                    effect->baseEffect->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kValueModifier) {
                    if (effect->baseEffect->data.primaryAV == RE::ActorValue::kHealth) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool ItemEvaluator::IsMagickaPotion(RE::TESBoundObject* item)
    {
        auto alchemy = item ? item->As<RE::AlchemyItem>() : nullptr;
        if (!alchemy || alchemy->IsPoison()) return false;
        if (alchemy->IsFood() && !IsDrink(item)) return false;

        for (auto effect : alchemy->effects) {
            if (effect && effect->baseEffect) {
                if (effect->baseEffect->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kPeakValueModifier ||
                    effect->baseEffect->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kValueModifier) {
                    if (effect->baseEffect->data.primaryAV == RE::ActorValue::kMagicka) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool ItemEvaluator::IsStaminaPotion(RE::TESBoundObject* item)
    {
        auto alchemy = item ? item->As<RE::AlchemyItem>() : nullptr;
        if (!alchemy || alchemy->IsPoison()) return false;
        if (alchemy->IsFood() && !IsDrink(item)) return false;

        for (auto effect : alchemy->effects) {
            if (effect && effect->baseEffect) {
                if (effect->baseEffect->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kPeakValueModifier ||
                    effect->baseEffect->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kValueModifier) {
                    if (effect->baseEffect->data.primaryAV == RE::ActorValue::kStamina) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool ItemEvaluator::IsPoison(RE::TESBoundObject* item)
    {
        auto alchemy = item ? item->As<RE::AlchemyItem>() : nullptr;
        return alchemy && alchemy->IsPoison();
    }

    ItemEvaluator::PoisonType ItemEvaluator::GetPoisonType(RE::TESBoundObject* item)
    {
        auto* alchemy = item ? item->As<RE::AlchemyItem>() : nullptr;
        if (!alchemy || !alchemy->IsPoison()) return PoisonType::Other;
        for (auto* effect : alchemy->effects) {
            if (!effect || !effect->baseEffect) continue;
            switch (effect->baseEffect->data.primaryAV) {
            case RE::ActorValue::kHealth: return PoisonType::Health;
            case RE::ActorValue::kMagicka: return PoisonType::Magicka;
            case RE::ActorValue::kStamina: return PoisonType::Stamina;
            default: break;
            }
        }
        return PoisonType::Other;
    }

    bool ItemEvaluator::IsInvisibilityPotion(RE::TESBoundObject* item)
    {
        auto* alchemy = item ? item->As<RE::AlchemyItem>() : nullptr;
        if (!alchemy || alchemy->IsPoison()) return false;
        for (auto* effect : alchemy->effects) {
            if (effect && effect->baseEffect &&
                effect->baseEffect->GetArchetype() == RE::EffectArchetypes::ArchetypeID::kInvisibility) return true;
        }
        return false;
    }

    // Takipçinin envanterindeki TÜM soul gem sayısı (dolu/boş fark etmez)
    uint32_t ItemEvaluator::CountSoulGems(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && item && item->As<RE::TESSoulGem>()) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    bool ItemEvaluator::IsRawMeat(RE::TESBoundObject* item)
    {
        if (!item) return false;

        // Vanilla Skyrim çiğ et FormID'leri (pişirilmemiş)
        static const std::unordered_set<RE::FormID> kRawMeatIDs = {
            0x00023D96u, // Raw Beef
            0x00023D97u, // Raw Rabbit Leg
            0x0005AF6Eu, // Raw Pheasant
            0x0005AF6Fu, // Raw Venison
            0x00065C88u, // Horse Meat
            0x000CC844u, // Dog Meat
            0x000B08B0u, // Raw Horker Meat
            0x0006AC0Du, // Raw Goat Leg
        };

        if (item->GetFormID() < 0x01000000u && kRawMeatIDs.contains(item->GetFormID())) return true;

        // İsim tabanlı kontrol — "Raw " ile başlayan her şey çiğ ettir
        std::string name = item->GetName();
        if (name.empty()) return false;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.rfind("raw ", 0) == 0) return true;
        if (name.find(" raw") != std::string::npos) return true;
        return false;
    }

    bool ItemEvaluator::IsSoupOrStew(RE::TESBoundObject* item)
    {
        if (!IsFood(item)) return false;
        std::string name = item->GetName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        return name.find("soup") != std::string::npos ||
               name.find("stew") != std::string::npos ||
               name.find("broth") != std::string::npos ||
               name.find("porridge") != std::string::npos ||
               name.find("gruel") != std::string::npos ||
               name.find("chowder") != std::string::npos ||
               name.find("bisque") != std::string::npos;
    }

    bool ItemEvaluator::IsFood(RE::TESBoundObject* item)
    {
        auto alchemy = item ? item->As<RE::AlchemyItem>() : nullptr;
        return alchemy && (alchemy->IsFood() || IsDrink(item));
    }

    bool ItemEvaluator::IsDrink(RE::TESBoundObject* item)
    {
        if (!item) return false;

        // FormID / Mod check for SunHelm drinks and waterskins
        // SunHelm FormIDs:
        // _SHWaterBottleMead: 0xXX07AA96
        // _SHWaterBottleWine: 0xXX070897
        // _SHSaltBottleWine:  0xXX265BE3
        // _SHWaterskin_1:     0xXX4DE9AE
        // _SHWaterskin_2:     0xXX4DE9AF
        // _SHWaterskin_3:     0xXX4DE9B0
        RE::FormID formID = item->GetFormID();
        uint32_t localID = formID & 0x00FFFFFF;
        if (localID == 0x07AA96 || localID == 0x070897 || localID == 0x265BE3 ||
            localID == 0x4DE9AE || localID == 0x4DE9AF || localID == 0x4DE9B0) {
            return true;
        }

        // Must be an AlchemyItem (potion/food/drink)
        auto alchemy = item->As<RE::AlchemyItem>();
        if (!alchemy || alchemy->IsPoison()) return false;

        // Check EditorID if available
        const char* editorID = item->GetFormEditorID();
        if (editorID && editorID[0] != '\0') {
            std::string edid = editorID;
            std::transform(edid.begin(), edid.end(), edid.begin(), ::tolower);
            if (edid.find("_shwater") != std::string::npos ||
                edid.find("waterskin") != std::string::npos ||
                edid.find("drink") != std::string::npos ||
                edid.find("bottle") != std::string::npos) {
                return true;
            }
        }

        // Check Display Name
        std::string name = item->GetName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        // SunHelm & modded waterskins, bottles, drinks
        if (name.find("waterskin") != std::string::npos ||
            name.find("water skin") != std::string::npos ||
            name.find("water bottle") != std::string::npos ||
            name.find("matara") != std::string::npos) {
            return true;
        }

        // General drink keywords (for both food-flagged and custom drinks)
        return name.find("ale") != std::string::npos ||
               name.find("mead") != std::string::npos ||
               name.find("wine") != std::string::npos ||
               name.find("water") != std::string::npos ||
               name.find("milk") != std::string::npos ||
               name.find("brandy") != std::string::npos ||
               name.find("juice") != std::string::npos ||
               name.find("tea") != std::string::npos ||
               name.find("cider") != std::string::npos ||
               name.find("beer") != std::string::npos ||
               name.find("su") != std::string::npos;
    }

    bool ItemEvaluator::IsAlcohol(RE::TESBoundObject* item)
    {
        if (!IsDrink(item)) return false;

        // SunHelm water bottles made from wine/mead bottles are pure water, not alcohol
        const char* editorID = item->GetFormEditorID();
        if (editorID && editorID[0] != '\0') {
            std::string edid = editorID;
            std::transform(edid.begin(), edid.end(), edid.begin(), ::tolower);
            if (edid.find("_shwater") != std::string::npos ||
                edid.find("waterskin") != std::string::npos) {
                return false;
            }
        }

        std::string name = item->GetName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        // Water in wine/mead bottles should not be alcohol
        if (name.find("water") != std::string::npos || name.find("waterskin") != std::string::npos) {
            return false;
        }

        return name.find("ale") != std::string::npos ||
               name.find("mead") != std::string::npos ||
               name.find("wine") != std::string::npos ||
               name.find("brandy") != std::string::npos ||
               name.find("beer") != std::string::npos ||
               name.find("rum") != std::string::npos ||
               name.find("vodka") != std::string::npos ||
               name.find("liquor") != std::string::npos ||
               name.find("şarap") != std::string::npos ||
               name.find("bira") != std::string::npos;
    }

    bool ItemEvaluator::IsNormalDrink(RE::TESBoundObject* item)
    {
        return IsDrink(item) && !IsAlcohol(item);
    }

    bool ItemEvaluator::IsFoodLarge(RE::TESBoundObject* item)
    {
        if (!IsFood(item) || IsDrink(item)) return false;
        std::string name = item->GetName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        return name.find("meat") != std::string::npos ||
               name.find("beef") != std::string::npos ||
               name.find("venison") != std::string::npos ||
               name.find("steak") != std::string::npos ||
               name.find("chop") != std::string::npos ||
               name.find("leg") != std::string::npos ||
               name.find("chicken") != std::string::npos ||
               name.find("pheasant") != std::string::npos ||
               name.find("horker") != std::string::npos ||
               name.find("mammoth") != std::string::npos ||
               name.find("salmon") != std::string::npos ||
               name.find("fish") != std::string::npos ||
               name.find("pork") != std::string::npos ||
               name.find("rabbit") != std::string::npos ||
               name.find("goat") != std::string::npos ||
               name.find("roast") != std::string::npos;
    }

    bool ItemEvaluator::IsFoodSmall(RE::TESBoundObject* item)
    {
        if (!IsFood(item) || IsDrink(item)) return false;
        std::string name = item->GetName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        return name.find("apple") != std::string::npos ||
               name.find("carrot") != std::string::npos ||
               name.find("cabbage") != std::string::npos ||
               name.find("potato") != std::string::npos ||
               name.find("leek") != std::string::npos ||
               name.find("tomato") != std::string::npos ||
               name.find("berry") != std::string::npos ||
               name.find("berries") != std::string::npos ||
               name.find("gourd") != std::string::npos ||
               name.find("mushroom") != std::string::npos ||
               name.find("nut") != std::string::npos ||
               name.find("garlic") != std::string::npos;
    }

    bool ItemEvaluator::IsFoodMedium(RE::TESBoundObject* item)
    {
        if (!IsFood(item) || IsDrink(item)) return false;
        // Large ve Small kategorisine girmeyen yiyecekler Medium'dur (ekmek, peynir, çorba, tatlı vb.)
        if (IsFoodLarge(item) || IsFoodSmall(item)) return false;
        return true;
    }

    bool ItemEvaluator::IsArrow(RE::TESBoundObject* item)
    {
        auto ammo = item ? item->As<RE::TESAmmo>() : nullptr;
        return ammo && !ammo->IsBolt();
    }

    bool ItemEvaluator::IsBolt(RE::TESBoundObject* item)
    {
        auto ammo = item ? item->As<RE::TESAmmo>() : nullptr;
        return ammo && ammo->IsBolt();
    }

    uint32_t ItemEvaluator::CountHealthPotions(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && IsHealthPotion(item)) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    uint32_t ItemEvaluator::CountMagickaPotions(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && IsMagickaPotion(item)) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    uint32_t ItemEvaluator::CountStaminaPotions(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && IsStaminaPotion(item)) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    uint32_t ItemEvaluator::CountFoodLarge(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && IsFoodLarge(item)) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    uint32_t ItemEvaluator::CountFoodMedium(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && IsFoodMedium(item)) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    uint32_t ItemEvaluator::CountFoodSmall(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && IsFoodSmall(item)) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    uint32_t ItemEvaluator::CountDrinkNormal(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && IsNormalDrink(item)) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    uint32_t ItemEvaluator::CountDrinkAlcohol(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && IsAlcohol(item)) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    uint32_t ItemEvaluator::CountArrows(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && IsArrow(item)) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    uint32_t ItemEvaluator::CountBolts(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && IsBolt(item)) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    uint32_t ItemEvaluator::CountPoisons(RE::Actor* actor)
    {
        if (!actor) return 0;
        uint32_t total = 0;
        for (const auto& [item, invData] : actor->GetInventory()) {
            if (invData.first > 0 && IsPoison(item)) {
                total += static_cast<uint32_t>(invData.first);
            }
        }
        return total;
    }

    bool ItemEvaluator::IsItemUsableByRole(FollowerRole role, RE::TESBoundObject* item)
    {
        if (!item) return false;

        // Weapons
        if (auto weapon = item->As<RE::TESObjectWEAP>()) {
            switch (role) {
            case FollowerRole::Mage:
            case FollowerRole::Necromancer:
                // Mages ONLY use staves or backup daggers. NO bows, crossbows, swords, axes, maces, greatswords!
                return weapon->IsStaff() || weapon->IsOneHandedDagger();

            case FollowerRole::Assassin:
                // Assassins use daggers, 1H swords, and bows. NO two-handed weapons!
                return weapon->IsOneHandedDagger() || weapon->IsOneHandedSword() || weapon->IsBow();

            case FollowerRole::Thief:
                // Thieves use daggers, 1H swords, and bows. NO two-handed weapons!
                return weapon->IsOneHandedDagger() || weapon->IsOneHandedSword() || weapon->IsBow();

            case FollowerRole::Archer:
                // Archers use bows, daggers, 1H swords. NO two-handed warhammers/battleaxes!
                return weapon->IsBow() || weapon->IsOneHandedDagger() || weapon->IsOneHandedSword();

            case FollowerRole::Crossbowman:
                // Crossbowmen use crossbows, daggers, 1H swords.
                return weapon->IsCrossbow() || weapon->IsBow() || weapon->IsOneHandedDagger() || weapon->IsOneHandedSword();

            case FollowerRole::Warrior:
            case FollowerRole::Knight:
            case FollowerRole::Paladin:
                // Melee classes use melee weapons. Staves are not appropriate.
                return !weapon->IsStaff();

            case FollowerRole::Bard:
                // Bards use 1H swords, daggers, or staves. NO heavy 2H warhammers/battleaxes!
                return weapon->IsOneHandedSword() || weapon->IsOneHandedDagger() || weapon->IsStaff();

            case FollowerRole::Alchemist:
                return weapon->IsOneHandedDagger() || weapon->IsOneHandedSword() || weapon->IsBow();

            default:
                return true;
            }
        }

        // Armor
        if (auto armor = item->As<RE::TESObjectARMO>()) {
            bool isHeavy = armor->IsHeavyArmor();
            bool isLight = armor->IsLightArmor();
            bool isShield = armor->IsShield();

            switch (role) {
            case FollowerRole::Mage:
            case FollowerRole::Necromancer:
                // Pure cloth only! NO heavy, NO light armor, NO shields!
                return !isHeavy && !isLight && !isShield;

            case FollowerRole::Assassin:
            case FollowerRole::Thief:
                // Light armor or cloth only! NO heavy armor, NO shields!
                return !isHeavy && !isShield;

            case FollowerRole::Archer:
            case FollowerRole::Crossbowman:
                // Light armor or cloth only! NO heavy armor, NO shields!
                return !isHeavy && !isShield;

            case FollowerRole::Bard:
                // Light armor or cloth only! NO heavy armor.
                return !isHeavy;

            case FollowerRole::Warrior:
            case FollowerRole::Knight:
            case FollowerRole::Paladin:
                // Heavy or Light armor and shields.
                return true;

            default:
                return true;
            }
        }

        return true;
    }

    float ItemEvaluator::EvaluateItem(RE::TESBoundObject* item, const FollowerProfile& profile, RE::Actor* actor)
    {
        if (!item || !actor) return 0.0f;

        // SINIFSIZ POLITIKA: Rol/sınıf kapısı YOKTUR. (Eski kodda rol-uymayan
        // eşya 0.0f dönüyordu; bu, sınıf-temelli yağmanın kalıntısıydı ve oylarla
        // kaldırıldı — loot dağıtımı ceset başına atanmış tek takipçiye yapılır. Bu fonksiyon şu an
        // dağıtımda kullanılmıyor; ileride "akıllı skor" opsiyonu için saklanıyor.)

        // ── GLOBAL FILTER: non-playable items (mod body parts, TNG GenitCover, Head, Body00, Hands, etc.) ──
        // Items with no display name or that are not playable are internal records and should never be looted.
        const char* itemName = item->GetName();
        if (!itemName || itemName[0] == '\0') return 0.0f;

        // Filter ARMO items flagged as non-playable (catches TNG/SAM+ body addon armors)
        if (auto armoCheck = item->As<RE::TESObjectARMO>()) {
            // Non-playable flag check via formFlags
            if ((armoCheck->GetFormFlags() & RE::TESObjectARMO::RecordFlags::kNonPlayable) != 0) return 0.0f;
            // Extra safety: zero-value armor with no armor rating and no weight is almost certainly a body addon
            if (armoCheck->GetArmorRating() == 0 && armoCheck->GetWeight() == 0.0f &&
                armoCheck->GetGoldValue() <= 0) {
                return 0.0f;
            }
        }

        // Check if item is gold
        if (item->GetFormID() == 0x0000000F) {
            return profile.preferences.allowLootGold ? 1000.0f : 0.0f;
        }

        // King Core BehaviorProfile entegrasyonu
        KingCore::BehaviorProfile bp{};
        bool hasBP = ClassInterface::GetSingleton()->GetBehaviorProfile(actor, bp);

        float score = 0.0f;

        // ── AMMO ──
        if (auto ammo = item->As<RE::TESAmmo>()) {
            if (!profile.preferences.allowLootArrows) return 0.0f;
            uint32_t limit = ammo->IsBolt() ? profile.preferences.maxBolts
                                             : profile.preferences.maxArrows;
            if (hasBP && bp.targetAmmoCount > 0) limit = bp.targetAmmoCount;

            if (limit > 0) {
                int32_t currentAmmo = GetItemCount(actor, ammo);
                if (static_cast<uint32_t>(currentAmmo) < limit) {
                    float weightMultiplier = hasBP ? (static_cast<float>(bp.ammoWeight) / 50.0f) : 1.0f;
                    score = (50.0f + ammo->data.damage * 2.0f) * weightMultiplier;
                }
            }
            return score;
        }

        // ── POTIONS ──
        if (!profile.preferences.allowLootPotions) goto weaponCheck;

        if (IsHealthPotion(item)) {
            uint32_t targetHp = hasBP ? bp.targetHealthPotions : profile.preferences.maxHealthPotions;
            int32_t count = GetItemCount(actor, item);
            if (static_cast<uint32_t>(count) < targetHp) {
                float weightMultiplier = hasBP ? (static_cast<float>(bp.healthPotionWeight) / 50.0f) : 1.0f;
                return 80.0f * weightMultiplier;
            }
        }
        if (IsMagickaPotion(item)) {
            uint32_t targetMp = hasBP ? bp.targetMagickaPotions : profile.preferences.maxMagickaPotions;
            if (targetMp > 0) {
                int32_t count = GetItemCount(actor, item);
                if (static_cast<uint32_t>(count) < targetMp) {
                    float weightMultiplier = hasBP ? (static_cast<float>(bp.magickaPotionWeight) / 50.0f) : 1.0f;
                    return 80.0f * weightMultiplier;
                }
            }
        }
        if (IsStaminaPotion(item)) {
            uint32_t targetSp = hasBP ? bp.targetStaminaPotions : profile.preferences.maxStaminaPotions;
            int32_t count = GetItemCount(actor, item);
            if (static_cast<uint32_t>(count) < targetSp) {
                float weightMultiplier = hasBP ? (static_cast<float>(bp.staminaPotionWeight) / 50.0f) : 1.0f;
                return 75.0f * weightMultiplier;
            }
        }
        if (IsPoison(item)) {
            int32_t count = GetItemCount(actor, item);
            if (static_cast<uint32_t>(count) < profile.preferences.maxPoisons) {
                // Gizli/simyacı sınıflar: yüksek öncelik
                if ((hasBP && bp.prefersStealth) || profile.role == FollowerRole::Assassin ||
                    profile.role == FollowerRole::Thief || profile.role == FollowerRole::Alchemist) {
                    float weightMultiplier = hasBP ? (static_cast<float>(bp.poisonWeight) / 50.0f) : 1.0f;
                    return 70.0f * weightMultiplier;
                }
                // Diğer tüm takipçiler: düşük öncelik ama yine de alınır
                return 35.0f;
            }
        }

        // ── FOOD & DRINK (reserve looting — always loot up to limits) ──
        if (profile.preferences.allowLootFood) {
            // Çiğ/pişirilmemiş et asla alınmaz
            if (IsRawMeat(item)) return 0.0f;

            if (IsDrink(item) && !IsAlcohol(item)) {
                uint32_t current = CountDrinkNormal(actor);
                if (current < profile.preferences.maxDrinkNormal) return 55.0f;
            } else if (IsAlcohol(item)) {
                if (profile.preferences.buyDrinkAlcohol) {
                    uint32_t current = CountDrinkAlcohol(actor);
                    if (current < profile.preferences.maxDrinkAlcohol) return 30.0f;
                }
            } else if (IsFood(item)) {
                if (IsFoodLarge(item)) {
                    uint32_t current = CountFoodLarge(actor);
                    if (current < profile.preferences.maxFoodLarge) return 60.0f;
                } else if (IsFoodMedium(item)) {
                    uint32_t current = CountFoodMedium(actor);
                    if (current < profile.preferences.maxFoodMedium) return 55.0f;
                } else if (IsFoodSmall(item)) {
                    uint32_t current = CountFoodSmall(actor);
                    if (current < profile.preferences.maxFoodSmall) return 50.0f;
                } else {
                    // unknown food — low-priority loot
                    return 40.0f;
                }
            }
        }

weaponCheck:
        // ── WEAPONS ──
        if (auto weapon = item->As<RE::TESObjectWEAP>()) {
            if (!profile.preferences.allowLootWeapons) return 0.0f;

            float damage = static_cast<float>(weapon->GetAttackDamage());
            float weaponWeightMultiplier = hasBP ? (static_cast<float>(bp.weaponWeight) / 50.0f) : 1.0f;

            if (profile.role == FollowerRole::Archer && weapon->IsBow()) {
                score = (100.0f + damage * 3.0f) * weaponWeightMultiplier;
            } else if (profile.role == FollowerRole::Crossbowman && weapon->IsCrossbow()) {
                score = (100.0f + damage * 3.0f) * weaponWeightMultiplier;
            } else if (profile.role == FollowerRole::Assassin && weapon->IsOneHandedDagger()) {
                score = (100.0f + damage * 3.0f) * weaponWeightMultiplier;
            } else if ((profile.role == FollowerRole::Warrior || profile.role == FollowerRole::Knight) &&
                       (weapon->IsOneHandedSword() || weapon->IsTwoHandedSword() || weapon->IsOneHandedAxe() ||
                        weapon->IsTwoHandedAxe() || weapon->IsOneHandedMace())) {
                score = (100.0f + damage * 2.5f) * weaponWeightMultiplier;
            } else if (profile.role == FollowerRole::Mage && weapon->IsStaff()) {
                score = 100.0f * weaponWeightMultiplier;
            } else {
                // Role-agnostic fallback: any decent weapon is worth picking up (lower priority)
                score = (30.0f + damage * 1.0f) * weaponWeightMultiplier;
            }
            // Büyülü silahlar önemli bonus alır
            if (score > 0.0f && weapon->formEnchanting) {
                float enchBonus = 40.0f;
                if (weapon->amountofEnchantment > 0) {
                    enchBonus += std::min(30.0f, static_cast<float>(weapon->amountofEnchantment) * 0.1f);
                }
                score += enchBonus;
            }
            return score;
        }

        // ── ARMOR ──
        if (auto armor = item->As<RE::TESObjectARMO>()) {
            if (!profile.preferences.allowLootArmor) return 0.0f;

            float armorRating = static_cast<float>(armor->GetArmorRating());
            bool isLight = armor->IsLightArmor();
            bool isHeavy = armor->IsHeavyArmor();
            float armorWeightMultiplier = hasBP ? (static_cast<float>(bp.armorWeight) / 50.0f) : 1.0f;

            if (hasBP && bp.preferredArmor != KingCore::ArmorPreference::Any) {
                if (bp.preferredArmor == KingCore::ArmorPreference::Heavy && isHeavy) {
                    score = (70.0f + armorRating * 1.5f) * armorWeightMultiplier;
                } else if (bp.preferredArmor == KingCore::ArmorPreference::Light && isLight) {
                    score = (70.0f + armorRating * 1.5f) * armorWeightMultiplier;
                } else if (bp.preferredArmor == KingCore::ArmorPreference::Cloth && !isLight && !isHeavy) {
                    score = 60.0f * armorWeightMultiplier;
                } else {
                    // Non-preferred armor type: low priority
                    score = (15.0f + armorRating * 0.5f) * armorWeightMultiplier;
                }
            } else {
                if (profile.role == FollowerRole::Archer || profile.role == FollowerRole::Assassin ||
                    profile.role == FollowerRole::Thief || profile.role == FollowerRole::Crossbowman ||
                    profile.role == FollowerRole::Druid) {
                    if (isLight) score = 70.0f + armorRating * 1.5f;
                    else score = (15.0f + armorRating * 0.5f) * armorWeightMultiplier;
                } else if (profile.role == FollowerRole::Warrior || profile.role == FollowerRole::Knight ||
                           profile.role == FollowerRole::Paladin) {
                    if (isHeavy) score = 70.0f + armorRating * 1.5f;
                    else score = (15.0f + armorRating * 0.5f) * armorWeightMultiplier;
                } else if (profile.role == FollowerRole::Mage || profile.role == FollowerRole::Necromancer ||
                           profile.role == FollowerRole::Bard) {
                    if (!isLight && !isHeavy) score = 65.0f; // Cübbe ve kıyafet tercihi
                    else score = 10.0f * armorWeightMultiplier;
                } else {
                    // Diğer sınıflar
                    score = (15.0f + armorRating * 0.5f) * armorWeightMultiplier;
                }
            }
            // Büyülü zırhlar önemli bonus alır
            if (score > 0.0f && armor->formEnchanting) {
                score += 40.0f;
            }
            return score;
        }

        // ── SOUL GEMS ──
        // Dolu/boş fark etmeksizin stok limitine kadar toplanır. SoulKeeper aktifken
        // bile toplama açık kalır; yalnızca SATIN ALMA SoulKeeper'a devredilir.
        if (item->As<RE::TESSoulGem>()) {
            if (CountSoulGems(actor) < profile.preferences.maxSoulGems) return 45.0f;
            return 0.0f;
        }

        // ── MISC ITEMS (lockpicks, torches, gems, keys, etc.) ──
        if (auto misc = item->As<RE::TESObjectMISC>()) {
            // Lockpicks (FormID 0x00000A)
            if (misc->GetFormID() == 0x0000000A) {
                int32_t currentLockpicks = GetItemCount(actor, misc);
                if (currentLockpicks < 30) return 45.0f;
                return 0.0f;
            }
            // Gems / valuables: gold value > 50, weight < 1
            float goldValue = static_cast<float>(misc->GetGoldValue());
            float weight = misc->GetWeight();
            if (goldValue > 50.0f && weight < 1.0f) {
                return 35.0f + goldValue * 0.05f;
            }
        }

        // ── LIGHTS (Torches) ──
        if (auto light = item->As<RE::TESObjectLIGH>()) {
            if (light->CanBeCarried()) {
                int32_t currentTorches = GetItemCount(actor, light);
                if (currentTorches < 5) return 25.0f;
            }
        }

        return score;
    }
}

