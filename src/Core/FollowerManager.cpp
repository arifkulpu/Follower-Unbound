#include "Core/FollowerManager.h"
#include "Core/ClassInterface.h"
#include "Core/Settings.h"
#include "Core/TNGCompatibility.h"
#include "AI/AnimationManager.h"
#include "AI/ConsumableAI.h"
#include "AI/ItemEvaluator.h"
#include "Economy/ShoppingAI.h"
#include "UI/NotificationManager.h"

namespace FollowerUnbound
{
    static bool HasNearbyCraftingStation(RE::Actor* actor, bool alchemy)
    {
        if (!actor || !actor->Is3DLoaded()) return false;
        auto* cell = actor->GetParentCell();
        if (!cell) return false;

        bool found = false;
        // Skyrim units: 400 units is roughly a small room's working distance.
        cell->ForEachReferenceInRange(actor->GetPosition(), 400.0f, [&](RE::TESObjectREFR* ref) {
            if (!ref) return RE::BSContainer::ForEachResult::kContinue;
            auto* base = ref->GetBaseObject();
            auto* keywords = base ? base->As<RE::BGSKeywordForm>() : nullptr;
            if (!keywords) return RE::BSContainer::ForEachResult::kContinue;

            // Use editor IDs so this also recognizes equivalent stations added by mods.
            const bool matches = alchemy
                ? (keywords->HasKeywordString("WICraftingAlchemy") || keywords->HasKeywordString("CraftingAlchemyWorkbench"))
                : (keywords->HasKeywordString("CraftingSmithingForge"));
            if (matches) {
                found = true;
                return RE::BSContainer::ForEachResult::kStop;
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        return found;
    }

    void FollowerManager::Initialize()
    {
        logger::info("FollowerManager initializing...");
    }

    // Returns true only when the actor is genuinely a player follower right now.
    // Checks both the teammate flag and the vanilla CurrentFollowerFaction (0x5C84E).
    static bool IsActualFollower(RE::Actor* actor)
    {
        if (!actor || actor->IsPlayerRef() || actor->IsDead()) return false;

        // Primary check: engine teammate flag (set by ALL follower frameworks + vanilla)
        if (actor->IsPlayerTeammate()) return true;

        // Secondary: vanilla CurrentFollowerFaction
        auto followerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84E);
        if (followerFaction && actor->IsInFaction(followerFaction)) return true;

        // Vanilla DialogueFollower quest (0x750BA) alias check
        auto followerQuest = RE::TESForm::LookupByID<RE::TESQuest>(0x000750BA);
        if (followerQuest) {
            for (auto alias : followerQuest->aliases) {
                auto refAlias = alias ? skyrim_cast<RE::BGSRefAlias*>(alias) : nullptr;
                if (refAlias) {
                    auto ref = refAlias->GetActorReference();
                    if (ref && ref->GetFormID() == actor->GetFormID()) return true;
                }
            }
        }

        // Animal / Pet / Dog Follower Factions (0x000282C9: AnimalFollowerFaction, 0x000282CC: CurrentAnimalFollowerFaction)
        auto animalFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x000282C9);
        if (animalFollowerFaction && actor->IsInFaction(animalFollowerFaction)) return true;

        auto currentAnimalFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x000282CC);
        if (currentAnimalFaction && actor->IsInFaction(currentAnimalFaction)) return true;

        // Player Horse check: Player owns or rides this horse
        auto playerHorseFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x00068DFA); // PlayerHorseFaction
        if (playerHorseFaction && actor->IsInFaction(playerHorseFaction)) return true;

        return false;
    }

    bool FollowerManager::IsAnimalActor(RE::Actor* actor)
    {
        if (!actor) return false;

        // 1. Explicit Animal / Creature keyword check on Actor
        if (actor->HasKeywordString("ActorTypeAnimal") || actor->HasKeywordString("ActorTypeCreature")) {
            return true;
        }

        // 2. Keyword check on ActorBase
        auto base = actor->GetActorBase();
        if (base) {
            if (base->HasKeywordString("ActorTypeAnimal") || base->HasKeywordString("ActorTypeCreature") ||
                base->HasKeywordID(0x00013795) || base->HasKeywordID(0x00013796)) {
                return true;
            }
        }

        auto race = actor->GetRace();
        if (!race) return false;

        // 3. Keyword check on Race
        if (race->HasKeywordString("ActorTypeAnimal") || race->HasKeywordString("ActorTypeCreature") ||
            race->HasKeywordID(0x00013795) || race->HasKeywordID(0x00013796)) {
            return true;
        }

        // 4. Animal race flags: CantOpenDoors flag is standard on animals/creatures
        if (race->data.flags.all(RE::RACE_DATA::Flag::kCantOpenDoors)) {
            return true;
        }

        // 5. Name & EditorID pattern matching
        std::string raceName = race->GetName();
        std::transform(raceName.begin(), raceName.end(), raceName.begin(), ::tolower);

        std::string raceEdID = race->GetFormEditorID();
        std::transform(raceEdID.begin(), raceEdID.end(), raceEdID.begin(), ::tolower);

        static const std::vector<std::string> kAnimalKeywords = {
            "cat", "dog", "horse", "wolf", "fox", "bear", "cow", "goat", "chicken",
            "skeever", "spider", "mudcrab", "troll", "sabrecat", "deer", "elk", "rabbit",
            "chaurus", "dragon", "creature", "canine", "feline", "animal", "boar", "horker",
            "mammoth", "slaughterfish", "husky", "deathhound"
        };

        for (const auto& kw : kAnimalKeywords) {
            if (!raceName.empty() && raceName.find(kw) != std::string::npos && raceName.find("khajiit") == std::string::npos) {
                return true;
            }
            if (!raceEdID.empty() && raceEdID.find(kw) != std::string::npos && raceEdID.find("khajiit") == std::string::npos) {
                return true;
            }
        }

        // Check ActorBase EditorID (e.g. ACLCatOwnedAnise)
        if (base) {
            std::string baseEdID = base->GetFormEditorID();
            std::transform(baseEdID.begin(), baseEdID.end(), baseEdID.begin(), ::tolower);
            for (const auto& kw : kAnimalKeywords) {
                if (!baseEdID.empty() && baseEdID.find(kw) != std::string::npos && baseEdID.find("khajiit") == std::string::npos) {
                    return true;
                }
            }
        }

        // 6. Humanoid check: FaceGenHead is present on all humanoid races (including Khajiit, Argonian, Elves, Men)
        bool hasFaceGen = race->data.flags.all(RE::RACE_DATA::Flag::kFaceGenHead);
        if (!hasFaceGen) {
            return true; // No FaceGen head -> animal / creature
        }

        return false;
    }

    bool FollowerManager::IsHumanoidRace(RE::Actor* actor)
    {
        return !IsAnimalActor(actor);
    }

    bool FollowerManager::RegisterFollower(RE::Actor* actor)
    {
        if (!actor || actor->IsPlayerRef()) return false;

        bool isAnimal = IsAnimalActor(actor);
        auto settings = Settings::GetSingleton();

        // İnsansı olmayan yaratıklar (kedi, köpek, at vb.):
        // Eğer Evcil Hayvan Sistemi (enablePetSystem) kapalıysa engelle.
        if (isAnimal && !settings->enablePetSystem) {
            logger::info("RegisterFollower: Skipped non-humanoid actor {} [{:08X}] (Pet system disabled)",
                actor->GetName(), actor->GetFormID());
            return false;
        }

        // Block anyone not currently a real follower
        if (!IsActualFollower(actor)) {
            return false;
        }

        std::unique_lock lock(_mutex);
        RE::FormID formID = actor->GetFormID();
        if (_profiles.contains(formID)) {
            return false;  // already tracked
        }

        std::string name = actor->GetName();
        if (name.empty()) name = isAnimal ? "Pet" : "Follower";

        FollowerProfile profile(formID, name);
        if (!isAnimal) {
            profile.role = ClassInterface::GetSingleton()->ResolveRole(actor);
            profile.gold = 300;
            profile.isAnimal = false;
            profile.classBoundDisplayName = ClassInterface::GetSingleton()->GetClassDisplayName(actor, profile.role);
        } else {
            profile.role = FollowerRole::None;
            profile.isAnimal = true;
            profile.gold = 0;
            profile.classBoundDisplayName = "Animal";
        }

        auto storedProfile = std::make_shared<FollowerProfile>(std::move(profile));
        auto [it, inserted] = _profiles.emplace(formID, std::move(storedProfile));

        // Kıyafet koruma ayarı açıksa: yeni kaydedilen takipçinin üzerindeki
        // kıyafet/zırh/silahlar hemen kişisel koruma listesine kaydedilir.
        // (Not: _mutex zaten kilitli — GetProfile çağrılamaz, iterator kullanılır.)
        if (inserted && !isAnimal && settings->protectEquippedClothing) {
            CaptureEquippedProtection(actor, *it->second);
            logger::info("CaptureEquippedProtection: {} -> {} item(s) protected",
                it->second->name, it->second->protectedEquipment.size());
        }

        logger::info("Registered {}: {} [FormID: {:08X}]", isAnimal ? "pet/animal" : "follower", name, formID);
        return true;
    }

    static bool IsRangedWeapon(RE::TESBoundObject* bound)
    {
        if (!bound) return false;
        if (auto* weapon = bound->As<RE::TESObjectWEAP>()) {
            auto type = weapon->GetWeaponType();
            return (type == RE::WEAPON_TYPE::kBow || type == RE::WEAPON_TYPE::kCrossbow || type == RE::WEAPON_TYPE::kStaff);
        }
        return false;
    }

    void FollowerManager::UpdateCombatWeaponSwitching(RE::Actor* actor, FollowerProfile& profile)
    {
        if (!actor || actor->IsDead() || profile.isAnimal || !actor->IsInCombat()) return;

        // Yardımcı silah tanımlanmamışsa sadece asli loadout güvenceye alınır
        if (profile.assignedSecondaryRight == 0 && profile.assignedSecondaryLeft == 0) {
            EnforceAssignedLoadout(actor, profile, true);
            return;
        }

        // Hedef aktörü bul
        auto* target = actor->GetActorRuntimeData().currentCombatTarget.get().get();
        if (!target) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && player->IsInCombat()) {
                target = player->GetActorRuntimeData().currentCombatTarget.get().get();
            }
        }
        if (!target || target->IsDead()) return;

        const float distance = actor->GetPosition().GetDistance(target->GetPosition());

        auto* primRight = RE::TESForm::LookupByID<RE::TESBoundObject>(profile.assignedPrimaryRight);
        auto* primLeft  = RE::TESForm::LookupByID<RE::TESBoundObject>(profile.assignedPrimaryLeft);
        auto* secRight  = RE::TESForm::LookupByID<RE::TESBoundObject>(profile.assignedSecondaryRight);
        auto* secLeft   = RE::TESForm::LookupByID<RE::TESBoundObject>(profile.assignedSecondaryLeft);

        const bool primHasRanged = IsRangedWeapon(primRight) || IsRangedWeapon(primLeft);
        const bool secHasRanged  = IsRangedWeapon(secRight)  || IsRangedWeapon(secLeft);

        // İki set arasında menzilli/yakın ayrımı yoksa silah değiştirmeye gerek yok
        if (primHasRanged == secHasRanged) return;

        // Hedef yakın (< 400 birim) -> Yakın dövüş silahına geç
        // Hedef uzak (> 650 birim) -> Menzilli silaha geç
        // 400-650 arası histerezis (tampon) bölgesidir, gereksiz el değiştirme yapılmaz
        bool wantSecondary = false;
        if (distance > 650.0f) {
            wantSecondary = secHasRanged; // Menzilli olan Yardımcı ise Yardımcıya geç
        } else if (distance < 400.0f) {
            wantSecondary = !secHasRanged; // Yakın dövüş olan Yardımcı ise Yardımcıya geç
        } else {
            return; // Tampon bölge
        }

        auto* currentRight = actor->GetEquippedObject(false);
        auto* currentLeft  = actor->GetEquippedObject(true);

        RE::FormID desiredRightID = wantSecondary ? profile.assignedSecondaryRight : profile.assignedPrimaryRight;
        RE::FormID desiredLeftID  = wantSecondary ? profile.assignedSecondaryLeft  : profile.assignedPrimaryLeft;

        bool needsSwitch = false;
        if (desiredRightID != 0 && (!currentRight || currentRight->GetFormID() != desiredRightID)) needsSwitch = true;
        if (desiredLeftID != 0 && (!currentLeft || currentLeft->GetFormID() != desiredLeftID)) needsSwitch = true;

        if (needsSwitch) {
            auto* equipMgr = RE::ActorEquipManager::GetSingleton();
            if (!equipMgr) return;
            auto inv = actor->GetInventory();

            auto* rightSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F42);
            auto* leftSlot  = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F43);

            if (desiredRightID != 0) {
                if (auto* obj = RE::TESForm::LookupByID<RE::TESBoundObject>(desiredRightID)) {
                    if (inv.find(obj) != inv.end()) equipMgr->EquipObject(actor, obj, nullptr, 1, rightSlot, false, false, false, true);
                }
            }
            if (desiredLeftID != 0) {
                if (auto* obj = RE::TESForm::LookupByID<RE::TESBoundObject>(desiredLeftID)) {
                    if (inv.find(obj) != inv.end()) equipMgr->EquipObject(actor, obj, nullptr, 1, leftSlot, false, false, false, true);
                }
            }
            logger::info("CombatWeaponSwitch: {} switched to {} loadout (distance={:.0f} to target {})",
                actor->GetName(), wantSecondary ? "Secondary" : "Primary", distance, target->GetName());
        }
    }

    void FollowerManager::EnforceAssignedLoadout(RE::Actor* actor, FollowerProfile& profile, bool forcePrimary)
    {
        if (!actor || actor->IsDead() || profile.isAnimal) return;
        auto* equipMgr = RE::ActorEquipManager::GetSingleton();
        if (!equipMgr) return;

        auto inv = actor->GetInventory();
        auto tryEquipForm = [&](RE::FormID formID, RE::BGSEquipSlot* slot = nullptr) {
            if (formID == 0) return;
            auto* bound = RE::TESForm::LookupByID<RE::TESBoundObject>(formID);
            if (!bound) return;
            auto it = inv.find(bound);
            if (it != inv.end() && it->second.first > 0) {
                equipMgr->EquipObject(actor, bound, nullptr, 1, slot, false, false, false, true);
            }
        };

        // Zırhlar
        tryEquipForm(profile.assignedHead);
        tryEquipForm(profile.assignedBody);
        tryEquipForm(profile.assignedHands);
        tryEquipForm(profile.assignedFeet);

        // Eğer savaşta ise ve dinamik geçiş geçerliyse, silah kontrolünü UpdateCombatWeaponSwitching'e bırak
        if (actor->IsInCombat() && !forcePrimary && (profile.assignedSecondaryRight != 0 || profile.assignedSecondaryLeft != 0)) {
            UpdateCombatWeaponSwitching(actor, profile);
            return;
        }

        // Silahlar (Öncelikle Asli Silahlar)
        auto* rightSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F42);
        auto* leftSlot  = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F43);

        if (profile.assignedPrimaryRight != 0) {
            tryEquipForm(profile.assignedPrimaryRight, rightSlot);
        }
        if (profile.assignedPrimaryLeft != 0) {
            tryEquipForm(profile.assignedPrimaryLeft, leftSlot);
        }
    }

    void FollowerManager::RestoreSavedProfile(FollowerProfile profile)
    {
        const auto targetFormID = profile.formID;
        if (targetFormID == 0) return;
        if (profile.name.empty()) profile.name = "Follower";

        std::unique_lock lock(_mutex);
        auto it = _profiles.find(targetFormID);
        if (it != _profiles.end() && it->second) {
            if (profile.classBoundDisplayName.empty()) profile.classBoundDisplayName = it->second->classBoundDisplayName;
            profile.isAnimal = it->second->isAnimal;
            if (!it->second->name.empty() && it->second->name != "Follower") profile.name = it->second->name;
            *it->second = std::move(profile);
            if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(targetFormID)) {
                EnforceAssignedLoadout(actor, *it->second);
            }
        } else {
            auto sharedProf = std::make_shared<FollowerProfile>(std::move(profile));
            _profiles.emplace(targetFormID, sharedProf);
            if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(targetFormID)) {
                EnforceAssignedLoadout(actor, *sharedProf);
            }
        }
    }

    void FollowerManager::CaptureEquippedProtection(RE::Actor* actor, FollowerProfile& profile)
    {
        if (!actor || actor->IsDead() || profile.isAnimal) return;

        // TNG's internal body-coverage form is always protected, but is hidden
        // from the player's normal apparel protection list.
        auto protectCurrentItem = [&](RE::TESForm* form, std::string_view slot) {
            if (!form) return;
            const RE::FormID itemID = form->GetFormID();
            const bool alreadyProtected = profile.IsProtected(itemID);
            profile.ProtectItem(itemID);
            if (!alreadyProtected) {
                logger::info("Protection: {} captured {} [{} / {:08X}].",
                    profile.name, form->GetName(), slot, itemID);
            }
        };

        if (auto* tngBlock = GetTngHiddenBodyItem()) {
            protectCurrentItem(tngBlock, "TNG hidden body item");
        }

        // Sağ el / sol el (silah, kalkan, meşale vb.)
        if (auto rightHand = actor->GetEquippedObject(false)) {
            protectCurrentItem(rightHand, "right hand");
        }
        if (auto leftHand = actor->GetEquippedObject(true)) {
            protectCurrentItem(leftHand, "left hand");
        }

        // Üzerinde giyili tüm kıyafet/zırh parçaları (gövde, kafa, eldiven, ayak vb.)
        auto inventory = actor->GetInventory();
        for (const auto& [item, invData] : inventory) {
            if (invData.first <= 0 || !item) continue;
            if (invData.second && invData.second->extraLists) {
                for (auto* xList : *invData.second->extraLists) {
                    if (xList && (xList->HasType(RE::ExtraDataType::kWorn) ||
                                  xList->HasType(RE::ExtraDataType::kWornLeft))) {
                        protectCurrentItem(item, "worn apparel/armor");
                        break;
                    }
                }
            }
        }
    }

    void FollowerManager::UnregisterFollower(RE::FormID formID)
    {
        std::unique_lock lock(_mutex);
        if (auto it = _profiles.find(formID); it != _profiles.end()) {
            logger::info("Unregistered follower: {} [FormID: {:08X}]", it->second->name, formID);
            _profiles.erase(it);
        }
    }

    bool FollowerManager::IsFollower(RE::FormID formID) const
    {
        std::shared_lock lock(_mutex);
        return _profiles.contains(formID);
    }

    std::shared_ptr<FollowerProfile> FollowerManager::GetProfile(RE::FormID formID)
    {
        std::shared_lock lock(_mutex);
        auto it = _profiles.find(formID);
        if (it != _profiles.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::unordered_map<RE::FormID, FollowerProfile> FollowerManager::GetProfilesSnapshot() const
    {
        std::shared_lock lock(_mutex);
        std::unordered_map<RE::FormID, FollowerProfile> snapshot;
        snapshot.reserve(_profiles.size());
        for (const auto& [formID, profile] : _profiles) {
            if (profile) snapshot.emplace(formID, *profile);
        }
        return snapshot;
    }

    void FollowerManager::RefreshFollowerRoles()
    {
        std::unique_lock lock(_mutex);
        auto classIntfc = ClassInterface::GetSingleton();
        for (auto& [formID, profile] : _profiles) {
            if (!profile) continue;
            auto& data = *profile;
            if (data.isAnimal) {
                data.role = FollowerRole::None;
                data.classBoundDisplayName = "Animal";
                continue;
            }
            auto form = RE::TESForm::LookupByID(formID);
            if (form) {
                auto actor = form->As<RE::Actor>();
                if (actor) {
                    data.role = classIntfc->ResolveRole(actor);
                    data.classBoundDisplayName = classIntfc->GetClassDisplayName(actor, data.role);
                }
            }
        }
    }

    std::vector<RE::Actor*> FollowerManager::GetActiveFollowers()
    {
        std::vector<RE::Actor*> followers;
        std::shared_lock lock(_mutex);

        for (const auto& [formID, profile] : _profiles) {
            auto form = RE::TESForm::LookupByID(formID);
            if (!form) continue;
            auto actor = form->As<RE::Actor>();
            if (actor && !actor->IsDead() && actor->Is3DLoaded()) {
                followers.push_back(actor);
            }
        }

        return followers;
    }

    void FollowerManager::ScanForActiveFollowers()
    {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        // ── STEP 1: Discover and register genuine followers ──
        // Only IsPlayerTeammate() or CurrentFollowerFaction membership qualify.
        // Faction-name-based guessing is disabled — it caused false positives like
        // Companions guild members (Farkas, Eorlund, etc.) being registered.

        auto settings = Settings::GetSingleton();
        auto processLists = RE::ProcessLists::GetSingleton();
        if (processLists) {
            for (auto& handle : processLists->highActorHandles) {
                auto actorPtr = handle.get();
                if (!actorPtr) continue;
                auto actor = actorPtr.get();
                if (!actor || actor->IsPlayerRef() || actor->IsDead()) continue;

                if (IsActualFollower(actor)) {
                    bool isAnimal = IsAnimalActor(actor);
                    if (!isAnimal || settings->enablePetSystem) {
                        RegisterFollower(actor);
                    }
                }
            }
        }

        // Also check current cell references (nearby actors not in ProcessLists high tier)
        auto* cell = player->GetParentCell();
        if (cell) {
            for (auto& ref : cell->GetRuntimeData().references) {
                if (!ref || !ref->Is(RE::FormType::ActorCharacter)) continue;
                auto* actor = ref->As<RE::Actor>();
                if (!actor || actor->IsPlayerRef() || actor->IsDead()) continue;
                if (IsActualFollower(actor)) {
                    bool isAnimal = IsAnimalActor(actor);
                    if (!isAnimal || settings->enablePetSystem) {
                        RegisterFollower(actor);
                    }
                }
            }
        }

        // ── STEP 2: Housekeeping — unregister actors who are no longer followers ──
        // Collect stale FormIDs outside the lock first to avoid deadlock inside RegisterFollower.
        std::vector<RE::FormID> toRemove;
        {
            std::unique_lock lock(_mutex);
            for (auto& [formID, profile] : _profiles) {
                if (!profile) continue;
                auto& data = *profile;
                auto form = RE::TESForm::LookupByID(formID);
                auto actor = form ? form->As<RE::Actor>() : nullptr;

                // Dinamik kontrol: Hayvan durumunu doğrula (önceki oturumlardan kalma hatalı kayıtları düzelt)
                if (actor) {
                    if (data.name.empty() || data.name == "Follower") {
                        data.name = actor->GetName();
                    }
                    bool isAnimal = IsAnimalActor(actor);
                    data.isAnimal = isAnimal;
                    if (isAnimal) {
                        data.role = FollowerRole::None;
                        data.gold = 0;
                        data.classBoundDisplayName = "Animal";
                    }
                }

                // Remove if: actor gone, dead, or no longer a real follower
                // Veya hayvan profili var ama enablePetSystem kapatılmış
                if (!actor || actor->IsDead() || !IsActualFollower(actor) || (data.isAnimal && !settings->enablePetSystem)) {
                    toRemove.push_back(formID);
                }
            }
        }
        for (auto fid : toRemove) {
            UnregisterFollower(fid);
        }

        // Refresh roles for all remaining tracked followers
        RefreshFollowerRoles();

        // Takipçilerin oyuncunun atadığı asli loadout'u taşıdığını doğrula
        {
            std::shared_lock lock(_mutex);
            for (const auto& [fid, prof] : _profiles) {
                if (!prof || prof->isAnimal) continue;
                if (auto* fActor = RE::TESForm::LookupByID<RE::Actor>(fid)) {
                    if (fActor->Is3DLoaded() && !fActor->IsDead()) {
                        EnforceAssignedLoadout(fActor, *prof);
                    }
                }
            }
        }
    }

    void FollowerManager::Update(float deltaGameHours)
    {
        _timeSinceLastScan += deltaGameHours;
        if (_timeSinceLastScan >= 0.02f) {
            _timeSinceLastScan = 0.0f;
            ScanForActiveFollowers();
        }

        // Not: gerçek ihtiyaç tick'i UpdateGameTimeNeeds()'de yapılıyor
        // Update() sadece scan için kullanılıyor (frame hook kaldırıldı)
    }

    // Oyun yüklendikten hemen sonra çağrılır — _lastGameTimeDays'i başlatır.
    // Bu olmadan ilk Wait/Sleep olayında delta = currentDays - 0 hesaplanır
    // ve ilk çağrı "sadece zamanı kaydet" dalından döner (ihtiyaçlar azalmaz).
    void FollowerManager::InitGameTime()
    {
        auto calendar = RE::Calendar::GetSingleton();
        if (!calendar) return;
        _lastGameTimeDays = calendar->GetDaysPassed();
        logger::info("InitGameTime: zaman başlatıldı ({:.4f} oyun günü)", _lastGameTimeDays);
    }

    void FollowerManager::UpdateGameTimeNeeds()
    {
        auto calendar = RE::Calendar::GetSingleton();
        if (!calendar) return;

        float currentDays = calendar->GetDaysPassed();

        // _lastGameTimeDays henüz başlatılmamışsa (InitGameTime çağrılmadı)
        // sadece zamanı kaydet, tick yapma.
        if (_lastGameTimeDays <= 0.0f) {
            _lastGameTimeDays = currentDays;
            logger::warn("UpdateGameTimeNeeds: ilk çağrı — zaman sıfırdan başlatıldı ({:.4f})", currentDays);
            return;
        }

        float deltaDays = currentDays - _lastGameTimeDays;
        if (deltaDays <= 0.0f) return;   // Zaman geçmemiş, hiçbir şey yapma

        _lastGameTimeDays = currentDays;
        float deltaHours = deltaDays * 24.0f;

        // Settings'den tüketim hızlarını al
        auto settings   = Settings::GetSingleton();
        float hungerRate  = settings->HungerRatePerHour();
        float thirstRate  = settings->ThirstRatePerHour();
        float fatigueRate = settings->FatigueRatePerHour();

        logger::debug("UpdateGameTimeNeeds: {:.2f} oyun saati geçti | hız H:{:.2f} T:{:.2f} F:{:.2f}",
            deltaHours, hungerRate, thirstRate, fatigueRate);

        auto consumableAI = ConsumableAI::GetSingleton();
        auto shoppingAI   = ShoppingAI::GetSingleton();

        // Önce tüm profilleri tick'le (kısa, mutex kilitli)
        struct FollowerNeedEntry
        {
            RE::FormID formID;
            float urgencyScore; // Daha küçük puan = daha acil ihtiyaç (en düşük min(hunger, thirst))
        };
        std::vector<FollowerNeedEntry> followerQueue;

        {
            std::unique_lock lock(_mutex);
            for (auto& [formID, profile] : _profiles) {
                if (!profile) continue;
                auto& data = *profile;
                float oldHunger  = data.needs.hunger;
                float oldThirst  = data.needs.thirst;
                float oldFatigue = data.needs.fatigue;

                data.needs.Tick(deltaHours, hungerRate, thirstRate, fatigueRate);

                logger::debug("  [{}] Açlık: {:.0f}→{:.0f}  Susuzluk: {:.0f}→{:.0f}  Yorgunluk: {:.0f}→{:.0f}",
                    data.name,
                    oldHunger,  data.needs.hunger,
                    oldThirst,  data.needs.thirst,
                    oldFatigue, data.needs.fatigue);

                float urgency = std::min(data.needs.hunger, data.needs.thirst);
                followerQueue.push_back({ formID, urgency });
            }
        }

        // İhtiyacı en kritik olan (açlık veya susuzluğu en düşük olan) takipçi İLK sıraya geçer
        std::sort(followerQueue.begin(), followerQueue.end(), [](const FollowerNeedEntry& a, const FollowerNeedEntry& b) {
            return a.urgencyScore < b.urgencyScore;
        });

        // Mutex serbest bırakıldıktan sonra AI'ları tetikle (en acil olandan başlayarak)
        for (const auto& entry : followerQueue) {
            auto form = RE::TESForm::LookupByID(entry.formID);
            if (!form) continue;
            auto actor = form->As<RE::Actor>();
            if (!actor || actor->IsDead() || !actor->Is3DLoaded()) continue;

            // Profil pointer'ı kısa süreli lock ile al
            auto profile = GetProfile(entry.formID);
            if (!profile) continue;

            consumableAI->UpdateNeedsConsumption(actor, *profile);

            // Hayvanlar alışveriş yapmaz veya eşya satmaz
            if (!profile->isAnimal) {
                CheckAutonomousCrafting(actor, *profile);
                shoppingAI->ProcessSelling(actor, *profile);
                shoppingAI->ProcessShopping(actor, *profile);
            }
        }
    }

    void FollowerManager::TriggerFollowerConsumption()
    {
        // Diyalog sırasında Scaleform çökmesi veya animasyon hatası olmaması için kontrol
        auto ui = RE::UI::GetSingleton();
        if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) return;

        auto consumableAI = ConsumableAI::GetSingleton();
        auto shoppingAI = ShoppingAI::GetSingleton();

        static auto lastTradeCheckTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        bool canTradeTick = (std::chrono::duration<float>(now - lastTradeCheckTime).count() >= 2.0f);
        if (canTradeTick) {
            lastTradeCheckTime = now;
        }

        struct FollowerNeedEntry
        {
            RE::FormID formID;
            float urgencyScore;
        };
        std::vector<FollowerNeedEntry> followerQueue;

        {
            std::shared_lock lock(_mutex);
            for (const auto& [formID, profile] : _profiles) {
                if (!profile) continue;
                float urgency = std::min(profile->needs.hunger, profile->needs.thirst);
                followerQueue.push_back({ formID, urgency });
            }
        }

        // İhtiyacı en kritik olan takipçi ilk tüketimi yapar
        std::sort(followerQueue.begin(), followerQueue.end(), [](const FollowerNeedEntry& a, const FollowerNeedEntry& b) {
            return a.urgencyScore < b.urgencyScore;
        });

        for (const auto& entry : followerQueue) {
            auto form = RE::TESForm::LookupByID(entry.formID);
            if (!form) continue;
            auto actor = form->As<RE::Actor>();
            if (!actor || actor->IsDead() || !actor->Is3DLoaded()) continue;

            auto profile = GetProfile(entry.formID);
            if (!profile) continue;

            if (actor->IsInCombat()) {
                if (!profile->isAnimal) {
                    consumableAI->UpdateCombatConsumables(actor, *profile);
                }
            } else {
                consumableAI->UpdateNeedsConsumption(actor, *profile);

                // Hayvan olmayan takipçiler savaş dışındayken silahlarını zehirleyip savaşa hazır bekler
                if (!profile->isAnimal) {
                    consumableAI->UpdateOutOfCombatPoison(actor, *profile);
                }

                // Hayvan olmayan takipçiler yakında satıcı varsa düzenli satış ve alışveriş yapar
                if (!profile->isAnimal && canTradeTick) {
                    // 1. Önce bilmediği büyü kitaplarını oku ve öğren (satıcının eline düşmesin!)
                    CheckSpellTomeLearning(actor, *profile);
                    // 2. Kendi malzemelerini kullanarak üretim yap
                    CheckAutonomousCrafting(actor, *profile);
                    // 3. Fazla eşyaları ve zaten bildiği kopya büyü kitaplarını sat
                    shoppingAI->ProcessSelling(actor, *profile);
                    // 4. İhtiyaçlarını satın al
                    shoppingAI->ProcessShopping(actor, *profile);
                }
            }
        }
    }

    void FollowerManager::CheckAutonomousCrafting(RE::Actor* actor, FollowerProfile& profile)
    {
        if (!actor || actor->IsDead() || actor->IsInCombat() || profile.isAnimal) return;
        if (!profile.preferences.allowCraftPotions && !profile.preferences.allowCraftArrows &&
            !profile.preferences.allowCraftBolts && !profile.preferences.allowCraftTorches) return;

        const bool nearForge = (profile.preferences.allowCraftArrows || profile.preferences.allowCraftBolts || profile.preferences.allowCraftTorches)
            ? HasNearbyCraftingStation(actor, false) : false;
        const bool nearAlchemy = profile.preferences.allowCraftPotions
            ? HasNearbyCraftingStation(actor, true) : false;

        // Ok veya arbalet kullanıcısı mı ya da cephane üretimi açık mı?
        bool isRangedClass = (profile.role == FollowerRole::Archer ||
                              profile.role == FollowerRole::Crossbowman);

        auto evaluator = ItemEvaluator::GetSingleton();
        auto notifier = NotificationManager::GetSingleton();

        // Malzeme FormID'leri:
        // Firewood: 0x0006F993
        // Iron Ingot: 0x0005ACE4
        // Steel Ingot: 0x0005ACE5
        static auto* firewoodObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0006F993);
        static auto* ironIngotObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0005ACE4);
        static auto* steelIngotObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0005ACE5);
        static auto* linenWrapObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00034CD6);
        static auto* torchObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0001D4EC);
        static auto* blueFlowerObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00077E1C);
        static auto* wheatObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0004B0BA);
        static auto* redFlowerObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00077E1D);
        static auto* whiteCapObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0004DA22);
        static auto* purpleFlowerObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00077E1E);
        static auto* pineThrushEggObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00023D6F);
        static auto* healingPotionObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0003EADE);
        static auto* magickaPotionObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0003EAE1);
        static auto* staminaPotionObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00039BE8);

        // Çıktı FormID'leri:
        // Iron Arrow: 0x0001397D (1 Firewood + 1 Iron Ingot -> 24 Arrows)
        // Steel Arrow: 0x0001397E (1 Firewood + 1 Steel Ingot -> 24 Arrows)
        // Steel Bolt: 0x02000BB3 (Dawnguard) (1 Firewood + 1 Steel Ingot -> 10 Bolts)
        static auto* ironArrowObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0001397D);
        static auto* steelArrowObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0001397E);

        // Dawnguard çelik cıvata (Steel Bolt) lookup
        static RE::TESBoundObject* steelBoltObj = []() -> RE::TESBoundObject* {
            if (auto dataHandler = RE::TESDataHandler::GetSingleton()) {
                return dataHandler->LookupForm<RE::TESBoundObject>(0x00000BB3, "Dawnguard.esm");
            }
            return nullptr;
        }();

        // Use simple vanilla recipes that share the restorative effect:
        // Blue Mountain Flower + Wheat, Red Mountain Flower + White Cap,
        // and Purple Mountain Flower + Pine Thrush Egg. Craft one potion per
        // update, respecting each follower's existing potion-stock target.
        if (profile.preferences.allowCraftPotions && nearAlchemy) {
            struct PotionRecipe { RE::TESBoundObject* first; RE::TESBoundObject* second; RE::TESBoundObject* result; int32_t stock; uint32_t target; const char* label; };
            const auto stock = ShoppingAI::GetSupplyCounts(actor);
            const PotionRecipe recipes[] = {
                { blueFlowerObj, wheatObj, healingPotionObj, stock.healthStandard, profile.preferences.maxHealthStandard, "Healing Potion" },
                { redFlowerObj, whiteCapObj, magickaPotionObj, stock.magickaStandard, profile.preferences.maxMagickaStandard, "Magicka Potion" },
                { purpleFlowerObj, pineThrushEggObj, staminaPotionObj, stock.staminaStandard, profile.preferences.maxStaminaStandard, "Stamina Potion" }
            };
            for (const auto& recipe : recipes) {
                if (!recipe.first || !recipe.second || !recipe.result || recipe.stock >= static_cast<int32_t>(recipe.target)) continue;
                if (evaluator->GetItemCount(actor, recipe.first) <= 0 || evaluator->GetItemCount(actor, recipe.second) <= 0) continue;
                actor->RemoveItem(recipe.first, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                actor->RemoveItem(recipe.second, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                actor->AddObjectToContainer(recipe.result, nullptr, 1, nullptr);
                if (Settings::GetSingleton()->notifySelling) {
                    notifier->ShowHUDMessage(std::format("{} crafted a {}.", actor->GetName(), recipe.label));
                }
                logger::info("Autonomous Crafting: {} crafted {}; potion stock {}/{}.", actor->GetName(), recipe.label, recipe.stock + 1, recipe.target);
                return;
            }
        }

        // 1. ARBALET OKU (BOLT) ÜRETİMİ
        bool wantsBolts = profile.preferences.allowCraftBolts;
        if (wantsBolts && nearForge && steelBoltObj) {
            uint32_t currentBolts = evaluator->CountBolts(actor);
            if (currentBolts < profile.preferences.maxBolts) {
                int32_t firewoodCount = evaluator->GetItemCount(actor, firewoodObj);
                int32_t steelCount = evaluator->GetItemCount(actor, steelIngotObj);

                if (firewoodCount >= 1 && steelCount >= 1) {
                    actor->RemoveItem(firewoodObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                    actor->RemoveItem(steelIngotObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                    actor->AddObjectToContainer(steelBoltObj, nullptr, 10, nullptr);

                    if (Settings::GetSingleton()->notifySelling) {
                        notifier->ShowHUDMessage(std::format("{} crafted 10x Steel Bolts.", actor->GetName()));
                    }
                    logger::info("Autonomous Crafting: {} crafted 10x Steel Bolts.", actor->GetName());
                    return;
                }
            }
        }

        // 2. YAY OKU (ARROW) ÜRETİMİ
        bool wantsArrows = profile.preferences.allowCraftArrows &&
                           (isRangedClass || profile.preferences.buyArrows);
        if (wantsArrows && nearForge) {
            uint32_t currentArrows = evaluator->CountArrows(actor);
            if (currentArrows < profile.preferences.maxArrows) {
                int32_t firewoodCount = evaluator->GetItemCount(actor, firewoodObj);
                int32_t steelCount = evaluator->GetItemCount(actor, steelIngotObj);
                int32_t ironCount = evaluator->GetItemCount(actor, ironIngotObj);

                if (firewoodCount >= 1 && steelCount >= 1 && steelArrowObj) {
                    actor->RemoveItem(firewoodObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                    actor->RemoveItem(steelIngotObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                    actor->AddObjectToContainer(steelArrowObj, nullptr, 24, nullptr);

                    if (Settings::GetSingleton()->notifySelling) {
                        notifier->ShowHUDMessage(std::format("{} crafted 24x Steel Arrows.", actor->GetName()));
                    }
                    logger::info("Autonomous Crafting: {} crafted 24x Steel Arrows.", actor->GetName());
                    return;
                } else if (firewoodCount >= 1 && ironCount >= 1 && ironArrowObj) {
                    actor->RemoveItem(firewoodObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                    actor->RemoveItem(ironIngotObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                    actor->AddObjectToContainer(ironArrowObj, nullptr, 24, nullptr);

                    if (Settings::GetSingleton()->notifySelling) {
                        notifier->ShowHUDMessage(std::format("{} crafted 24x Iron Arrows.", actor->GetName()));
                    }
                    logger::info("Autonomous Crafting: {} crafted 24x Iron Arrows.", actor->GetName());
                    return;
                }
            }
        }

        // 3. TORCHES: one firewood + one linen wrap produces one vanilla torch.
        // Add only the missing amount, so the configured stock limit is never exceeded.
        if (profile.preferences.allowCraftTorches && nearForge && torchObj && linenWrapObj && firewoodObj) {
            const auto currentTorches = static_cast<uint32_t>(std::max(0, ShoppingAI::GetSupplyCounts(actor).torches));
            const auto targetTorches = profile.preferences.maxTorches;
            if (currentTorches < targetTorches &&
                evaluator->GetItemCount(actor, firewoodObj) > 0 &&
                evaluator->GetItemCount(actor, linenWrapObj) > 0) {
                actor->RemoveItem(firewoodObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                actor->RemoveItem(linenWrapObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                actor->AddObjectToContainer(torchObj, nullptr, 1, nullptr);
                if (Settings::GetSingleton()->notifySelling) {
                    notifier->ShowHUDMessage(std::format("{} crafted a torch ({}/{}).", actor->GetName(), currentTorches + 1, targetTorches));
                }
                logger::info("Autonomous Crafting: {} crafted a torch ({}/{}).", actor->GetName(), currentTorches + 1, targetTorches);
            }
        }
    }

    void FollowerManager::OnSleepStop()
    {
        // Oyuncu uyudu/kalktı → tüm takipçiler de uyudu sayılır
        // Önce gerçek oyun zamanını tick'le (uyku süresini yansıt)
        UpdateGameTimeNeeds();

        auto animMgr = AnimationManager::GetSingleton();

        std::unique_lock lock(_mutex);
        for (auto& [formID, profile] : _profiles) {
            if (!profile) continue;
            // Yorgunluğu tam doldur
            profile->needs.RestoreFromSleep();

            // Takipçiye uyku sonrası animasyonu oynat
            auto form = RE::TESForm::LookupByID(formID);
            if (form) {
                auto actor = form->As<RE::Actor>();
                if (actor && !actor->IsDead()) {
                    animMgr->PlaySleepRestoreAnimation(actor);
                }
            }
        }
        logger::info("OnSleepStop: {} follower(s) rested and restored.", _profiles.size());
    }


    static bool IsInSafeLocation(RE::Actor* actor)
    {
        if (!actor) return false;
        auto* cell = actor->GetParentCell();
        if (!cell) return false;

        auto* loc = cell->GetLocation();
        auto* kwLoc = loc ? loc->As<RE::BGSKeywordForm>() : nullptr;

        // 1. İç mekan kontrolü (Hanlar, Evler, Şehir binaları)
        if (cell->IsInteriorCell()) {
            if (kwLoc) {
                if (kwLoc->HasKeywordString("LocTypeDungeon") || kwLoc->HasKeywordString("LocSetDungeon")) {
                    return false;
                }
                if (kwLoc->HasKeywordString("LocTypeInn") || kwLoc->HasKeywordString("LocTypePlayerHouse") ||
                    kwLoc->HasKeywordString("LocTypeHouse") || kwLoc->HasKeywordString("LocTypeDwelling") ||
                    kwLoc->HasKeywordString("LocTypeStore") || kwLoc->HasKeywordString("LocTypeGuild") ||
                    kwLoc->HasKeywordString("LocTypeTemple") || kwLoc->HasKeywordString("LocTypeCastle")) {
                    return true;
                }
            }
            // Zindan değilse iç mekanlar (oyuncu evi, han vb.) güvenli sayılır
            return true;
        }

        // 2. Dış mekan / Şehir / Kasaba kontrolü
        if (kwLoc) {
            if (kwLoc->HasKeywordString("LocTypeCity") || kwLoc->HasKeywordString("LocTypeTown") ||
                kwLoc->HasKeywordString("LocTypeSettlement") || kwLoc->HasKeywordString("LocTypeHabitation")) {
                return true;
            }
        }

        return false;
    }

    void FollowerManager::CheckSpellTomeLearning(RE::Actor* actor, FollowerProfile& profile)
    {
        if (!actor || actor->IsDead() || actor->IsInCombat() || profile.isAnimal) return;
        auto* settings = Settings::GetSingleton();
        if (!settings->enableSpellTomeLearning) return;

        // Güvenli ortam kontrolü: Han, Oyuncu Evi, Şehir/Kasaba
        if (!IsInSafeLocation(actor)) return;

        auto now = std::chrono::steady_clock::now();
        const auto actorID = actor->GetFormID();

        // Eğer şu anda kitap okuyorsa süresini kontrol et (4 saniye boyunca okur)
        if (auto it = _readingActiveFollowers.find(actorID); it != _readingActiveFollowers.end()) {
            float elapsed = std::chrono::duration<float>(now - it->second).count();
            if (elapsed < 4.0f) {
                return; // Kitap okuma animasyonu hala devam ediyor
            } else {
                // Süre bitti: animasyonu kapat
                AnimationManager::GetSingleton()->StopReadAnimation(actor);
                _readingActiveFollowers.erase(it);
                return;
            }
        }

        static std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> s_lastReadTime;
        if (auto it = s_lastReadTime.find(actorID); it != s_lastReadTime.end()) {
            if (std::chrono::duration<float>(now - it->second).count() < 15.0f) {
                return; // Kitaplar arasında makul bekleme süresi (15 sn)
            }
        }

        auto* animMgr = AnimationManager::GetSingleton();
        auto* notifMgr = NotificationManager::GetSingleton();

        for (const auto& [item, invData] : actor->GetInventory()) {
            if (!item || invData.first <= 0) continue;
            auto* book = item->As<RE::TESObjectBOOK>();
            if (!book) continue;

            // Büyü kitabı mı (Spell Tome)? Büyü öğretmeli
            auto* spell = book->data.teaches.spell;
            if (!spell) continue;

            // Takipçi bu büyüyü zaten biliyor mu?
            if (actor->HasSpell(spell)) continue;

            // Oturuyor mu kontrol et
            auto* actorState = actor->AsActorState();
            const bool isSitting = actorState && (actorState->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal);

            // Okuma animasyonu başlat (Immersive Interactions uyumlu)
            animMgr->PlayReadAnimation(actor, isSitting);

            // Büyüyü öğren
            actor->AddSpell(spell);

            // Kitabı envanterden tüket (1 adet)
            actor->RemoveItem(book, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

            s_lastReadTime[actorID] = now;
            _readingActiveFollowers[actorID] = now; // 4 saniye boyunca okumada tut

            const char* spellName = spell->GetName() ? spell->GetName() : "Spell";
            const char* bookName = book->GetName() ? book->GetName() : "Spell Tome";

            if (settings->notifySpellLearned) {
                std::string msg = std::format("{} read {} and learned {}!", actor->GetName(), bookName, spellName);
                notifMgr->ShowHUDMessage(msg);
            }
            logger::info("SpellTomeLearning: {} read '{}' [{:08X}] and learned spell '{}' [{:08X}].",
                actor->GetName(), bookName, book->GetFormID(), spellName, spell->GetFormID());

            break; // Seans başına 1 kitap okur
        }
    }

    void FollowerManager::Clear()
    {
        std::unique_lock lock(_mutex);
        _profiles.clear();
    }
}
