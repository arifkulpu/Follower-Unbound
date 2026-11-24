#include "Core/FollowerManager.h"
#include "Core/EventManager.h"
#include "AI/MountManager.h"
#include "Core/ClassInterface.h"
#include "Core/Settings.h"
#include "Core/TNGCompatibility.h"
#include "AI/AnimationManager.h"
#include "AI/ConsumableAI.h"
#include "AI/ItemEvaluator.h"
#include "AI/TradeManager.h"
#include "Economy/ShoppingAI.h"
#include "UI/NotificationManager.h"
#include "UI/EquipPrompt.h"

namespace FollowerUnbound
{
    enum class CraftStationType {
        Forge,
        Alchemy,
        Cooking
    };

    static RE::TESObjectREFR* FindNearbyCraftingStation(RE::Actor* actor, CraftStationType type)
    {
        if (!actor || !actor->Is3DLoaded()) return nullptr;
        auto* cell = actor->GetParentCell();
        if (!cell) return nullptr;

        RE::TESObjectREFR* foundRef = nullptr;
        // Skyrim units: 400 units is roughly a small room's working distance.
        cell->ForEachReferenceInRange(actor->GetPosition(), 400.0f, [&](RE::TESObjectREFR* ref) {
            if (!ref) return RE::BSContainer::ForEachResult::kContinue;
            auto* base = ref->GetBaseObject();
            auto* keywords = base ? base->As<RE::BGSKeywordForm>() : nullptr;
            if (!keywords) return RE::BSContainer::ForEachResult::kContinue;

            // Use editor IDs so this also recognizes equivalent stations added by mods.
            bool matches = false;
            switch (type) {
            case CraftStationType::Alchemy:
                matches = keywords->HasKeywordString("WICraftingAlchemy") || keywords->HasKeywordString("CraftingAlchemyWorkbench");
                break;
            case CraftStationType::Cooking:
                matches = keywords->HasKeywordString("CraftingCookpot") || keywords->HasKeywordString("CraftTypeCookpot") ||
                          keywords->HasKeywordString("isCookingSpit") || keywords->HasKeywordString("isSmallCookingPot");
                break;
            case CraftStationType::Forge:
            default:
                matches = keywords->HasKeywordString("CraftingSmithingForge");
                break;
            }

            if (matches) {
                foundRef = ref;
                return RE::BSContainer::ForEachResult::kStop;
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        return foundRef;
    }

    static bool HasNearbyCraftingStation(RE::Actor* actor, CraftStationType type)
    {
        return FindNearbyCraftingStation(actor, type) != nullptr;
    }

    void FollowerManager::Initialize()
    {
        logger::info("FollowerManager initializing...");
    }

    // Returns true only when the actor is genuinely a player follower right now.
    // Checks both the teammate flag and the vanilla CurrentFollowerFaction (0x5C84E).
    // Returns true only when the actor is genuinely a player follower right now.
    // Checks both the teammate flag, vanilla CurrentFollowerFaction, and popular follower frameworks (NFF, EFF, AFT).
    static bool IsActualFollower(RE::Actor* actor)
    {
        if (!actor || actor->IsPlayerRef() || actor->IsDead()) return false;

        // Geçici görev refakatçileri (Escort NPCs):
        // Maurice Jondrelle (Base: 0x0001C605, Ref: 0x0001C606, Quest T03 dunEldergleamT03: 0x0001C48E)
        // Eldergleam Sanctuary görevinde Maurice oyuncuya refakatçi olur ancak görev bitince
        // Bethesda Teammate bayrağını temizlemez. Bu nedenle T03 aşaması >= 40 ise takipçi sayılmaz.
        auto base = actor->GetActorBase();
        if (base && base->GetFormID() == 0x0001C605) {
            static auto* t03Quest = RE::TESForm::LookupByID<RE::TESQuest>(0x0001C48E);
            if (t03Quest && (t03Quest->currentStage >= 40 || t03Quest->IsCompleted())) {
                if (actor->IsPlayerTeammate()) {
                    actor->GetActorRuntimeData().boolBits.reset(RE::Actor::BOOL_BITS::kPlayerTeammate);
                }
                return false;
            }
        }

        // Primary check: engine teammate flag (set by ALL follower frameworks + vanilla)
        if (actor->IsPlayerTeammate()) return true;

        // Secondary: vanilla CurrentFollowerFaction (0x0005C84E)
        static auto* followerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84E);
        if (followerFaction && actor->IsInFaction(followerFaction)) return true;

        // Nether's Follower Framework (NFF) Faction check (nwsFF_FollowerFac: local ID 0x16EB3 in nwsFollowerFramework.esp)
        static auto* nffFollowerFaction = []() -> RE::TESFaction* {
            if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
                return dataHandler->LookupForm<RE::TESFaction>(0x00016EB3, "nwsFollowerFramework.esp");
            }
            return nullptr;
        }();
        if (nffFollowerFaction && actor->IsInFaction(nffFollowerFaction)) return true;

        // Extensible Follower Framework (EFF) Faction check (FollowerFaction: 0x00000EB4 in EFFCore.esm)
        static auto* effFollowerFaction = []() -> RE::TESFaction* {
            if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
                return dataHandler->LookupForm<RE::TESFaction>(0x00000EB4, "EFFCore.esm");
            }
            return nullptr;
        }();
        if (effFollowerFaction && actor->IsInFaction(effFollowerFaction)) return true;

        // Amazing Follower Tweaks (AFT) / IAFT Faction check (TweakFollowerFaction: 0x00001802 in AmazingFollowerTweaks.esp)
        static auto* aftFollowerFaction = []() -> RE::TESFaction* {
            if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
                return dataHandler->LookupForm<RE::TESFaction>(0x00001802, "AmazingFollowerTweaks.esp");
            }
            return nullptr;
        }();
        if (aftFollowerFaction && actor->IsInFaction(aftFollowerFaction)) return true;

        // Vanilla DialogueFollower quest (0x750BA) alias check
        static auto* followerQuest = RE::TESForm::LookupByID<RE::TESQuest>(0x000750BA);
        if (followerQuest) {
            for (auto alias : followerQuest->aliases) {
                auto refAlias = alias ? skyrim_cast<RE::BGSRefAlias*>(alias) : nullptr;
                if (refAlias) {
                    auto ref = refAlias->GetActorReference();
                    if (ref && ref->GetFormID() == actor->GetFormID()) return true;
                }
            }
        }

        // Faction list iterate check (modlu takipçi çerçevelerinin "follower" fraksiyonları)
        bool hasFollowerFactionName = false;
        actor->VisitFactions([&](RE::TESFaction* faction, std::int8_t rank) {
            if (hasFollowerFactionName || !faction || rank < 0) return false;
            if (const char* name = faction->GetName()) {
                std::string fname(name);
                std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
                if (fname == "current follower" || fname == "framework follower" || fname == "player follower") {
                    hasFollowerFactionName = true;
                    return true;
                }
            }
            return false;
        });
        if (hasFollowerFactionName) return true;

        // Modumuzun kendi 10 adet çoklu takipçi yuvası fraksiyonları (FU_FollowerSlotFaction1..10)
        if (FollowerManager::GetSingleton()->IsInFollowerSlot(actor)) return true;

        // Animal / Pet / Dog Follower Factions (0x000282C9: AnimalFollowerFaction, 0x000282CC: CurrentAnimalFollowerFaction)
        static auto* animalFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x000282C9);
        if (animalFollowerFaction && actor->IsInFaction(animalFollowerFaction)) return true;

        static auto* currentAnimalFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x000282CC);
        if (currentAnimalFaction && actor->IsInFaction(currentAnimalFaction)) return true;

        // Player Horse check: Player owns or rides this horse
        static auto* playerHorseFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x00068DFA); // PlayerHorseFaction
        if (playerHorseFaction && actor->IsInFaction(playerHorseFaction)) return true;

        return false;
    }

    bool FollowerManager::IsAnimalActor(RE::Actor* actor)
    {
        if (!actor) return false;

        // 0. Positive Humanoid / NPC check: If the actor, base, or race explicitly has ActorTypeNPC (0x00013794),
        // it is GUARANTEED to be a humanoid character (even if custom modded race lacks FaceGen flag).
        static const RE::FormID kActorTypeNPC = 0x00013794;
        if (actor->HasKeywordString("ActorTypeNPC")) {
            return false;
        }
        auto base = actor->GetActorBase();
        if (base && (base->HasKeywordString("ActorTypeNPC") || base->HasKeywordID(kActorTypeNPC))) {
            return false;
        }
        auto race = actor->GetRace();
        if (race && (race->HasKeywordString("ActorTypeNPC") || race->HasKeywordID(kActorTypeNPC))) {
            return false;
        }

        // 1. Explicit Animal / Creature keyword check on Actor
        if (actor->HasKeywordString("ActorTypeAnimal") || actor->HasKeywordString("ActorTypeCreature")) {
            return true;
        }

        // 2. Keyword check on ActorBase
        if (base) {
            if (base->HasKeywordString("ActorTypeAnimal") || base->HasKeywordString("ActorTypeCreature") ||
                base->HasKeywordID(0x00013795) || base->HasKeywordID(0x00013796)) {
                return true;
            }
        }

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

        // 6. Humanoid check fallback:
        // FaceGenHead is present on all vanilla humanoid races (including Khajiit, Argonian, Elves, Men).
        // Modded custom races (like Goblins, Dwarves, custom mesh races) might omit FaceGenHead,
        // but if they don't match any animal keywords or animal flags, and their skeleton/biped is humanoid,
        // or if they can open doors, they should NOT be treated as animals!
        bool hasFaceGen = race->data.flags.all(RE::RACE_DATA::Flag::kFaceGenHead);
        if (hasFaceGen) {
            return false;
        }

        // If no FaceGen, check if it's explicitly child or human skeleton/biped
        if (race->data.flags.all(RE::RACE_DATA::Flag::kChild)) {
            return false;
        }

        // If it can't open doors or has animal tags, it already returned true above.
        // For remaining actors without FaceGen: if base is ActorCharacter (humanoid base type), don't treat as animal.
        if (base && base->GetFormType() == RE::FormType::NPC) {
            return false;
        }

        return true; // Unknown creature
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

        std::shared_ptr<FollowerProfile> storedProfile;
        bool isBrandNewProfile = false;
        if (auto d = _dormantProfiles.find(formID); d != _dormantProfiles.end() && d->second) {
            // Daha önce takipçi olmuş: atanmış loadout, tercihler ve tasarruf korunur.
            storedProfile = d->second;
            _dormantProfiles.erase(d);
            _dormantMeta.erase(formID);
            storedProfile->isAnimal = profile.isAnimal;
            storedProfile->role = profile.role;
            storedProfile->classBoundDisplayName = profile.classBoundDisplayName;
            if (storedProfile->name.empty() || storedProfile->name == "Follower") storedProfile->name = profile.name;
            logger::info("Registered follower {} [{:08X}] restored from dormant profile.", storedProfile->name, formID);
        } else {
            storedProfile = std::make_shared<FollowerProfile>(std::move(profile));
            isBrandNewProfile = true;
        }

        // İLK DEFA KATILAN TAKİPÇİLER (Starter Supplies & 100% Needs):
        // İhtiyaçları tamamen %100 doldurulur.
        storedProfile->needs.hunger = 100.0f;
        storedProfile->needs.thirst = 100.0f;
        storedProfile->needs.fatigue = 100.0f;

        // İlk defa katılan insansı takipçilerin envanterine 2 içecek ve 3 orta boy yemek verilir.
        if (isBrandNewProfile && !isAnimal) {
            static auto* breadItem = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00065C97); // FoodBread01 (Ekmek Somunu)
            static auto* drinkItem = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00034CDF); // FoodAle (Bira / İçecek)
            if (breadItem) {
                actor->AddObjectToContainer(breadItem, nullptr, 3, nullptr);
            }
            if (drinkItem) {
                actor->AddObjectToContainer(drinkItem, nullptr, 2, nullptr);
            }
            logger::info("RegisterFollower: Granted starter supplies (3x FoodBread, 2x Drink) to {} [{:08X}]",
                storedProfile->name, formID);
        }

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

    bool FollowerManager::IsTwoHandedWeapon(RE::TESForm* form)
    {
        auto* w = form ? form->As<RE::TESObjectWEAP>() : nullptr;
        return w && (w->IsTwoHandedAxe() || w->IsTwoHandedSword() || w->IsBow() || w->IsCrossbow());
    }

    void FollowerManager::NoteOwnEquip(RE::FormID actorID)
    {
        std::lock_guard lk(_equipStateMutex);
        _ownEquipTime[actorID] = std::chrono::steady_clock::now();
    }

    bool FollowerManager::WasOwnEquipRecent(RE::FormID actorID) const
    {
        std::lock_guard lk(_equipStateMutex);
        auto it = _ownEquipTime.find(actorID);
        return it != _ownEquipTime.end() &&
            std::chrono::steady_clock::now() - it->second < std::chrono::milliseconds(1500);
    }

    void FollowerManager::ClearAssignment(FollowerProfile& p, RE::FormID id)
    {
        if (!id) return;
        for (auto* slot : { &p.assignedPrimaryRight, &p.assignedPrimaryLeft, &p.assignedSecondaryRight,
                            &p.assignedSecondaryLeft, &p.assignedHead, &p.assignedBody,
                            &p.assignedHands, &p.assignedFeet }) {
            if (*slot == id) *slot = 0;
        }
    }

    void FollowerManager::AssignLoadoutItem(FollowerProfile& p, RE::TESBoundObject* item, bool left, bool secondary)
    {
        if (!item) return;
        using Slot = RE::BIPED_MODEL::BipedObjectSlot;
        const RE::FormID id = item->GetFormID();
        p.ProtectItem(id, true);

        auto& R = secondary ? p.assignedSecondaryRight : p.assignedPrimaryRight;
        auto& L = secondary ? p.assignedSecondaryLeft : p.assignedPrimaryLeft;

        if (auto* armor = item->As<RE::TESObjectARMO>()) {
            const auto mask = static_cast<uint32_t>(armor->GetSlotMask());
            if (mask & static_cast<uint32_t>(Slot::kShield)) {
                // Kalkan sol ele gider; sağda çift elli silah varsa birlikte kuşanılamaz.
                if (IsTwoHandedWeapon(RE::TESForm::LookupByID(R))) R = 0;
                L = id;
                return;
            }
            if (mask & static_cast<uint32_t>(Slot::kHead)) p.assignedHead = id;
            if (mask & static_cast<uint32_t>(Slot::kBody)) p.assignedBody = id;
            if (mask & static_cast<uint32_t>(Slot::kHands)) p.assignedHands = id;
            if (mask & static_cast<uint32_t>(Slot::kFeet)) p.assignedFeet = id;
            return;
        }

        if (item->As<RE::TESObjectWEAP>()) {
            if (IsTwoHandedWeapon(item)) {      // yay / arbalet / çift elli: iki el
                R = id;
                L = 0;
            } else if (left) {
                if (IsTwoHandedWeapon(RE::TESForm::LookupByID(R))) R = 0;
                L = id;
            } else {
                R = id;
            }
        }
    }

    void FollowerManager::ReplaceAssignment(FollowerProfile& p, RE::FormID oldID, RE::TESBoundObject* newItem)
    {
        if (!newItem) return;

        // A better-item prompt can legitimately have no currently equipped
        // counterpart (or replace an item which was never assigned through our
        // UI).  Leaving the new item unassigned lets a later enforcement pass
        // restore an unrelated loadout instead of the item the player accepted.
        // In that case assign it to its natural primary slot.
        if (!oldID) {
            AssignLoadoutItem(p, newItem, false, false);
            return;
        }
        bool left = false, secondary = false, found = true;
        if (p.assignedPrimaryRight == oldID) { left = false; secondary = false; }
        else if (p.assignedPrimaryLeft == oldID) { left = true; secondary = false; }
        else if (p.assignedSecondaryRight == oldID) { left = false; secondary = true; }
        else if (p.assignedSecondaryLeft == oldID) { left = true; secondary = true; }
        else if (p.assignedHead == oldID || p.assignedBody == oldID ||
                 p.assignedHands == oldID || p.assignedFeet == oldID) { /* zırh: slot maskesinden */ }
        else found = false;
        if (!found) {
            // The old item was equipped by vanilla/another mod, rather than by
            // our loadout system.  Keep the accepted upgrade persistent anyway.
            AssignLoadoutItem(p, newItem, false, false);
            return;
        }
        ClearAssignment(p, oldID);
        AssignLoadoutItem(p, newItem, left, secondary);
    }

    void FollowerManager::EnforceAssignedLoadout(RE::Actor* actor, FollowerProfile& profile,
        bool ignoreCooldown, bool allowInCombat)
    {
        if (!actor || actor->IsDead() || profile.isAnimal) return;

        // OStim sahnesi sırasında takipçilerin kıyafet/zırh giymesini kesinlikle engelle
        if (EventManager::IsOstimBusy()) return;

        // Varsayılan olarak savaşta müdahale yok; yalnızca EnforceCombatWeapons (allowInCombat) girer.
        if (!allowInCombat && actor->IsInCombat()) return;
        if (!actor->Is3DLoaded() || AnimationManager::IsRidingWithPlayer(actor)) return;

        // Aynı takipçiye 3 sn'de bir dokun (ignoreCooldown: kullanıcı atama yaptı).
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard lk(_equipStateMutex);
            auto& last = _enforceLast[actor->GetFormID()];
            if (!ignoreCooldown && last.time_since_epoch().count() != 0 &&
                now - last < std::chrono::seconds(3)) return;
            last = now;
        }

        auto* equipMgr = RE::ActorEquipManager::GetSingleton();
        if (!equipMgr) return;

        auto inv = actor->GetInventory();
        // Yalnızca ATANMIŞ eşyalar için kullanılır: envanterde yoksa ama hayali çantadaysa geri verilir.
        auto has = [&](RE::TESBoundObject* b) {
            if (!b) return false;
            auto it = inv.find(b);
            if (it != inv.end() && it->second.first > 0) return true;
            if (RestoreFromBackpack(actor, profile, b->GetFormID(), -1, 1) > 0) {
                logger::info("Backpack: {} retrieved assigned item {} from backpack.", actor->GetName(), b->GetName());
                return true;
            }
            return false;
        };

        // ── ZIRH: zaten giyiliyse tekrar kuşandırma (takıp-çıkarma döngüsünün ana sebebi buydu) ──
        auto equipArmor = [&](RE::FormID id) {
            if (!id) return;
            auto* a = RE::TESForm::LookupByID<RE::TESObjectARMO>(id);
            if (!a || !has(a)) return;
            auto* currentWorn = actor->GetWornArmor(a->GetSlotMask());
            if (currentWorn == a) return;
            // Eğer o slotta başka bir zırh varsa, yenisini giymeden önce eskisini çıkar ve korumasını kaldır
            if (currentWorn && currentWorn != a) {
                profile.ProtectItem(currentWorn->GetFormID(), false);
                equipMgr->UnequipObject(actor, currentWorn, nullptr, 1, nullptr, false, false, false, true);
            }
            NoteOwnEquip(actor->GetFormID());
            equipMgr->EquipObject(actor, a, nullptr, 1, nullptr, false, false, false, true);
        };
        equipArmor(profile.assignedHead);
        equipArmor(profile.assignedBody);
        equipArmor(profile.assignedHands);
        equipArmor(profile.assignedFeet);

        // Kıyafet Bütünlüğü Kuralı (Otomatik Çıkartma): tercih edilen zırh tipine uymayan,
        // atanmamış giyili parçayı çıkar.
        if (profile.preferences.preferredArmorType != 0) {
            for (const auto& [item, invData] : inv) {
                if (invData.first <= 0 || !invData.second || !invData.second->extraLists || !item) continue;
                auto* armor = item->As<RE::TESObjectARMO>();
                if (!armor) continue;

                bool worn = false;
                for (auto* xList : *invData.second->extraLists) {
                    if (xList && (xList->HasType(RE::ExtraDataType::kWorn) ||
                                  xList->HasType(RE::ExtraDataType::kWornLeft))) {
                        worn = true;
                        break;
                    }
                }
                if (!worn) continue;

                const auto armorID = armor->GetFormID();
                if (armorID == profile.assignedHead || armorID == profile.assignedBody ||
                    armorID == profile.assignedHands || armorID == profile.assignedFeet ||
                    profile.IsProtected(armorID)) {
                    continue;
                }
                if (!ItemEvaluator::MatchesPreferredArmor(armor, profile.preferences.preferredArmorType)) {
                    NoteOwnEquip(actor->GetFormID());
                    equipMgr->UnequipObject(actor, armor, nullptr, 1, nullptr, false, false, false, true);
                    logger::info("EnforceAssignedLoadout: {} unequipped mismatched armor {} due to preferred type rule.",
                        actor->GetName(), armor->GetName());
                }
            }
        }

        // ── SİLAH ──
        auto* rightSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F42);
        auto* leftSlot  = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F43);
        auto* rightObj  = RE::TESForm::LookupByID<RE::TESBoundObject>(profile.assignedPrimaryRight);
        auto* leftObj   = RE::TESForm::LookupByID<RE::TESBoundObject>(profile.assignedPrimaryLeft);
        const bool rightTwoHanded = IsTwoHandedWeapon(rightObj);

        // Çift el silah kuşanma döngüsü koruması:
        // Skyrim'in motoru barış anında sol eldeki silahı otomatik kınına sokar (sheathe).
        // Eğer takipçi savaşta değilse ve silahı kınındaysa, motorun sol eli sürekli tekrar
        // çekip geri takmasını (draw/sheathe loop) engellemek için barış anında gereksiz yere zorlama yapılmaz.
        auto* actorState = actor->AsActorState();
        const bool isSheathed = actorState && actorState->GetWeaponState() == RE::WEAPON_STATE::kSheathed;
        const bool inCombat = actor->IsInCombat() || allowInCombat;
        // Kullanıcı eylemi (UI ataması) ve savaş sonu çağrısı kınındaki takipçide de uygulanmalı;
        // aksi halde barışta yapılan atama hiçbir zaman kuşandırılmazdı.
        const bool force = ignoreCooldown || allowInCombat;

        if (rightObj && has(rightObj) && actor->GetEquippedObject(false) != rightObj) {
            if (force || inCombat || !isSheathed || actor->GetEquippedObject(false) == nullptr) {
                NoteOwnEquip(actor->GetFormID());
                equipMgr->EquipObject(actor, rightObj, nullptr, 1, rightTwoHanded ? nullptr : rightSlot,
                    false, false, false, false);
            }
        }

        // Sağda çift elli varsa sol eli HİÇ kuşandırma: ikisi birbirini düşürür.
        if (leftObj && has(leftObj) && !rightTwoHanded && actor->GetEquippedObject(true) != leftObj) {
            const bool isShield = leftObj->As<RE::TESObjectARMO>() != nullptr;
            // Kalkanlar barış anında da taşınabilir; ancak tek el silahlar barış anında kınındayken
            // sürekli EquipObject ile çağrılmamalı (döngüyü önler).
            if (isShield || force || inCombat || !isSheathed) {
                NoteOwnEquip(actor->GetFormID());
                equipMgr->EquipObject(actor, leftObj, nullptr, 1, isShield ? nullptr : leftSlot,
                    false, false, false, false);
            }
        }

        // Atama uygulandıktan sonra, seçilmeyen silah/zırhları hayali çantaya al (yalnızca barışta).
        if (!allowInCombat && !actor->IsInCombat()) {
            StashUnassignedGear(actor, profile);
        }
    }

    void FollowerManager::NoteBackpackRetrieval(RE::FormID actorID, RE::FormID itemID)
    {
        std::lock_guard lk(_equipStateMutex);
        _backpackGrace[(static_cast<uint64_t>(actorID) << 32) | itemID] = std::chrono::steady_clock::now();
    }

namespace
    {
        // Çantadaki özel kayıttan ekstra veri listesi kurar (motor AddObjectToContainer ile sahiplenir).
        RE::ExtraDataList* BuildExtraList(const BackpackCustomItem& c)
        {
            auto* xl = new RE::ExtraDataList();
            if (c.hasEnchant) {
                if (auto* ench = RE::TESForm::LookupByID<RE::EnchantmentItem>(c.enchantID)) {
                    auto* e = RE::BSExtraData::Create<RE::ExtraEnchantment>();
                    e->enchantment = ench;
                    e->charge = c.enchantCharge;
                    xl->Add(e);
                }
            }
            if (c.hasHealth) {
                auto* h = RE::BSExtraData::Create<RE::ExtraHealth>();
                h->health = c.health;
                xl->Add(h);
            }
            if (c.hasCharge) {
                auto* ch = RE::BSExtraData::Create<RE::ExtraCharge>();
                ch->charge = c.charge;
                xl->Add(ch);
            }
            if (!c.name.empty()) {
                auto* t = RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
                t->SetName(c.name.c_str());
                xl->Add(t);
            }
            return xl;
        }
    }

    int32_t FollowerManager::StashItem(RE::Actor* actor, FollowerProfile& profile, RE::TESBoundObject* obj,
        int32_t wanted, bool allowWorn)
    {
        if (!actor || !obj) return 0;

        // Hayali çanta ağırlık sınırı kontrolü
        auto* settings = Settings::GetSingleton();
        if (settings && settings->enableBackpackWeightLimit && settings->backpackWeightLimit > 0.0f) {
            const float currentBpWeight = profile.GetBackpackWeight();
            const float itemWeight = obj->GetWeight();
            if (currentBpWeight + (itemWeight > 0.0f ? itemWeight : 0.0f) > settings->backpackWeightLimit) {
                return 0; // Kapasite dolu; eşyayı çantaya atma, normal envanterde kalsın
            }
        }

        auto inv = actor->GetInventory();
        auto it = inv.find(obj);
        if (it == inv.end() || it->second.first <= 0 || !it->second.second) return 0;

        const int32_t total = it->second.first;
        const RE::FormID id = obj->GetFormID();
        auto* entry = it->second.second.get();

        struct Custom { RE::ExtraDataList* xl; BackpackCustomItem c; };
        std::vector<Custom> customs;
        int32_t customTotal = 0;
        std::vector<RE::ExtraDataList*> wornLists;
        if (entry->extraLists) {
            for (auto* x : *entry->extraLists) {
                if (!x) continue;
                const bool isWorn = x->HasType(RE::ExtraDataType::kWorn) || x->HasType(RE::ExtraDataType::kWornLeft);
                if (!allowWorn && isWorn) return 0;
                if (isWorn) wornLists.push_back(x);
                // Görev eşyası / zehirli silah: ekstra veri çantada saklanamaz, dokunma.
                if (x->HasType(RE::ExtraDataType::kAliasInstanceArray) || x->HasType(RE::ExtraDataType::kPoison)) return 0;

                Custom cu;
                cu.xl = x;
                cu.c.itemID = id;
                cu.c.count = std::max<int32_t>(1, static_cast<int32_t>(x->GetCount()));
                if (x->HasType(RE::ExtraDataType::kEnchantment)) {
                    if (auto* e = x->GetByType<RE::ExtraEnchantment>(); e && e->enchantment) {
                        cu.c.hasEnchant = true;
                        cu.c.enchantID = e->enchantment->GetFormID();
                        cu.c.enchantCharge = e->charge;
                    }
                }
                if (x->HasType(RE::ExtraDataType::kHealth)) {
                    if (auto* h = x->GetByType<RE::ExtraHealth>()) { cu.c.hasHealth = true; cu.c.health = h->health; }
                }
                if (x->HasType(RE::ExtraDataType::kCharge)) {
                    if (auto* ch = x->GetByType<RE::ExtraCharge>()) { cu.c.hasCharge = true; cu.c.charge = ch->charge; }
                }
                if (x->HasType(RE::ExtraDataType::kTextDisplayData)) {
                    if (auto* t = x->GetByType<RE::ExtraTextDisplayData>(); t && t->displayName.c_str() && t->displayName.c_str()[0]) {
                        cu.c.name = t->displayName.c_str();
                    }
                }
                if (!cu.c.hasEnchant && !cu.c.hasHealth && !cu.c.hasCharge && cu.c.name.empty()) continue;   // sade örnek
                customTotal += cu.c.count;
                customs.push_back(std::move(cu));
            }
        }

        const int32_t plainAvail = std::max<int32_t>(0, total - customTotal);
        int32_t moved = 0;
        if (wanted < 0) {
            for (auto& cu : customs) {
                actor->RemoveItem(obj, cu.c.count, RE::ITEM_REMOVE_REASON::kRemove, cu.xl, nullptr);
                profile.backpackCustom.push_back(cu.c);
                moved += cu.c.count;
            }
            if (plainAvail > 0) {
                RE::ExtraDataList* targetXl = (allowWorn && !wornLists.empty()) ? wornLists.front() : nullptr;
                actor->RemoveItem(obj, plainAvail, RE::ITEM_REMOVE_REASON::kRemove, targetXl, nullptr);
                profile.AddToBackpack(id, plainAvail);
                moved += plainAvail;
            }
        } else if (plainAvail > 0) {
            const int32_t n = std::min(wanted, plainAvail);
            RE::ExtraDataList* targetXl = (allowWorn && !wornLists.empty()) ? wornLists.front() : nullptr;
            actor->RemoveItem(obj, n, RE::ITEM_REMOVE_REASON::kRemove, targetXl, nullptr);
            profile.AddToBackpack(id, n);
            moved = n;
        } else if (!customs.empty()) {
            auto& cu = customs.front();
            const int32_t n = std::min(wanted, cu.c.count);
            cu.c.count = n;
            actor->RemoveItem(obj, n, RE::ITEM_REMOVE_REASON::kRemove, cu.xl, nullptr);
            profile.backpackCustom.push_back(cu.c);
            moved = n;
        }
        return moved;
    }

    int32_t FollowerManager::RestoreFromBackpack(RE::TESObjectREFR* dest, FollowerProfile& profile,
        RE::FormID itemID, int32_t customIndex, int32_t count)
    {
        auto* obj = RE::TESForm::LookupByID<RE::TESBoundObject>(itemID);
        if (!dest || !obj || count <= 0) return 0;

        if (customIndex >= 0) {
            if (customIndex >= static_cast<int32_t>(profile.backpackCustom.size())) return 0;
            auto& c = profile.backpackCustom[static_cast<size_t>(customIndex)];
            if (c.itemID != itemID || c.count <= 0) return 0;
            const int32_t n = std::min(count, c.count);
            auto* xl = BuildExtraList(c);
            dest->AddObjectToContainer(obj, xl, n, nullptr);
            c.count -= n;
            if (c.count <= 0) profile.backpackCustom.erase(profile.backpackCustom.begin() + customIndex);
            return n;
        }

        if (const int32_t have = profile.GetBackpackCount(itemID); have > 0) {
            const int32_t n = std::min(count, have);
            profile.RemoveFromBackpack(itemID, n);
            dest->AddObjectToContainer(obj, nullptr, n, nullptr);
            return n;
        }
        for (size_t i = 0; i < profile.backpackCustom.size(); ++i) {
            if (profile.backpackCustom[i].itemID == itemID) {
                return RestoreFromBackpack(dest, profile, itemID, static_cast<int32_t>(i), count);
            }
        }
        return 0;
    }

    void FollowerManager::ReturnBackpackToPlayer(FollowerProfile& profile)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;
        uint32_t total = 0;
        for (const auto& [id, count] : profile.backpackItems) {
            auto* obj = RE::TESForm::LookupByID<RE::TESBoundObject>(id);
            if (!obj || count <= 0) continue;
            player->AddObjectToContainer(obj, nullptr, count, nullptr);
            total += static_cast<uint32_t>(count);
        }
        profile.backpackItems.clear();
        for (const auto& c : profile.backpackCustom) {
            auto* obj = RE::TESForm::LookupByID<RE::TESBoundObject>(c.itemID);
            if (!obj || c.count <= 0) continue;
            player->AddObjectToContainer(obj, BuildExtraList(c), c.count, nullptr);
            total += static_cast<uint32_t>(c.count);
        }
        profile.backpackCustom.clear();
        if (total > 0) {
            NotificationManager::GetSingleton()->ShowHUDMessage(
                std::format("{}'s backpack ({} items) was returned to you.", profile.name, total));
            logger::info("Backpack: returned {} items of dormant follower {} to the player.", total, profile.name);
        }
    }

    void FollowerManager::ProcessDormantBackpacks()
    {
        auto* calendar = RE::Calendar::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!calendar || !player || player->IsInCombat()) return;
        const float nowDays = calendar->GetDaysPassed();

        std::vector<std::shared_ptr<FollowerProfile>> due;
        {
            std::shared_lock lock(_mutex);
            for (const auto& [fid, prof] : _dormantProfiles) {
                if (!prof || (prof->backpackItems.empty() && prof->backpackCustom.empty())) continue;
                auto m = _dormantMeta.find(fid);
                if (m == _dormantMeta.end() || m->second.dead) continue;      // ölenin (diriltilebilir) çantası korunur
                if (nowDays - m->second.sinceDays < 1.0f) continue;           // 1 oyun günü bekle
                due.push_back(prof);
            }
        }
        for (auto& prof : due) ReturnBackpackToPlayer(*prof);
    }

    void FollowerManager::StashUnassignedGear(RE::Actor* actor, FollowerProfile& profile)
    {
        if (!actor || actor->IsDead() || profile.isAnimal) return;
        if (actor->IsInCombat() || !actor->Is3DLoaded() || AnimationManager::IsRidingWithPlayer(actor)) return;

        const bool weaponLoadout = profile.assignedPrimaryRight || profile.assignedPrimaryLeft ||
                                   profile.assignedSecondaryRight || profile.assignedSecondaryLeft;
        const bool armorLoadout = profile.assignedHead || profile.assignedBody ||
                                  profile.assignedHands || profile.assignedFeet;

        using Slot = RE::BIPED_MODEL::BipedObjectSlot;
        constexpr uint32_t kMainArmorSlots = static_cast<uint32_t>(Slot::kHead) | static_cast<uint32_t>(Slot::kBody) |
                                             static_cast<uint32_t>(Slot::kHands) | static_cast<uint32_t>(Slot::kFeet);
        constexpr uint32_t kShieldSlot = static_cast<uint32_t>(Slot::kShield);

        uint32_t assignedArmorMask = 0;
        for (auto id : { profile.assignedHead, profile.assignedBody, profile.assignedHands, profile.assignedFeet }) {
            if (auto* a = RE::TESForm::LookupByID<RE::TESObjectARMO>(id)) {
                assignedArmorMask |= static_cast<uint32_t>(a->GetSlotMask()) & kMainArmorSlots;
            }
        }

        const RE::FormID assignedIDs[] = { profile.assignedPrimaryRight, profile.assignedPrimaryLeft,
            profile.assignedSecondaryRight, profile.assignedSecondaryLeft,
            profile.assignedHead, profile.assignedBody, profile.assignedHands, profile.assignedFeet };
        std::unordered_set<RE::FormID> protectedSet(profile.protectedEquipment.begin(), profile.protectedEquipment.end());
        const bool styleSet = ItemEvaluator::HasWeaponStylePreference(profile.preferences);

        const auto now = std::chrono::steady_clock::now();
        auto inv = actor->GetInventory();
        int stashed = 0;
        for (const auto& [obj, data] : inv) {
            if (stashed >= 50) break;
            if (!obj || data.first <= 0 || !data.second) continue;
            const RE::FormID id = obj->GetFormID();
            if (IsTngHiddenBodyItem(obj)) continue;
            bool isAssigned = false;
            for (auto a : assignedIDs) if (a && a == id) { isAssigned = true; break; }
            if (isAssigned || protectedSet.contains(id)) continue;

            // Kuşanılmış silah ve zırhlar asla hayali çantaya atılmaz (üzerlerinde kalır).
            bool isEquippedOrWorn = false;
            if (data.second->IsWorn()) {
                isEquippedOrWorn = true;
            }
            if (data.second->extraLists) {
                for (auto* xl : *data.second->extraLists) {
                    if (xl && (xl->HasType(RE::ExtraDataType::kWorn) || xl->HasType(RE::ExtraDataType::kWornLeft))) {
                        isEquippedOrWorn = true;
                        break;
                    }
                }
            }
            if (actor->GetEquippedObject(false) == obj || actor->GetEquippedObject(true) == obj) {
                isEquippedOrWorn = true;
            }
            if (isEquippedOrWorn) continue;

            bool shouldStash = false;
            if (auto* weapon = obj->As<RE::TESObjectWEAP>()) {
                // Kuşanılmamış ve atanmamış TÜM silahlar doğrudan hayali çantaya aktarılır.
                shouldStash = true;
            } else if (auto* armor = obj->As<RE::TESObjectARMO>()) {
                // Kuşanılmamış ve atanmamış TÜM zırhlar/kıyafetler doğrudan hayali çantaya aktarılır.
                shouldStash = true;
            }
            if (!shouldStash) continue;

            // Bekleyen yükseltme teklifi: oyuncu karar verene kadar eşya envanterde kalsın.
            if (EquipPrompt::IsOfferPending(actor->GetFormID(), id)) continue;

            // Oyuncu çantadan yeni aldıysa (atamak için) 2 dakika yeniden çantaya konmaz.
            {
                std::lock_guard lk(_equipStateMutex);
                auto it = _backpackGrace.find((static_cast<uint64_t>(actor->GetFormID()) << 32) | id);
                if (it != _backpackGrace.end()) {
                    if (now - it->second < std::chrono::seconds(120)) continue;
                    _backpackGrace.erase(it);
                }
            }

            // Kuşanılı / zehirli / görev eşyaları StashItem içinde atlanır; büyülü-temperli-adlı eşyalar
            // ekstra verileriyle birlikte çantaya alınır.
            const int32_t moved = StashItem(actor, profile, obj, -1, false);
            if (moved > 0) {
                ++stashed;
                logger::info("Backpack: {} stashed unassigned {} x{}.", actor->GetName(), obj->GetName(), moved);
            }
        }
    }

    void FollowerManager::EnforceCombatWeapons(RE::Actor* actor)
    {
        if (!actor || actor->IsDead() || !actor->Is3DLoaded() || AnimationManager::IsRidingWithPlayer(actor)) return;
        const auto actorID = actor->GetFormID();
        auto profile = GetProfile(actorID);
        if (!profile || profile->isAnimal) return;
        // Oyuncu asli silah atamadıysa AI'ya karışma.
        if (!profile->assignedPrimaryRight && !profile->assignedPrimaryLeft) return;
        if (WasOwnEquipRecent(actorID)) return;

        const RE::FormID allowed[] = { profile->assignedPrimaryRight, profile->assignedPrimaryLeft,
                                       profile->assignedSecondaryRight, profile->assignedSecondaryLeft };
        const bool useStyle = ItemEvaluator::HasWeaponStylePreference(profile->preferences);
        auto isAllowed = [&](RE::TESForm* f) {
            if (!f) return true;
            for (auto a : allowed) if (a && a == f->GetFormID()) return true;
            // Seçilen asli/yardımcı silah stiline uyan her silah da izinli (ör. yardımcı stil: Yay).
            if (useStyle) {
                if (auto* w = f->As<RE::TESObjectWEAP>()) {
                    return ItemEvaluator::MatchesWeaponPreferences(w, profile->preferences);
                }
            }
            return false;
        };
        auto isShield = [](RE::TESForm* f) {
            auto* a = f ? f->As<RE::TESObjectARMO>() : nullptr;
            return a && (static_cast<uint32_t>(a->GetSlotMask()) &
                         static_cast<uint32_t>(RE::BIPED_MODEL::BipedObjectSlot::kShield)) != 0;
        };

        auto* r = actor->GetEquippedObject(false);
        auto* l = actor->GetEquippedObject(true);
        const bool badRight = r && r->As<RE::TESObjectWEAP>() && !isAllowed(r);
        const bool badLeft  = l && (l->As<RE::TESObjectWEAP>() || isShield(l)) && !isAllowed(l);
        if (!badRight && !badLeft) return;

        // Döngü koruması: 15 sn içinde en fazla 2 düzeltme.
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard lk(_equipStateMutex);
            auto& st = _combatFix[actorID];
            if (st.windowStart.time_since_epoch().count() == 0 || now - st.windowStart > std::chrono::seconds(15)) {
                st.windowStart = now;
                st.count = 0;
            }
            if (st.count >= 2) return;
            ++st.count;
        }

        logger::info("CombatWeapons: {} picked unassigned weapon (R={} L={}); restoring assigned loadout.",
            actor->GetName(), r ? r->GetName() : "-", l ? l->GetName() : "-");
        EnforceAssignedLoadout(actor, *profile, true, true);
    }

    void FollowerManager::RestoreSavedProfile(FollowerProfile profile)
    {
        const auto targetFormID = profile.formID;
        if (targetFormID == 0) return;
        if (profile.name.empty()) profile.name = "Follower";

        std::shared_ptr<FollowerProfile> applied;
        {
            std::unique_lock lock(_mutex);
            _dormantProfiles.erase(targetFormID);
            _dormantMeta.erase(targetFormID);
            auto it = _profiles.find(targetFormID);
            if (it != _profiles.end() && it->second) {
                if (profile.classBoundDisplayName.empty()) profile.classBoundDisplayName = it->second->classBoundDisplayName;
                profile.isAnimal = it->second->isAnimal;
                if (!it->second->name.empty() && it->second->name != "Follower") profile.name = it->second->name;
                *it->second = std::move(profile);
                applied = it->second;
            } else {
                applied = std::make_shared<FollowerProfile>(std::move(profile));
                _profiles.emplace(targetFormID, applied);
            }
        }
        // Motor çağrısı kilit dışında (kilit altında EquipObject deadlock riski taşır).
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(targetFormID)) {
            // Oyunu kayıttan yüklediğimizde motor teammate flag'ini ve fraksiyonları sıfırlamış olabilir.
            // Aktif kayıtlı takipçinin teammate bayrağını ve CurrentFollowerFaction üyeliğini koru!
            if (!applied->isAnimal) {
                static auto* vanillaFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84E);
                if (vanillaFollowerFaction && !actor->IsInFaction(vanillaFollowerFaction)) {
                    actor->AddToFaction(vanillaFollowerFaction, 0);
                }
                if (!actor->IsPlayerTeammate()) {
                    actor->GetActorRuntimeData().boolBits.set(RE::Actor::BOOL_BITS::kPlayerTeammate);
                }
                if (auto* av = actor->AsActorValueOwner()) {
                    if (av->GetActorValue(RE::ActorValue::kWaitingForPlayer) != 0.0f) {
                        av->SetActorValue(RE::ActorValue::kWaitingForPlayer, 0.0f);
                    }
                }
                actor->EvaluatePackage();
            }

            for (auto spellID : applied->learnedSpells) {
                if (auto* sp = RE::TESForm::LookupByID<RE::SpellItem>(spellID)) {
                    if (!actor->HasSpell(sp)) actor->AddSpell(sp);
                }
            }
            EnforceAssignedLoadout(actor, *applied);
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
        bool dead = false;
        if (auto* a = RE::TESForm::LookupByID<RE::Actor>(formID)) dead = a->IsDead();
        float days = 0.0f;
        if (auto* cal = RE::Calendar::GetSingleton()) days = cal->GetDaysPassed();

        std::unique_lock lock(_mutex);
        if (auto it = _profiles.find(formID); it != _profiles.end()) {
            logger::info("Unregistered follower: {} [FormID: {:08X}] (profile kept dormant)", it->second->name, formID);
            // Profili silme: oyuncu takipçiyi tekrar alırsa ayarlar (loadout, tercih, tasarruf) kaybolmasın.
            // Ölen takipçinin (LifeAgain diriltmesi) çantası korunur; bırakılan takipçinin çantası 1 oyun günü
            // sonra oyuncuya geri verilir (ProcessDormantBackpacks).
            if (it->second && !it->second->isAnimal) {
                _dormantProfiles[formID] = it->second;
                _dormantMeta[formID] = DormantMeta{ days, dead };
            }
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

    std::unordered_map<RE::FormID, FollowerProfile> FollowerManager::GetProfilesSnapshot(bool includeDormant) const
    {
        std::shared_lock lock(_mutex);
        std::unordered_map<RE::FormID, FollowerProfile> snapshot;
        snapshot.reserve(_profiles.size() + (includeDormant ? _dormantProfiles.size() : 0));
        for (const auto& [formID, profile] : _profiles) {
            if (profile) snapshot.emplace(formID, *profile);
        }
        if (includeDormant) {
            for (const auto& [formID, profile] : _dormantProfiles) {
                if (profile) snapshot.emplace(formID, *profile);
            }
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
        ProcessDormantBackpacks();

        // Refresh roles for all remaining tracked followers
        RefreshFollowerRoles();

        // Takipçilerin oyuncunun atadığı asli loadout'u taşıdığını doğrula.
        // Motor çağrısı (EquipObject) kilit altında yapılmaz: önce liste kopyalanır.
        {
            std::vector<std::pair<RE::FormID, std::shared_ptr<FollowerProfile>>> toEnforce;
            {
                std::shared_lock lock(_mutex);
                for (const auto& [fid, prof] : _profiles) {
                    if (prof && !prof->isAnimal) toEnforce.emplace_back(fid, prof);
                }
            }
            for (auto& [fid, prof] : toEnforce) {
                auto* fActor = RE::TESForm::LookupByID<RE::Actor>(fid);
                if (fActor && fActor->Is3DLoaded() && !fActor->IsDead()) {
                    EnforceAssignedLoadout(fActor, *prof);
                }
            }
        }

        // 10 Takipçi Yuvası & Vanilla PlayerFollowerCount Senkronizasyonu
        SyncFollowerSlots();
        SyncPlayerFollowerCount();
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
        float days = calendar->GetDaysPassed();
        float hour = calendar->GetHour();
        _lastGameTimeDays = days + (hour / 24.0f);
        logger::info("InitGameTime: zaman başlatıldı ({:.4f} gün, saat {:.2f})", _lastGameTimeDays, hour);
        // Oyun yüklendi: kayıtta açık kalmış bir ticaret sahnesi/quest'i varsa kapat
        TradeManager::GetSingleton()->AbortAll("load");
    }

    void FollowerManager::UpdateGameTimeNeeds()
    {
        auto calendar = RE::Calendar::GetSingleton();
        if (!calendar) return;

        float days = calendar->GetDaysPassed();
        float hour = calendar->GetHour();
        // Skyrim'de GameDaysPassed tam gün geçişlerinde veya beklemede artarken, GameHour
        // normal oynanışta her saniye akar (timescale hızında).
        // Bu yüzden gerçek akıcı oyun zamanı = gün + (saat / 24).
        float currentDays = days + (hour / 24.0f);

        // _lastGameTimeDays henüz başlatılmamışsa (InitGameTime çağrılmadı)
        // sadece zamanı kaydet, tick yapma.
        if (_lastGameTimeDays <= 0.0f) {
            _lastGameTimeDays = currentDays;
            logger::info("UpdateGameTimeNeeds: ilk çağrı — zaman sıfırdan başlatıldı ({:.4f})", currentDays);
            return;
        }

        float deltaDays = currentDays - _lastGameTimeDays;
        // Gün dönümü veya geriye sarma koruması
        if (deltaDays < 0.0f) {
            deltaDays = 0.0f;
            _lastGameTimeDays = currentDays;
            return;
        }
        if (deltaDays < 0.00001f) return;   // Çok ufak fraksiyon, bekle

        _lastGameTimeDays = currentDays;
        float deltaHours = deltaDays * 24.0f;

        // Settings'den tüketim hızlarını al
        auto settings   = Settings::GetSingleton();
        float hungerRate  = settings->HungerRatePerHour();
        float thirstRate  = settings->ThirstRatePerHour();
        float fatigueRate = settings->FatigueRatePerHour();

        logger::debug("UpdateGameTimeNeeds: {:.3f} oyun saati geçti | hız H:{:.2f} T:{:.2f} F:{:.2f}",
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
            // At binen veya ata binme/inme aşamasında olan takipçilerin animasyon ve eylemlerini dondur
            if (MountManager::IsMountBusy(actor)) continue;

            // Profil pointer'ı kısa süreli lock ile al
            auto profile = GetProfile(entry.formID);
            if (!profile) continue;

            consumableAI->UpdateNeedsConsumption(actor, *profile);

            // Hayvanlar alışveriş yapmaz veya eşya satmaz
            if (!profile->isAnimal) {
                CheckAutonomousCrafting(actor, *profile);
                // Satıcıya yürüme açıksa TradeManager yönetir; kapalıysa (veya eklenti yoksa) eski anlık yol
                if (!TradeManager::GetSingleton()->HandleTradeTick(actor, *profile)) {
                    shoppingAI->ProcessSelling(actor, *profile);
                    shoppingAI->ProcessShopping(actor, *profile);
                }
            }
        }
    }

    void FollowerManager::TriggerFollowerConsumption()
    {
        // OStim sahnesi aktifken veya sahne bitiminden sonraki 15 sn boyunca takipçi eylemlerini tamamen dondur
        if (EventManager::IsOstimBusy()) return;

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
            // At binen veya ata binme/inme aşamasında olan takipçilerin animasyon ve eylemlerini dondur
            if (MountManager::IsMountBusy(actor)) continue;

            auto profile = GetProfile(entry.formID);
            if (!profile) continue;

            if (actor->IsInCombat()) {
                if (!profile->isAnimal) {
                    consumableAI->UpdateCombatConsumables(actor, *profile);
                }
            } else {
                consumableAI->UpdateNeedsConsumption(actor, *profile);

                // Savaş dışı iyileşme (Canı az ise iksir iç veya healing büyüsü yap)
                if (!profile->isAnimal) {
                    consumableAI->UpdateOutOfCombatHealing(actor, *profile);
                    // Hayali çanta temizliği: Envanterde kalan kuşanılmamış/atanmamış tüm silah & zırhlar çantaya çekilir
                    StashUnassignedGear(actor, *profile);
                }

                // Hayvan olmayan takipçiler savaş dışındayken silahlarını zehirleyip savaşa hazır bekler
                if (!profile->isAnimal) {
                    consumableAI->UpdateOutOfCombatPoison(actor, *profile);
                }

                // Büyü kitabı öğrenme: Şehir, kasaba veya oyuncu evinde her zaman güvenle okur
                if (!profile->isAnimal) {
                    CheckSpellTomeLearning(actor, *profile);
                }

                // Hayvan olmayan takipçiler yakında satıcı varsa düzenli satış ve alışveriş yapar
                if (!profile->isAnimal && canTradeTick) {
                    // 2. Kendi malzemelerini kullanarak üretim yap
                    CheckAutonomousCrafting(actor, *profile);
                    // 3-4. Satış ve alışveriş: satıcıya yürüme açıksa TradeManager yönetir (varışta ShoppingAI çağrılır);
                    //      kapalıysa (veya eklenti yoksa) eski anlık yol
                    if (!TradeManager::GetSingleton()->HandleTradeTick(actor, *profile)) {
                        // 3. Fazla eşyaları ve zaten bildiği kopya büyü kitaplarını sat
                        shoppingAI->ProcessSelling(actor, *profile);
                        // 4. İhtiyaçlarını satın al
                        shoppingAI->ProcessShopping(actor, *profile);
                    }
                }
            }
        }
    }

    void FollowerManager::CheckAutonomousCrafting(RE::Actor* actor, FollowerProfile& profile)
    {
        if (!actor || actor->IsDead() || actor->IsInCombat() || profile.isAnimal) return;
        if (!profile.preferences.allowCraftPotions && !profile.preferences.allowCraftArrows &&
            !profile.preferences.allowCraftBolts && !profile.preferences.allowCraftTorches &&
            !profile.preferences.allowCraftFood) return;

        auto* alchemyStation = profile.preferences.allowCraftPotions ? FindNearbyCraftingStation(actor, CraftStationType::Alchemy) : nullptr;
        auto* forgeStation = (profile.preferences.allowCraftArrows || profile.preferences.allowCraftBolts || profile.preferences.allowCraftTorches)
            ? FindNearbyCraftingStation(actor, CraftStationType::Forge) : nullptr;
        auto* cookingStation = profile.preferences.allowCraftFood ? FindNearbyCraftingStation(actor, CraftStationType::Cooking) : nullptr;

        const bool nearForge = (forgeStation != nullptr);
        const bool nearAlchemy = (alchemyStation != nullptr);
        const bool nearCooking = (cookingStation != nullptr);

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

        // Yemek Malzemeleri:
        static auto* saltPileObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00034CDF); // Salt Pile (Tuz)
        static auto* potatoObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00064B41);   // Patates
        static auto* cabbageObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00064B3F);  // Lahana
        static auto* leekObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x000669A5);     // Pırasa
        static auto* tomatoObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00064B42);   // Domates
        static auto* appleObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00064B2E);    // Kırmızı Elma
        static auto* rawBeefObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x00065C99);  // Çiğ Dana Eti
        static auto* rawVenisonObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x000669A2); // Çiğ Geyik Eti

        // Yemek Çıktıları:
        static auto* vegSoupObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x000F431E);       // Sebze Çorbası
        static auto* beefStewObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x000F4314);      // Dana Güveç
        static auto* venisonStewObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x000F431D);   // Geyik Güveç
        static auto* appleCabbageStewObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x000EBA01); // Elmalı Lahana Güveci
        static auto* cabbagePotatoSoupObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x000F431B); // Lahana Patates Çorbası
        static auto* cookedBeefObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x000721E8);    // Pişmiş Et
        static auto* cookedVenisonObj = RE::TESForm::LookupByID<RE::TESBoundObject>(0x000722BD); // Pişmiş Geyik Eti

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

        // 1. SİMYA MASASI İLE İKSİR ÜRETİMİ (Simya Tezgahını Kullanarak)
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

                // Tezgah animasyonunu oynat
                AnimationManager::SendAnimEvent(actor, "IdleAlchemyEnter");
                if (Settings::GetSingleton()->notifySelling) {
                    notifier->ShowHUDMessage(std::format("{} crafted a {}.", actor->GetName(), recipe.label));
                }
                logger::info("Autonomous Crafting: {} crafted {}; potion stock {}/{}.", actor->GetName(), recipe.label, recipe.stock + 1, recipe.target);
                return;
            }
        }

        // 2. OCAK VE KAZANLARDA VANILYA YEMEKLERİ PİŞİRME
        if (profile.preferences.allowCraftFood && nearCooking) {
            const auto supplyCounts = ShoppingAI::GetSupplyCounts(actor);
            const uint32_t curMedium = static_cast<uint32_t>(std::max<int32_t>(0, supplyCounts.foodM));
            const uint32_t maxMedium = profile.preferences.maxFoodMedium;
            const uint32_t curLarge = static_cast<uint32_t>(std::max<int32_t>(0, supplyCounts.foodL));
            const uint32_t maxLarge = profile.preferences.maxFoodLarge;

            struct FoodRecipe {
                std::vector<RE::TESBoundObject*> ingredients;
                RE::TESBoundObject* output;
                bool isLarge;
                const char* name;
            };

            const FoodRecipe foodRecipes[] = {
                // Sebze Çorbası: Lahana + Patates + Pırasa + Domates
                { { cabbageObj, potatoObj, leekObj, tomatoObj }, vegSoupObj, false, "Vegetable Soup" },
                // Elmalı Lahana Güveci: Tuz + Elma + Lahana
                { { saltPileObj, appleObj, cabbageObj }, appleCabbageStewObj, false, "Apple Cabbage Stew" },
                // Lahana Patates Çorbası: Lahana + Patates + Pırasa + Tuz
                { { cabbageObj, potatoObj, leekObj, saltPileObj }, cabbagePotatoSoupObj, false, "Cabbage Potato Soup" },
                // Geyik Güveci: Çiğ Geyik Eti + Tuz + Patates + Pırasa
                { { rawVenisonObj, saltPileObj, potatoObj, leekObj }, venisonStewObj, false, "Venison Stew" },
                // Dana Güveci: Çiğ Dana Eti + Patates + Tuz
                { { rawBeefObj, potatoObj, saltPileObj }, beefStewObj, false, "Beef Stew" },
                // Pişmiş Dana Eti: Çiğ Dana Eti + Tuz
                { { rawBeefObj, saltPileObj }, cookedBeefObj, true, "Cooked Beef" },
                // Pişmiş Geyik Eti: Çiğ Geyik Eti + Tuz
                { { rawVenisonObj, saltPileObj }, cookedVenisonObj, true, "Cooked Venison" }
            };

            for (const auto& rec : foodRecipes) {
                if (!rec.output) continue;
                if (rec.isLarge && curLarge >= maxLarge) continue;
                if (!rec.isLarge && curMedium >= maxMedium) continue;

                bool hasAll = true;
                for (auto* ing : rec.ingredients) {
                    if (!ing || evaluator->GetItemCount(actor, ing) <= 0) {
                        hasAll = false;
                        break;
                    }
                }
                if (!hasAll) continue;

                // Malzemeleri tüket
                for (auto* ing : rec.ingredients) {
                    actor->RemoveItem(ing, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                }
                actor->AddObjectToContainer(rec.output, nullptr, 1, nullptr);

                // Pişirme animasyonunu tetikle
                AnimationManager::SendAnimEvent(actor, "IdleCraftingCookingPotAddIngredient");
                if (Settings::GetSingleton()->notifySelling) {
                    notifier->ShowHUDMessage(std::format("{} cooked {}.", actor->GetName(), rec.name));
                }
                logger::info("Autonomous Cooking: {} cooked {}.", actor->GetName(), rec.name);
                return;
            }
        }

        // Devam eden demirci ocağı çalışması kontrolü (4 saniye çekiçleme animasyonu)
        const auto actorID = actor->GetFormID();
        const auto now = std::chrono::steady_clock::now();
        bool stillCrafting = false;
        bool finishedCrafting = false;
        {
            std::lock_guard cl(_craftMutex);
            if (auto it = _craftingActiveFollowers.find(actorID); it != _craftingActiveFollowers.end()) {
                float elapsed = std::chrono::duration<float>(now - it->second).count();
                if (elapsed < 4.0f) {
                    stillCrafting = true;
                } else {
                    finishedCrafting = true;
                    _craftingActiveFollowers.erase(it);
                }
            }
        }
        if (stillCrafting) return;
        if (finishedCrafting) {
            AnimationManager::GetSingleton()->StopSmithingAnimation(actor);
            return;
        }

        // Demirci ocağına yaklaşma ve çekiçleme başlatma yardımcısı
        auto startForgeWork = [&](RE::TESObjectREFR* forge) {
            if (!forge) return;
            // Ocağa doğru yüzünü döndür
            const auto from = actor->GetPosition();
            const auto to = forge->GetPosition();
            actor->data.angle.z = std::atan2(to.x - from.x, to.y - from.y);

            // Eğer mesafe 120 birimden fazlaysa ocağın önüne yürü
            if (actor->GetPosition().GetDistance(forge->GetPosition()) > 130.0f) {
                auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
                if (policy) {
                    const auto handle = policy->GetHandleForObject(RE::FormType::ActorCharacter, actor);
                    if (handle != policy->EmptyHandle()) {
                        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
                        auto* args = RE::MakeFunctionArguments(
                            static_cast<RE::TESObjectREFR*>(forge),
                            0.0f, 90.0f, 0.0f, 0.0f, 0.0f, 0.0f, 250.0f, 20.0f
                        );
                        vm->DispatchMethodCall2(handle, "Actor", "KeepOffsetFromActor", args, callback);
                    }
                }
            }

            // Animasyon ve ses efekti başlat
            AnimationManager::GetSingleton()->PlaySmithingAnimation(actor, forge);
            std::lock_guard cl(_craftMutex);
            _craftingActiveFollowers[actorID] = now;
        };

        // 1. ARBALET OKU (BOLT) ÜRETİMİ
        bool wantsBolts = profile.preferences.allowCraftBolts;
        if (wantsBolts && nearForge && steelBoltObj) {
            uint32_t currentBolts = evaluator->CountBolts(actor);
            if (currentBolts < profile.preferences.maxBolts) {
                int32_t firewoodCount = evaluator->GetItemCount(actor, firewoodObj);
                int32_t steelCount = evaluator->GetItemCount(actor, steelIngotObj);

                if (firewoodCount >= 1 && steelCount >= 1) {
                    startForgeWork(forgeStation);
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
                    startForgeWork(forgeStation);
                    actor->RemoveItem(firewoodObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                    actor->RemoveItem(steelIngotObj, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                    actor->AddObjectToContainer(steelArrowObj, nullptr, 24, nullptr);

                    if (Settings::GetSingleton()->notifySelling) {
                        notifier->ShowHUDMessage(std::format("{} crafted 24x Steel Arrows.", actor->GetName()));
                    }
                    logger::info("Autonomous Crafting: {} crafted 24x Steel Arrows.", actor->GetName());
                    return;
                } else if (firewoodCount >= 1 && ironCount >= 1 && ironArrowObj) {
                    startForgeWork(forgeStation);
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
                startForgeWork(forgeStation);
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

        // Animasyon (motor çağrısı) kilit dışında: önce yorgunluk sıfırlanır, FormID'ler toplanır.
        std::vector<RE::FormID> restedIDs;
        {
            std::unique_lock lock(_mutex);
            for (auto& [formID, profile] : _profiles) {
                if (!profile) continue;
                profile->needs.RestoreFromSleep();
                restedIDs.push_back(formID);
            }
        }
        for (auto formID : restedIDs) {
            auto form = RE::TESForm::LookupByID(formID);
            if (form) {
                auto actor = form->As<RE::Actor>();
                if (actor && !actor->IsDead()) {
                    animMgr->PlaySleepRestoreAnimation(actor);
                }
            }
        }
        logger::info("OnSleepStop: {} follower(s) rested and restored.", restedIDs.size());
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

        // Eğer şu anda kitap okuyorsa süresini kontrol et (Her kitap için 10 saniye okuma süresi)
        bool stillReading = false;
        bool finishedReading = false;
        {
            std::lock_guard rl(_readMutex);
            if (auto it = _readingActiveFollowers.find(actorID); it != _readingActiveFollowers.end()) {
                float elapsed = std::chrono::duration<float>(now - it->second).count();
                if (elapsed < 10.0f) {
                    stillReading = true;      // Kitap okuma animasyonu hala devam ediyor (10 sn)
                } else {
                    finishedReading = true;   // 10 sn doldu
                    _readingActiveFollowers.erase(it);
                }
            }
        }
        if (stillReading) return;
        if (finishedReading) {
            AnimationManager::GetSingleton()->StopReadAnimation(actor);
            return;
        }

        static std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> s_lastReadTime;
        static std::mutex s_lastReadMutex;
        {
            std::lock_guard rl(s_lastReadMutex);
            if (auto it = s_lastReadTime.find(actorID); it != s_lastReadTime.end()) {
                // Kitaplar arasında 12 saniyelik doğal geçiş (10 sn okuma + 2 sn nefes alma)
                if (std::chrono::duration<float>(now - it->second).count() < 12.0f) {
                    return;
                }
            }
        }

        auto* animMgr = AnimationManager::GetSingleton();
        auto* notifMgr = NotificationManager::GetSingleton();
        auto* avOwner = actor->AsActorValueOwner();
        const float maxMagicka = avOwner ? std::max(avOwner->GetPermanentActorValue(RE::ActorValue::kMagicka), avOwner->GetBaseActorValue(RE::ActorValue::kMagicka)) : 0.0f;

        for (const auto& [item, invData] : actor->GetInventory()) {
            if (!item || invData.first <= 0) continue;
            auto* book = item->As<RE::TESObjectBOOK>();
            if (!book || !book->data.flags.all(RE::OBJ_BOOK::Flag::kTeachesSpell)) continue;

            auto* spell = book->data.teaches.spell;
            if (!spell || spell->GetFormType() != RE::FormType::Spell) continue;

            // Takipçi bu büyüyü zaten biliyor mu?
            if (actor->HasSpell(spell)) continue;

            // ── MANA (MAGICKA) GEREKSİNİMİ KONTROLÜ ──
            // Çırak (Novice/Apprentice): 125 Magicka, Kalfa (Adept): 250 Magicka, Uzman/Usta (Expert/Master): 400 Magicka
            int32_t minSkill = 0;
            if (auto* effect = spell->GetCostliestEffectItem()) {
                if (effect->baseEffect) {
                    minSkill = effect->baseEffect->data.minimumSkill;
                }
            }

            float requiredMagicka = 125.0f; // Varsayılan çırak seviyesi
            if (minSkill >= 75) {
                requiredMagicka = 400.0f;   // Uzman (Expert: 75+) / Usta (Master: 100)
            } else if (minSkill >= 50) {
                requiredMagicka = 250.0f;   // Kalfa (Adept: 50+)
            } else {
                requiredMagicka = 125.0f;   // Acemi (Novice: 0+) / Çırak (Apprentice: 25+)
            }

            if (maxMagicka < requiredMagicka) {
                logger::info("SpellTomeLearning: {} has insufficient magicka ({:.0f}/{:.0f}) for '{}' (skill level requirement {}).",
                    actor->GetName(), maxMagicka, requiredMagicka, spell->GetName() ? spell->GetName() : "Spell", minSkill);
                continue; // Manası yetmediği için bu kitabı henüz öğrenemez
            }

            // Oturuyor mu kontrol et
            auto* actorState = actor->AsActorState();
            const bool isSitting = actorState && (actorState->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal);

            // Okuma animasyonu başlat (10 saniye boyunca okur)
            animMgr->PlayReadAnimation(actor, isSitting);

            // Büyüyü öğren ve profile kaydet
            actor->AddSpell(spell);
            profile.learnedSpells.insert(spell->GetFormID());

            // Kitabı envanterden tüket (1 adet)
            actor->RemoveItem(book, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

            {
                std::lock_guard rl(s_lastReadMutex);
                s_lastReadTime[actorID] = now;
            }
            {
                std::lock_guard rl(_readMutex);
                _readingActiveFollowers[actorID] = now; // 10 saniye boyunca okumada tut
            }

            const char* spellName = spell->GetName() ? spell->GetName() : "Spell";
            const char* bookName = book->GetName() ? book->GetName() : "Spell Tome";

            if (settings->notifySpellLearned) {
                std::string msg = std::format("{} read {} and learned {}! (Mana: {:.0f}/{:.0f})",
                    actor->GetName(), bookName, spellName, maxMagicka, requiredMagicka);
                notifMgr->ShowHUDMessage(msg);
            }
            logger::info("SpellTomeLearning: {} read '{}' [{:08X}] and learned spell '{}' [{:08X}] (Required Magicka: {:.0f}).",
                actor->GetName(), bookName, book->GetFormID(), spellName, spell->GetFormID(), requiredMagicka);

            break; // Seans başına 1 kitap okur, sonraki kitap 10-12 saniye sonra
        }
    }

    
    void FollowerManager::EnsureFollowerSlotsLoaded()
    {
        if (_followerSlotsLoaded) return;
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) return;

        constexpr std::string_view kPlugin = "FollowerUnboundTrade.esp";
        constexpr RE::FormID kFirstFactId = 0x83D;
        constexpr RE::FormID kFirstQuestId = 0x847;

        bool allOk = true;
        for (int i = 0; i < 10; ++i) {
            _followerSlots[i].faction = data->LookupForm<RE::TESFaction>(kFirstFactId + i, kPlugin);
            _followerSlots[i].quest = data->LookupForm<RE::TESQuest>(kFirstQuestId + i, kPlugin);
            if (!_followerSlots[i].faction || !_followerSlots[i].quest) {
                allOk = false;
            }
        }
        if (allOk) {
            _followerSlotsLoaded = true;
            logger::info("FollowerManager: Successfully initialized all 10 multi-follower recruitment slots from {}", kPlugin);
        }
    }

    bool FollowerManager::IsInFollowerSlot(RE::Actor* actor)
    {
        if (!actor) return false;
        EnsureFollowerSlotsLoaded();
        if (!_followerSlotsLoaded) return false;
        for (int i = 0; i < 10; ++i) {
            if (_followerSlots[i].faction && actor->IsInFaction(_followerSlots[i].faction)) {
                return true;
            }
        }
        return false;
    }

    void FollowerManager::SyncFollowerSlots()
    {
        EnsureFollowerSlotsLoaded();
        if (!_followerSlotsLoaded) return;

        // Aktif (insansı) takipçileri topla
        std::vector<RE::Actor*> activeFollowers;
        {
            std::shared_lock lock(_mutex);
            for (const auto& [fid, prof] : _profiles) {
                if (prof && !prof->isAnimal) {
                    if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid)) {
                        if (!actor->IsDead()) {
                            activeFollowers.push_back(actor);
                        }
                    }
                }
            }
        }

        // 1. Artık takipçi olmayan veya yeri değişen aktörlerin yuvalarını temizle
        for (int i = 0; i < 10; ++i) {
            RE::FormID assignedFid = _slotAssignedActors[i];
            if (assignedFid == 0) continue;

            bool stillActive = false;
            for (auto* actor : activeFollowers) {
                if (actor->GetFormID() == assignedFid) {
                    stillActive = true;
                    break;
                }
            }

            if (!stillActive) {
                // Yuvayı boşalt
                if (auto* oldActor = RE::TESForm::LookupByID<RE::Actor>(assignedFid)) {
                    oldActor->RemoveFromFaction(_followerSlots[i].faction);
                    static auto* vanillaFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84E);
                    if (vanillaFollowerFaction) {
                        oldActor->RemoveFromFaction(vanillaFollowerFaction);
                    }
                    oldActor->GetActorRuntimeData().boolBits.reset(RE::Actor::BOOL_BITS::kPlayerTeammate);
                    oldActor->EvaluatePackage();
                }
                if (_followerSlots[i].quest->IsEnabled()) {
                    _followerSlots[i].quest->Stop();
                }
                logger::info("FollowerManager: Cleared follower slot {} (actor {:08X} dismissed)", i + 1, assignedFid);
                _slotAssignedActors[i] = 0;
            }
        }

        // 2. Henüz bir yuvaya atanmamış aktif takipçileri boş yuvalara yerleştir
        for (auto* actor : activeFollowers) {
            RE::FormID fid = actor->GetFormID();
            bool alreadyAssigned = false;
            for (int i = 0; i < 10; ++i) {
                if (_slotAssignedActors[i] == fid) {
                    alreadyAssigned = true;
                    break;
                }
            }
            if (alreadyAssigned) continue;

            // Boş yuva bul
            for (int i = 0; i < 10; ++i) {
                if (_slotAssignedActors[i] == 0) {
                    _slotAssignedActors[i] = fid;
                    actor->AddToFaction(_followerSlots[i].faction, 0);

                    // Hem kendi yuvamıza hem de vanilla CurrentFollowerFaction'a (0x0005C84E) ekle!
                    // Böylece:
                    // 1) Skyrim motoru oyunu kaydedip yüklediğinde NPC'yi gerçek takipçi olarak tanır.
                    // 2) Vanilla diyaloglarında "Beni takip et" seçeneği gizlenir (çünkü zaten takipçidir).
                    // 3) NPC'ler save load sonrasında da oyuncuyu takip etmeye devam eder.
                    static auto* vanillaFollowerFaction = RE::TESForm::LookupByID<RE::TESFaction>(0x0005C84E);
                    if (vanillaFollowerFaction && !actor->IsInFaction(vanillaFollowerFaction)) {
                        actor->AddToFaction(vanillaFollowerFaction, 0);
                    }

                    // Ensure engine teammate status is true so game behaves properly
                    if (!actor->IsPlayerTeammate()) {
                        actor->GetActorRuntimeData().boolBits.set(RE::Actor::BOOL_BITS::kPlayerTeammate);
                    }

                    if (auto* av = actor->AsActorValueOwner()) {
                        if (av->GetActorValue(RE::ActorValue::kWaitingForPlayer) != 0.0f) {
                            av->SetActorValue(RE::ActorValue::kWaitingForPlayer, 0.0f);
                        }
                    }

                    // Reset and start slot quest to guarantee alias condition fills for this actor
                    if (_followerSlots[i].quest->IsEnabled()) {
                        _followerSlots[i].quest->Reset();
                    } else {
                        _followerSlots[i].quest->Start();
                    }

                    actor->EvaluatePackage();
                    logger::info("FollowerManager: Assigned {} [{:08X}] to multi-follower slot {} (AI follow quest started)",
                        actor->GetName(), fid, i + 1);
                    break;
                }
            }
        }

        // Framework Ayarı: Tüm Takipçileri Ölümsüz / Korumalı yap (Essential / Protected)
        if (Settings::GetSingleton()->frameworkEssential) {
            for (auto* actor : activeFollowers) {
                if (actor && !actor->IsDead()) {
                    actor->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kProtected);
                }
            }
        }
    }

    void FollowerManager::SyncPlayerFollowerCount()
    {
        // Vanilla PlayerFollowerCount global (0x000BCC98)
        auto* global = RE::TESForm::LookupByID<RE::TESGlobal>(0x000BCC98);
        if (!global) return;

        // İnsansı takipçi sayısını say
        size_t humanoidCount = 0;
        {
            std::shared_lock lock(_mutex);
            for (const auto& [fid, prof] : _profiles) {
                if (prof && !prof->isAnimal) {
                    humanoidCount++;
                }
            }
        }

        // Framework Ayarı: İkna Kabiliyeti (Speechcraft) ile takipçi sınırı ölçekleme
        size_t maxAllowed = 10;
        if (Settings::GetSingleton()->frameworkSpeechcraftScaling) {
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                if (auto* av = player->AsActorValueOwner()) {
                    float speech = av->GetActorValue(RE::ActorValue::kSpeech);
                    // 0-25: 1 takipçi, 25-50: 2, 50-75: 3, 75-99: 4, 100: 5 (veya max 10)
                    maxAllowed = std::clamp(static_cast<size_t>(1 + (speech / 20.0f)), size_t{ 1 }, size_t{ 10 });
                }
            }
        }

        // Eğer izin verilen sayıdan az takipçimiz varsa: PlayerFollowerCount'u 0 tut ki
        // vanilla 'Beni takip et' diyaloğu tüm NPC'lerde aktif kalsın ve yeni takipçi işe alınabilsin!
        // Sınıra ulaşıldığında: 1 (veya max) yaparak diyalog alımını sınırla.
        float targetValue = (humanoidCount < maxAllowed) ? 0.0f : static_cast<float>(humanoidCount);
        if (global->value != targetValue) {
            global->value = targetValue;
            logger::info("FollowerManager: Synced PlayerFollowerCount global -> {:.0f} (Active: {}/{})",
                targetValue, humanoidCount, maxAllowed);
        }
    }

    bool FollowerManager::TeleportFollowerToPlayer(RE::FormID formID)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;

        auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
        if (!actor || actor->IsDead() || actor->IsPlayerRef()) return false;

        // At üzerinde veya sahnede ise ışınlamayı güvenli şekilde yap
        actor->MoveTo(player);
        actor->EvaluatePackage();
        logger::info("TeleportFollowerToPlayer: Teleported {} [{:08X}] to player position.", actor->GetName(), formID);
        return true;
    }

    void FollowerManager::UpdateCatchupTeleport()
    {
        auto* settings = Settings::GetSingleton();
        if (!settings->enableFollowerTeleport) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || player->IsInCombat()) return;

        const auto now = std::chrono::steady_clock::now();
        const float thresholdDist = settings->followerTeleportDistance;
        const float delaySec = settings->followerTeleportDelaySeconds;

        auto followers = GetActiveFollowers();
        for (auto* actor : followers) {
            if (!actor || actor->IsDead() || actor->IsPlayerRef()) continue;
            if (actor->IsInCombat() || MountManager::IsMountBusy(actor) || AnimationManager::IsRidingWithPlayer(actor)) {
                _farDistanceStartTime.erase(actor->GetFormID());
                continue;
            }

            float dist = player->GetPosition().GetDistance(actor->GetPosition());
            const auto fid = actor->GetFormID();

            if (dist > thresholdDist) {
                auto it = _farDistanceStartTime.find(fid);
                if (it == _farDistanceStartTime.end()) {
                    _farDistanceStartTime[fid] = now;
                } else {
                    float elapsed = std::chrono::duration<float>(now - it->second).count();
                    if (elapsed >= delaySec) {
                        TeleportFollowerToPlayer(fid);
                        _farDistanceStartTime.erase(fid);
                    }
                }
            } else {
                _farDistanceStartTime.erase(fid);
            }
        }
    }

    void FollowerManager::Clear()
    {
        std::unique_lock lock(_mutex);
        _profiles.clear();
        _dormantProfiles.clear();
        _dormantMeta.clear();
        _farDistanceStartTime.clear();
    }
}
