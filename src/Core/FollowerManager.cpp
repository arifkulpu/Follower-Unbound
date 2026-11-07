#include "Core/FollowerManager.h"
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

        // (ActorBase EditorID alt dizi eşleşmesi kaldırıldı: "cat", "cow", "elk" gibi kısa kelimeler
        //  normal NPC EditorID'lerinde de geçebilir. Hayvanlar ırk adı/EditorID'si ve FaceGen
        //  kontrolüyle zaten yakalanıyor.)

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

        std::shared_ptr<FollowerProfile> storedProfile;
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
                    armorID == profile.assignedHands || armorID == profile.assignedFeet) {
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
        auto inv = actor->GetInventory();
        auto it = inv.find(obj);
        if (it == inv.end() || it->second.first <= 0 || !it->second.second) return 0;

        const int32_t total = it->second.first;
        const RE::FormID id = obj->GetFormID();
        auto* entry = it->second.second.get();

        struct Custom { RE::ExtraDataList* xl; BackpackCustomItem c; };
        std::vector<Custom> customs;
        int32_t customTotal = 0;
        if (entry->extraLists) {
            for (auto* x : *entry->extraLists) {
                if (!x) continue;
                if (!allowWorn && (x->HasType(RE::ExtraDataType::kWorn) || x->HasType(RE::ExtraDataType::kWornLeft))) return 0;
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
                actor->RemoveItem(obj, plainAvail, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                profile.AddToBackpack(id, plainAvail);
                moved += plainAvail;
            }
        } else if (plainAvail > 0) {
            const int32_t n = std::min(wanted, plainAvail);
            actor->RemoveItem(obj, n, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
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
        // Oyuncu hiçbir şey seçmediyse takipçinin envanterine dokunma.
        if (!weaponLoadout && !armorLoadout) return;

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
        const bool styleSet = ItemEvaluator::HasWeaponStylePreference(profile.preferences);

        const auto now = std::chrono::steady_clock::now();
        auto inv = actor->GetInventory();
        int stashed = 0;
        for (const auto& [obj, data] : inv) {
            if (stashed >= 8) break;     // takılma olmasın: tur başına en fazla 8 kalem
            if (!obj || data.first <= 0 || !data.second) continue;
            const RE::FormID id = obj->GetFormID();
            if (IsTngHiddenBodyItem(obj)) continue;
            bool isAssigned = false;
            for (auto a : assignedIDs) if (a && a == id) { isAssigned = true; break; }
            if (isAssigned) continue;

            bool shouldStash = false;
            if (auto* weapon = obj->As<RE::TESObjectWEAP>()) {
                // Seçilen silah stiline uyan silahlar (örn. yardımcı stil: Yay) AI'ya açık kalır.
                shouldStash = weaponLoadout &&
                    !(styleSet && ItemEvaluator::MatchesWeaponPreferences(weapon, profile.preferences));
            } else if (auto* armor = obj->As<RE::TESObjectARMO>()) {
                const auto mask = static_cast<uint32_t>(armor->GetSlotMask());
                if (mask & kShieldSlot) {
                    shouldStash = weaponLoadout;
                } else if (armorLoadout && (mask & kMainArmorSlots & assignedArmorMask)) {
                    shouldStash = true;
                }
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
        // Oyun yüklendi: kayıtta açık kalmış bir ticaret sahnesi/quest'i varsa kapat
        TradeManager::GetSingleton()->AbortAll("load");
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

        // Eğer şu anda kitap okuyorsa süresini kontrol et (4 saniye boyunca okur)
        bool stillReading = false;
        bool finishedReading = false;
        {
            std::lock_guard rl(_readMutex);
            if (auto it = _readingActiveFollowers.find(actorID); it != _readingActiveFollowers.end()) {
                float elapsed = std::chrono::duration<float>(now - it->second).count();
                if (elapsed < 4.0f) {
                    stillReading = true;      // Kitap okuma animasyonu hala devam ediyor
                } else {
                    finishedReading = true;   // Süre bitti
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
                if (std::chrono::duration<float>(now - it->second).count() < 15.0f) {
                    return; // Kitaplar arasında makul bekleme süresi (15 sn)
                }
            }
        }

        auto* animMgr = AnimationManager::GetSingleton();
        auto* notifMgr = NotificationManager::GetSingleton();

        for (const auto& [item, invData] : actor->GetInventory()) {
            if (!item || invData.first <= 0) continue;
            auto* book = item->As<RE::TESObjectBOOK>();
            if (!book || !book->data.flags.all(RE::OBJ_BOOK::Flag::kTeachesSpell)) continue;

            auto* spell = book->data.teaches.spell;
            if (!spell || spell->GetFormType() != RE::FormType::Spell) continue;

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

            {
                std::lock_guard rl(s_lastReadMutex);
                s_lastReadTime[actorID] = now;
            }
            {
                std::lock_guard rl(_readMutex);
                _readingActiveFollowers[actorID] = now; // 4 saniye boyunca okumada tut
            }

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
        _dormantProfiles.clear();
        _dormantMeta.clear();
    }
}
