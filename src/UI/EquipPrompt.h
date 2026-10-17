#pragma once
#include "PCH.h"
#include "Core/FollowerProfile.h"

namespace FollowerUnbound
{
    class EquipPrompt
    {
    public:
        // Candidate prompts are queued and shown through PrismaUI after loot
        // distribution completes; no game MessageBoxMenu is opened.
        static void ShowPrompt(
            RE::Actor* follower,
            FollowerProfile* profile,
            RE::TESBoundObject* newObj,
            RE::TESBoundObject* oldObj,
            std::string_view reason,
            std::string_view currentStats,
            std::string_view candidateStats);
        static bool ShowPurchasePrompt(
            RE::Actor* follower,
            RE::TESObjectREFR* stockContainer,
            RE::TESBoundObject* newObj,
            RE::TESBoundObject* oldObj,
            uint32_t price,
            std::string_view vendorName,
            std::string_view currentStats,
            std::string_view candidateStats);
        static bool IsPurchasePending(RE::FormID followerID, RE::FormID itemID);
        static uint32_t GetReservedGold(RE::FormID followerID);

        static void StartAfterLootDistribution();
        static void LootWorkStarted();
        static void Update();
        static bool HasActivePrompt();
        static bool HasPendingPrompts();
        static void Resolve(bool accept);
    };
}
