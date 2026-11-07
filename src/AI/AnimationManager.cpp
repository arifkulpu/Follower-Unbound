#include "AI/AnimationManager.h"
#include "RE/A/AIProcess.h"
#include "RE/T/TESIdleForm.h"

namespace FollowerUnbound
{
    bool AnimationManager::IsRidingWithPlayer(RE::Actor* actor)
    {
        if (!actor || actor->IsDead()) return false;
        if (actor->IsOnMount()) return true;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->IsOnMount() || actor == player || !actor->Is3DLoaded()) return false;

        // RSE follower attachment keeps the companion very close to the mounted
        // player, even though the follower itself may not report IsOnMount().
        return actor->GetPosition().GetDistance(player->GetPosition()) <= 420.0f;
    }

    bool AnimationManager::SendAnimEvent(RE::Actor* actor, const char* eventName)
    {
        if (!actor || IsRidingWithPlayer(actor)) return false;

        // NotifyAnimationGraph: SKSE/CommonLibSSE-NG API
        // Behavior graph uzerinden animation event gonderir.
        // Eating Animations and Sounds SE veya Potion Drinking Animation
        // gibi modlar bu event'leri kendi animasyonlariyla override eder.
        bool result = actor->NotifyAnimationGraph(eventName);
        logger::info("AnimEvent '{}' -> {} [result={}]",
            eventName, actor->GetName(), result);
        return result;
    }

    // Animasyon oynatılamazsa (event graph tarafından reddedilirse) sıradaki
    // alternatif event denenir. Hepsi başarısız olursa animasyon grafiği idle'a
    // sıfırlanıp zincir BİR KEZ daha denenir — yeme/içme animasyonlarının "arada
    // çalışıp arada çalışmamasının" yaygın nedeni aktörün hareket halinde olmasıdır.
    static bool PlayFirstValidAnim(RE::Actor* actor, std::initializer_list<const char*> events, const char* context)
    {
        for (const char* eventName : events) {
            if (AnimationManager::SendAnimEvent(actor, eventName)) {
                return true;
            }
        }

        // Savas sirasinda veya silah kınında değilken graph reset (IdleForceDefaultState) cagirmak
        // grafiği silah geçişinin ortasında sıfırlar ve durum makinesini bozar!
        if (actor->IsInCombat()) {
            return false;
        }
        if (auto* state = actor->AsActorState(); !state || state->GetWeaponState() != RE::WEAPON_STATE::kSheathed) {
            return false;
        }

        // Savas disinda ve yalnizca silah tam kınındayken graph reset + tek tekrar
        if (AnimationManager::SendAnimEvent(actor, "IdleForceDefaultState")) {
            for (const char* eventName : events) {
                if (AnimationManager::SendAnimEvent(actor, eventName)) {
                    logger::info("AnimationManager: '{}' animasyonu graph reset sonrasi oynatildi ({}).",
                                 context, actor->GetName());
                    return true;
                }
            }
        }

        logger::warn("AnimationManager: '{}' animasyonu hicbir fallback ile oynatilamadi ({}).",
            context, actor->GetName());
        return false;
    }

    void AnimationManager::PlayEatAnimation(RE::Actor* actor)
    {
        if (!actor || actor->IsDead() || actor->IsInCombat() || IsRidingWithPlayer(actor)) return;

        if (auto* state = actor->AsActorState()) {
            if (state->GetWeaponState() != RE::WEAPON_STATE::kSheathed) {
                SheatheWeapon(actor);
                return;
            }
        }

        // "IdleEat" — Eating Animations and Sounds SE (mod 42602) ve EAS-SunHelm
        // patch'i tarafından yakalanır. Oynatılmazsa (mod yok / hata) alternatif
        // ekmek yeme eventleri denenir.
        PlayFirstValidAnim(actor, { "IdleEat", "IdleEating" }, "yeme");
    }

    void AnimationManager::PlayDrinkAnimation(RE::Actor* actor, RE::TESBoundObject* item)
    {
        if (!actor || actor->IsDead() || actor->IsInCombat() || IsRidingWithPlayer(actor)) return;

        if (auto* state = actor->AsActorState()) {
            if (state->GetWeaponState() != RE::WEAPON_STATE::kSheathed) {
                SheatheWeapon(actor);
                return;
            }
        }

        // SunHelm içecek kontrolü (Matara / Şişe)
        bool isSunHelmDrink = false;
        if (item) {
            RE::FormID formID = item->GetFormID();
            uint32_t localID = formID & 0x00FFFFFF;
            if (localID == 0x07AA96 || localID == 0x070897 || localID == 0x265BE3 ||
                localID == 0x4DE9AE || localID == 0x4DE9AF || localID == 0x4DE9B0) {
                isSunHelmDrink = true;
            } else {
                std::string iName = item->GetName();
                std::transform(iName.begin(), iName.end(), iName.begin(), ::tolower);
                if (iName.find("waterskin") != std::string::npos || iName.find("water skin") != std::string::npos) {
                    isSunHelmDrink = true;
                }
            }
        }

        // SunHelm matara/şişe: önce IdleDrink (EAS-SunHelm patch su içme animasyonu),
        // sonra potion içme ve genel içme fallbackleri.
        if (isSunHelmDrink) {
            PlayFirstValidAnim(actor, { "IdleDrink", "drinkPotion", "IdleDrinking" }, "SunHelm su içme");
            return;
        }

        // Vanilla / Standart içecek animasyonu (Potion Drinking Animation SE veya EAS yakalar)
        PlayFirstValidAnim(actor, { "drinkPotion", "IdleDrink", "IdleDrinking" }, "içme");
    }

    void AnimationManager::PlayPotionAnimation(RE::Actor* actor)
    {
        if (!actor || actor->IsDead() || IsRidingWithPlayer(actor)) return;

        // Bleedout (yere diz çökme / can çekişme) durumunda asla ayakta iksir animasyonu başlatma!
        if (auto* state = actor->AsActorState()) {
            auto life = state->GetLifeState();
            if (life == RE::ACTOR_LIFE_STATE::kBleedout ||
                life == RE::ACTOR_LIFE_STATE::kDead ||
                life == RE::ACTOR_LIFE_STATE::kDying) {
                return;
            }
        }

        // Savaş ve tedavi iksirleri için:
        // Ultimate Animated Potions NG (SKSE_UAPNG / mod 97674) - "DrinkPotionStart", "DrinkPotion_Slow", "DrinkPotion_NoMove"
        // Potion Drinking Animation SE / EAS / Vanilla - "drinkPotion", "IdleDrink"
        PlayFirstValidAnim(actor, { "DrinkPotionStart", "DrinkPotion_Slow", "DrinkPotion_NoMove", "drinkPotion", "IdleDrink" }, "iksir içme");
    }

    void AnimationManager::PlaySleepRestoreAnimation(RE::Actor* actor)
    {
        if (!actor || actor->IsDead() || IsRidingWithPlayer(actor)) return;

        // Uyku: once mevcut animasyonu bitir, sonra idle'a don
        SendAnimEvent(actor, "IdleForceDefaultState");
    }

    bool AnimationManager::PlayReadAnimation(RE::Actor* actor, bool isSitting)
    {
        if (!actor || actor->IsDead() || actor->IsInCombat() || IsRidingWithPlayer(actor)) return false;
        if (!actor->Is3DLoaded()) return false;

        if (auto* actorState = actor->AsActorState(); actorState && actorState->IsWeaponDrawn()) {
            SheatheWeapon(actor);
        }

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        auto* process = actor->GetActorRuntimeData().currentProcess;

        // 1. Immersive Interactions / Vanilla Idle Formlari:
        // Stand: IdleBook_Reading (0x000BB052), Sit: IdleBookSitting_Reading (0x0003505C)
        const RE::FormID idleID = isSitting ? 0x0003505C : 0x000BB052;
        auto* readIdle = dataHandler ? dataHandler->LookupForm<RE::TESIdleForm>(idleID, "Skyrim.esm") : nullptr;
        if (readIdle && process && process->PlayIdle(actor, readIdle, nullptr)) {
            logger::info("ReadAnim: Idle {:08X} started for {} (sitting={}).", idleID, actor->GetName(), isSitting);
            return true;
        }

        // 2. Behavior graph fallbackleri
        if (actor->NotifyAnimationGraph("AO_BookStart") || actor->NotifyAnimationGraph("IdleBook_Reading") ||
            actor->NotifyAnimationGraph("IdleRead") || actor->NotifyAnimationGraph("IdleBookEnter")) {
            logger::info("ReadAnim: Graph event accepted for {}.", actor->GetName());
            return true;
        }

        return false;
    }

    void AnimationManager::StopReadAnimation(RE::Actor* actor)
    {
        if (!actor || !actor->Is3DLoaded() || IsRidingWithPlayer(actor)) return;

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        auto* process = actor->GetActorRuntimeData().currentProcess;

        auto* stopIdle = dataHandler ? dataHandler->LookupForm<RE::TESIdleForm>(0x0010D9EE, "Skyrim.esm") : nullptr;
        if (stopIdle && process) {
            process->PlayIdle(actor, stopIdle, nullptr);
        }

        actor->NotifyAnimationGraph("IdleForceDefaultState");
        actor->NotifyAnimationGraph("OffsetStop");
    }

    bool AnimationManager::PlayLootAnimation(RE::Actor* actor, RE::TESObjectREFR* target)
    {
        if (!actor || actor->IsDead() || actor->IsInCombat() || IsRidingWithPlayer(actor)) {
            logger::info("LootAnim: skipped (actor unavailable/dead/in combat).");
            return false;
        }
        if (!actor->Is3DLoaded()) {
            logger::info("LootAnim: {} has no loaded 3D; cannot play the search idle.", actor->GetName());
            return false;
        }

        if (auto* actorState = actor->AsActorState(); actorState && actorState->IsWeaponDrawn()) {
            SheatheWeapon(actor);
        }

        // Immersive Interactions uses the vanilla IdleSearchBody idle directly
        // (its Papyrus script calls PlayerRef.PlayIdle(idleSearchBody)). Sending
        // generic pickup graph events can be accepted without showing an animation.
        auto* idleSearchBody = RE::TESDataHandler::GetSingleton()
            ? RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESIdleForm>(0x000EFC64, "Skyrim.esm")
            : nullptr;
        auto* process = actor->GetActorRuntimeData().currentProcess;
        if (idleSearchBody && process && process->PlayIdle(actor, idleSearchBody, target)) {
            logger::info("LootAnim: IdleSearchBody started for {} on corpse {:08X}.",
                actor->GetName(), target ? target->GetFormID() : 0);
            return true;
        }
        logger::warn("LootAnim: IdleSearchBody failed for {} (idle={}, process={}).",
            actor->GetName(), idleSearchBody != nullptr, process != nullptr);

        // Other behavior graphs may not support IdleSearchBody. Try generic
        // events only as fallbacks and record which one the graph accepted.
        for (const char* eventName : { "IdleActivatePickLow", "IdleActivatePick", "IdleLoot" }) {
            if (actor->NotifyAnimationGraph(eventName)) {
                logger::info("LootAnim: fallback graph event '{}' accepted for {}.", eventName, actor->GetName());
                return true;
            }
        }
        logger::warn("LootAnim: no idle or fallback graph event accepted for {}.", actor->GetName());
        return false;
    }

    void AnimationManager::SheatheWeapon(RE::Actor* actor)
    {
        // Savaştaki veya 3D yüklü olmayan aktörlerde kın alma güvenli değil.
        if (!actor || actor->IsDead() || actor->IsInCombat() || !actor->Is3DLoaded() || IsRidingWithPlayer(actor)) return;

        auto* actorState = actor->AsActorState();
        if (!actorState) return;

        const auto ws = actorState->GetWeaponState();
        // Zaten kınında veya kınına girme aşamasındaysa dokunma (durum makinesini bozmayı önler)
        if (ws == RE::WEAPON_STATE::kSheathed || ws == RE::WEAPON_STATE::kSheathing ||
            ws == RE::WEAPON_STATE::kWantToSheathe) return;

        RE::FormID actorID = actor->GetFormID();
        auto now = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(_sheatheMutex);
            if (auto it = _lastSheatheTime.find(actorID); it != _lastSheatheTime.end()) {
                if (std::chrono::duration<float>(now - it->second).count() < 1.0f) {
                    return;
                }
            }
            _lastSheatheTime[actorID] = now;
        }

        // Motorun kendi doğal kına koyma çağrısı (DrawWeaponMagicHands(false))
        actor->DrawWeaponMagicHands(false);

        logger::info("SheatheWeapon: {} DrawWeaponMagicHands(false) called (initial state={})",
            actor->GetName(), static_cast<int>(ws));
    }

    void AnimationManager::PlayPoisonWeaponAnimation(RE::Actor* follower, bool isRanged)
    {
        if (!follower || follower->IsDead() || !follower->Is3DLoaded() || IsRidingWithPlayer(follower)) return;

        // 1. Zehir sürme ses efekti çal (ITMPotionUse / 0x000B6435)
        auto* audioManager = RE::BSAudioManager::GetSingleton();
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (audioManager && dataHandler) {
            auto* soundDesc = dataHandler->LookupForm<RE::BGSSoundDescriptorForm>(0x000B6435, "Skyrim.esm");
            if (soundDesc) {
                RE::BSSoundHandle handle;
                if (audioManager->BuildSoundDataFromDescriptor(handle, soundDesc)) {
                    handle.SetPosition(follower->GetPosition());
                    handle.Play();
                }
            }
        }

        // 2. Sol ele zehir şişesi mesh'i bağla (NPC L Hand)
        // Eğer önceki bir şişe kalmışsa önce temizle
        StopPoisonWeaponAnimation(follower);

        auto* rootNode = follower->Get3D();
        if (rootNode) {
            auto* handNode = follower->GetNodeByName("NPC L Hand");
            if (!handNode) {
                handNode = rootNode->GetObjectByName("NPC L Hand");
            }

            if (handNode && handNode->AsNode()) {
                RE::NiPointer<RE::NiNode> bottleNode;
                RE::BSModelDB::DBTraits::ArgsType args;
                const char* modelPath = "clutter\\potions\\poisondamagehealth01.nif";

                if (RE::BSModelDB::Demand(modelPath, bottleNode, args) == RE::BSResource::ErrorCode::kNone && bottleNode) {
                    bottleNode->name = "FU_PoisonBottle";
                    // Sol el pozisyonu ve açısı
                    bottleNode->local.translate = RE::NiPoint3(-1.0f, -7.5f, 3.5f);
                    bottleNode->local.scale = 1.0f;

                    handNode->AsNode()->AttachChild(bottleNode.get());
                    RE::NiUpdateData updateData;
                    bottleNode->Update(updateData);

                    std::lock_guard<std::mutex> lock(_bottleMutex);
                    _attachedPoisonBottles[follower->GetFormID()] = bottleNode;
                }
            }
        }

        // 3. Animated Poisons Redone (APR):
        // APR_PoisonAnim: 60 = weapon/bottle, 61 = arrow/bolt
        // APR_PoisonSpeed: animasyon hızı çarpanı (1.0f)
        // Event: APR_PoisonStart
        follower->SetGraphVariableInt("APR_PoisonAnim", isRanged ? 61 : 60);
        follower->SetGraphVariableFloat("APR_PoisonSpeed", 1.0f);

        bool aprTriggered = follower->NotifyAnimationGraph("APR_PoisonStart");
        if (aprTriggered) {
            logger::info("AnimationManager: {} Animated Poisons Redone (APR_PoisonStart) animasyonu oynatıldı.",
                follower->GetName());
            return;
        }

        // Fallback: Standart OAR / Immersive Interactions / Vanilla poison events
        PlayFirstValidAnim(follower, { "poisonWeapon", "IdlePoison", "IdlePoisonWeapon" }, "zehir sürme");
    }

    void AnimationManager::StopPoisonWeaponAnimation(RE::Actor* follower)
    {
        if (!follower) return;

        // 1. Zehir şişesi mesh'ini elden kaldır
        {
            std::lock_guard<std::mutex> lock(_bottleMutex);
            auto it = _attachedPoisonBottles.find(follower->GetFormID());
            if (it != _attachedPoisonBottles.end()) {
                if (auto bottle = it->second) {
                    if (auto parent = bottle->parent) {
                        parent->DetachChild(bottle.get());
                        RE::NiUpdateData updateData;
                        parent->Update(updateData);
                    }
                }
                _attachedPoisonBottles.erase(it);
            }
        }

        if (follower->IsDead() || !follower->Is3DLoaded()) return;

        // 2. Animated Poisons Redone animasyonunu durdur ve değişkenleri sıfırla
        follower->NotifyAnimationGraph("APR_PoisonStop");
        follower->SetGraphVariableInt("APR_PoisonAnim", 0);
    }
}



