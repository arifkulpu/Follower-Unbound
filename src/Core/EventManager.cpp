#include "Core/EventManager.h"
#include "AI/LootManager.h"
#include "AI/CombatTracker.h"
#include "AI/ConsumableAI.h"
#include "Core/FollowerManager.h"
#include "Core/Settings.h"
#include "AI/DroppedItemRecovery.h"
#include "AI/ItemEvaluator.h"
#include "Economy/EconomyManager.h"
#include "UI/NotificationManager.h"
#include "RE/T/TESFlora.h"
#include "KingCore/KingCoreAPI.h"

namespace FollowerUnbound
{
    namespace
    {
        void NotifyKingCoreFollowersRested(KingCore::RestEventType a_type)
        {
            auto* messaging = SKSE::GetMessagingInterface();
            auto* calendar = RE::Calendar::GetSingleton();
            if (!messaging || !calendar) return;
            KingCore::FollowerRestEvent event{};
            event.apiVersion = KingCore::API_VERSION;
            event.type = a_type;
            event.gameDaysPassed = calendar->GetDaysPassed();
            messaging->Dispatch(KingCore::MESSAGE_TYPE_FOLLOWER_RESTED, &event, sizeof(event), "KingCore");
        }

        bool IsOreOrGem(RE::TESBoundObject* item)
        {
            if (!item) return false;
            auto* misc = item->As<RE::TESObjectMISC>();
            if (!misc) return false;

            if (auto* kw = misc->As<RE::BGSKeywordForm>()) {
                if (kw->HasKeywordID(0x000914ED)) return true; // VendorItemGem
                if (kw->HasKeywordID(0x000914EA)) { // VendorItemOreIngot
                    std::string name = misc->GetName() ? misc->GetName() : "";
                    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                    if (name.find("ingot") == std::string::npos && name.find("külçe") == std::string::npos && name.find("kulce") == std::string::npos) {
                        return true;
                    }
                }
            }
            std::string name = misc->GetName() ? misc->GetName() : "";
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name.find("ore") != std::string::npos || name.find("cevher") != std::string::npos || name.find("maden") != std::string::npos || name.find("stalhrim") != std::string::npos) {
                return true;
            }
            return false;
        }

        bool IsFirewood(RE::TESBoundObject* item)
        {
            if (!item) return false;
            if (item->GetFormID() == 0x0006F993) return true;
            if (auto* misc = item->As<RE::TESObjectMISC>()) {
                if (auto* kw = misc->As<RE::BGSKeywordForm>()) {
                    if (kw->HasKeywordID(0x0008F959)) return true; // VendorItemFirewood
                }
                std::string name = misc->GetName() ? misc->GetName() : "";
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                if (name.find("firewood") != std::string::npos || name.find("yakacak") != std::string::npos || name.find("odun") != std::string::npos) {
                    return true;
                }
            }
            return false;
        }
    }

    void EventManager::Register()
    {
        auto ui = RE::UI::GetSingleton();
        if (ui) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
            logger::info("Registered MenuOpenCloseEvent sink.");
        }

        auto scriptSource = RE::ScriptEventSourceHolder::GetSingleton();
        if (scriptSource) {
            scriptSource->AddEventSink<RE::TESCombatEvent>(this);
            logger::info("Registered TESCombatEvent sink.");
            scriptSource->AddEventSink<RE::TESContainerChangedEvent>(this);
            logger::info("Registered TESContainerChangedEvent sink for dropped follower items.");
            scriptSource->AddEventSink<RE::TESHitEvent>(this);
            logger::info("Registered TESHitEvent sink.");
            scriptSource->AddEventSink<RE::TESWaitStopEvent>(this);
            logger::info("Registered TESWaitStopEvent sink.");
            scriptSource->AddEventSink<RE::TESSleepStopEvent>(this);
            logger::info("Registered TESSleepStopEvent sink.");
            scriptSource->AddEventSink<RE::TESFastTravelEndEvent>(this);
            logger::info("Registered TESFastTravelEndEvent sink.");
            scriptSource->AddEventSink<RE::TESCellFullyLoadedEvent>(this);
            logger::info("Registered TESCellFullyLoadedEvent sink.");
            scriptSource->AddEventSink<RE::TESDeathEvent>(this);
            logger::info("Registered TESDeathEvent sink.");
        }

    }

    RE::BSEventNotifyControl EventManager::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
    {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        // Sadece belirli oyuncu menüleri kapandığında kontrol yap.
        // DialogueMenu, Console, HUDMenu, Loading Menu, Fader Menu gibi menüler
        // Scaleform çökmelerine veya animasyon çakışmalarına yol açmamak için kesinlikle hariç tutulmalıdır.
        auto mName = a_event->menuName;

        if (mName == RE::ContainerMenu::MENU_NAME) {
            if (a_event->opening) {
                _lastContainerOpened = 0;
                const auto handle = RE::ContainerMenu::GetTargetRefHandle();
                auto target = RE::TESObjectREFR::LookupByHandle(handle);
                if (target) {
                    _lastContainerOpened = target->GetFormID();
                } else if (auto* pickData = RE::CrosshairPickData::GetSingleton()) {
                    auto crosshairTarget = pickData->target.get();
                    if (crosshairTarget) _lastContainerOpened = crosshairTarget->GetFormID();
                }
            } else {
                RE::FormID containerID = _lastContainerOpened;
                if (containerID == 0) {
                    const auto handle = RE::ContainerMenu::GetTargetRefHandle();
                    auto target = RE::TESObjectREFR::LookupByHandle(handle);
                    if (target) {
                        containerID = target->GetFormID();
                    } else if (auto* pickData = RE::CrosshairPickData::GetSingleton()) {
                        auto crosshairTarget = pickData->target.get();
                        if (crosshairTarget) containerID = crosshairTarget->GetFormID();
                    }
                }
                if (containerID != 0) {
                    LootManager::GetSingleton()->QueuePlayerLootDistribution(containerID);
                    LootManager::GetSingleton()->ScheduleProcessing();
                }
                _lastContainerOpened = 0;
            }
        }

        if (!a_event->opening) {
            // Konteyner / Envanter menüsü kapandı: 3 saniye sonra takipçiler yemek/içmek için hazır olacak
            if (mName == RE::InventoryMenu::MENU_NAME || mName == RE::ContainerMenu::MENU_NAME) {
                ConsumableAI::GetSingleton()->NotifyInventoryClosed();
            }

            // Güvenli menü filtrelemesi: Sadece oyuncunun doğrudan etkileşimde bulunduğu
            // ve diyalog dışı kalan ana menüler kapandığında ihtiyaçları tetikle.
            bool isSafeMenu = (mName == RE::InventoryMenu::MENU_NAME ||
                               mName == RE::ContainerMenu::MENU_NAME ||
                               mName == RE::JournalMenu::MENU_NAME ||
                               mName == RE::MapMenu::MENU_NAME ||
                               mName == RE::MagicMenu::MENU_NAME ||
                               mName == RE::TweenMenu::MENU_NAME ||
                               mName == RE::FavoritesMenu::MENU_NAME ||
                               mName == RE::BarterMenu::MENU_NAME ||
                               mName == RE::GiftMenu::MENU_NAME);

            if (isSafeMenu) {
                // Oyuncu o anda diyalogda değilse güncelle
                auto ui = RE::UI::GetSingleton();
                if (!ui || !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
                    FollowerManager::GetSingleton()->UpdateGameTimeNeeds();
                }
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventManager::ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>*)
    {
        if (!a_event || !a_event->actor) return RE::BSEventNotifyControl::kContinue;

        auto actor = a_event->actor->As<RE::Actor>();
        if (!actor) return RE::BSEventNotifyControl::kContinue;
        auto target = a_event->targetActor ? a_event->targetActor->As<RE::Actor>() : nullptr;

        // ── CombatTracker: düşman cesetleri burada işaretlenir ──
        // Oyuncuya/takipçiye savaşan aktörler "hostile participant" olarak kaydedilir;
        // savaş bitince geçici kayıt temizlenir (ölü düşman kaydı korunur).
        auto tracker = CombatTracker::GetSingleton();
        if (a_event->newState == RE::ACTOR_COMBAT_STATE::kCombat) {
            tracker->NotifyCombatStart(actor, target);
        } else {
            tracker->NotifyCombatActivity(actor, target);
        }

        auto manager = FollowerManager::GetSingleton();
        if (manager->IsFollower(actor->GetFormID())) {
            auto profile = manager->GetProfile(actor->GetFormID());
            if (profile) {
                if (a_event->newState == RE::ACTOR_COMBAT_STATE::kCombat) {
                    profile->currentState = FollowerState::Combat;
                    ConsumableAI::GetSingleton()->UpdateCombatConsumables(actor, *profile);
                } else if (a_event->newState == RE::ACTOR_COMBAT_STATE::kNone) {
                    profile->currentState = FollowerState::PostCombat;
                    // Savaş bitti: yeme/içme birkaç saniye geciktirilir
                    ConsumableAI::GetSingleton()->NotifyCombatEnded(actor->GetFormID());
                    tracker->NotifyCombatEnd(actor);
                }
            }
        } else if (actor->IsPlayerRef() && a_event->newState == RE::ACTOR_COMBAT_STATE::kNone) {
            tracker->NotifyCombatEnd(actor);
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventManager::ProcessEvent(const RE::TESContainerChangedEvent* a_event,
        RE::BSTEventSource<RE::TESContainerChangedEvent>*)
    {
        if (!a_event || a_event->baseObj == 0) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Share ingredient stacks when they actually enter the player's inventory.
        // The container-change event is more reliable than flora-specific harvest
        // callbacks and also covers pickable mushrooms/eggs and mod ingredients.
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto playerID = player ? player->GetFormID() : 0;
        if (playerID && a_event->newContainer == playerID) {
            auto* ui = RE::UI::GetSingleton();
            const bool inMenu = ui && (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) || ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME));

            // Gold purse / loose world gold bonus split:
            // Player keeps 100% of the gold, and active followers get equal bonus shares.
            if (!inMenu && a_event->baseObj == 0x0000000F && a_event->oldContainer == 0) {
                const auto totalGold = a_event->itemCount;
                if (totalGold > 0) {
                    if (auto* tasks = SKSE::GetTaskInterface()) {
                        tasks->AddTask([totalGold]() {
                            auto* manager = FollowerManager::GetSingleton();
                            if (!manager) return;
                            auto followers = manager->GetActiveFollowers();
                            followers.erase(std::remove_if(followers.begin(), followers.end(), [](RE::Actor* actor) {
                                return !actor || actor->IsDead() || FollowerManager::IsAnimalActor(actor);
                            }), followers.end());
                            if (followers.empty()) return;

                            const int32_t share = totalGold / static_cast<int32_t>(followers.size());
                            if (share <= 0) return;

                            auto* goldItem = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
                            if (!goldItem) return;

                            for (auto* follower : followers) {
                                follower->AddObjectToContainer(goldItem, nullptr, share, nullptr);
                                EconomyManager::GetSingleton()->AddGold(follower->GetFormID(), share);
                            }
                            if (Settings::GetSingleton()->notifyLoot) {
                                NotificationManager::GetSingleton()->ShowHUDMessage(
                                    std::format("Altın paylaşıldı: Takipçilerin her biri {} altın aldı.", share));
                            }
                            logger::info("GoldPurseShare: Player found {} gold; gave {} bonus gold each to {} active followers.",
                                totalGold, share, followers.size());
                        });
                    }
                }
            }

            // Mining ore/gems and chopping firewood:
            // Followers receive matching amounts of the harvested resources.
            if (!inMenu && a_event->baseObj != 0x0000000F && a_event->oldContainer == 0) {
                const auto resourceID = static_cast<RE::FormID>(a_event->baseObj);
                auto* resourceItem = RE::TESForm::LookupByID<RE::TESBoundObject>(resourceID);
                const auto resourceCount = std::clamp(std::abs(a_event->itemCount), 1, 50);
                if (resourceItem && (IsOreOrGem(resourceItem) || IsFirewood(resourceItem))) {
                    if (auto* tasks = SKSE::GetTaskInterface()) {
                        tasks->AddTask([resourceID, resourceCount]() {
                            auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(resourceID);
                            auto* manager = FollowerManager::GetSingleton();
                            if (!item || !manager) return;
                            auto followers = manager->GetActiveFollowers();
                            followers.erase(std::remove_if(followers.begin(), followers.end(), [](RE::Actor* actor) {
                                return !actor || actor->IsDead() || FollowerManager::IsAnimalActor(actor);
                            }), followers.end());
                            if (followers.empty()) return;

                            for (auto* follower : followers) {
                                follower->AddObjectToContainer(item, nullptr, resourceCount, nullptr);
                            }
                            if (Settings::GetSingleton()->notifyLoot) {
                                NotificationManager::GetSingleton()->ShowHUDMessage(
                                    std::format("Madencilik/Odun: Takipçilere {}x {} verildi.", resourceCount, item->GetName()));
                            }
                            logger::info("ResourceShare: Player gathered {}x {}; gave matching amount to {} followers.",
                                resourceCount, item->GetName(), followers.size());
                        });
                    }
                }
            }

            const auto ingredientID = static_cast<RE::FormID>(a_event->baseObj);
            auto* ingredient = RE::TESForm::LookupByID<RE::TESBoundObject>(ingredientID);
            const auto harvestedCount = std::clamp(std::abs(a_event->itemCount), 1, 50);
            if (ingredient && ingredient->As<RE::IngredientItem>()) {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([ingredientID, harvestedCount]() {
                        auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(ingredientID);
                        auto* manager = FollowerManager::GetSingleton();
                        if (!item || !manager) return;
                        auto followers = manager->GetActiveFollowers();
                        followers.erase(std::remove_if(followers.begin(), followers.end(), [](RE::Actor* actor) {
                            return !actor || actor->IsDead() || FollowerManager::IsAnimalActor(actor);
                        }), followers.end());
                        if (followers.empty()) {
                            logger::info("HarvestShare: picked up {}x {}, but no active followers were eligible.", harvestedCount, item->GetName());
                            return;
                        }

                        static std::unordered_map<RE::FormID, size_t> nextRecipient;
                        auto& cursor = nextRecipient[ingredientID];
                        int32_t distributed = 0;
                        for (int32_t i = 0; i < harvestedCount; ++i) {
                            bool assigned = false;
                            for (size_t attempt = 0; attempt < followers.size(); ++attempt) {
                                auto* recipient = followers[cursor % followers.size()];
                                cursor = (cursor + 1) % followers.size();
                                auto profile = manager->GetProfile(recipient->GetFormID());
                                if (!profile) continue;
                                const auto cap = std::clamp(profile->preferences.maxPotionFlowers, 1u, 50u);
                                const auto current = ItemEvaluator::GetItemCount(recipient, item);
                                const auto grant = std::min<int32_t>(1, std::max(0, static_cast<int32_t>(cap) - current));
                                if (grant <= 0) continue;
                                recipient->AddObjectToContainer(item, nullptr, grant, nullptr);
                                ++distributed;
                                assigned = true;
                                break;
                            }
                            if (!assigned) break;
                        }

                        if (distributed > 0) {
                            const auto message = std::format("Followers collected {}x {}.", distributed, item->GetName());
                            NotificationManager::GetSingleton()->ShowHUDMessage(message);
                            logger::info("HarvestShare: distributed {}/{}x {} among {} active followers (per-item cap applies).",
                                distributed, harvestedCount, item->GetName(), followers.size());
                        } else {
                            logger::info("HarvestShare: picked up {}x {}, but all followers are at their per-item caps.", harvestedCount, item->GetName());
                        }
                    });
                } else {
                    logger::warn("HarvestShare: SKSE task interface unavailable for {}.", ingredient->GetName());
                }
            }
        }

        if (a_event->newContainer != 0 || a_event->oldContainer == 0) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* manager = FollowerManager::GetSingleton();
        if (!manager->IsFollower(a_event->oldContainer)) return RE::BSEventNotifyControl::kContinue;
        auto* follower = RE::TESForm::LookupByID<RE::Actor>(a_event->oldContainer);
        if (!follower || follower->IsDead() || !follower->IsInCombat()) return RE::BSEventNotifyControl::kContinue;

        const auto count = std::abs(a_event->itemCount);
        if (count <= 0 || !a_event->reference) return RE::BSEventNotifyControl::kContinue;

        DroppedItemRecovery::GetSingleton()->RecordDrop(
            a_event->oldContainer, a_event->baseObj, count, a_event->reference);
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventManager::ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*)
    {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        auto target = a_event->target ? a_event->target->As<RE::Actor>() : nullptr;
        auto cause = a_event->cause ? a_event->cause->As<RE::Actor>() : nullptr;
        CombatTracker::GetSingleton()->NotifyCombatActivity(cause, target);

        // TESHitEvent sırasında oyun motoru hasar/zırh/ekipman hesaplamalarının ortasındadır.
        // Burada doğrudan DrinkPotion/RemoveItem çağırmak, FollowerEquipControl gibi harici
        // modların ekipman kancalarıyla (hooks) çakışarak STACK_OVERFLOW çökmesine yol açabilir.
        // Savaş iksirleri zaten TESCombatEvent ve saniyelik güvenli arka plan döngüsünde taranır.
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventManager::ProcessEvent(const RE::TESWaitStopEvent* a_event, RE::BSTEventSource<RE::TESWaitStopEvent>*)
    {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        // Oyuncu bekledi:
        // - Zamanı tick'le (ihtiyaçları azalt)
        // - Takipçiler hemen yememeli: 5 saniye bekleme süresi başlat
        ConsumableAI::GetSingleton()->NotifyWaitEnded();
        FollowerManager::GetSingleton()->UpdateGameTimeNeeds();
        NotifyKingCoreFollowersRested(KingCore::RestEventType::Wait);
        logger::info("TESWaitStopEvent: follower needs updated and wait delay started.");
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventManager::ProcessEvent(const RE::TESSleepStopEvent* a_event, RE::BSTEventSource<RE::TESSleepStopEvent>*)
    {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        // Oyuncu uyudu:
        // - Zamanı tick'le
        // - Takipçilerin yorgunluğunu %100 yap
        // - Takipçiler hemen yememeli: 5 saniye bekleme süresi başlat
        ConsumableAI::GetSingleton()->NotifySleepEnded();
        FollowerManager::GetSingleton()->OnSleepStop();
        NotifyKingCoreFollowersRested(KingCore::RestEventType::Sleep);
        logger::info("TESSleepStopEvent: follower needs updated, sleep delay started and sleep restored.");
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventManager::ProcessEvent(const RE::TESFastTravelEndEvent* a_event, RE::BSTEventSource<RE::TESFastTravelEndEvent>*)
    {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        // Hızlı seyahat tamamlandı — geçen zamanı hesapla ve ihtiyaçları güncelle
        // Takipçiler hemen yememeli: 3 saniye bekleme süresi başlat
        ConsumableAI::GetSingleton()->NotifyWaitEnded();
        FollowerManager::GetSingleton()->UpdateGameTimeNeeds();
        logger::info("TESFastTravelEndEvent: follower needs updated after journey, eat delay started.");
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventManager::ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*)
    {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        // Hücre yüklendiğinde / alan değiştirildiğinde zamanı güncelle
        FollowerManager::GetSingleton()->UpdateGameTimeNeeds();
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl EventManager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
    {
        if (!a_event || !a_event->actorDying) return RE::BSEventNotifyControl::kContinue;

        auto actor = a_event->actorDying->As<RE::Actor>();
        if (!actor || actor->IsPlayerRef()) return RE::BSEventNotifyControl::kContinue;

        CombatTracker::GetSingleton()->NotifyDeath(actor);

        RE::FormID formID = actor->GetFormID();
        auto manager = FollowerManager::GetSingleton();
        if (manager->IsFollower(formID)) {
            logger::info("TESDeathEvent: Follower {} [{:08X}] died — unregistering from system.",
                actor->GetName(), formID);
            manager->UnregisterFollower(formID);
        }

        return RE::BSEventNotifyControl::kContinue;
    }
}
