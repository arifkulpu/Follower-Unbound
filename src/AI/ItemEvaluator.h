#pragma once
#include "PCH.h"
#include "Core/FollowerProfile.h"

namespace FollowerUnbound
{
    class ItemEvaluator
    {
    public:
        enum class PoisonType { Health, Magicka, Stamina, Other };
        static ItemEvaluator* GetSingleton()
        {
            static ItemEvaluator instance;
            return &instance;
        }

        // Helper to get count of item in actor/refr inventory
        static int32_t GetItemCount(RE::TESObjectREFR* ref, RE::TESBoundObject* item)
        {
            if (!ref || !item) return 0;
            auto inv = ref->GetInventory();
            auto it = inv.find(item);
            return (it != inv.end()) ? it->second.first : 0;
        }

        // Returns score > 0 if desired, <= 0 if ignored
        float EvaluateItem(RE::TESBoundObject* item, const FollowerProfile& profile, RE::Actor* actor);

        // Class & Role based item compatibility
        bool IsItemUsableByRole(FollowerRole role, RE::TESBoundObject* item);

        bool IsHealthPotion(RE::TESBoundObject* item);
        bool IsMagickaPotion(RE::TESBoundObject* item);
        bool IsStaminaPotion(RE::TESBoundObject* item);
        bool IsPoison(RE::TESBoundObject* item);
        PoisonType GetPoisonType(RE::TESBoundObject* item);
        bool IsInvisibilityPotion(RE::TESBoundObject* item);

        // Food categorization
        bool IsFood(RE::TESBoundObject* item);
        bool IsFoodLarge(RE::TESBoundObject* item);
        bool IsFoodMedium(RE::TESBoundObject* item);
        bool IsFoodSmall(RE::TESBoundObject* item);

        // Drink categorization
        bool IsDrink(RE::TESBoundObject* item);
        bool IsAlcohol(RE::TESBoundObject* item);
        bool IsNormalDrink(RE::TESBoundObject* item);
        bool IsRawMeat(RE::TESBoundObject* item);
        bool IsSoupOrStew(RE::TESBoundObject* item);

        // Ammunition
        bool IsArrow(RE::TESBoundObject* item);
        bool IsBolt(RE::TESBoundObject* item);

        // Inventory category counters
        uint32_t CountHealthPotions(RE::Actor* actor);
        uint32_t CountMagickaPotions(RE::Actor* actor);
        uint32_t CountStaminaPotions(RE::Actor* actor);
        uint32_t CountFoodLarge(RE::Actor* actor);
        uint32_t CountFoodMedium(RE::Actor* actor);
        uint32_t CountFoodSmall(RE::Actor* actor);
        uint32_t CountDrinkNormal(RE::Actor* actor);
        uint32_t CountDrinkAlcohol(RE::Actor* actor);
        uint32_t CountArrows(RE::Actor* actor);
        uint32_t CountBolts(RE::Actor* actor);
        uint32_t CountPoisons(RE::Actor* actor);
        uint32_t CountSoulGems(RE::Actor* actor);

        static std::string GetWeaponTypeLabel(RE::TESObjectWEAP* weapon)
        {
            if (!weapon) return "Silah";
            switch (weapon->GetWeaponType()) {
            case RE::WEAPON_TYPE::kOneHandSword: return "Kılıç";
            case RE::WEAPON_TYPE::kOneHandDagger: return "Hançer";
            case RE::WEAPON_TYPE::kOneHandAxe: return "Savaş Baltası";
            case RE::WEAPON_TYPE::kOneHandMace: return "Gürz";
            case RE::WEAPON_TYPE::kTwoHandSword: return "Çift El Kılıç";
            case RE::WEAPON_TYPE::kTwoHandAxe:
                return weapon->HasKeywordString("WeapTypeWarhammer") ? "Savaş Çekici" : "Çift El Balta";
            case RE::WEAPON_TYPE::kBow: return "Yay";
            case RE::WEAPON_TYPE::kCrossbow: return "Arbalet";
            case RE::WEAPON_TYPE::kStaff: return "Asa";
            default: return "Silah";
            }
        }

        static std::string GetArmorSlotLabel(RE::TESObjectARMO* armor)
        {
            if (!armor) return "Zırh";
            using Slot = RE::BIPED_MODEL::BipedObjectSlot;
            const auto mask = static_cast<uint32_t>(armor->GetSlotMask());
            if (mask & static_cast<uint32_t>(Slot::kShield)) return "Kalkan";
            if (mask & (static_cast<uint32_t>(Slot::kHead) | static_cast<uint32_t>(Slot::kHair) | static_cast<uint32_t>(Slot::kCirclet))) return "Kask";
            if (mask & static_cast<uint32_t>(Slot::kBody)) return "Gövde";
            if (mask & static_cast<uint32_t>(Slot::kHands)) return "Eldiven";
            if (mask & static_cast<uint32_t>(Slot::kFeet)) return "Ayak";
            if (mask & static_cast<uint32_t>(Slot::kAmulet)) return "Takı";
            if (mask & static_cast<uint32_t>(Slot::kRing)) return "Takı";
            return "Kıyafet";
        }

        static std::string GetItemCategoryLabel(RE::TESBoundObject* item)
        {
            if (!item) return "";
            if (auto* w = item->As<RE::TESObjectWEAP>()) {
                return "Silah: " + GetWeaponTypeLabel(w);
            }
            if (auto* a = item->As<RE::TESObjectARMO>()) {
                const auto mask = static_cast<uint32_t>(a->GetSlotMask());
                if (mask & static_cast<uint32_t>(RE::BIPED_MODEL::BipedObjectSlot::kShield)) {
                    return "Kalkan";
                }
                return "Zırh: " + GetArmorSlotLabel(a);
            }
            return "";
        }

        static bool MatchesPreferredArmor(RE::TESObjectARMO* armor, uint32_t preferredType)
        {
            if (!armor || preferredType == 0) return true;
            using Slot = RE::BIPED_MODEL::BipedObjectSlot;
            const auto mask = static_cast<uint32_t>(armor->GetSlotMask());
            // Kalkan veya takılar kıyafet bütünlüğü kısıtlamasına dahil değildir
            if (mask & static_cast<uint32_t>(Slot::kShield) ||
                mask & static_cast<uint32_t>(Slot::kAmulet) ||
                mask & static_cast<uint32_t>(Slot::kRing)) {
                return true;
            }
            // Sadece 4 ana biped slotu (Kafa, Govde, Eller, Ayaklar) kontrol edilir; fener/aksesuar/eklenti slotlari serbesttir
            constexpr uint32_t kMainArmorSlots = static_cast<uint32_t>(Slot::kHead) |
                                                 static_cast<uint32_t>(Slot::kBody) |
                                                 static_cast<uint32_t>(Slot::kHands) |
                                                 static_cast<uint32_t>(Slot::kFeet);
            if ((mask & kMainArmorSlots) == 0) {
                return true;
            }

            using ArmorType = RE::BGSBipedObjectForm::ArmorType;
            const auto armorType = armor->GetArmorType();
            if (preferredType == 1) return armorType == ArmorType::kHeavyArmor;
            if (preferredType == 2) return armorType == ArmorType::kLightArmor;
            if (preferredType == 3) return armorType == ArmorType::kClothing || armor->GetArmorRating() == 0;
            return true;
        }

        static bool MatchesWeaponCategory(RE::TESObjectWEAP* weapon, uint32_t subType)
        {
            if (!weapon || subType == 0) return true;
            const auto t = weapon->GetWeaponType();
            // 1=Kılıç, 2=Hançer, 3=Balta, 4=Gürz, 5=Asa
            if (subType == 1) return t == RE::WEAPON_TYPE::kOneHandSword;
            if (subType == 2) return t == RE::WEAPON_TYPE::kOneHandDagger;
            if (subType == 3) return t == RE::WEAPON_TYPE::kOneHandAxe;
            if (subType == 4) return t == RE::WEAPON_TYPE::kOneHandMace;
            if (subType == 5) return t == RE::WEAPON_TYPE::kStaff;
            return true;
        }

        // ── Silah stili tercihleri (Silah sekmesi) ──
        // Stil: 0=Serbest, 1=Tek Elli, 2=Çift Elli, 3=Yay, 4=Arbalet, 5=Asa
        // Sağ el: 0=Herhangi, 1=Kılıç, 2=Hançer, 3=Balta, 4=Gürz, 5=Asa
        // Sol el: 0=Herhangi, 1=Kalkan, 2=Kılıç, 3=Hançer, 4=Balta, 5=Gürz, 6=Asa, 7=Büyü/Boş
        // Çift el türü: 0=Herhangi, 1=Büyük Kılıç, 2=Büyük Balta, 3=Savaş Çekici
        static int GetOneHandKind(RE::TESObjectWEAP* weapon)
        {
            if (!weapon) return 0;
            switch (weapon->GetWeaponType()) {
            case RE::WEAPON_TYPE::kOneHandSword: return 1;
            case RE::WEAPON_TYPE::kOneHandDagger: return 2;
            case RE::WEAPON_TYPE::kOneHandAxe: return 3;
            case RE::WEAPON_TYPE::kOneHandMace: return 4;
            case RE::WEAPON_TYPE::kStaff: return 5;
            default: return 0;
            }
        }

        static bool MatchesWeaponStyleSlot(RE::TESObjectWEAP* weapon, uint32_t style,
            uint32_t rightHand, uint32_t leftHand, uint32_t twoHandType)
        {
            if (!weapon) return false;
            const auto t = weapon->GetWeaponType();
            switch (style) {
            case 1: {   // Tek elli
                const int kind = GetOneHandKind(weapon);
                if (kind < 1 || kind > 4) return false;
                return rightHand == 0 || static_cast<uint32_t>(kind) == rightHand ||
                       (leftHand >= 2 && leftHand <= 5 && static_cast<uint32_t>(kind) == leftHand - 1);
            }
            case 2: {   // Çift elli
                if (t != RE::WEAPON_TYPE::kTwoHandSword && t != RE::WEAPON_TYPE::kTwoHandAxe) return false;
                if (twoHandType == 0) return true;
                const bool hammer = weapon->HasKeywordString("WeapTypeWarhammer");
                if (twoHandType == 1) return t == RE::WEAPON_TYPE::kTwoHandSword;
                if (twoHandType == 2) return t == RE::WEAPON_TYPE::kTwoHandAxe && !hammer;
                if (twoHandType == 3) return t == RE::WEAPON_TYPE::kTwoHandAxe && hammer;
                return true;
            }
            case 3: return t == RE::WEAPON_TYPE::kBow;
            case 4: return t == RE::WEAPON_TYPE::kCrossbow;
            case 5: return t == RE::WEAPON_TYPE::kStaff;
            default: return true;
            }
        }

        static bool HasWeaponStylePreference(const FollowerPreferences& p)
        {
            return p.primaryWeaponStyle != 0 || p.secondaryWeaponStyle != 0;
        }

        // Silah, asli VEYA yardımcı stile uyuyor mu? Hiç stil seçilmediyse true.
        static bool MatchesWeaponPreferences(RE::TESObjectWEAP* weapon, const FollowerPreferences& p)
        {
            if (!HasWeaponStylePreference(p)) return true;
            if (p.primaryWeaponStyle != 0 &&
                MatchesWeaponStyleSlot(weapon, p.primaryWeaponStyle, p.primaryRightHand, p.primaryLeftHand, p.primaryTwoHandType)) return true;
            if (p.secondaryWeaponStyle != 0 &&
                MatchesWeaponStyleSlot(weapon, p.secondaryWeaponStyle, p.secondaryRightHand, p.secondaryLeftHand, p.secondaryTwoHandType)) return true;
            return false;
        }

    private:
        ItemEvaluator() = default;
    };
}
