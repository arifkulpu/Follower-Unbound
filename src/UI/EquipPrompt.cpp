#include "UI/EquipPrompt.h"
#include "Core/FollowerManager.h"
#include "UI/PrismaUIManager.h"
#include "UI/SKSEMenuFramework.h"
#include "AI/AnimationManager.h"

namespace FollowerUnbound
{
    namespace
    {
        struct PromptRequest
        {
            RE::FormID followerID{ 0 };
            RE::FormID newObjID{ 0 };
            RE::FormID oldObjID{ 0 };
            std::string followerName;
            std::string newObjName;
            std::string reason;
        };

        std::deque<PromptRequest> g_promptQueue;
        std::optional<PromptRequest> g_activePrompt;
        bool g_waitingForLootCompletion{ false };
        std::chrono::steady_clock::time_point g_showAt{};

        bool IsAnotherMenuOpen()
        {
            if (SKSEMenuFramework::IsInstalled() && SKSEMenuFramework::IsAnyBlockingWindowOpened()) return true;
            auto* ui = RE::UI::GetSingleton();
            if (!ui) return true;
            constexpr std::array<std::string_view, 19> blockingMenus{
                "Dialogue Menu", "ContainerMenu", "InventoryMenu", "MessageBoxMenu", "BarterMenu",
                "GiftMenu", "Journal Menu", "MapMenu", "MagicMenu", "FavoritesMenu", "TweenMenu",
                "Sleep/Wait Menu", "Loading Menu", "Crafting Menu", "Book Menu", "Lockpicking Menu",
                "Training Menu", "StatsMenu", "Console"
            };
            return std::ranges::any_of(blockingMenus, [ui](std::string_view name) { return ui->IsMenuOpen(name); });
        }

        void PresentNext()
        {
            if (g_activePrompt || g_promptQueue.empty()) return;
            if (IsAnotherMenuOpen()) {
                g_waitingForLootCompletion = true;
                g_showAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                return;
            }
            g_activePrompt = std::move(g_promptQueue.front());
            g_promptQueue.pop_front();
            const auto& prompt = *g_activePrompt;
            PrismaUIManager::GetSingleton()->ShowEquipPrompt(
                prompt.followerID, prompt.newObjID, prompt.oldObjID,
                prompt.followerName, prompt.newObjName, prompt.reason);
        }
    }

    void EquipPrompt::ShowPrompt(
        RE::Actor* follower,
        FollowerProfile*,
        RE::TESBoundObject* newObj,
        RE::TESBoundObject* oldObj,
        std::string_view reason)
    {
        if (!follower || !newObj) return;
        g_promptQueue.push_back({
            follower->GetFormID(), newObj->GetFormID(), oldObj ? oldObj->GetFormID() : 0,
            follower->GetName(), newObj->GetName(), std::string(reason) });
        logger::info("EquipPrompt: queued PrismaUI decision for {} / {}; waiting for loot distribution completion.",
            follower->GetName(), newObj->GetName());
    }

    void EquipPrompt::StartAfterLootDistribution()
    {
        if (g_promptQueue.empty() || g_activePrompt || g_waitingForLootCompletion) return;
        std::stable_sort(g_promptQueue.begin(), g_promptQueue.end(), [](const PromptRequest& a, const PromptRequest& b) {
            return a.followerID < b.followerID;
        });
        g_waitingForLootCompletion = true;
        g_showAt = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        logger::info("EquipPrompt: all loot transfers completed; first PrismaUI prompt in 3 seconds ({} queued).",
            g_promptQueue.size());
    }

    void EquipPrompt::LootWorkStarted()
    {
        // If another container is opened during the three-second grace period,
        // restart that delay after its distribution also finishes.
        g_waitingForLootCompletion = false;
    }

    void EquipPrompt::Update()
    {
        if (!g_waitingForLootCompletion || std::chrono::steady_clock::now() < g_showAt) return;
        g_waitingForLootCompletion = false;
        PresentNext();
    }

    bool EquipPrompt::HasActivePrompt()
    {
        return g_activePrompt.has_value();
    }

    bool EquipPrompt::HasPendingPrompts()
    {
        return g_activePrompt.has_value() || !g_promptQueue.empty() || g_waitingForLootCompletion;
    }

    void EquipPrompt::Resolve(bool accept)
    {
        if (!g_activePrompt) return;
        auto prompt = std::move(*g_activePrompt);
        g_activePrompt.reset();

        if (accept) {
            auto* follower = RE::TESForm::LookupByID<RE::Actor>(prompt.followerID);
            auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(prompt.newObjID);
            auto profile = FollowerManager::GetSingleton()->GetProfile(prompt.followerID);
            if (follower && item && profile) {
                if (prompt.oldObjID != 0 && profile->IsProtected(prompt.oldObjID)) {
                    logger::info("EquipPrompt: '{}' kept protected equipment; skipped PrismaUI equip decision.", prompt.followerName);
                } else {
                    if (prompt.oldObjID != 0) profile->ProtectItem(prompt.oldObjID, false);
                    profile->ProtectItem(prompt.newObjID, true);
                    if (auto* equip = RE::ActorEquipManager::GetSingleton()) {
                        equip->EquipObject(follower, item, nullptr, 1, nullptr, false, false, false, true);
                        if (item->IsWeapon()) AnimationManager::GetSingleton()->SheatheWeapon(follower);
                        logger::info("EquipPrompt: PrismaUI accepted {}; equipped and protected {}.",
                            prompt.followerName, prompt.newObjName);
                    }
                }
            }
        } else {
            logger::info("EquipPrompt: PrismaUI declined {}; item remains in follower inventory.", prompt.newObjName);
        }
        if (!g_promptQueue.empty()) {
            g_waitingForLootCompletion = true;
            g_showAt = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        }
    }
}
