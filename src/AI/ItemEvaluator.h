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

    private:
        ItemEvaluator() = default;
    };
}
