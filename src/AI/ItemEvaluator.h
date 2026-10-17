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
            case RE::WEAPON_TYPE::kTwoHandAxe: return "Çift El Balta";
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

    private:
        ItemEvaluator() = default;
    };
}
