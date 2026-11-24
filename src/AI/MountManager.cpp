#include "AI/MountManager.h"
#include "Core/FollowerManager.h"
#include "Core/EventManager.h"
#include "Core/Settings.h"

namespace FollowerUnbound
{
    namespace
    {
        bool CallOnActor(RE::Actor* actor, const char* function, RE::BSScript::IFunctionArguments* args)
        {
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
            if (!policy || !actor) {
                delete args;
                return false;
            }
            const auto handle = policy->GetHandleForObject(RE::FormType::ActorCharacter, actor);
            if (handle == policy->EmptyHandle()) {
                delete args;
                return false;
            }
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            return vm->DispatchMethodCall2(handle, "Actor", function, args, callback);
        }
    }

    RE::Actor* MountManager::FindAssignedHorse(RE::Actor* follower)
    {
        if (!follower || !follower->Is3DLoaded()) return nullptr;

        auto* followerBase = follower->GetActorBase();
        if (!followerBase) return nullptr;

        RE::Actor* bestHorse = nullptr;
        float bestDistance = 4500.0f; // 4500 units search radius

        // 1. ProcessLists::highActorHandles: Hızlı ve exterior grid sınırlarını aşan tarama
        auto* processLists = RE::ProcessLists::GetSingleton();
        if (processLists) {
            for (auto& handle : processLists->highActorHandles) {
                auto actorPtr = handle.get();
                if (!actorPtr) continue;
                auto* actor = actorPtr.get();
                if (!actor || actor->IsDead() || !actor->IsAMount()) continue;

                // A Horse's Life doğrudan ActorOwner ataması yapar:
                // horseActor.SetActorOwner(newOwner.GetActorBase())
                auto* horseOwner = actor->GetActorOwner();
                if (horseOwner == followerBase) {
                    return actor;
                }

                // Faction mülkiyeti kontrolü (örnek: PlayerFollowerFaction / CurrentFollowerFaction atı)
                auto* factionOwner = actor->GetFactionOwner();
                if (factionOwner && follower->IsInFaction(factionOwner)) {
                    float dist = follower->GetPosition().GetDistance(actor->GetPosition());
                    if (dist < bestDistance) {
                        bestDistance = dist;
                        bestHorse = actor;
                    }
                }
            }
        }
        if (bestHorse) return bestHorse;

        // 2. Yedek Hücre Taraması
        auto* cell = follower->GetParentCell();
        if (cell) {
            cell->ForEachReferenceInRange(follower->GetPosition(), 4500.0f, [&](RE::TESObjectREFR* ref) {
                if (!ref) return RE::BSContainer::ForEachResult::kContinue;
                auto* actor = ref->As<RE::Actor>();
                if (!actor || actor->IsDead() || !actor->IsAMount()) return RE::BSContainer::ForEachResult::kContinue;

                auto* horseOwner = actor->GetActorOwner();
                if (horseOwner == followerBase) {
                    bestHorse = actor;
                    return RE::BSContainer::ForEachResult::kStop;
                }

                auto* factionOwner = actor->GetFactionOwner();
                if (factionOwner && follower->IsInFaction(factionOwner)) {
                    float dist = follower->GetPosition().GetDistance(actor->GetPosition());
                    if (dist < bestDistance) {
                        bestDistance = dist;
                        bestHorse = actor;
                    }
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });
        }

        return bestHorse;
    }

    bool MountManager::MountHorse(RE::Actor* follower, RE::Actor* horse)
    {
        if (!follower || !horse || follower->IsOnMount() || !follower->Is3DLoaded() || !horse->Is3DLoaded()) {
            return false;
        }

        float dist = follower->GetPosition().GetDistance(horse->GetPosition());
        if (dist <= 400.0f) {
            // Ata yaklaştı: offset yürümesini durdur ve ata bindir
            CallOnActor(follower, "StopKeepOffsetFromActor", RE::MakeFunctionArguments());

            bool mounted = follower->PutActorOnMountQuick();
            if (!mounted) {
                horse->ActivateRef(follower, 0, nullptr, 1, false);
            }
            logger::info("MountManager: {} mounted horse {} (dist={:.0f}).",
                follower->GetName(), horse->GetName(), dist);
            return true;
        }

        // Mesafe 400 birimden fazlaysa ata doğru yönlendir
        auto* args = RE::MakeFunctionArguments(
            static_cast<RE::TESObjectREFR*>(horse),
            0.0f, 90.0f, 0.0f, 0.0f, 0.0f, 0.0f, 350.0f, 20.0f
        );
        CallOnActor(follower, "KeepOffsetFromActor", args);
        logger::info("MountManager: {} moving towards assigned horse {} (dist={:.0f}).",
            follower->GetName(), horse->GetName(), dist);
        return false;
    }

    bool MountManager::DismountHorse(RE::Actor* follower)
    {
        if (!follower) return false;

        // Her halükarda at hedefine yürümeyi durdur
        CallOnActor(follower, "StopKeepOffsetFromActor", RE::MakeFunctionArguments());

        if (follower->IsOnMount()) {
            CallOnActor(follower, "Dismount", RE::MakeFunctionArguments());
            logger::info("MountManager: {} dismounting horse.", follower->GetName());
            return true;
        }

        return false;
    }

    void MountManager::Update()
    {
        if (!Settings::GetSingleton()->enableHorseMounting) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) return;

        // Throttle to every ~500ms
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<float>(now - _lastUpdate).count() < 0.5f) {
            return;
        }
        _lastUpdate = now;

        // OStim sahnesi aktifse at işlemlerini durdur
        if (EventManager::IsOstimBusy()) return;

        bool isPlayerMounted = player->IsOnMount();
        auto* followerManager = FollowerManager::GetSingleton();
        if (!followerManager) return;

        auto activeFollowers = followerManager->GetActiveFollowers();

        // 1. OYUNCU ATTAN İNDİĞİNDE: Takipçiler de atlarından inmeli
        if (!isPlayerMounted) {
            if (_playerMountedLastTick) {
                logger::info("MountManager: Player dismounted. Dismounting all mounted followers.");
                for (auto* follower : activeFollowers) {
                    if (follower) {
                        DismountHorse(follower);
                    }
                }
            }
        }
        // 2. OYUNCU AT ÜSTÜNDEYKEN: Takipçiler de atlarına binmeli ve at üstünde kalmalı
        else {
            for (auto* follower : activeFollowers) {
                if (!follower || follower->IsDead()) continue;

                // Takipçi zaten at üstündeyse:
                if (follower->IsOnMount()) {
                    // Savaş durumundaysa (Mounted Combat):
                    // Oyuncu at üzerindeyken savaşa girdiğimizde takipçi at üstünde savaşmaya devam eder.
                    if (follower->IsInCombat() || player->IsInCombat()) {
                        follower->EvaluatePackage(false, false);
                    }
                    continue;
                }

                // Takipçi henüz ata binmemişse, atanan atı bul ve bindir:
                const auto fid = follower->GetFormID();
                if (auto it = _lastMountAttempt.find(fid); it != _lastMountAttempt.end()) {
                    if (std::chrono::duration<float>(now - it->second).count() < 2.5f) {
                        continue; // 2.5 saniye arayla dene
                    }
                }
                _lastMountAttempt[fid] = now;

                auto* horse = FindAssignedHorse(follower);
                if (horse) {
                    MountHorse(follower, horse);
                }
            }
        }

        _playerMountedLastTick = isPlayerMounted;
    }
}
