#include "UI/EquipPrompt.h"
#include "Core/FollowerManager.h"
#include "UI/PrismaUIManager.h"
#include "UI/SKSEMenuFramework.h"
#include "AI/AnimationManager.h"
#include "Economy/EconomyManager.h"
#include "AI/ItemEvaluator.h"
#include "UI/NotificationManager.h"

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
            std::string slotLabel;
            std::string reason;
            bool vendorPurchase{ false };
            RE::FormID stockContainerID{ 0 };
            uint32_t price{ 0 };
            uint32_t spendableGold{ 0 };
            std::string vendorName;
            std::string currentStats;
            std::string candidateStats;
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
            if (prompt.vendorPurchase) {
                PrismaUIManager::GetSingleton()->ShowEquipmentPurchasePrompt(
                    prompt.followerID, prompt.newObjID, prompt.oldObjID,
                    prompt.followerName, prompt.newObjName, prompt.slotLabel, prompt.vendorName,
                    prompt.price, prompt.spendableGold, prompt.currentStats, prompt.candidateStats);
            } else {
                PrismaUIManager::GetSingleton()->ShowEquipPrompt(
                    prompt.followerID, prompt.newObjID, prompt.oldObjID,
                    prompt.followerName, prompt.newObjName, prompt.slotLabel, prompt.reason,
                    prompt.currentStats, prompt.candidateStats);
            }
        }
    }

    void EquipPrompt::ShowPrompt(
        RE::Actor* follower,
        FollowerProfile*,
        RE::TESBoundObject* newObj,
        RE::TESBoundObject* oldObj,
        std::string_view reason,
        std::string_view currentStats,
        std::string_view candidateStats)
    {
        if (!follower || !newObj) return;
        const auto slotLabel = ItemEvaluator::GetItemCategoryLabel(newObj);
        g_promptQueue.push_back({
            follower->GetFormID(), newObj->GetFormID(), oldObj ? oldObj->GetFormID() : 0,
            follower->GetName(), newObj->GetName(), slotLabel, std::string(reason), false, 0, 0, {},
            std::string(currentStats), std::string(candidateStats) });
        logger::info("EquipPrompt: queued PrismaUI decision for {} / {} ({}); waiting for loot distribution completion.",
            follower->GetName(), newObj->GetName(), slotLabel);
    }

    bool EquipPrompt::ShowPurchasePrompt(
        RE::Actor* follower,
        RE::TESObjectREFR* stockContainer,
        RE::TESBoundObject* newObj,
        RE::TESBoundObject* oldObj,
        uint32_t price,
        std::string_view vendorName,
        std::string_view currentStats,
        std::string_view candidateStats)
    {
        if (!follower || !stockContainer || !newObj) return false;
        const auto followerID = follower->GetFormID();
        const auto itemID = newObj->GetFormID();
        if (IsPurchasePending(followerID, itemID)) return false;
        auto profile = FollowerManager::GetSingleton()->GetProfile(followerID);
        if (!profile || profile->declinedEquipmentPurchases.contains(itemID)) return false;
        auto* gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        const int32_t physicalGold = gold ? ItemEvaluator::GetItemCount(follower, gold) : 0;
        const uint32_t balance = std::min(profile->gold, static_cast<uint32_t>(std::max(0, physicalGold)));
        const uint32_t reserve = profile->preferences.savingsEnabled
            ? std::min(profile->preferences.savedGold, balance) : 0;

        PromptRequest request;
        request.followerID = followerID;
        request.newObjID = itemID;
        request.oldObjID = oldObj ? oldObj->GetFormID() : 0;
        request.followerName = follower->GetName();
        request.newObjName = newObj->GetName();
        request.slotLabel = ItemEvaluator::GetItemCategoryLabel(newObj);
        request.reason = "A vendor has a compatible upgrade for sale.";
        request.vendorPurchase = true;
        request.stockContainerID = stockContainer->GetFormID();
        request.price = price;
        request.spendableGold = balance - reserve;
        request.vendorName = std::string(vendorName);
        request.currentStats = std::string(currentStats);
        request.candidateStats = std::string(candidateStats);
        g_promptQueue.push_back(std::move(request));
        logger::info("EquipmentPurchase: queued offer for {} / {} from {} at {} gold.",
            follower->GetName(), newObj->GetName(), vendorName, price);
        return true;
    }

    bool EquipPrompt::IsPurchasePending(RE::FormID followerID, RE::FormID itemID)
    {
        const auto matches = [followerID, itemID](const PromptRequest& request) {
            return request.vendorPurchase && request.followerID == followerID && request.newObjID == itemID;
        };
        return (g_activePrompt && matches(*g_activePrompt)) || std::ranges::any_of(g_promptQueue, matches);
    }

    uint32_t EquipPrompt::GetReservedGold(RE::FormID followerID)
    {
        uint64_t total = 0;
        const auto addIfOwned = [followerID, &total](const PromptRequest& request) {
            if (request.vendorPurchase && request.followerID == followerID) total += request.price;
        };
        if (g_activePrompt) addIfOwned(*g_activePrompt);
        for (const auto& request : g_promptQueue) addIfOwned(request);
        return static_cast<uint32_t>(std::min<uint64_t>(total, std::numeric_limits<uint32_t>::max()));
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
                if (prompt.vendorPurchase) {
                    auto* stock = RE::TESForm::LookupByID<RE::TESObjectREFR>(prompt.stockContainerID);
                    auto* gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
                    const int32_t stockCount = ItemEvaluator::GetItemCount(stock, item);
                    const int32_t physicalGold = gold ? ItemEvaluator::GetItemCount(follower, gold) : 0;
                    const uint32_t balance = std::min(profile->gold, static_cast<uint32_t>(std::max(0, physicalGold)));
                    const uint32_t reserve = profile->preferences.savingsEnabled
                        ? std::min(profile->preferences.savedGold, balance) : 0;
                    const uint32_t spendable = balance - reserve;
                    if (!stock || stockCount <= 0) {
                        NotificationManager::GetSingleton()->ShowHUDMessage(
                            std::format("{}: {} is no longer in stock.", prompt.followerName, prompt.newObjName));
                    } else if (!gold || spendable < prompt.price || physicalGold < static_cast<int32_t>(prompt.price)) {
                        NotificationManager::GetSingleton()->ShowHUDMessage(
                            std::format("{} cannot afford {}: needs {} gold, has {} spendable.", prompt.followerName, prompt.newObjName, prompt.price, spendable));
                    } else if (!EconomyManager::GetSingleton()->RemoveGold(prompt.followerID, static_cast<int32_t>(prompt.price))) {
                        NotificationManager::GetSingleton()->ShowHUDMessage(
                            std::format("{}'s gold changed; {} could not be purchased.", prompt.followerName, prompt.newObjName));
                    } else {
                        const int32_t followerCountBefore = ItemEvaluator::GetItemCount(follower, item);
                        stock->RemoveItem(item, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                        follower->AddObjectToContainer(item, nullptr, 1, nullptr);
                        const int32_t followerCountAfter = ItemEvaluator::GetItemCount(follower, item);
                        if (followerCountAfter <= followerCountBefore) {
                            stock->AddObjectToContainer(item, nullptr, 1, nullptr);
                            profile->gold += prompt.price;
                            NotificationManager::GetSingleton()->ShowHUDMessage(
                                std::format("{}'s inventory did not accept {}; the purchase was cancelled and gold refunded.", prompt.followerName, prompt.newObjName));
                            logger::error("EquipmentPurchase: inventory transfer failed for {} / {}; rolled back stock and gold.",
                                prompt.followerName, prompt.newObjName);
                        } else {
                            follower->RemoveItem(gold, static_cast<int32_t>(prompt.price), RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

                            auto* oldItem = prompt.oldObjID ? RE::TESForm::LookupByID<RE::TESBoundObject>(prompt.oldObjID) : nullptr;
                            if (oldItem) {
                                profile->ProtectItem(prompt.oldObjID, false);
                                if (auto* equip = RE::ActorEquipManager::GetSingleton()) {
                                    equip->UnequipObject(follower, oldItem, nullptr, 1, nullptr, false, false, false, true);
                                }
                            }
                            profile->ProtectItem(prompt.newObjID, true);
                            if (auto* equip = RE::ActorEquipManager::GetSingleton()) {
                                equip->EquipObject(follower, item, nullptr, 1, nullptr, false, false, false, true);
                                if (item->IsWeapon()) AnimationManager::GetSingleton()->SheatheWeapon(follower);
                            }
                            NotificationManager::GetSingleton()->ShowHUDMessage(
                                std::format("{} bought and equipped {} for {} gold; it is now in their inventory.", prompt.followerName, prompt.newObjName, prompt.price));
                            logger::info("EquipmentPurchase: {} bought {}, paid {} gold, protected it, and released old equipment for sale.",
                                prompt.followerName, prompt.newObjName, prompt.price);
                        }
                    }
                } else if (prompt.oldObjID != 0 && profile->IsProtected(prompt.oldObjID)) {
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
        } else if (prompt.vendorPurchase) {
            if (auto profile = FollowerManager::GetSingleton()->GetProfile(prompt.followerID)) {
                profile->declinedEquipmentPurchases.insert(prompt.newObjID);
            }
            logger::info("EquipmentPurchase: {} declined {}; this item will not be offered to this follower again.",
                prompt.followerName, prompt.newObjName);
        } else {
            logger::info("EquipPrompt: PrismaUI declined {}; item remains in follower inventory.", prompt.newObjName);
        }
        if (!g_promptQueue.empty()) {
            g_waitingForLootCompletion = true;
            g_showAt = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        }
    }
}
