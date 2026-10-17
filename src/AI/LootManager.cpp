#include "AI/LootManager.h"
#include "AI/ItemEvaluator.h"
#include "AI/AnimationManager.h"
#include "AI/DroppedItemRecovery.h"
#include "Economy/EconomyManager.h"
#include "UI/NotificationManager.h"
#include "UI/EquipPrompt.h"
#include "Core/Settings.h"
#include "Core/ClassInterface.h"
#include "Core/TNGCompatibility.h"
#include <random>

namespace FollowerUnbound
{
    namespace
    {
        std::string DescribeGearEnchantment(RE::TESBoundObject* item, const RE::InventoryEntryData* entry)
        {
            RE::EnchantmentItem* enchantment = entry ? entry->GetEnchantment() : nullptr;
            if (!enchantment) {
                if (auto* weapon = item ? item->As<RE::TESObjectWEAP>() : nullptr) enchantment = weapon->formEnchanting;
                else if (auto* armor = item ? item->As<RE::TESObjectARMO>() : nullptr) enchantment = armor->formEnchanting;
            }
            if (!enchantment) return {};
            std::string description;
            if (const char* name = enchantment->GetName(); name && name[0]) description = name;
            std::string effects;
            uint32_t shown = 0;
            for (auto* effect : enchantment->effects) {
                if (!effect || !effect->baseEffect) continue;
                const char* effectName = effect->baseEffect->GetName();
                if (!effectName || !effectName[0]) continue;
                if (!effects.empty()) effects += "; ";
                effects += effectName;
                if (effect->GetMagnitude() > 0.0f) effects += std::format(" {:.0f} pts", effect->GetMagnitude());
                if (effect->GetDuration() > 0) effects += std::format(" / {}s", effect->GetDuration());
                if (++shown >= 3) break;
            }
            if (!effects.empty()) {
                if (!description.empty()) description += ": ";
                description += effects;
            }
            return description;
        }

        std::string DescribeGearStats(RE::TESBoundObject* item, const RE::InventoryEntryData* entry)
        {
            if (!item) return "None";
            const char* rawName = item->GetName();
            const std::string_view itemName = rawName && rawName[0] ? rawName : "Unknown item";
            std::string stats;
            if (auto* weapon = item->As<RE::TESObjectWEAP>())
                stats = std::format("{} · Damage {} · Value {}", itemName, weapon->GetAttackDamage(), weapon->GetGoldValue());
            else if (auto* armor = item->As<RE::TESObjectARMO>())
                stats = std::format("{} · Armor {} · Value {}", itemName, armor->GetArmorRating(), armor->GetGoldValue());
            else
                stats = std::format("{} · Value {}", itemName, item->GetGoldValue());
            const auto enchantment = DescribeGearEnchantment(item, entry);
            if (!enchantment.empty()) stats += std::format(" · {}", enchantment);
            return stats;
        }

        std::string GetFollowerGearStats(RE::Actor* follower, RE::TESBoundObject* item)
        {
            if (!item) return "None";
            if (follower) {
                const auto inventory = follower->GetInventory();
                if (const auto it = inventory.find(item); it != inventory.end())
                    return DescribeGearStats(item, it->second.second.get());
            }
            return DescribeGearStats(item, nullptr);
        }

        bool IsNonPlayerOwned(RE::TESForm* owner, RE::PlayerCharacter* player)
        {
            if (!owner || !player || owner == player || owner == player->GetActorBase()) return false;
            if (auto* faction = owner->As<RE::TESFaction>()) {
                return !player->IsInFaction(faction);
            }
            return true;
        }

        bool IsClassBoundThief(RE::Actor* actor)
        {
            auto* api = ClassInterface::GetSingleton()->GetClassBoundAPI();
            return api && actor && api->IsActorEligible(actor) &&
                api->GetFollowerClass(actor) == KingCore::ClassID::Thief;
        }

        bool IsDeficientSupplyItem(RE::Actor* follower, FollowerProfile* profile, RE::TESBoundObject* item)
        {
            if (!follower || !profile || !item) return false;
            const auto& p = profile->preferences;
            auto* eval = ItemEvaluator::GetSingleton();

            if (auto* alch = item->As<RE::AlchemyItem>()) {
                if (alch->IsPoison()) {
                    return p.buyPoisons && (eval->CountPoisons(follower) < p.maxPoisons);
                }
                if (alch->IsFood()) {
                    if (p.buyFoodLarge && eval->CountFoodLarge(follower) < p.maxFoodLarge) return true;
                    if (p.buyFoodMedium && eval->CountFoodMedium(follower) < p.maxFoodMedium) return true;
                    if (p.buyFoodSmall && eval->CountFoodSmall(follower) < p.maxFoodSmall) return true;
                    return false;
                }
                if (p.buyHealthPotions && eval->CountHealthPotions(follower) < p.maxHealthPotions) return true;
                if (p.buyMagickaPotions && eval->CountMagickaPotions(follower) < p.maxMagickaPotions) return true;
                if (p.buyStaminaPotions && eval->CountStaminaPotions(follower) < p.maxStaminaPotions) return true;
                return false;
            }
            if (auto* ammo = item->As<RE::TESAmmo>()) {
                if (ammo->IsBolt()) {
                    return p.buyBolts && (eval->CountBolts(follower) < p.maxBolts);
                }
                return p.buyArrows && (eval->CountArrows(follower) < p.maxArrows);
            }
            return false;
        }

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
                    if (settings->equipmentUpgradeChance < 100) {
                        thread_local std::mt19937 rng{ std::random_device{}() };
                        std::uniform_int_distribution<uint32_t> dist(1, 100);
                        if (dist(rng) > settings->equipmentUpgradeChance) {
                            logger::debug("AutoEquip: {} weapon upgrade chance check failed (chance: {}%).",
                                follower->GetName(), settings->equipmentUpgradeChance);
                            return;
                        }
                    }

                    if (settings->promptOnBetterEquipment) {
                        std::string reason = std::format("Hasar: {:.0f} (Eski: {:.0f}){}",
                            newDamage, currentDamage, newEnchanted ? ", Büyülü" : "");
                        EquipPrompt::ShowPrompt(follower, profile, weapon, current, reason,
                            GetFollowerGearStats(follower, current), GetFollowerGearStats(follower, weapon));
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
                    if (settings->equipmentUpgradeChance < 100) {
                        thread_local std::mt19937 rng{ std::random_device{}() };
                        std::uniform_int_distribution<uint32_t> dist(1, 100);
                        if (dist(rng) > settings->equipmentUpgradeChance) {
                            logger::debug("AutoEquip: {} armor upgrade chance check failed (chance: {}%).",
                                follower->GetName(), settings->equipmentUpgradeChance);
                            return;
                        }
                    }

                    if (settings->promptOnBetterEquipment) {
                        std::string reason = std::format("Zırh: {:.0f} (Eski: {:.0f}){}",
                            newRating, wornRating, newEnchanted ? ", Büyülü" : "");
                        EquipPrompt::ShowPrompt(follower, profile, armor, wornInSlot, reason,
                            GetFollowerGearStats(follower, wornInSlot), GetFollowerGearStats(follower, armor));
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

    void LootManager::QueuePlayerLootDistribution(RE::FormID containerID, uint32_t delayMs)
    {
        if (containerID == 0 || !Settings::GetSingleton()->autoLootCorpses) return;
        EquipPrompt::LootWorkStarted();
        std::lock_guard lock(_lootMutex);
        // Avoid duplicate if the same corpse is already at the end of the queue
        if (!_playerLootRequests.empty() && _playerLootRequests.back().formID == containerID) return;

        auto now = std::chrono::steady_clock::now();
        // Initial delay before followers start looting it
        auto startAt = now + std::chrono::milliseconds(delayMs);
        // If there are previous corpses queued, chain them sequentially so they are looted in order
        if (!_playerLootRequests.empty() && _playerLootRequests.back().availableAt > now) {
            startAt = std::max(startAt, _playerLootRequests.back().availableAt + std::chrono::milliseconds(1500));
        }

        if (_playerLootRequests.size() >= 32) _playerLootRequests.pop_front();
        _playerLootRequests.push_back({ containerID, startAt });
        logger::info("LootShare: queued container {:08X} (queue size={}, delay={}ms).",
            containerID, _playerLootRequests.size(), delayMs);
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
            const auto now = std::chrono::steady_clock::now();
            if (now < _playerLootRequests.front().availableAt) {
                return; // Wait for the 2-second delay before followers begin looting
            }
            containerID = _playerLootRequests.front().formID;
        }

        logger::info("LootShare: processing closed container {:08X}.", containerID);
        auto* container = RE::TESForm::LookupByID<RE::TESObjectREFR>(containerID);
        logger::info("LootShare: container lookup {:08X} {}.", containerID, container ? "succeeded" : "failed");
        if (!container) {
            // Container invalid or despawned, pop it
            std::lock_guard lock(_lootMutex);
            if (!_playerLootRequests.empty() && _playerLootRequests.front().formID == containerID) {
                _playerLootRequests.pop_front();
            }
            return;
        }

        const bool processed = ProcessContainerLoot(container);
        if (processed) {
            std::lock_guard lock(_lootMutex);
            if (!_playerLootRequests.empty() && _playerLootRequests.front().formID == containerID) {
                _playerLootRequests.pop_front();
            }
        } else {
            // Followers were temporarily ineligible (e.g. still concluding combat); do not drop!
            std::lock_guard lock(_lootMutex);
            if (!_playerLootRequests.empty() && _playerLootRequests.front().formID == containerID) {
                _playerLootRequests.front().availableAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
            }
        }
    }

    bool LootManager::ProcessContainerLoot(RE::TESObjectREFR* corpseRef)
    {
        if (!corpseRef) return true;
        if (!Settings::GetSingleton()->autoLootCorpses) {
            logger::info("LootShare: skipped container {:08X} because autoLootCorpses setting is false.", corpseRef->GetFormID());
            return true;
        }
        // Static quest containers (chests tied to a quest alias) are protected.
        // Dead actors/NPCs are enemy corpses and must NEVER be skipped by alias checks!
        auto* corpseActor = corpseRef->As<RE::Actor>();
        if (!corpseActor && IsQuestItem(corpseRef)) {
            logger::info("LootShare: skipped container {:08X} because it is a quest item / alias.", corpseRef->GetFormID());
            return true;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto isPlayerOwner = [player](RE::TESForm* owner) {
            if (!player || !owner) return false;
            if (owner == player || owner == player->GetActorBase()) return true;
            auto* faction = owner->As<RE::TESFaction>();
            return faction && player->IsInFaction(faction);
        };

        auto* cell = corpseRef->GetParentCell();

        // For dead actors/NPCs, NEVER skip based on cell ownership! They are dead enemies in the world.
        if (!corpseActor) {
            if (isPlayerOwner(corpseRef->GetOwner()) || isPlayerOwner(corpseRef->GetFactionOwner())) {
                logger::info("LootShare: skipped explicitly player-owned container {:08X}.", corpseRef->GetFormID());
                return true;
            }
            if (cell && (isPlayerOwner(cell->GetOwner()) || isPlayerOwner(cell->GetFactionOwner()))) {
                logger::info("LootShare: skipped container {:08X} in a player-owned cell.", corpseRef->GetFormID());
                return true;
            }
        }

        auto followers = FollowerManager::GetSingleton()->GetActiveFollowers();
        logger::info("LootShare: active followers retrieved: {} follower(s).", followers.size());
        struct LootRecipient
        {
            RE::Actor* actor;
            std::shared_ptr<FollowerProfile> profile;
            bool classBoundThief;
            float currentWeight;
            float maxWeight;
        };
        static std::unordered_set<RE::FormID> s_encumberedNotified;
        std::vector<LootRecipient> recipients;
        for (auto* follower : followers) {
            if (!follower) continue;
            auto profile = FollowerManager::GetSingleton()->GetProfile(follower->GetFormID());
            if (!profile) {
                logger::info("LootShare: follower {:08X} ({}) has no profile.", follower->GetFormID(), follower->GetName());
                continue;
            }
            if (profile->isAnimal) {
                logger::info("LootShare: follower {} is an animal, skipping.", follower->GetName());
                continue;
            }
            if (!profile->preferences.allowLoot) {
                logger::info("LootShare: follower {} has allowLoot=false in preferences.", follower->GetName());
                continue;
            }
            if (follower->IsDead()) {
                logger::info("LootShare: follower {} is dead, skipping.", follower->GetName());
                continue;
            }
            // If follower is in combat, only skip if the player is also actively in combat.
            // If the player has safely opened and looted the corpse, combat is effectively done.
            if (follower->IsInCombat() && player && player->IsInCombat()) {
                logger::info("LootShare: follower {} and player are both in combat, skipping for now.", follower->GetName());
                continue;
            }
            const float curW = follower->GetWeightInContainer();
            const float maxW = follower->GetTotalCarryWeight();
            if (curW < maxW * 0.9f) {
                s_encumberedNotified.erase(follower->GetFormID());
            }
            recipients.push_back({ follower, std::move(profile), IsClassBoundThief(follower), curW, maxW });
        }
        std::sort(recipients.begin(), recipients.end(), [](const LootRecipient& a, const LootRecipient& b) {
            return a.actor->GetFormID() < b.actor->GetFormID();
        });
        if (recipients.empty()) {
            logger::info("LootShare: no eligible active followers currently for container {:08X}; will retry.", corpseRef->GetFormID());
            return false;
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
            // 1-second interval between items so each follower has time to kneel and play the search animation
            transfer.readyAt = std::chrono::steady_clock::now() + (std::chrono::milliseconds(1000) * queuedItems);
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

        struct LootItemEntry
        {
            RE::TESBoundObject* item{ nullptr };
            int32_t count{ 0 };
            RE::InventoryEntryData* entryData{ nullptr };
            int priority{ 3 };
        };
        std::vector<LootItemEntry> entries;
        entries.reserve(inventory.size());

        for (const auto& [item, data] : inventory) {
            const int32_t count = data.first;
            if (!item || count <= 0) continue;
            const bool gold = item->GetFormID() == 0x0000000F;
            if (!gold && !IsLootableItem(item)) continue;

            int priority = 3;
            if (gold) {
                priority = 0;
            } else {
                bool isDeficient = false;
                for (const auto& rec : recipients) {
                    if (IsDeficientSupplyItem(rec.actor, rec.profile.get(), item)) {
                        isDeficient = true;
                        break;
                    }
                }
                if (isDeficient) {
                    priority = 1;
                } else if (item->As<RE::TESObjectWEAP>() || item->As<RE::TESObjectARMO>() ||
                           item->As<RE::TESAmmo>() || item->As<RE::AlchemyItem>() ||
                           item->As<RE::IngredientItem>() || item->As<RE::TESSoulGem>()) {
                    priority = 2;
                } else {
                    priority = 3;
                }
            }
            entries.push_back({ item, count, data.second.get(), priority });
        }

        std::stable_sort(entries.begin(), entries.end(), [](const LootItemEntry& a, const LootItemEntry& b) {
            return a.priority < b.priority;
        });

        for (const auto& entry : entries) {
            auto* item = entry.item;
            const int32_t count = entry.count;
            if (!item || count <= 0) continue;
            auto* entryData = entry.entryData;
            const bool stolenItem = entryData && IsNonPlayerOwned(entryData->GetOwner(), player);
            std::vector<size_t> eligibleRecipients;
            eligibleRecipients.reserve(n);
            for (size_t i = 0; i < n; ++i) {
                if (!stolenItem || recipients[i].classBoundThief) eligibleRecipients.push_back(i);
            }
            if (eligibleRecipients.empty()) {
                logger::info("LootShare: skipped stolen item {} [{:08X}] from container {:08X}; no ClassBound Thief follower is eligible.",
                    item->GetName(), item->GetFormID(), corpseRef->GetFormID());
                continue;
            }
            const size_t recipientCount = eligibleRecipients.size();
            const size_t recipientStart = stolenItem ? (_nextRecipientOffset % recipientCount) : (cursor % n);
            const bool gold = item->GetFormID() == 0x0000000F;
            if (gold) {
                if (!settings->lootGold) continue;
                const int32_t share = count / static_cast<int32_t>(recipientCount);
                if (share <= 0) continue;
                for (size_t i = 0; i < recipientCount; ++i) {
                    queueTransfer(item, share, eligibleRecipients[(recipientStart + i) % recipientCount], true);
                }
                continue;
            }

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

            const float itemWeight = item->GetWeight();
            int32_t remainingToDistribute = count;

            for (size_t attempt = 0; attempt < recipientCount && remainingToDistribute > 0; ++attempt) {
                const size_t recIdx = eligibleRecipients[(recipientStart + attempt) % recipientCount];
                auto& rec = recipients[recIdx];

                int32_t canTake = remainingToDistribute;
                if (itemWeight > 0.0f) {
                    const float availableWeight = rec.maxWeight - rec.currentWeight;
                    if (availableWeight <= 0.0f) {
                        if (!s_encumberedNotified.contains(rec.actor->GetFormID())) {
                            s_encumberedNotified.insert(rec.actor->GetFormID());
                            NotificationManager::GetSingleton()->ShowHUDMessage(
                                std::format("{}: Artık daha fazla eşya taşıyamam.", rec.actor->GetName()));
                        }
                        continue;
                    }
                    const int32_t maxByWeight = static_cast<int32_t>(availableWeight / itemWeight);
                    if (maxByWeight <= 0) {
                        if (!s_encumberedNotified.contains(rec.actor->GetFormID())) {
                            s_encumberedNotified.insert(rec.actor->GetFormID());
                            NotificationManager::GetSingleton()->ShowHUDMessage(
                                std::format("{}: Artık daha fazla eşya taşıyamam.", rec.actor->GetName()));
                        }
                        continue;
                    }
                    canTake = std::min(remainingToDistribute, maxByWeight);
                }

                if (canTake > 0) {
                    queueTransfer(item, canTake, recIdx, false);
                    rec.currentWeight += (itemWeight > 0.0f ? canTake * itemWeight : 0.0f);
                    remainingToDistribute -= canTake;
                }
            }

            if (!stolenItem && remainingToDistribute < count) {
                cursor = (cursor + 1) % n;
            }
        }
        _nextRecipientOffset = (_nextRecipientOffset + 1) % n;
        logger::info("LootShare: queued {} item transfers from container {:08X}; remainder stays behind.",
            queuedItems, corpseRef->GetFormID());
        return true;
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

            // Play / sustain the body search loot animation on the follower
            AnimationManager::GetSingleton()->PlayLootAnimation(actor, source);

            // Re-read available item count immediately before moving it.
            // The player may have taken items or stack counts may have shifted.
            const int32_t availableCount = ItemEvaluator::GetItemCount(source, item);
            logger::info("LootTransfer #{}: source count check item={:08X} ({}) available={} requested={}",
                pt.transferID, pt.itemFormID, item->GetName(), availableCount, pt.count);

            if (availableCount <= 0) {
                logger::info("LootTransfer #{}: skipped transfer for {} from {:08X} (item no longer present).",
                    pt.transferID, item->GetName(), pt.sourceFormID);
                continue;
            }

            const int32_t transferCount = std::min(pt.count, availableCount);

            logger::info("LootTransfer #{}: RemoveItem started item={} x{} source={:08X} recipient={:08X}.",
                pt.transferID, item->GetName(), transferCount, pt.sourceFormID, pt.actorFormID);
            source->RemoveItem(item, transferCount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, actor);
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
            if (_pendingTransfers.empty()) {
                if (settings->notifyLoot) {
                    logger::info("LootTransfer: loot notification flush started.");
                    notifier->FlushLootNotifications();
                    logger::info("LootTransfer: loot notification flush returned.");
                }
                for (auto* f : FollowerManager::GetSingleton()->GetActiveFollowers()) {
                    if (f) {
                        AnimationManager::GetSingleton()->SheatheWeapon(f);
                    }
                }
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
            logger::debug("LootTask: UI task finished; scheduled flag cleared.");
        });
        logger::debug("LootTask: UI task queued.");
    }


}
