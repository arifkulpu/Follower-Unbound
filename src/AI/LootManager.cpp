#include "AI/LootManager.h"
#include "AI/ItemEvaluator.h"
#include "AI/AnimationManager.h"
#include "AI/DroppedItemRecovery.h"
#include "Economy/EconomyManager.h"
#include "UI/NotificationManager.h"
#include "UI/EquipPrompt.h"
#include "Core/Settings.h"
#include "Core/TNGCompatibility.h"
#include <random>

namespace FollowerUnbound
{
    namespace
    {
        RE::TESObjectARMO* ChooseConfiguredUnderwear()
        {
            static const auto candidates = [] {
                std::vector<RE::TESObjectARMO*> result;
                const auto iniPath = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "Underwear.ini";
                std::ifstream ini(iniPath);
                if (!ini) {
                    logger::info("LootUnderwear: Underwear.ini not found at {}; corpse clothing remains unchanged.", iniPath.string());
                    return result;
                }

                std::string line;
                bool inGeneral = false;
                while (std::getline(ini, line)) {
                    line.erase(0, line.find_first_not_of(" \t\r\n"));
                    if (line.empty() || line[0] == ';' || line[0] == '#') continue;
                    if (line.front() == '[' && line.back() == ']') {
                        inGeneral = line == "[General]";
                        continue;
                    }
                    if (!inGeneral || line.rfind("Underwear", 0) != 0) continue;
                    const auto equal = line.find('=');
                    const auto tilde = line.find('~', equal == std::string::npos ? 0 : equal + 1);
                    if (equal == std::string::npos || tilde == std::string::npos) continue;

                    auto formText = line.substr(equal + 1, tilde - equal - 1);
                    formText.erase(0, formText.find_first_not_of(" \t"));
                    formText.erase(formText.find_last_not_of(" \t") + 1);
                    auto plugin = line.substr(tilde + 1);
                    plugin.erase(0, plugin.find_first_not_of(" \t"));
                    plugin.erase(plugin.find_last_not_of(" \t\r\n") + 1);
                    try {
                        const auto localID = static_cast<RE::FormID>(std::stoul(formText, nullptr, 0));
                        auto* armor = RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectARMO>(localID, plugin);
                        constexpr auto bodySlot = static_cast<uint32_t>(RE::BIPED_MODEL::BipedObjectSlot::kBody);
                        if (armor && (static_cast<uint32_t>(armor->GetSlotMask()) & bodySlot) != 0) {
                            result.push_back(armor);
                        }
                    } catch (const std::exception&) {
                        logger::warn("LootUnderwear: could not parse configured form '{}~{}'.", formText, plugin);
                    }
                }
                logger::info("LootUnderwear: loaded {} configured body-slot item(s).", result.size());
                return result;
            }();

            if (candidates.empty()) return nullptr;
            thread_local std::mt19937 rng{ std::random_device{}() };
            std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
            return candidates[pick(rng)];
        }

        void RestoreCorpseUnderwear(RE::TESObjectREFR* sourceRef, RE::TESBoundObject* lootedItem)
        {
            auto* lootedArmor = lootedItem ? lootedItem->As<RE::TESObjectARMO>() : nullptr;
            auto* corpse = sourceRef ? sourceRef->As<RE::Actor>() : nullptr;
            if (!lootedArmor || !corpse || !corpse->IsDead()) return;
            constexpr auto bodySlot = static_cast<uint32_t>(RE::BIPED_MODEL::BipedObjectSlot::kBody);
            if ((static_cast<uint32_t>(lootedArmor->GetSlotMask()) & bodySlot) == 0) return;

            // Only replace underwear after the last body-slot garment is removed.
            for (const auto& [item, inv] : corpse->GetInventory()) {
                auto* armor = item ? item->As<RE::TESObjectARMO>() : nullptr;
                if (armor && inv.first > 0 &&
                    (static_cast<uint32_t>(armor->GetSlotMask()) & bodySlot) != 0) {
                    return;
                }
            }

            auto* underwear = ChooseConfiguredUnderwear();
            if (!underwear) return;
            corpse->AddObjectToContainer(underwear, nullptr, 1, nullptr);
            if (auto* equipManager = RE::ActorEquipManager::GetSingleton()) {
                equipManager->EquipObject(corpse, underwear, nullptr, 1, nullptr, false, false, false, true);
                logger::info("LootUnderwear: equipped {} on stripped corpse {:08X} after follower loot.",
                    underwear->GetName(), corpse->GetFormID());
            }
        }

        // ── OTOMATİK KUŞANIM ──
        // Yağmalanan eşya takipçinin üzerindekilerden daha iyiyse hemen kuşanılır.
        // Silah: saldırı hasarı karşılaştırılır. Zırh: aynı vücut slotundaki
        // giyili parça ile zırh değeri karşılaştırılır (takı/kolye hariç).
        void TryAutoEquip(RE::Actor* follower, FollowerProfile* profile, RE::TESBoundObject* obj)
        {
            if (!follower || !obj) return;
            if (IsTngHiddenBodyItem(obj)) return;
            auto settings = Settings::GetSingleton();
            if (!settings->enableAutoEquip) return;

            // Savaştaki veya 3D hazır olmayan aktöre auto-equip yapma:
            // EquipObject animasyon geçişi sırasında çağrılırsa silah
            // "T-pose" gibi elde asılı takılı kalabilir.
            if (follower->IsInCombat() || !follower->Is3DLoaded()) return;

            auto equipMgr = RE::ActorEquipManager::GetSingleton();
            if (!equipMgr) return;

            auto animMgr = AnimationManager::GetSingleton();


            // ── KIYAFET KORUMA KURALI ──
            // Korunan (kaydedilmiş) bir eşyanın üzerine otomatik giyim YAZMAZ.
            // Koruma listesi boşsa (kayıt yapılmamışsa) takipçi serbestçe giyinebilir.
            auto isProtected = [&](RE::FormID formID) {
                return profile && profile->IsProtected(formID);
            };

            if (auto weapon = obj->As<RE::TESObjectWEAP>()) {
                auto* currentForm = follower->GetEquippedObject(false);
                auto* current = currentForm ? currentForm->As<RE::TESObjectWEAP>() : nullptr;
                if (current && isProtected(current->GetFormID())) {
                    logger::info("AutoEquip: {} kept protected weapon {}.", follower->GetName(), current->GetName());
                    return;
                }

                float currentDamage = current ? static_cast<float>(current->GetAttackDamage()) : 0.0f;
                float newDamage = static_cast<float>(weapon->GetAttackDamage());

                bool newEnchanted = (weapon->formEnchanting != nullptr);
                bool curEnchanted = (current && current->formEnchanting != nullptr);

                // Silah yükseltmesi mi? Hasar daha yüksek VEYA yeni silah büyülü (mevcut büyüsüzken)
                bool isUpgrade = (newDamage > currentDamage) || (newEnchanted && !curEnchanted);

                if (isUpgrade) {
                    if (settings->promptOnBetterEquipment) {
                        std::string reason = std::format("Hasar: {:.0f} (Eski: {:.0f}){}",
                            newDamage, currentDamage, newEnchanted ? ", Büyülü" : "");
                        EquipPrompt::ShowPrompt(follower, profile, weapon, current, reason);
                        return;
                    }

                    // Otomatik kuşanma modu (onay sorma kapalıysa)
                    if (currentForm && profile) {
                        profile->ProtectItem(currentForm->GetFormID(), false);
                    }
                    if (profile) {
                        profile->ProtectItem(weapon->GetFormID(), true);
                    }
                    equipMgr->EquipObject(follower, weapon, nullptr, 1, nullptr, false, false, false, true);
                    animMgr->SheatheWeapon(follower);
                    if (settings->notifyAutoEquip) {
                        NotificationManager::GetSingleton()->ShowHUDMessage(
                            std::format("{} kuşanıldı: {}.", follower->GetName(), weapon->GetName()));
                    }
                    logger::debug("AutoEquip: {} equipped weapon {} (dmg {:.0f} > {:.0f})",
                        follower->GetName(), weapon->GetName(), newDamage, currentDamage);
                }
                return;
            }

            if (auto armor = obj->As<RE::TESObjectARMO>()) {
                using Slot = RE::BIPED_MODEL::BipedObjectSlot;
                auto newMask = static_cast<uint32_t>(armor->GetSlotMask());

                // Takı (kolye/yüzük) otomatik kuşanılmaz
                constexpr uint32_t kJewelryMask = static_cast<uint32_t>(Slot::kAmulet) |
                                                  static_cast<uint32_t>(Slot::kRing);
                constexpr uint32_t kIgnoredBits = kJewelryMask | static_cast<uint32_t>(Slot::kShield);
                if ((newMask & kJewelryMask) != 0 && (newMask & ~kIgnoredBits) == 0) {
                    return;
                }

                // Do not rely only on the engine's current worn flags here.
                // Outfit/framework changes during fast travel can temporarily
                // make those flags stale while the protected item remains in
                // the follower's inventory. A protected item occupying the
                // same biped slot is a hard lock for auto-equip.
                for (const auto& [item, inv] : follower->GetInventory()) {
                    auto* protectedArmor = item ? item->As<RE::TESObjectARMO>() : nullptr;
                    if (!protectedArmor || inv.first <= 0 || !isProtected(protectedArmor->GetFormID())) continue;
                    const auto protectedMask = static_cast<uint32_t>(protectedArmor->GetSlotMask());
                    if ((protectedMask & newMask) != 0) {
                        logger::info("AutoEquip: {} refused {} because protected armor {} occupies the same slot ({} / {}).",
                            follower->GetName(), armor->GetName(), protectedArmor->GetName(), protectedMask, newMask);
                        return;
                    }
                }

                // Aynı slotu kaplayan giyili parçayı bul
                RE::TESObjectARMO* wornInSlot = nullptr;
                float wornRating = 0.0f;
                bool slotOccupied = false;
                for (const auto& [item, inv] : follower->GetInventory()) {
                    auto* a = item ? item->As<RE::TESObjectARMO>() : nullptr;
                    if (!a || inv.first <= 0 || !inv.second || !inv.second->extraLists) continue;
                    bool worn = false;
                    for (auto* xList : *inv.second->extraLists) {
                        if (xList && (xList->HasType(RE::ExtraDataType::kWorn) ||
                                      xList->HasType(RE::ExtraDataType::kWornLeft))) {
                            worn = true;
                            break;
                        }
                    }
                    if (!worn) continue;
                    if ((static_cast<uint32_t>(a->GetSlotMask()) & newMask) != 0) {
                        if (isProtected(a->GetFormID())) {
                            logger::info("AutoEquip: {} kept protected armor {} instead of {}.",
                                follower->GetName(), a->GetName(), armor->GetName());
                            return;
                        }
                        slotOccupied = true;
                        if (!wornInSlot || a->GetArmorRating() < wornRating) {
                            wornInSlot = a;
                            wornRating = a->GetArmorRating();
                        }
                    }
                }

                float newRating = armor->GetArmorRating();
                bool newEnchanted = (armor->formEnchanting != nullptr);
                bool curEnchanted = (wornInSlot && wornInSlot->formEnchanting != nullptr);

                // Büyücü / Mage kontrolü
                bool isMage = profile && (profile->role == FollowerRole::Mage ||
                                          profile->role == FollowerRole::Necromancer ||
                                          profile->role == FollowerRole::Druid);

                bool isUpgrade = false;
                if (isMage && settings->magePreferRobes) {
                    // Mage robe preference is a real armor-type preference: do not
                    // replace a robe/clothing slot with light or heavy armor just
                    // because its armor rating is higher.
                    const bool newIsClothing = armor->GetArmorType() == RE::BGSBipedObjectForm::ArmorType::kClothing;
                    const bool currentIsClothing = wornInSlot &&
                        wornInSlot->GetArmorType() == RE::BGSBipedObjectForm::ArmorType::kClothing;
                    if (newIsClothing) {
                        isUpgrade = !slotOccupied || (wornInSlot && !currentIsClothing) ||
                            (currentIsClothing && newEnchanted && !curEnchanted);
                    }
                } else {
                    // Savaşçı / Genel takipçiler: daha yüksek zırh veya büyülü alternatif
                    isUpgrade = (!slotOccupied && newRating > 0.0f) ||
                                (newRating > wornRating) ||
                                (newEnchanted && !curEnchanted && newRating >= (wornRating * 0.8f));
                }

                if (isUpgrade) {
                    if (settings->promptOnBetterEquipment) {
                        std::string reason = std::format("Zırh: {:.0f} (Eski: {:.0f}){}",
                            newRating, wornRating, newEnchanted ? ", Büyülü" : "");
                        EquipPrompt::ShowPrompt(follower, profile, armor, wornInSlot, reason);
                        return;
                    }

                    // Otomatik kuşanma modu
                    if (wornInSlot && profile) {
                        profile->ProtectItem(wornInSlot->GetFormID(), false);
                    }
                    if (profile) {
                        profile->ProtectItem(armor->GetFormID(), true);
                    }
                    equipMgr->EquipObject(follower, armor, nullptr, 1, nullptr, false, false, false, true);
                    if (settings->notifyAutoEquip) {
                        NotificationManager::GetSingleton()->ShowHUDMessage(
                            std::format("{} kuşanıldı: {}.", follower->GetName(), armor->GetName()));
                    }
                    logger::debug("AutoEquip: {} equipped armor {} (rating {:.0f} > {:.0f})",
                        follower->GetName(), armor->GetName(), newRating, wornRating);
                }
            }
        }
    }

    bool LootManager::IsLootableItem(RE::TESBoundObject* item)
    {
        if (!item) return false;

        // Gold handled separately
        if (item->GetFormID() == 0x0000000F) return false;

        // Internal / non-playable records (body addons, mod placeholders) have no name
        const char* itemName = item->GetName();
        if (!itemName || itemName[0] == '\0') return false;

        // İç çamaşırı asla yağmalanmaz — ölü NPC'ler çıplak kalmasın
        // (Underwear modları ölü NPC'lere "Underwear" itemi ekler)
        {
            std::string lower(itemName);
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("underwear") != std::string::npos ||
                lower.find("under clothes") != std::string::npos ||
                lower.find("underclothes") != std::string::npos ||
                lower.find("iç çamaşır") != std::string::npos) {
                return false;
            }
        }

        if (auto armo = item->As<RE::TESObjectARMO>()) {
            if (IsTngHiddenBodyItem(armo)) return false;
            if ((armo->GetFormFlags() & RE::TESObjectARMO::RecordFlags::kNonPlayable) != 0) return false;
            if (armo->GetArmorRating() == 0 && armo->GetWeight() == 0.0f && armo->GetGoldValue() <= 0) return false;
        }

        // Raw meat is never looted (followers only eat cooked food)
        if (ItemEvaluator::GetSingleton()->IsRawMeat(item)) return false;

        return true;
    }

    bool LootManager::IsQuestItem(RE::TESObjectREFR* ref)
    {
        if (!ref) return false;
        if (ref->extraList.GetByType(RE::ExtraDataType::kFromAlias)) return true;
        if (ref->extraList.GetByType(RE::ExtraDataType::kAliasInstanceArray)) return true;
        return false;
    }

    void LootManager::QueuePlayerLootDistribution(RE::FormID containerID)
    {
        if (containerID == 0 || !Settings::GetSingleton()->autoLootCorpses) return;
        EquipPrompt::LootWorkStarted();
        std::lock_guard lock(_lootMutex);
        if (_playerLootRequests.size() >= 32) _playerLootRequests.pop_front();
        _playerLootRequests.push_back(containerID);
    }

    bool LootManager::HasPendingLootWork()
    {
        {
            std::lock_guard lock(_lootMutex);
            if (!_playerLootRequests.empty()) return true;
        }
        std::lock_guard lock(_transferMutex);
        return !_pendingTransfers.empty();
    }

    void LootManager::ProcessPlayerLootDistributions()
    {
        // Finish one container's transfer queue before reading another closed
        // container. Mixing sources lets stale transfers from an earlier menu
        // run alongside a newly opened inventory session.
        {
            std::lock_guard lock(_transferMutex);
            if (!_pendingTransfers.empty()) {
                logger::debug("LootShare: deferring next container; {} transfer(s) remain.", _pendingTransfers.size());
                return;
            }
        }

        RE::FormID containerID = 0;
        {
            std::lock_guard lock(_lootMutex);
            if (_playerLootRequests.empty()) return;
            containerID = _playerLootRequests.front();
            _playerLootRequests.pop_front();
        }
        logger::info("LootShare: processing closed container {:08X}.", containerID);
        auto* container = RE::TESForm::LookupByID<RE::TESObjectREFR>(containerID);
        logger::info("LootShare: container lookup {:08X} {}.", containerID, container ? "succeeded" : "failed");
        if (container) ProcessContainerLoot(container);
    }

    void LootManager::ProcessContainerLoot(RE::TESObjectREFR* corpseRef)
    {
        if (!corpseRef || !Settings::GetSingleton()->autoLootCorpses) return;
        if (IsQuestItem(corpseRef)) return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto isPlayerOwner = [player](RE::TESForm* owner) {
            if (!player || !owner) return false;
            if (owner == player || owner == player->GetActorBase()) return true;
            auto* faction = owner->As<RE::TESFaction>();
            return faction && player->IsInFaction(faction);
        };
        if (isPlayerOwner(corpseRef->GetOwner()) || isPlayerOwner(corpseRef->GetFactionOwner())) {
            logger::info("LootShare: skipped explicitly player-owned container {:08X}.", corpseRef->GetFormID());
            return;
        }
        auto* cell = corpseRef->GetParentCell();
        if (cell && (isPlayerOwner(cell->GetOwner()) || isPlayerOwner(cell->GetFactionOwner()))) {
            logger::info("LootShare: skipped container {:08X} in a player-owned cell.", corpseRef->GetFormID());
            return;
        }
        auto followers = FollowerManager::GetSingleton()->GetActiveFollowers();
        struct LootRecipient
        {
            RE::Actor* actor;
            std::shared_ptr<FollowerProfile> profile;
        };
        std::vector<LootRecipient> recipients;
        for (auto* follower : followers) {
            auto profile = follower ? FollowerManager::GetSingleton()->GetProfile(follower->GetFormID()) : nullptr;
            if (!follower || !profile || profile->isAnimal || !profile->preferences.allowLoot ||
                follower->IsDead() || follower->IsInCombat()) {
                continue;
            }
            recipients.push_back({ follower, std::move(profile) });
        }
        std::sort(recipients.begin(), recipients.end(), [](const LootRecipient& a, const LootRecipient& b) {
            return a.actor->GetFormID() < b.actor->GetFormID();
        });
        if (recipients.empty()) {
            logger::info("LootShare: no eligible active followers for container {:08X}.", corpseRef->GetFormID());
            return;
        }

        logger::info("LootShare: reading inventory for container {:08X}.", corpseRef->GetFormID());
        auto inventory = corpseRef->GetInventory();
        logger::info("LootShare: inventory read complete for {:08X}; {} entries.", corpseRef->GetFormID(), inventory.size());
        auto settings = Settings::GetSingleton();
        const size_t n = recipients.size();
        size_t cursor = _nextRecipientOffset % n;
        int32_t queuedItems = 0;

        // Death Drop Overhaul leaves real, tracked weapon/shield references in
        // the world. Those are not part of the corpse's normal inventory list,
        // so queue them separately for a nearby follower to physically pick up.
        // The ExtraItemDropper tag ties each reference to the exact dead NPC,
        // avoiding a broad scan of unrelated ground items.
        if (auto* corpse = corpseRef->As<RE::Actor>(); corpse && corpse->IsDead() && cell) {
            constexpr float kGroundLootRadius = 1400.0f;  // about 20 metres
            constexpr uint32_t kShieldSlot = static_cast<uint32_t>(RE::BIPED_MODEL::BipedObjectSlot::kShield);
            size_t dropCursor = cursor;
            int32_t queuedGroundItems = 0;
            cell->ForEachReference([&](RE::TESObjectREFR* ref) {
                if (!ref || !ref->extraList.HasType(RE::ExtraDataType::kItemDropper)) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                auto* dropperData = ref->extraList.GetByType<RE::ExtraItemDropper>();
                auto dropperRef = dropperData ? dropperData->dropper.get() : RE::NiPointer<RE::TESObjectREFR>{};
                if (!dropperRef || dropperRef->GetFormID() != corpse->GetFormID()) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                auto* item = ref->GetBaseObject() ? ref->GetBaseObject()->As<RE::TESBoundObject>() : nullptr;
                if (!item) return RE::BSContainer::ForEachResult::kContinue;

                const bool isWeapon = item->As<RE::TESObjectWEAP>() != nullptr;
                auto* armor = item->As<RE::TESObjectARMO>();
                const bool isShield = armor &&
                    (static_cast<uint32_t>(armor->GetSlotMask()) & kShieldSlot) != 0;
                if ((!isWeapon || !settings->lootWeapons) && (!isShield || !settings->lootArmor)) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                for (size_t i = 0; i < n; ++i) {
                    const size_t recipientIndex = (dropCursor + i) % n;
                    auto* follower = recipients[recipientIndex].actor;
                    const auto& preferences = recipients[recipientIndex].profile->preferences;
                    if ((isWeapon && !preferences.allowLootWeapons) ||
                        (isShield && !preferences.allowLootArmor)) {
                        continue;
                    }
                    if (!follower->Is3DLoaded() || follower->GetParentCell() != cell ||
                        follower->GetPosition().GetDistance(ref->GetPosition()) > kGroundLootRadius) {
                        continue;
                    }
                    DroppedItemRecovery::GetSingleton()->RecordWorldLoot(
                        follower->GetFormID(), item->GetFormID(), ref->GetHandle(), kGroundLootRadius);
                    dropCursor = (recipientIndex + 1) % n;
                    ++queuedGroundItems;
                    logger::info("LootShare: queued Death Drop Overhaul ground item {} for follower={:08X} from corpse={:08X}.",
                        item->GetName(), follower->GetFormID(), corpse->GetFormID());
                    break;
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            logger::info("LootShare: queued {} nearby Death Drop Overhaul weapon/shield reference(s) for corpse={:08X}.",
                queuedGroundItems, corpse->GetFormID());
        }

        logger::info("LootShare: container {:08X} closed; sharing only its remaining {} inventory entries among {} followers.",
            corpseRef->GetFormID(), inventory.size(), n);

        auto queueTransfer = [&](RE::TESBoundObject* item, int32_t count, size_t recipient, bool gold) {
            if (!item || count <= 0) return;
            PendingTransfer transfer;
            transfer.transferID = _nextTransferID.fetch_add(1, std::memory_order_relaxed);
            transfer.itemFormID = item->GetFormID();
            transfer.actorFormID = recipients[recipient].actor->GetFormID();
            transfer.sourceFormID = corpseRef->GetFormID();
            transfer.count = count;
            transfer.isGold = gold;
            // Auto-equip decisions are queued by EquipPrompt and displayed in
            // PrismaUI after the complete container distribution finishes.
            transfer.autoEquip = !gold && (item->As<RE::TESObjectWEAP>() || item->As<RE::TESObjectARMO>());
            transfer.followerName = recipients[recipient].actor->GetName();
            transfer.itemName = gold ? "Gold" : item->GetName();
            std::lock_guard lock(_transferMutex);
            _pendingTransfers.push_back(std::move(transfer));
            const auto& queued = _pendingTransfers.back();
            logger::info("LootShare: queued transfer #{} item={:08X} count={} recipient={:08X} source={:08X} gold={} autoEquip={} pending={}",
                queued.transferID, queued.itemFormID, queued.count, queued.actorFormID, queued.sourceFormID,
                queued.isGold, queued.autoEquip, _pendingTransfers.size());
            ++queuedItems;
        };

        for (const auto& [item, data] : inventory) {
            const int32_t count = data.first;
            if (!item || count <= 0) continue;
            const bool gold = item->GetFormID() == 0x0000000F;
            if (gold) {
                if (!settings->lootGold) continue;
                const int32_t share = count / static_cast<int32_t>(n);
                if (share <= 0) continue;
                for (size_t i = 0; i < n; ++i) queueTransfer(item, share, i, true);
                // Gold remainder stays in the container. Do not let gold change
                // the rotating first recipient for ordinary items.
                continue;
            }
            if (!IsLootableItem(item)) continue;

            bool enabled = settings->lootMisc;
            if (item->As<RE::TESObjectWEAP>()) enabled = settings->lootWeapons;
            else if (item->As<RE::TESObjectARMO>()) enabled = settings->lootArmor;
            else if (item->As<RE::TESAmmo>()) enabled = settings->lootArrows;
            else if (auto* alchemy = item->As<RE::AlchemyItem>()) {
                if (alchemy->IsPoison()) enabled = settings->lootPotions;
                else enabled = settings->lootPotions || settings->lootFood;
            } else if (item->As<RE::IngredientItem>()) enabled = settings->lootIngredients;
            else if (item->As<RE::TESSoulGem>()) enabled = settings->lootSoulGems;
            if (!enabled) continue;

            const int32_t each = count / static_cast<int32_t>(n);
            const int32_t remainder = count % static_cast<int32_t>(n);
            for (size_t i = 0; i < n; ++i) {
                int32_t assigned = each;
                if (i < static_cast<size_t>(remainder)) ++assigned;
                const size_t recipient = (cursor + i) % n;
                if (assigned > 0) queueTransfer(item, assigned, recipient, false);
            }
            cursor = (cursor + static_cast<size_t>(count % static_cast<int32_t>(n))) % n;
        }
        _nextRecipientOffset = (_nextRecipientOffset + 1) % n;
        logger::info("LootShare: queued {} item transfers from container {:08X}; remainder stays behind.",
            queuedItems, corpseRef->GetFormID());
    }
    void LootManager::FlushPendingTransfers()
    {
        // Keep each UI task to a single engine inventory mutation. Skyrim's
        // RemoveItem can trigger inventory/equipment callbacks; batching several
        // actors or sources in one task risks hanging the menu/game thread.
        std::vector<PendingTransfer> batch;
        {
            std::lock_guard lock(_transferMutex);
            if (_pendingTransfers.empty()) return;
            const auto now = std::chrono::steady_clock::now();
            for (auto it = _pendingTransfers.begin(); it != _pendingTransfers.end() &&
                 batch.size() < static_cast<size_t>(kMaxTransfersPerTick);) {
                if (it->readyAt <= now) {
                    batch.push_back(std::move(*it));
                    it = _pendingTransfers.erase(it);
                } else {
                    ++it;
                }
            }
            if (batch.empty()) return;
        }

        auto notifier = NotificationManager::GetSingleton();
        auto settings = Settings::GetSingleton();
        logger::info("LootTransfer: isolated task started; {} item record(s) dequeued.", batch.size());

        for (auto& pt : batch) {
            auto* itemForm   = RE::TESForm::LookupByID(pt.itemFormID);
            auto* actorForm  = RE::TESForm::LookupByID(pt.actorFormID);
            auto* sourceForm = RE::TESForm::LookupByID(pt.sourceFormID);

            auto* item   = itemForm   ? itemForm->As<RE::TESBoundObject>() : nullptr;
            auto* actor  = actorForm  ? actorForm->As<RE::Actor>()         : nullptr;
            auto* source = sourceForm ? sourceForm->As<RE::TESObjectREFR>() : nullptr;

            logger::info("LootTransfer #{}: form lookups item={} actor={} source={}",
                pt.transferID, item ? "ok" : "null", actor ? "ok" : "null", source ? "ok" : "null");
            if (!item || !actor || !source) {
                logger::warn("LootTransfer #{}: skipped because a form lookup failed.", pt.transferID);
                continue;
            }

            // Re-read only the requested stack immediately before moving it.
            // The player may have reopened the source or another system may
            // have changed its inventory since the distribution was queued.
            logger::info("LootTransfer #{}: source count check started item={:08X} source={:08X} expected={}",
                pt.transferID, pt.itemFormID, pt.sourceFormID, pt.count);
            const auto currentStack = source->GetInventoryCounts(
                [itemID = pt.itemFormID](RE::TESBoundObject& candidate) {
                    return candidate.GetFormID() == itemID;
                }, true);
            const auto currentItem = currentStack.find(item);
            logger::info("LootTransfer #{}: source count check finished; matching={} count={}",
                pt.transferID, currentItem != currentStack.end(),
                currentItem != currentStack.end() ? currentItem->second : 0);
            if (currentItem == currentStack.end() || currentItem->second < pt.count) {
                logger::info("LootTransfer #{}: skipped stale transfer for {} x{} from {:08X} (stack changed).",
                    pt.transferID, item->GetName(), pt.count, pt.sourceFormID);
                continue;
            }

            logger::info("LootTransfer #{}: RemoveItem started item={} x{} source={:08X} recipient={:08X}.",
                pt.transferID, item->GetName(), pt.count, pt.sourceFormID, pt.actorFormID);
            source->RemoveItem(item, pt.count, RE::ITEM_REMOVE_REASON::kRemove, nullptr, actor);
            logger::info("LootTransfer #{}: RemoveItem returned.", pt.transferID);

            // Underwear.dll intentionally excludes followers from its own
            // stripping hook. Restore a configured body-slot item on the NPC
            // corpse when a follower removes its final body garment.
            RestoreCorpseUnderwear(source, item);

            if (pt.isGold) {
                logger::info("LootTransfer #{}: virtual gold accounting started amount={}.", pt.transferID, pt.count);
                EconomyManager::GetSingleton()->AddGold(pt.actorFormID, pt.count);
                logger::info("LootTransfer #{}: virtual gold accounting returned.", pt.transferID);
            }

            if (pt.autoEquip) {
                logger::info("LootTransfer #{}: auto-equip started.", pt.transferID);
                auto profile = FollowerManager::GetSingleton()->GetProfile(actor->GetFormID());
                if (profile) TryAutoEquip(actor, profile.get(), item);
                logger::info("LootTransfer #{}: auto-equip returned; profile={}.", pt.transferID, profile ? "ok" : "null");
            }

            if (settings->notifyLoot && !pt.followerName.empty() && !pt.itemName.empty()) {
                logger::info("LootTransfer #{}: notification enqueue started.", pt.transferID);
                notifier->QueueLootNotification(pt.followerName, pt.itemName, static_cast<uint32_t>(pt.count));
                logger::info("LootTransfer #{}: notification enqueue returned.", pt.transferID);
            }
        }

        // Son transfer bitti mi? Bildirim flushu sadece kuyruk tamamen boşaldığında.
        {
            std::lock_guard lock(_transferMutex);
            logger::info("LootTransfer: transfer queue check; pending={}.", _pendingTransfers.size());
            if (_pendingTransfers.empty() && settings->notifyLoot) {
                logger::info("LootTransfer: loot notification flush started.");
                notifier->FlushLootNotifications();
                logger::info("LootTransfer: loot notification flush returned.");
            }
        }
        logger::info("LootTransfer: isolated task finished.");
    }

    void LootManager::ScheduleProcessing()
    {

        bool expected = false;
        if (!_processingScheduled.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }

        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            _processingScheduled.store(false, std::memory_order_release);
            return;
        }

        // Loot processing touches TES forms, actor processes, inventories and
        // animation graphs. SKSE AddTask may run on a worker thread; use the UI
        // task queue so these engine mutations are serialized with game/UI work.
        taskInterface->AddUITask([this]() {
            logger::info("LootTask: UI task started.");
            ProcessPlayerLootDistributions();
            FlushPendingTransfers();
            if (!HasPendingLootWork()) {
                EquipPrompt::StartAfterLootDistribution();
                EquipPrompt::Update();
            }

            _processingScheduled.store(false, std::memory_order_release);
            logger::info("LootTask: UI task finished; scheduled flag cleared.");
        });
        logger::debug("LootTask: UI task queued.");
    }


}
