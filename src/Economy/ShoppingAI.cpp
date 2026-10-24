#include "Economy/ShoppingAI.h"
#include "Economy/EconomyManager.h"
#include "AI/ItemEvaluator.h"
#include "AI/ConsumableAI.h"
#include "UI/NotificationManager.h"
#include "UI/EquipPrompt.h"
#include "Core/Settings.h"
#include "Core/TNGCompatibility.h"
#include "Core/ClassInterface.h"
#include "Core/FollowerManager.h"
#include "RE/I/ID.h"
#include "RE/B/BGSEntryPointFunctionDataOneValue.h"
#include "RE/B/BGSEntryPointPerkEntry.h"
#include "RE/G/GameSettingCollection.h"
#include <array>
#include <cmath>
#include <random>

namespace FollowerUnbound
{
    namespace
    {
        class SellPricePerkVisitor final : public RE::PerkEntryVisitor
        {
        public:
            explicit SellPricePerkVisitor(RE::Actor* actor) : _actor(actor) {}

            RE::BSContainer::ForEachResult Visit(RE::BGSPerkEntry* perkEntry) override
            {
                if (!_actor || !perkEntry || perkEntry->GetType() != RE::PERK_ENTRY_TYPE::kEntryPoint) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                auto* entry = static_cast<RE::BGSEntryPointPerkEntry*>(perkEntry);
                if (!entry->IsEntryPoint(RE::BGSEntryPoint::ENTRY_POINTS::kModSellPrices)) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                // Perk ranks are linked. Only the highest owned rank in a chain applies.
                if (entry->perk && entry->perk->nextPerk && _actor->HasPerk(entry->perk->nextPerk)) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                if (entry->entryData.function == RE::BGSEntryPointPerkEntry::EntryData::Function::kMultiplyValue &&
                    entry->functionData && entry->functionData->GetType() == RE::BGSEntryPointFunctionData::FunctionType::kOneValue) {
                    const auto* value = static_cast<const RE::BGSEntryPointFunctionDataOneValue*>(entry->functionData);
                    if (std::isfinite(value->data) && value->data > 0.0f) {
                        _multiplier *= value->data;
                    }
                }
                return RE::BSContainer::ForEachResult::kContinue;
            }

            [[nodiscard]] float GetMultiplier() const { return _multiplier; }

        private:
            RE::Actor* _actor{ nullptr };
            float _multiplier{ 1.0f };
        };

        static uint32_t CalculateGameSellPrice(RE::Actor* seller, int32_t itemBaseValue)
        {
            if (!seller || itemBaseValue <= 0) return 0;

            float barterMax = 3.3f;
            float barterMin = 2.0f;
            float sellPriceCap = 0.95f;
            if (auto* settings = RE::GameSettingCollection::GetSingleton()) {
                if (auto* setting = settings->GetSetting("fBarterMax")) barterMax = setting->GetFloat();
                if (auto* setting = settings->GetSetting("fBarterMin")) barterMin = setting->GetFloat();
                if (auto* setting = settings->GetSetting("fBarterSellMax")) sellPriceCap = setting->GetFloat();
            }

            auto* actorValues = seller->AsActorValueOwner();
            const float rawSpeech = actorValues ? actorValues->GetActorValue(RE::ActorValue::kSpeech) : 15.0f;
            const float speech = std::clamp(rawSpeech, 0.0f, 100.0f);
            const float priceFactor = barterMax - (barterMax - barterMin) * (speech / 100.0f);
            if (!std::isfinite(priceFactor) || priceFactor <= 0.0f) return 1;

            SellPricePerkVisitor perkVisitor(seller);
            if (seller->HasPerkEntries(RE::BGSEntryPoint::ENTRY_POINTS::kModSellPrices)) {
                seller->ForEachPerkEntry(RE::BGSEntryPoint::ENTRY_POINTS::kModSellPrices, perkVisitor);
            }

            const float speechModifier = actorValues ? actorValues->GetActorValue(RE::ActorValue::kSpeechcraftModifier) : 0.0f;
            const float speechPowerModifier = actorValues ? actorValues->GetActorValue(RE::ActorValue::kSpeechcraftPowerModifier) : 0.0f;
            const float sellModifier = perkVisitor.GetMultiplier() *
                std::max(0.0f, 1.0f + speechModifier / 100.0f) *
                std::max(0.0f, 1.0f + speechPowerModifier / 100.0f);

            const float calculated = static_cast<float>(itemBaseValue) * sellModifier / priceFactor;
            const float capped = std::min(calculated, static_cast<float>(itemBaseValue) * std::clamp(sellPriceCap, 0.0f, 1.0f));
            return std::max(1u, static_cast<uint32_t>(std::lround(capped)));
        }

        void MaybePlayShoppingSound(RE::Actor* follower)
        {
            if (!follower || !follower->Is3DLoaded()) return;

            // Shopping can be processed every few seconds for every follower.
            // Keep the sound occasional and impose a shared cooldown so a group
            // shopping together does not produce a burst of coin sounds.
            static std::mt19937 rng{ std::random_device{}() };
            static std::chrono::steady_clock::time_point lastPlayed{};
            static std::unordered_map<RE::FormID, size_t> soundByFollower;
            static constexpr std::array<const char*, 7> soundPaths{
                "sound\\fx\\followerunbound\\coin-donation-1.wav",
                "sound\\fx\\followerunbound\\coin-donation-2.wav",
                "sound\\fx\\followerunbound\\multi-coin-payout-1.wav",
                "sound\\fx\\followerunbound\\multi-coin-payout-2.wav",
                "sound\\fx\\followerunbound\\multi-coin-payout-3.wav",
                "sound\\fx\\followerunbound\\coin-payout-1.wav",
                "sound\\fx\\followerunbound\\coin-payout-2.wav"
            };
            const auto now = std::chrono::steady_clock::now();
            if (now - lastPlayed < std::chrono::seconds(8)) return;

            std::uniform_int_distribution<int> chance(1, 2);
            if (chance(rng) != 1) return;

            auto* audioManager = RE::BSAudioManager::GetSingleton();
            if (!audioManager) return;

            const auto followerID = follower->GetFormID();
            auto soundIt = soundByFollower.find(followerID);
            if (soundIt == soundByFollower.end()) {
                std::array<bool, soundPaths.size()> assigned{};
                for (const auto& [_, index] : soundByFollower) {
                    if (index < assigned.size()) assigned[index] = true;
                }

                std::vector<size_t> available;
                for (size_t i = 0; i < assigned.size(); ++i) {
                    if (!assigned[i]) available.push_back(i);
                }
                const size_t candidateCount = available.empty() ? soundPaths.size() : available.size();
                std::uniform_int_distribution<size_t> pick(0, candidateCount - 1);
                const size_t soundIndex = available.empty() ? pick(rng) : available[pick(rng)];
                soundIt = soundByFollower.emplace(followerID, soundIndex).first;
            }

            RE::BSResource::ID soundFile;
            soundFile.GenerateFromPath(soundPaths[soundIt->second]);

            RE::BSSoundHandle handle;
            audioManager->BuildSoundDataFromFile(handle, soundFile, 0x1A, 128);
            auto* player = RE::PlayerCharacter::GetSingleton();
            handle.SetPosition(player ? player->GetPosition() : follower->GetPosition());
            handle.SetVolume(1.0f);
            if (handle.Play()) {
                lastPlayed = now;
                logger::info("Shopping sound variant {} played for {}.", soundIt->second + 1, follower->GetName());
            } else {
                logger::warn("Shopping sound variant {} could not be played for {}.", soundIt->second + 1, follower->GetName());
            }
        }

        // Keep player-designated armor equipped if an outfit framework changes
        // the follower's gear during fast travel or cell transitions.
        static void RestoreProtectedArmor(RE::Actor* follower, FollowerProfile& profile)
        {
            if (!follower || follower->IsDead() || follower->IsInCombat() || !follower->Is3DLoaded()) return;
            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!equipManager) return;

            struct ProtectedArmor
            {
                RE::TESObjectARMO* armor;
                uint32_t mask;
                bool worn;
            };
            std::vector<ProtectedArmor> protectedArmors;
            std::vector<std::pair<uint32_t, RE::FormID>> wornArmorMasks;

            for (const auto& [item, inv] : follower->GetInventory()) {
                auto* armor = item ? item->As<RE::TESObjectARMO>() : nullptr;
                if (!armor || inv.first <= 0 || IsTngHiddenBodyItem(armor)) continue;

                bool worn = false;
                if (inv.second && inv.second->extraLists) {
                    for (auto* list : *inv.second->extraLists) {
                        if (list && (list->HasType(RE::ExtraDataType::kWorn) ||
                                     list->HasType(RE::ExtraDataType::kWornLeft))) {
                            worn = true;
                            break;
                        }
                    }
                }
                const auto mask = static_cast<uint32_t>(armor->GetSlotMask());
                if (worn) wornArmorMasks.emplace_back(mask, armor->GetFormID());
                if (profile.IsProtected(armor->GetFormID())) {
                    protectedArmors.push_back({ armor, mask, worn });
                }
            }

            // A protected item already worn in a slot wins. Otherwise restore a
            // protected item only when an unprotected item displaced it.
            for (const auto& candidate : protectedArmors) {
                if (candidate.worn || candidate.mask == 0) continue;
                bool protectedSlotAlreadyWorn = false;
                bool displaced = false;
                for (const auto& [wornMask, wornID] : wornArmorMasks) {
                    if ((candidate.mask & wornMask) == 0) continue;
                    if (profile.IsProtected(wornID)) protectedSlotAlreadyWorn = true;
                    else displaced = true;
                }
                if (!protectedSlotAlreadyWorn && displaced) {
                    equipManager->EquipObject(follower, candidate.armor, nullptr, 1, nullptr, false, false, false, true);
                    logger::info("ProtectionRestore: {} re-equipped protected armor {} [{:08X}] after outfit change.",
                        profile.name, candidate.armor->GetName(), candidate.armor->GetFormID());
                    // Avoid trying another protected item for the same slot on
                    // this pass; the next poll observes the engine's worn state.
                    for (const auto& other : protectedArmors) {
                        if ((other.mask & candidate.mask) != 0) {
                            wornArmorMasks.emplace_back(candidate.mask, candidate.armor->GetFormID());
                        }
                    }
                }
            }
        }
    }

    struct CandidateVendor
    {
        RE::Actor* actor{ nullptr };
        uint32_t vendorMask{ 0 };
        float distance{ 0.0f };
    };

    // Satıcı tipi yetki maskesi: hangi kategori hangi satıcıdan alınabilir?
    enum VendMask : uint32_t
    {
        kVendNone = 0,
        kVendSmith = 1 << 0,      // Demirci/okçu — silah, zırh, ok, cıvata
        kVendApothecary = 1 << 1, // Simyacı — iksir, zehir
        kVendInn = 1 << 2,        // Hancı — yemek, içecek
        kVendGeneral = 1 << 3,    // Genel tüccar — her şey
        kVendWizard = 1 << 4,     // Saray büyücüsü/büyücü — soul gem
        kVendHealer = 1 << 5      // Rahip/şifacı — yalnızca tedavi iksirleri
    };

    static uint32_t DetermineVendorMask(RE::Actor* vendor)
    {
        if (!vendor) return kVendNone;

        std::string name = vendor->GetName();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        const auto matches = [&name](std::initializer_list<const char*> names) {
            return std::any_of(names.begin(), names.end(), [&name](const char* part) { return name.find(part) != std::string::npos; });
        };

        uint32_t mask = kVendNone;
        if (matches({ "arcadia", "lami", "bothela", "angeline morrard", "vivienne onis", "quintus navale", "nurelion", "elgrim", "frida", "zaria", "atub", "bolar", "murbul", "sharamph", "babette", "milore ienth", "elynea mothren", "feran sadri", "florentius baenius", "muiri", "hafjorg", "edla", "herluin lothaire" })) mask |= kVendApothecary;
        if (matches({ "farengar", "sybille stentor", "wylandriah", "wuunferth", "calcelmo", "madena", "falion", "dravynea", "colette marence", "drevis neloren", "enthir", "faralda", "nelacar", "neloth", "phinis gestor", "talvas fathryon", "tolfdir", "florentius baenius" })) mask |= kVendWizard;
        if (matches({ "adrianne avenicci", "ulfberth war-bear", "eorlund gray-mane", "beirand", "fihada", "balimund", "oengul war-anvil", "hermir strong-heart", "moth gro-bagol", "ghorza gra-bagol", "alvor", "rustleif", "seren", "lod", "dushnamub", "gharol", "shahu", "mahuwa", "yarlakh", "glover mallory", "baldor iron-shaper", "gunmar", "hestla", "filnjar", "shuftharz", "asbjorn fire-tamer" })) mask |= kVendSmith;
        if (matches({ "belethor", "sayma", "bersi honey-hand", "brand-shei", "grelka", "elrindir", "revyn sadri", "aval atheron", "niranye", "lucan valerius", "camilla valerius", "solaf", "birna", "ri'saad", "ahkari", "ma'dran", "tonilia", "mallus maccius", "endon", "fethis alor", "falas selvayn", "sorine jurard", "ronthil", "gulum-ei", "atahbah", "ma'jhad", "zaynabi", "arnskar ember-master", "syndus", "vanryth gatharian" })) mask |= kVendGeneral;
        if (matches({ "hulda", "ysolda", "corpulus vinius", "keerava", "elda early-dawn", "ambrys rendar", "kleppr", "frabbi", "orgnar", "valga vinicia", "thoring", "jonna", "dagur", "faida", "hadring", "iddra", "voldsee", "kjeld", "gelmund", "vekel the man", "talen-jei", "lynly star-sung", "mralki", "addvar", "anoriath", "carlotta valentia", "evette san", "garyn ienth", "hillevi cruel-sea", "hogni red-arm", "jala", "marise aravel" })) mask |= kVendInn;
        if (matches({ "heimskr", "danica pure-spring", "jensine", "ahlam", "ortus endario", "freir", "helgird", "runil", "alessandra", "maramal", "dinya balu", "hamal", "senna", "orla", "anwen", "brother verulus", "styrr", "nura snow-shod", "erandur" })) mask |= kVendHealer;

        auto addFaction = [&](uint32_t factionID, uint32_t bit) {
            auto* faction = RE::TESForm::LookupByID<RE::TESFaction>(factionID);
            if (faction && vendor->IsInFaction(faction)) mask |= bit;
        };
        addFaction(0x000513A6, kVendWizard);     // JobCourtWizardFaction
        addFaction(0x000509FB, kVendSmith);      // JobBlacksmithFaction
        addFaction(0x000509F9, kVendApothecary);  // JobApothecaryFaction
        addFaction(0x000509FA, kVendInn);         // JobInnkeeperFaction
        addFaction(0x00051596, kVendGeneral);     // JobMerchantFaction

        if (auto* base = vendor->GetActorBase()) {
            for (auto& fInfo : base->factions) {
                if (!fInfo.faction) continue;
                const uint32_t id = fInfo.faction->GetFormID();
                if (id == 0x000513A6) mask |= kVendWizard;
                if (id == 0x000509FB) mask |= kVendSmith;
                if (id == 0x000509F9) mask |= kVendApothecary;
                if (id == 0x000509FA) mask |= kVendInn;
                if (id == 0x00051596) mask |= kVendGeneral;

                const char* raw = fInfo.faction->GetName();
                if (!raw) continue;
                std::string factionName(raw);
                std::transform(factionName.begin(), factionName.end(), factionName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (factionName.find("court wizard") != std::string::npos || factionName.find("courtwizard") != std::string::npos) mask |= kVendWizard;
                if (factionName.find("apothecary") != std::string::npos || factionName.find("alchem") != std::string::npos) mask |= kVendApothecary;
                if (factionName.find("blacksmith") != std::string::npos || factionName.find("fletcher") != std::string::npos || factionName.find("weapon") != std::string::npos || factionName.find("armor") != std::string::npos) mask |= kVendSmith;
                if (factionName.find("innkeeper") != std::string::npos || factionName.find("food") != std::string::npos || factionName.find("tavern") != std::string::npos || factionName.find("wench") != std::string::npos) mask |= kVendInn;
                if (factionName.find("merchant") != std::string::npos || factionName.find("trader") != std::string::npos || factionName.find("general") != std::string::npos || factionName.find("goods") != std::string::npos || factionName.find("fence") != std::string::npos || factionName.find("caravan") != std::string::npos || factionName.find("pawn") != std::string::npos) mask |= kVendGeneral;
            }
        }

        // Name fallback for modded vendors, layered on top of faction/name roster roles.
        if (name.find("wench") != std::string::npos || name.find("maid") != std::string::npos || name.find("server") != std::string::npos) mask |= kVendInn;
        if (name.find("blacksmith") != std::string::npos || name.find("fletcher") != std::string::npos || name.find("forge") != std::string::npos || name.find("armorer") != std::string::npos) mask |= kVendSmith;
        if (name.find("alchem") != std::string::npos || name.find("apothecary") != std::string::npos) mask |= kVendApothecary;
        if (name.find("trader") != std::string::npos || name.find("merchant") != std::string::npos || name.find("general") != std::string::npos || name.find("goods") != std::string::npos || name.find("pawn") != std::string::npos || name.find("caravan") != std::string::npos) mask |= kVendGeneral;
        if (name.find("wizard") != std::string::npos || name.find("enchanter") != std::string::npos || name.find("büyücü") != std::string::npos || name.find("buyucu") != std::string::npos) mask |= kVendWizard;
        return mask;
    }
    static std::vector<CandidateVendor> FindNearbyVendors(RE::Actor* follower, RE::ProcessLists* processLists, float maxDistance)
    {
        std::vector<CandidateVendor> vendors;
        if (!follower || !processLists) return vendors;

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* playerCell = player ? player->GetParentCell() : nullptr;
        auto* followerCell = follower->GetParentCell();
        // Interior cell coordinates are local to each cell and can overlap.
        // Distance alone can therefore mistake a merchant in another shop
        // for a nearby vendor. Prefer the player's current interior (the place
        // where the follower is shopping); fall back to the follower's cell.
        auto* shoppingInterior = playerCell && playerCell->IsInteriorCell() ? playerCell :
                                 (followerCell && followerCell->IsInteriorCell() ? followerCell : nullptr);

        auto collect = [&](auto& handles) {
            for (auto& handle : handles) {
                auto actorPtr = handle.get();
                auto* candidate = actorPtr ? actorPtr.get() : nullptr;
                if (!candidate || candidate->IsDead() || candidate == follower || candidate->IsPlayerRef()) continue;
                if (shoppingInterior && candidate->GetParentCell() != shoppingInterior) continue;
                const float distance = follower->GetPosition().GetDistance(candidate->GetPosition());
                if (distance > maxDistance) continue;
                if (std::any_of(vendors.begin(), vendors.end(), [candidate](const CandidateVendor& existing) {
                    return existing.actor == candidate;
                })) continue;

                const auto vendorMask = DetermineVendorMask(candidate);
                if (vendorMask != kVendNone) vendors.push_back({ candidate, vendorMask, distance });
            }
        };

        // Vendors may be in either high or low process. Deduplicate because
        // actors can move between those arrays as their process level changes.
        collect(processLists->highActorHandles);
        collect(processLists->lowActorHandles);
        std::sort(vendors.begin(), vendors.end(), [](const CandidateVendor& a, const CandidateVendor& b) {
            return a.distance < b.distance;
        });
        return vendors;
    }

    // ─────────────────────────────────────────────────────────────────────
    // SARF SINIFLANDIRICI — alış ve satış tarafı AYNI sınıflandırıcıyı
    // kullanır; böylece limit sayımları birebir örtüşür ve "satın al →
    // sayaç görmüyor → tekrar satın al" sonsuz döngüsü yaşanmaz.
    // ─────────────────────────────────────────────────────────────────────
    enum SupplyCat
    {
        SOther = 0,
        SHealth,
        SMagicka,
        SStamina,
        SPoison,
        SFoodL,
        SFoodM,
        SFoodS,
        SDrinkN,
        SDrinkA,
        SArrow,
        SBolt,
        SIngredient,
        SSoulGem,
        STorch,
        SLanternOil,
        SSoap,
        STreatment,
        SCurePoison,
        SSoulPetty,
        SSoulLesser,
        SSoulCommon,
        SSoulGreater,
        SSoulGrand,
        SSoulBlack,
        SBlueMountainFlower,
        SWheat,
        SRedMountainFlower,
        SWhiteCap,
        SPurpleMountainFlower,
        SPineThrushEgg,
        SHealthMinor, SHealthStandard, SHealthPlentiful, SHealthVigorous, SHealthExtreme, SHealthUltimate,
        SMagickaMinor, SMagickaStandard, SMagickaPlentiful, SMagickaVigorous, SMagickaExtreme, SMagickaUltimate,
        SStaminaMinor, SStaminaStandard, SStaminaPlentiful, SStaminaVigorous, SStaminaExtreme, SStaminaUltimate,
        SPoisonHealth, SPoisonMagicka, SPoisonStamina, SPoisonOther, SInvisibility
    };

    static SupplyCat ClassifySupply(ItemEvaluator* ev, RE::TESBoundObject* obj)
    {
        if (!obj) return SOther;
        if (auto* alchemy = obj->As<RE::AlchemyItem>()) {
            switch (obj->GetFormID()) {
            case 0x0003EADD: return SHealthMinor;
            case 0x0003EADE: return SHealthStandard;
            case 0x0003EADF: return SHealthPlentiful;
            case 0x0003EAE3: return SHealthVigorous;
            case 0x00039BE4: return SHealthExtreme;
            case 0x00039BE5: return SHealthUltimate;
            case 0x0003EAE0: return SMagickaMinor;
            case 0x0003EAE1: return SMagickaStandard;
            case 0x0003EAE2: return SMagickaPlentiful;
            case 0x0003EAE4: return SMagickaVigorous;
            case 0x00039BE6: return SMagickaExtreme;
            case 0x00039BE7: return SMagickaUltimate;
            case 0x0003EAE5: return SStaminaMinor;
            case 0x00039BE8: return SStaminaStandard;
            case 0x0003EAE7: return SStaminaPlentiful;
            case 0x0003EAE8: return SStaminaVigorous;
            case 0x0003EAE6: return SStaminaExtreme;
            case 0x00039CF3: return SStaminaUltimate;
            default: break;
            }
            // Prefer effects so compatible potions from leveled/modded vendor
            // lists are counted even when they use a different base form.
            if (alchemy->HasEffect(RE::EffectArchetype::kCureDisease)) return STreatment;
            if (alchemy->HasEffect(RE::EffectArchetype::kCurePoison)) return SCurePoison;
            if (ev->IsInvisibilityPotion(obj)) return SInvisibility;
            if (ev->IsPoison(obj)) {
                switch (ev->GetPoisonType(obj)) {
                case ItemEvaluator::PoisonType::Health: return SPoisonHealth;
                case ItemEvaluator::PoisonType::Magicka: return SPoisonMagicka;
                case ItemEvaluator::PoisonType::Stamina: return SPoisonStamina;
                default: return SPoisonOther;
                }
            }
            // Keep the vanilla forms as a fallback for plugins that alter effects.
            if (obj->GetFormID() == 0x000AE723) return STreatment;
            if (obj->GetFormID() == 0x00065A64) return SCurePoison;
        }
        if (auto ammo = obj->As<RE::TESAmmo>()) {
            return ammo->IsBolt() ? SBolt : SArrow;
        }
        if (auto soulGem = obj->As<RE::TESSoulGem>()) {
            if (soulGem->CanHoldNPCSoul()) return SSoulBlack;
            switch (soulGem->GetMaximumCapacity()) {
            case RE::SOUL_LEVEL::kPetty: return SSoulPetty;
            case RE::SOUL_LEVEL::kLesser: return SSoulLesser;
            case RE::SOUL_LEVEL::kCommon: return SSoulCommon;
            case RE::SOUL_LEVEL::kGreater: return SSoulGreater;
            case RE::SOUL_LEVEL::kGrand: return SSoulGrand;
            default: return SSoulGrand;
            }
            return SSoulGem; // dolu/boş fark etmez
        }
        switch (obj->GetFormID()) {
        case 0x00077E1C: return SBlueMountainFlower;
        case 0x0004B0BA: return SWheat;
        case 0x00077E1D: return SRedMountainFlower;
        case 0x0004DA22: return SWhiteCap;
        case 0x00077E1E: return SPurpleMountainFlower;
        case 0x00023D6F: return SPineThrushEgg;
        default: break;
        }
        if (obj->GetFormType() == RE::FormType::Light) {
            auto* light = obj->As<RE::TESObjectLIGH>();
            if (light && light->CanBeCarried()) {
                return STorch;
            }
        }
        if (auto alch = obj->As<RE::AlchemyItem>()) {
            if (ev->IsPoison(obj)) return SPoison;
            if (alch->IsFood()) {
                if (ev->IsDrink(obj)) return ev->IsAlcohol(obj) ? SDrinkA : SDrinkN;
                if (ev->IsFoodLarge(obj)) return SFoodL;
                if (ev->IsFoodSmall(obj)) return SFoodS;
                return SFoodM;
            }
            if (ev->IsHealthPotion(obj)) return SHealth;
            if (ev->IsMagickaPotion(obj)) return SMagicka;
            if (ev->IsStaminaPotion(obj)) return SStamina;
        }
        if (obj->As<RE::IngredientItem>()) return SIngredient;

        // İsim bazlı tanıma (modlu eşyalar: meşale, fener yağı, sabun)
        std::string name = obj->GetName();
        if (!name.empty()) {
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name.find("torch") != std::string::npos) return STorch;
            if (name.find("lantern oil") != std::string::npos || name.find("lantern fuel") != std::string::npos || name.find("fener ya") != std::string::npos) return SLanternOil;
            if (name.find("soap") != std::string::npos || name.find("sabun") != std::string::npos) return SSoap;
        }

        return SOther;
    }

    // Kullanıcının belirttiği FormID'ler veya RE::TESSoulGem::GetContainedSoul() bazlı dolu/boş kontrolü
    static bool IsSoulGemFull(RE::TESBoundObject* obj)
    {
        if (!obj) return false;
        switch (obj->GetFormID()) {
        case 0x0002E4E3: // Petty Full
        case 0x0002E4E5: // Lesser Full
        case 0x0002E4F3: // Common Full
        case 0x0002E4FB: // Greater Full
        case 0x0002E4FF: // Grand Full
        case 0x0002E504: // Black Full
            return true;
        case 0x0002E4E2: // Petty Empty
        case 0x0002E4E4: // Lesser Empty
        case 0x0002E4E6: // Common Empty
        case 0x0002E4F4: // Greater Empty
        case 0x0002E4FC: // Grand Empty
        case 0x0002E500: // Black Empty
            return false;
        default:
            break;
        }
        if (auto* gem = obj->As<RE::TESSoulGem>()) {
            return gem->GetContainedSoul() != RE::SOUL_LEVEL::kNone;
        }
        return false;
    }

    // Tek geçişte takipçinin tüm sarf stoklarını sayar (performans: 15 ayrı
    // GetInventory çağrısı yerine 1 çağrı).
    static SupplyCounts CountSupplies(RE::Actor* actor, ItemEvaluator* ev)
    {
        SupplyCounts c;
        if (!actor) return c;
        for (const auto& [obj, inv] : actor->GetInventory()) {
            if (!obj || inv.first <= 0) continue;
            const bool isFull = IsSoulGemFull(obj);
            switch (ClassifySupply(ev, obj)) {
            case SHealth:      c.health += inv.first; break;
            case SMagicka:     c.magicka += inv.first; break;
            case SStamina:     c.stamina += inv.first; break;
            case SPoison:      c.poison += inv.first; break;
            case SFoodL:       c.foodL += inv.first; break;
            case SFoodM:       c.foodM += inv.first; break;
            case SFoodS:       c.foodS += inv.first; break;
            case SDrinkN:      c.drinkN += inv.first; break;
            case SDrinkA:      c.drinkA += inv.first; break;
            case SArrow:       c.arrows += inv.first; break;
            case SBolt:        c.bolts += inv.first; break;
            case SSoulGem:     c.soulGems += inv.first; break;
            case SSoulPetty:
                c.pettySoulGems += inv.first;
                c.soulGems += inv.first;
                if (isFull) c.pettySoulGemsFull += inv.first; else c.pettySoulGemsEmpty += inv.first;
                break;
            case SSoulLesser:
                c.lesserSoulGems += inv.first;
                c.soulGems += inv.first;
                if (isFull) c.lesserSoulGemsFull += inv.first; else c.lesserSoulGemsEmpty += inv.first;
                break;
            case SSoulCommon:
                c.commonSoulGems += inv.first;
                c.soulGems += inv.first;
                if (isFull) c.commonSoulGemsFull += inv.first; else c.commonSoulGemsEmpty += inv.first;
                break;
            case SSoulGreater:
                c.greaterSoulGems += inv.first;
                c.soulGems += inv.first;
                if (isFull) c.greaterSoulGemsFull += inv.first; else c.greaterSoulGemsEmpty += inv.first;
                break;
            case SSoulGrand:
                c.grandSoulGems += inv.first;
                c.soulGems += inv.first;
                if (isFull) c.grandSoulGemsFull += inv.first; else c.grandSoulGemsEmpty += inv.first;
                break;
            case SSoulBlack:
                c.blackSoulGems += inv.first;
                c.soulGems += inv.first;
                if (isFull) c.blackSoulGemsFull += inv.first; else c.blackSoulGemsEmpty += inv.first;
                break;
            case SBlueMountainFlower: c.blueMountainFlowers += inv.first; break;
            case SWheat: c.wheat += inv.first; break;
            case SRedMountainFlower: c.redMountainFlowers += inv.first; break;
            case SWhiteCap: c.whiteCaps += inv.first; break;
            case SPurpleMountainFlower: c.purpleMountainFlowers += inv.first; break;
            case SPineThrushEgg: c.pineThrushEggs += inv.first; break;
            case SPoisonHealth: c.poisonHealth += inv.first; c.poison += inv.first; break;
            case SPoisonMagicka: c.poisonMagicka += inv.first; c.poison += inv.first; break;
            case SPoisonStamina: c.poisonStamina += inv.first; c.poison += inv.first; break;
            case SPoisonOther: c.poisonOther += inv.first; c.poison += inv.first; break;
            case SInvisibility: c.invisibilityPotions += inv.first; break;
            case SHealthMinor: c.healthMinor += inv.first; c.health += inv.first; break;
            case SHealthStandard: c.healthStandard += inv.first; c.health += inv.first; break;
            case SHealthPlentiful: c.healthPlentiful += inv.first; c.health += inv.first; break;
            case SHealthVigorous: c.healthVigorous += inv.first; c.health += inv.first; break;
            case SHealthExtreme: c.healthExtreme += inv.first; c.health += inv.first; break;
            case SHealthUltimate: c.healthUltimate += inv.first; c.health += inv.first; break;
            case SMagickaMinor: c.magickaMinor += inv.first; c.magicka += inv.first; break;
            case SMagickaStandard: c.magickaStandard += inv.first; c.magicka += inv.first; break;
            case SMagickaPlentiful: c.magickaPlentiful += inv.first; c.magicka += inv.first; break;
            case SMagickaVigorous: c.magickaVigorous += inv.first; c.magicka += inv.first; break;
            case SMagickaExtreme: c.magickaExtreme += inv.first; c.magicka += inv.first; break;
            case SMagickaUltimate: c.magickaUltimate += inv.first; c.magicka += inv.first; break;
            case SStaminaMinor: c.staminaMinor += inv.first; c.stamina += inv.first; break;
            case SStaminaStandard: c.staminaStandard += inv.first; c.stamina += inv.first; break;
            case SStaminaPlentiful: c.staminaPlentiful += inv.first; c.stamina += inv.first; break;
            case SStaminaVigorous: c.staminaVigorous += inv.first; c.stamina += inv.first; break;
            case SStaminaExtreme: c.staminaExtreme += inv.first; c.stamina += inv.first; break;
            case SStaminaUltimate: c.staminaUltimate += inv.first; c.stamina += inv.first; break;
            case STorch:       c.torches += inv.first; break;
            case SLanternOil:  c.lanternOil += inv.first; break;
            case SSoap:        c.soap += inv.first; break;
            case STreatment:  c.treatmentPotions += inv.first; break;
            case SCurePoison: c.curePoisonPotions += inv.first; break;
            default: break;
            }
        }
        return c;
    }

    SupplyCounts ShoppingAI::GetSupplyCounts(RE::Actor* follower)
    {
        return CountSupplies(follower, ItemEvaluator::GetSingleton());
    }

    struct StockEntry
    {
        RE::TESBoundObject* obj{ nullptr };
        int32_t stock{ 0 };
        const char* vendorName{ nullptr };
        uint32_t vendorMask{ kVendNone };
        RE::FormID stockContainerID{ 0 };
        std::string enchantmentDescription;
    };

    struct VendorStock
    {
        std::vector<StockEntry> buckets[64]; // SupplyCat-1 indexli (SOther excluded)
        std::vector<StockEntry> equipment;
    };

    static std::string DescribeItemEnchantment(RE::TESBoundObject* item, const RE::InventoryEntryData* entry = nullptr)
    {
        RE::EnchantmentItem* enchantment = entry ? entry->GetEnchantment() : nullptr;
        if (!enchantment) {
            if (auto* weapon = item ? item->As<RE::TESObjectWEAP>() : nullptr) enchantment = weapon->formEnchanting;
            else if (auto* armor = item ? item->As<RE::TESObjectARMO>() : nullptr) enchantment = armor->formEnchanting;
        }
        if (!enchantment) return {};

        std::string description;
        const char* enchantName = enchantment->GetName();
        if (enchantName && enchantName[0]) description = enchantName;
        std::string effects;
        uint32_t shown = 0;
        for (auto* effect : enchantment->effects) {
            if (!effect || !effect->baseEffect) continue;
            const char* rawEffectName = effect->baseEffect->GetName();
            if (!rawEffectName || !rawEffectName[0]) continue;
            if (!effects.empty()) effects += "; ";
            effects += rawEffectName;
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

    static void ScanVendorStock(RE::Actor* vendor, ItemEvaluator* ev, const char* vendorName, uint32_t vendorMask, VendorStock& out)
    {
        if (!vendor) return;
        std::unordered_set<RE::TESObjectREFR*> scannedContainers;
        auto scanContainer = [&](RE::TESObjectREFR* container) {
            if (!container || !scannedContainers.insert(container).second) return;
            // Kesinlikle aktörlerin (NPC) kendi envanterini sandık olarak tarama! Sadece gerçek sandıkları tara.
            if (container->As<RE::Actor>()) return;
            for (const auto& [obj, invData] : container->GetInventory()) {
                if (!obj || invData.first <= 0 || obj->GetFormID() == 0x0000000F) continue;
                SupplyCat cat = ClassifySupply(ev, obj);
                if (cat != SOther) {
                    out.buckets[static_cast<int>(cat) - 1].push_back({ obj, invData.first, vendorName, vendorMask, container->GetFormID(), {} });
                } else if (obj->As<RE::TESObjectWEAP>() || obj->As<RE::TESObjectARMO>()) {
                    out.equipment.push_back({ obj, invData.first, vendorName, vendorMask, container->GetFormID(),
                        DescribeItemEnchantment(obj, invData.second.get()) });
                }
            }
        };

        // Satıcıların şahsi envanteri (üstlerindeki kıyafetler, silahlar vb.) ASLA taranmaz.
        // Yalnızca satıcı fraksiyonuna bağlı merchant container (satıcı sandığı) taranır.
        if (auto* faction = vendor->GetVendorFaction()) {
            scanContainer(faction->vendorData.merchantContainer);
        }
        if (auto* base = vendor->GetActorBase()) {
            for (const auto& factionInfo : base->factions) {
                if (factionInfo.faction && factionInfo.faction->vendorData.merchantContainer) {
                    scanContainer(factionInfo.faction->vendorData.merchantContainer);
                }
            }
        }
    }

    struct UpgradeComparison
    {
        RE::TESBoundObject* current{ nullptr };
        int64_t improvement{ 0 };
        std::string currentStats;
        std::string candidateStats;
    };

    static bool IsWornInventoryItem(const RE::InventoryEntryData* entry)
    {
        if (!entry || !entry->extraLists) return false;
        for (auto* list : *entry->extraLists) {
            if (list && (list->HasType(RE::ExtraDataType::kWorn) || list->HasType(RE::ExtraDataType::kWornLeft))) return true;
        }
        return false;
    }

    static std::optional<UpgradeComparison> CompareEquipment(RE::Actor* follower, const FollowerProfile& profile,
        RE::TESBoundObject* candidate, std::string_view candidateEnchant)
    {
        if (!follower || !candidate || !ItemEvaluator::GetSingleton()->IsItemUsableByRole(profile.role, candidate)) return std::nullopt;
        const int32_t candidateValue = std::max(0, candidate->GetGoldValue());
        if (auto* weapon = candidate->As<RE::TESObjectWEAP>()) {
            if ((weapon->GetFormFlags() & RE::TESObjectWEAP::RecordFlags::kNonPlayable) != 0) return std::nullopt;
            RE::TESObjectWEAP* current = nullptr;
            for (bool leftHand : { false, true }) {
                auto* equipped = follower->GetEquippedObject(leftHand);
                auto* currentWeapon = equipped ? equipped->As<RE::TESObjectWEAP>() : nullptr;
                if (!currentWeapon || currentWeapon->GetWeaponType() != weapon->GetWeaponType()) continue;
                if (!current || currentWeapon->GetAttackDamage() > current->GetAttackDamage()) current = currentWeapon;
            }
            if (!current) return std::nullopt;
            std::string currentEnchant;
            auto inventory = follower->GetInventory();
            if (auto it = inventory.find(current); it != inventory.end()) currentEnchant = DescribeItemEnchantment(current, it->second.second.get());
            const bool candidateEnchanted = !candidateEnchant.empty();
            const bool currentEnchanted = !currentEnchant.empty();
            const int64_t newScore = static_cast<int64_t>(weapon->GetAttackDamage()) * 100000 + (candidateEnchanted ? 10000 : 0) + candidateValue;
            const int64_t oldScore = static_cast<int64_t>(current->GetAttackDamage()) * 100000 + (currentEnchanted ? 10000 : 0) + std::max(0, current->GetGoldValue());
            if (newScore <= oldScore) return std::nullopt;
            return UpgradeComparison{ current, newScore - oldScore,
                std::format("{} · Damage {} · Value {}{}", current->GetName(), current->GetAttackDamage(), current->GetGoldValue(), currentEnchanted ? std::format(" · {}", currentEnchant) : ""),
                std::format("{} · Damage {} · Value {}{}", weapon->GetName(), weapon->GetAttackDamage(), candidateValue, candidateEnchanted ? std::format(" · {}", candidateEnchant) : "") };
        }

        auto* armor = candidate->As<RE::TESObjectARMO>();
        if (!armor || IsTngHiddenBodyItem(armor) || static_cast<uint32_t>(armor->GetSlotMask()) == 0 ||
            (armor->GetFormFlags() & RE::TESObjectARMO::RecordFlags::kNonPlayable) != 0) return std::nullopt;

        // Kıyafet Bütünlüğü Kuralı: Takipçinin tercih ettiği zırh türüne uymuyorsa satın alma teklifi yapma
        if (auto profile = FollowerManager::GetSingleton()->GetProfile(follower->GetFormID())) {
            if (!ItemEvaluator::MatchesPreferredArmor(armor, profile->preferences.preferredArmorType)) {
                return std::nullopt;
            }
        }

        const auto candidateMask = static_cast<uint32_t>(armor->GetSlotMask());
        RE::TESObjectARMO* current = nullptr;
        int64_t oldScore = -1;
        for (const auto& [form, entry] : follower->GetInventory()) {
            auto* worn = form ? form->As<RE::TESObjectARMO>() : nullptr;
            if (!worn || entry.first <= 0 || !IsWornInventoryItem(entry.second.get()) || IsTngHiddenBodyItem(worn)) continue;
            if ((static_cast<uint32_t>(worn->GetSlotMask()) & candidateMask) == 0) continue;
            const auto enchant = DescribeItemEnchantment(worn, entry.second.get());
            const int64_t score = static_cast<int64_t>(worn->GetArmorRating()) * 100000 +
                (!enchant.empty() ? 10000 : 0) + std::max(0, worn->GetGoldValue());
            if (!current || score > oldScore) { current = worn; oldScore = score; }
        }
        if (!current) return std::nullopt;
        const bool candidateEnchanted = !candidateEnchant.empty();
        const int64_t newScore = static_cast<int64_t>(armor->GetArmorRating()) * 100000 +
            (candidateEnchanted ? 10000 : 0) + candidateValue;
        if (newScore <= oldScore) return std::nullopt;
        std::string currentEnchant;
        for (const auto& [form, entry] : follower->GetInventory()) {
            if (form == current && entry.first > 0) { currentEnchant = DescribeItemEnchantment(current, entry.second.get()); break; }
        }
        return UpgradeComparison{ current, newScore - oldScore,
            std::format("{} · Armor {} · Value {}{}", current->GetName(), current->GetArmorRating(), current->GetGoldValue(), currentEnchant.empty() ? "" : std::format(" · {}", currentEnchant)),
            std::format("{} · Armor {} · Value {}{}", armor->GetName(), armor->GetArmorRating(), candidateValue, candidateEnchanted ? std::format(" · {}", candidateEnchant) : "") };
    }

    static uint32_t CalculateFollowerPurchasePrice(RE::Actor* follower, RE::TESBoundObject* item)
    {
        if (!follower || !item) return 0;
        const int32_t value = item->GetGoldValue();
        float speech = 15.0f;
        if (auto* owner = follower->AsActorValueOwner()) speech = owner->GetActorValue(RE::ActorValue::kSpeech);
        const float discount = std::clamp(speech * 0.002f, 0.0f, 0.25f);
        return value > 0 ? std::max(1u, static_cast<uint32_t>(static_cast<float>(value) * (1.0f - discount))) : 5u;
    }

    // Ok/arbalet oku: takipçinin halihazırda kullandığı ok tipini tercih et
    static StockEntry* PickAmmoEntry(std::vector<StockEntry>& stock, RE::Actor* follower, uint32_t requiredMask)
    {
        if (stock.empty()) return nullptr;
        if (auto currentAmmo = follower->GetCurrentAmmo()) {
            for (auto& e : stock) {
                if ((e.vendorMask & requiredMask) && e.obj->GetFormID() == currentAmmo->GetFormID()) return &e;
            }
        }
        // Envanterinde en çok taşıdığı ok tipi
        StockEntry* best = nullptr;
        StockEntry* firstEligible = nullptr;
        int32_t bestCount = 0;
        for (auto& e : stock) {
            if ((e.vendorMask & requiredMask) == 0) continue;
            if (!firstEligible) firstEligible = &e;
            int32_t c = ItemEvaluator::GetItemCount(follower, e.obj);
            if (c > bestCount) {
                bestCount = c;
                best = &e;
            }
        }
        return best ? best : firstEligible;
    }

    void ShoppingAI::ProcessShopping(RE::Actor* follower, FollowerProfile& profile)
    {
        if (!follower || follower->IsDead() || follower->IsInCombat()) return;

        auto player = RE::PlayerCharacter::GetSingleton();
        if (player && player->IsInCombat()) return;

        // System toggle
        auto settings = Settings::GetSingleton();
        if (!settings->enableShoppingSystem) return;

        // Dialogue check: Do not execute shopping actions or HUD notifications while in dialogue
        auto ui = RE::UI::GetSingleton();
        if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) return;

        RE::FormID formID = follower->GetFormID();
        auto now = std::chrono::steady_clock::now();

        // 1. Cooldown check per follower: At least 3 seconds between shop rounds
        if (auto it = _lastShopTime.find(formID); it != _lastShopTime.end()) {
            float elapsed = std::chrono::duration<float>(now - it->second).count();
            if (elapsed < 3.0f) {
                return;
            }
        }

        // Gold item form (Gold001 = 0x0000000F)
        auto goldItem = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        if (!goldItem) return;

        int32_t invGold = ItemEvaluator::GetItemCount(follower, goldItem);
        if (invGold <= 0) return;

        // Sync profile gold with real inventory gold
        profile.gold = static_cast<uint32_t>(invGold);
        auto& pref = profile.preferences;
        if (pref.savingsEnabled) {
            pref.savedGold = std::min({ pref.savedGold, pref.savingsTarget, static_cast<uint32_t>(invGold) });
        } else {
            pref.savedGold = 0;
        }

        auto evaluator = ItemEvaluator::GetSingleton();

        // ── ADIM 1: İHTİYAÇ VAR MI? (satıcı taramasından ÖNCE — perf) ──
        SupplyCounts supplies = GetSupplyCounts(follower);
        struct Need
        {
            SupplyCat cat;
            int32_t current;
            uint32_t limit;
            bool enabled;
            bool isAmmo;
            uint32_t vendMask;
            const char* label;
            uint8_t soulGemFill{ 0 }; // 0 = Any, 1 = Empty, 2 = Full
        };

        std::vector<Need> needs;
        needs.reserve(48);

        auto addNeed = [&](SupplyCat cat, int32_t current, uint32_t limit, bool enabled, bool isAmmo, uint32_t mask, const char* label, uint8_t gemFill = 0) {
            needs.push_back({ cat, current, limit, enabled, isAmmo, mask, label, gemFill });
        };

        // Life Again injury events determine treatment potion demand. Keep this
        // ahead of routine supply purchases so injured followers are prioritized.
        const uint32_t treatmentDemand = ClassInterface::GetSingleton()->GetTreatmentPotionDemand(formID);
        addNeed(STreatment, supplies.treatmentPotions, std::max(pref.maxTreatmentPotions, treatmentDemand), pref.buyTreatmentPotions, false,
                kVendApothecary | kVendGeneral | kVendWizard | kVendHealer, "Cure Disease Potion");
        addNeed(SCurePoison, supplies.curePoisonPotions, pref.maxCurePoisonPotions, pref.buyCurePoisonPotions, false,
                kVendApothecary | kVendGeneral | kVendWizard | kVendHealer, "Cure Poison Potion");

        // İçecekler ve yemekler → hancı veya genel tüccar
        addNeed(SDrinkN, supplies.drinkN, pref.maxDrinkNormal, pref.buyDrinkNormal, false, kVendInn | kVendGeneral, "Water/Milk");
        addNeed(SDrinkA, supplies.drinkA, pref.maxDrinkAlcohol, pref.buyDrinkAlcohol, false, kVendInn | kVendGeneral, "Alcoholic Drink");
        addNeed(SFoodL, supplies.foodL, pref.maxFoodLarge, pref.buyFoodLarge, false, kVendInn | kVendGeneral, "Large Food (Meat/Fish)");
        addNeed(SFoodM, supplies.foodM, pref.maxFoodMedium, pref.buyFoodMedium, false, kVendInn | kVendGeneral, "Medium Food (Bread/Stew)");
        addNeed(SFoodS, supplies.foodS, pref.maxFoodSmall, pref.buyFoodSmall, false, kVendInn | kVendGeneral, "Small Food (Fruit/Vegetable)");
        // Healing ingredients support autonomous potion crafting. Purchase only
        // the missing pair needed to reach each existing potion stock target.
        const bool craftPotions = pref.allowCraftPotions;
        const auto ingredientNeed = [&](int32_t ingredientCount, int32_t partnerCount, int32_t potionCount, uint32_t potionTarget, uint32_t ingredientTarget) {
            const uint32_t missingPotions = potionCount < static_cast<int32_t>(potionTarget)
                ? potionTarget - static_cast<uint32_t>(potionCount) : 0u;
            const auto availablePairs = static_cast<uint32_t>(std::max(0, std::min(ingredientCount, partnerCount)));
            const uint32_t missingPairs = missingPotions > availablePairs ? missingPotions - availablePairs : 0u;
            return std::min(ingredientTarget, static_cast<uint32_t>(std::max(0, ingredientCount)) + missingPairs);
        };
        addNeed(SBlueMountainFlower, supplies.blueMountainFlowers,
                ingredientNeed(supplies.blueMountainFlowers, supplies.wheat, supplies.healthStandard, pref.maxHealthStandard, pref.maxBuyBlueMountainFlowers), craftPotions && supplies.healthStandard < static_cast<int32_t>(pref.maxHealthStandard), false, kVendApothecary, "Blue Mountain Flower");
        addNeed(SWheat, supplies.wheat,
                ingredientNeed(supplies.wheat, supplies.blueMountainFlowers, supplies.healthStandard, pref.maxHealthStandard, pref.maxBuyWheat), craftPotions && supplies.healthStandard < static_cast<int32_t>(pref.maxHealthStandard), false, kVendApothecary | kVendGeneral, "Wheat");
        addNeed(SRedMountainFlower, supplies.redMountainFlowers,
                ingredientNeed(supplies.redMountainFlowers, supplies.whiteCaps, supplies.magickaStandard, pref.maxMagickaStandard, pref.maxBuyRedMountainFlowers), craftPotions && supplies.magickaStandard < static_cast<int32_t>(pref.maxMagickaStandard), false, kVendApothecary, "Red Mountain Flower");
        addNeed(SWhiteCap, supplies.whiteCaps,
                ingredientNeed(supplies.whiteCaps, supplies.redMountainFlowers, supplies.magickaStandard, pref.maxMagickaStandard, pref.maxBuyWhiteCaps), craftPotions && supplies.magickaStandard < static_cast<int32_t>(pref.maxMagickaStandard), false, kVendApothecary, "White Cap");
        addNeed(SPurpleMountainFlower, supplies.purpleMountainFlowers,
                ingredientNeed(supplies.purpleMountainFlowers, supplies.pineThrushEggs, supplies.staminaStandard, pref.maxStaminaStandard, pref.maxBuyPurpleMountainFlowers), craftPotions && supplies.staminaStandard < static_cast<int32_t>(pref.maxStaminaStandard), false, kVendApothecary, "Purple Mountain Flower");
        addNeed(SPineThrushEgg, supplies.pineThrushEggs,
                ingredientNeed(supplies.pineThrushEggs, supplies.purpleMountainFlowers, supplies.staminaStandard, pref.maxStaminaStandard, pref.maxBuyPineThrushEggs), craftPotions && supplies.staminaStandard < static_cast<int32_t>(pref.maxStaminaStandard), false, kVendApothecary, "Pine Thrush Egg");

        // İksirler ve zehirler → simyacı veya genel tüccar
        addNeed(SHealthMinor, supplies.healthMinor, pref.maxHealthMinor, pref.buyHealthMinor, false, kVendApothecary | kVendGeneral, "Minor Health Potion");
        addNeed(SHealthStandard, supplies.healthStandard, pref.maxHealthStandard, pref.buyHealthStandard, false, kVendApothecary | kVendGeneral, "Health Potion");
        addNeed(SHealthPlentiful, supplies.healthPlentiful, pref.maxHealthPlentiful, pref.buyHealthPlentiful, false, kVendApothecary | kVendGeneral, "Plentiful Health Potion");
        addNeed(SHealthVigorous, supplies.healthVigorous, pref.maxHealthVigorous, pref.buyHealthVigorous, false, kVendApothecary | kVendGeneral, "Vigorous Health Potion");
        addNeed(SHealthExtreme, supplies.healthExtreme, pref.maxHealthExtreme, pref.buyHealthExtreme, false, kVendApothecary | kVendGeneral, "Extreme Health Potion");
        addNeed(SHealthUltimate, supplies.healthUltimate, pref.maxHealthUltimate, pref.buyHealthUltimate, false, kVendApothecary | kVendGeneral, "Ultimate Health Potion");
        addNeed(SMagickaMinor, supplies.magickaMinor, pref.maxMagickaMinor, pref.buyMagickaMinor, false, kVendApothecary | kVendGeneral, "Minor Magicka Potion");
        addNeed(SMagickaStandard, supplies.magickaStandard, pref.maxMagickaStandard, pref.buyMagickaStandard, false, kVendApothecary | kVendGeneral, "Magicka Potion");
        addNeed(SMagickaPlentiful, supplies.magickaPlentiful, pref.maxMagickaPlentiful, pref.buyMagickaPlentiful, false, kVendApothecary | kVendGeneral, "Plentiful Magicka Potion");
        addNeed(SMagickaVigorous, supplies.magickaVigorous, pref.maxMagickaVigorous, pref.buyMagickaVigorous, false, kVendApothecary | kVendGeneral, "Vigorous Magicka Potion");
        addNeed(SMagickaExtreme, supplies.magickaExtreme, pref.maxMagickaExtreme, pref.buyMagickaExtreme, false, kVendApothecary | kVendGeneral, "Extreme Magicka Potion");
        addNeed(SMagickaUltimate, supplies.magickaUltimate, pref.maxMagickaUltimate, pref.buyMagickaUltimate, false, kVendApothecary | kVendGeneral, "Ultimate Magicka Potion");
        addNeed(SStaminaMinor, supplies.staminaMinor, pref.maxStaminaMinor, pref.buyStaminaMinor, false, kVendApothecary | kVendGeneral, "Minor Stamina Potion");
        addNeed(SStaminaStandard, supplies.staminaStandard, pref.maxStaminaStandard, pref.buyStaminaStandard, false, kVendApothecary | kVendGeneral, "Stamina Potion");
        addNeed(SStaminaPlentiful, supplies.staminaPlentiful, pref.maxStaminaPlentiful, pref.buyStaminaPlentiful, false, kVendApothecary | kVendGeneral, "Plentiful Stamina Potion");
        addNeed(SStaminaVigorous, supplies.staminaVigorous, pref.maxStaminaVigorous, pref.buyStaminaVigorous, false, kVendApothecary | kVendGeneral, "Vigorous Stamina Potion");
        addNeed(SStaminaExtreme, supplies.staminaExtreme, pref.maxStaminaExtreme, pref.buyStaminaExtreme, false, kVendApothecary | kVendGeneral, "Extreme Stamina Potion");
        addNeed(SStaminaUltimate, supplies.staminaUltimate, pref.maxStaminaUltimate, pref.buyStaminaUltimate, false, kVendApothecary | kVendGeneral, "Ultimate Stamina Potion");
        addNeed(SPoisonHealth, supplies.poisonHealth, pref.maxPoisonHealth, pref.buyPoisons && pref.buyPoisonHealth, false, kVendApothecary | kVendGeneral, "Health Damage Poison");
        addNeed(SPoisonMagicka, supplies.poisonMagicka, pref.maxPoisonMagicka, pref.buyPoisons && pref.buyPoisonMagicka, false, kVendApothecary | kVendGeneral, "Magicka Damage Poison");
        addNeed(SPoisonStamina, supplies.poisonStamina, pref.maxPoisonStamina, pref.buyPoisons && pref.buyPoisonStamina, false, kVendApothecary | kVendGeneral, "Stamina Damage Poison");
        addNeed(SPoisonOther, supplies.poisonOther, pref.maxPoisonOther, pref.buyPoisons && pref.buyPoisonOther, false, kVendApothecary | kVendGeneral, "Other Poison");
        addNeed(SInvisibility, supplies.invisibilityPotions, pref.maxInvisibilityPotions, pref.buyInvisibilityPotions, false, kVendApothecary | kVendGeneral, "Invisibility Potion");
        // Ok/cıvata → demirci veya genel tüccar
        addNeed(SArrow, supplies.arrows, pref.maxArrows, pref.buyArrows, true, kVendSmith | kVendGeneral, "Arrows");
        addNeed(SBolt, supplies.bolts, pref.maxBolts, pref.buyBolts, true, kVendSmith | kVendGeneral, "Crossbow Bolts");

        // SoulKeeper purchasing has been retired; buy each vanilla gem capacity independently.
        constexpr uint32_t soulGemVendors = kVendGeneral | kVendWizard;
        auto getGemCount = [](uint8_t fillMode, int32_t total, int32_t emptyCount, int32_t fullCount) -> int32_t {
            if (fillMode == 1) return emptyCount; // Empty only
            if (fillMode == 2) return fullCount;  // Full only
            return total;                         // Any (0)
        };
        addNeed(SSoulPetty, getGemCount(pref.soulGemFillPetty, supplies.pettySoulGems, supplies.pettySoulGemsEmpty, supplies.pettySoulGemsFull),
                pref.maxPettySoulGems, pref.buyPettySoulGems, false, soulGemVendors, "Petty Soul Gem", pref.soulGemFillPetty);
        addNeed(SSoulLesser, getGemCount(pref.soulGemFillLesser, supplies.lesserSoulGems, supplies.lesserSoulGemsEmpty, supplies.lesserSoulGemsFull),
                pref.maxLesserSoulGems, pref.buyLesserSoulGems, false, soulGemVendors, "Lesser Soul Gem", pref.soulGemFillLesser);
        addNeed(SSoulCommon, getGemCount(pref.soulGemFillCommon, supplies.commonSoulGems, supplies.commonSoulGemsEmpty, supplies.commonSoulGemsFull),
                pref.maxCommonSoulGems, pref.buyCommonSoulGems, false, soulGemVendors, "Common Soul Gem", pref.soulGemFillCommon);
        addNeed(SSoulGreater, getGemCount(pref.soulGemFillGreater, supplies.greaterSoulGems, supplies.greaterSoulGemsEmpty, supplies.greaterSoulGemsFull),
                pref.maxGreaterSoulGems, pref.buyGreaterSoulGems, false, soulGemVendors, "Greater Soul Gem", pref.soulGemFillGreater);
        addNeed(SSoulGrand, getGemCount(pref.soulGemFillGrand, supplies.grandSoulGems, supplies.grandSoulGemsEmpty, supplies.grandSoulGemsFull),
                pref.maxGrandSoulGems, pref.buyGrandSoulGems, false, soulGemVendors, "Grand Soul Gem", pref.soulGemFillGrand);
        addNeed(SSoulBlack, getGemCount(pref.soulGemFillBlack, supplies.blackSoulGems, supplies.blackSoulGemsEmpty, supplies.blackSoulGemsFull),
                pref.maxBlackSoulGems, pref.buyBlackSoulGems, false, soulGemVendors, "Black Soul Gem", pref.soulGemFillBlack);

        // Ekstra Gereçler (Meşale, Fener Yağı, Sabun)
        addNeed(STorch, supplies.torches, pref.maxTorches, pref.buyTorches, false, kVendGeneral | kVendInn, "Torch");
        addNeed(SLanternOil, supplies.lanternOil, pref.maxLanternOil, pref.buyLanternOil, false, kVendGeneral | kVendInn | kVendSmith, "Lantern Oil");
        addNeed(SSoap, supplies.soap, pref.maxSoap, pref.buySoap, false, kVendGeneral | kVendInn | kVendApothecary, "Soap");

        auto isNeeded = [](const Need& n) {
            if (!n.enabled || n.limit == 0 || n.current >= static_cast<int32_t>(n.limit)) return false;
            if (n.isAmmo && (n.limit - n.current) < 10) return false; // küçük miktarlar için uğraşılmaz
            return true;
        };

        bool anyNeeded = false;
        for (const auto& n : needs) {
            if (isNeeded(n)) {
                anyNeeded = true;
                break;
            }
        }
        const bool equipmentOffersEnabled = pref.allowBuyEquipment && settings->enableEquipmentPurchaseOffers;
        const uint32_t pendingEquipmentGold = EquipPrompt::GetReservedGold(formID);
        if (!anyNeeded && !equipmentOffersEnabled) return;

        // ── ADIM 2: YAKIN SATICILARI TOPLA (1500 birim, en fazla 6 aday) ──
        auto processLists = RE::ProcessLists::GetSingleton();
        if (!processLists) return;

        auto nearbyVendors = FindNearbyVendors(follower, processLists, 1500.0f);
        if (nearbyVendors.empty()) return;

        // ── ADIM 3: UYUMLU SATICILARIN STOĞUNU TARA (en fazla 6 yakındaki aday) ──
        VendorStock stock;
        int stockScans = 0;
        for (const auto& cand : nearbyVendors) {
            if (stockScans >= 6) break;
            uint32_t vMask = cand.vendorMask;

            bool useful = equipmentOffersEnabled && (vMask & (kVendSmith | kVendGeneral)) != 0;
            for (const auto& n : needs) {
                if (!useful && isNeeded(n) && (n.vendMask & vMask)) {
                    useful = true;
                    break;
                }
            }
            if (!useful) continue;

            ScanVendorStock(cand.actor, evaluator, cand.actor->GetName(), vMask, stock);
            stockScans++;
        }

        // Offer only the strongest affordable compatible upgrade per follower
        // and shopping pass, avoiding repeated prompts for inventory items.
        if (equipmentOffersEnabled && pendingEquipmentGold == 0) {
            const uint32_t balance = std::min(profile.gold, static_cast<uint32_t>(std::max(0, invGold)));
            const uint32_t reserve = pref.savingsEnabled ? std::min(pref.savedGold, balance) : 0;
            const uint32_t spendable = balance - reserve;
            const StockEntry* bestEntry = nullptr;
            std::optional<UpgradeComparison> bestComparison;
            uint32_t bestPrice = 0;
            int64_t bestImprovement = 0;
            for (const auto& entry : stock.equipment) {
                if (!entry.obj || entry.stock <= 0 ||
                    (entry.vendorMask & (kVendSmith | kVendGeneral)) == 0 ||
                    profile.declinedEquipmentPurchases.contains(entry.obj->GetFormID()) ||
                    EquipPrompt::IsPurchasePending(formID, entry.obj->GetFormID()) ||
                    ItemEvaluator::GetItemCount(follower, entry.obj) > 0) continue;

                const uint32_t price = CalculateFollowerPurchasePrice(follower, entry.obj);
                if (price == 0 || price > spendable) continue;
                auto comparison = CompareEquipment(follower, profile, entry.obj, entry.enchantmentDescription);
                if (!comparison || comparison->improvement <= bestImprovement) continue;
                bestEntry = &entry;
                bestPrice = price;
                bestImprovement = comparison->improvement;
                bestComparison = std::move(comparison);
            }

            if (bestEntry && bestComparison) {
                if (settings->equipmentUpgradeChance < 100) {
                    thread_local std::mt19937 rng{ std::random_device{}() };
                    std::uniform_int_distribution<uint32_t> dist(1, 100);
                    if (dist(rng) > settings->equipmentUpgradeChance) {
                        bestEntry = nullptr;
                    }
                }
            }

            if (bestEntry && bestComparison) {
                auto* stockContainer = RE::TESForm::LookupByID<RE::TESObjectREFR>(bestEntry->stockContainerID);
                if (EquipPrompt::ShowPurchasePrompt(follower, stockContainer, bestEntry->obj,
                    bestComparison->current, bestPrice, bestEntry->vendorName ? bestEntry->vendorName : "Nearby seller",
                    bestComparison->currentStats, bestComparison->candidateStats)) {
                    _lastShopTime[formID] = now;
                    EquipPrompt::StartAfterLootDistribution();
                    logger::info("EquipmentPurchase: presenting {} offer to {} ({} gold, improvement {}).",
                        bestEntry->obj->GetName(), follower->GetName(), bestPrice, bestImprovement);
                    return;
                }
            }
        }

        // Explain why a nearby merchant did or did not satisfy each enabled
        // purchase target. This is rate-limited and makes seller classification
        // and stock matching observable in the user's SKSE log.
        static std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> lastSupplyCheckLog;
        auto lastSupplyLog = lastSupplyCheckLog.find(formID);
        if (lastSupplyLog == lastSupplyCheckLog.end() || std::chrono::duration<float>(now - lastSupplyLog->second).count() >= 30.0f) {
            auto vendorLabel = [](uint32_t mask) {
                std::string label;
                const auto append = [&label](const char* value) {
                    if (!label.empty()) label += "+";
                    label += value;
                };
                if (mask & kVendSmith) append("smith");
                if (mask & kVendApothecary) append("apothecary");
                if (mask & kVendInn) append("inn");
                if (mask & kVendGeneral) append("general");
                if (mask & kVendWizard) append("wizard");
                if (mask & kVendHealer) append("healer");
                return label.empty() ? std::string("unknown") : label;
            };
            std::string vendorSummary;
            for (const auto& vendor : nearbyVendors) {
                if (!vendorSummary.empty()) vendorSummary += ", ";
                vendorSummary += std::format("{}({})", vendor.actor->GetName(), vendorLabel(vendor.vendorMask));
            }
            std::string demandSummary;
            for (const auto& need : needs) {
                if (!isNeeded(need)) continue;
                const auto& entries = stock.buckets[static_cast<int>(need.cat) - 1];
                int32_t eligibleStock = 0;
                for (const auto& entry : entries) {
                    if ((entry.vendorMask & need.vendMask) != 0) eligibleStock += entry.stock;
                }
                if (!demandSummary.empty()) demandSummary += ", ";
                demandSummary += std::format("{} {}/{} stock={}", need.label, need.current, need.limit, eligibleStock);
            }
            logger::info("Shopping scan: follower={} spendableGold={} vendors=[{}] unmet=[{}].",
                follower->GetName(), std::max(0, invGold - static_cast<int32_t>(pref.savingsEnabled ? pref.savedGold : 0)),
                vendorSummary.empty() ? "none" : vendorSummary, demandSummary.empty() ? "none" : demandSummary);
            lastSupplyCheckLog[formID] = now;
        }

        // If a recipe pair is already carried or stocked by an alchemist, defer
        // the finished-potion purchase so the next crafting tick can use it.
        auto available = [&](SupplyCat cat, int32_t carried, uint32_t vendorMask) {
            if (carried > 0) return true;
            const auto& bucket = stock.buckets[static_cast<int>(cat) - 1];
            return std::any_of(bucket.begin(), bucket.end(), [vendorMask](const StockEntry& entry) {
                return entry.stock > 0 && (entry.vendorMask & vendorMask) != 0;
            });
        };
        auto deferPotionPurchase = [&](SupplyCat potion, SupplyCat ingredientA, int32_t countA, uint32_t vendorsA,
                                       SupplyCat ingredientB, int32_t countB, uint32_t vendorsB) {
            if (!craftPotions || !available(ingredientA, countA, vendorsA) || !available(ingredientB, countB, vendorsB)) return;
            for (auto& need : needs) if (need.cat == potion) need.enabled = false;
        };
        deferPotionPurchase(SHealthStandard, SBlueMountainFlower, supplies.blueMountainFlowers, kVendApothecary, SWheat, supplies.wheat, kVendApothecary | kVendGeneral);
        deferPotionPurchase(SMagickaStandard, SRedMountainFlower, supplies.redMountainFlowers, kVendApothecary, SWhiteCap, supplies.whiteCaps, kVendApothecary);
        deferPotionPurchase(SStaminaStandard, SPurpleMountainFlower, supplies.purpleMountainFlowers, kVendApothecary, SPineThrushEgg, supplies.pineThrushEggs, kVendApothecary);

        // Report medical stock only occasionally; this distinguishes an item
        // that a merchant does not carry from one blocked by funds or settings.
        if (isNeeded(needs[0]) || isNeeded(needs[1])) {
            static std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> lastMedicalCheckLog;
            auto last = lastMedicalCheckLog.find(formID);
            if (last == lastMedicalCheckLog.end() || std::chrono::duration<float>(now - last->second).count() >= 30.0f) {
                auto describeStock = [&](SupplyCat cat) {
                    std::string result;
                    const auto& entries = stock.buckets[static_cast<int>(cat) - 1];
                    for (const auto& entry : entries) {
                        if (!result.empty()) result += ", ";
                        result += std::format("{:08X} {} x{} @{}", entry.obj->GetFormID(), entry.obj->GetName(), entry.stock,
                            entry.vendorName ? entry.vendorName : "?");
                    }
                    return result.empty() ? std::string("none") : result;
                };
                logger::info("Shopping medical check: {} gold={}, cureDisease={}/{} sellerStock=[{}], curePoison={}/{} sellerStock=[{}].",
                    follower->GetName(), invGold, supplies.treatmentPotions, needs[0].limit, describeStock(STreatment),
                    supplies.curePoisonPotions, needs[1].limit, describeStock(SCurePoison));
                lastMedicalCheckLog[formID] = now;
            }
        }

        // ── ADIM 4: ÖNCELİK SIRASIYLA SATIN AL (tur başına en fazla 3 işlem) ──
        auto econ = EconomyManager::GetSingleton();
        auto notifier = NotificationManager::GetSingleton();
        int purchasesThisRound = 0;
        constexpr int kMaxPurchasesPerRound = 3;
        std::unordered_set<SupplyCat> attemptedCategories;
        struct StockShortageNotice
        {
            SupplyCat category;
            std::string message;
        };
        std::vector<StockShortageNotice> stockShortageNotices;
        std::unordered_map<SupplyCat, uint32_t> purchasedByCategory;
        std::unordered_map<SupplyCat, int32_t> stockAtVisit;
        std::unordered_map<SupplyCat, std::unordered_set<std::string>> sellersAtVisit;
        for (const auto& need : needs) {
            if (!isNeeded(need)) continue;
            for (const auto& entry : stock.buckets[static_cast<int>(need.cat) - 1]) {
                if ((entry.vendorMask & need.vendMask) == 0 || entry.stock <= 0) continue;
                stockAtVisit[need.cat] += entry.stock;
                if (entry.vendorName) sellersAtVisit[need.cat].insert(entry.vendorName);
            }
        }

        auto purchase = [&](RE::TESBoundObject* obj, uint32_t desiredQty, const char* vendorName, RE::FormID containerID) -> uint32_t {
            if (!obj || desiredQty == 0) return 0;

            uint32_t unitPrice = 5;
            int32_t objVal = obj->GetGoldValue();
            if (objVal > 0) {
                float speechSkill = 15.0f;
                if (auto avOwner = follower->AsActorValueOwner()) {
                    speechSkill = avOwner->GetActorValue(RE::ActorValue::kSpeech);
                }
                float discount = std::clamp(speechSkill * 0.002f, 0.0f, 0.25f);
                unitPrice = std::max(1u, static_cast<uint32_t>(static_cast<float>(objVal) * (1.0f - discount)));
            }

            const uint32_t balance = std::min(profile.gold, static_cast<uint32_t>(std::max(0, invGold)));
            const uint32_t reserve = pref.savingsEnabled ? std::min(pref.savedGold, balance) : 0;
            const uint32_t afterSavings = balance - reserve;
            const uint32_t reservedForEquipment = std::min(afterSavings, EquipPrompt::GetReservedGold(formID));
            const uint32_t spendable = afterSavings - reservedForEquipment;
            uint32_t maxAffordable = (unitPrice > 0) ? (spendable / unitPrice) : desiredQty;
            uint32_t finalQty = std::min(desiredQty, maxAffordable);
            if (finalQty == 0) return 0;

            uint32_t totalCost = unitPrice * finalQty;
            if (spendable < totalCost || invGold < static_cast<int32_t>(totalCost)) {
                return 0;
            }

            // Deduct physical gold from inventory
            follower->RemoveItem(goldItem, static_cast<int32_t>(totalCost), RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            econ->RemoveGold(formID, static_cast<int32_t>(totalCost));

            // Eşyayı satıcının sandığından düş ve ödenen altını sandığa koy (kesinlikle aktör olmamalı)
            if (containerID != 0) {
                if (auto* chest = RE::TESForm::LookupByID<RE::TESObjectREFR>(containerID)) {
                    if (!chest->As<RE::Actor>()) {
                        chest->RemoveItem(obj, static_cast<int32_t>(finalQty), RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                        chest->AddObjectToContainer(goldItem, nullptr, static_cast<int32_t>(totalCost), nullptr);
                    }
                }
            }

            // Add purchased item
            follower->AddObjectToContainer(obj, nullptr, static_cast<int32_t>(finalQty), nullptr);

            invGold -= static_cast<int32_t>(totalCost);
            _lastShopTime[formID] = now;
            // Alış-satış döngüsü koruması: yeni alınan eşya bir süre satılmaz
            _lastPurchaseTime[formID][obj->GetFormID()] = now;

            if (settings->notifyShopping) {
                std::string msg = std::format("{} purchased {}x {} from {}. (-{} Gold)",
                    follower->GetName(), finalQty, obj->GetName(), vendorName ? vendorName : "?", totalCost);
                notifier->ShowHUDMessage(msg);
            }
            logger::info("Shopping: {} purchased {}x {} from {} for {} gold.", follower->GetName(), finalQty, obj->GetName(), vendorName ? vendorName : "?", totalCost);
            return finalQty;
        };

        auto tryBuyStock = [&](SupplyCat cat, uint32_t desiredQty, bool isAmmo, uint32_t requiredMask, const char* label, uint8_t gemFill = 0) -> bool {
            auto& bucket = stock.buckets[static_cast<int>(cat) - 1];
            if (bucket.empty() || desiredQty == 0) return false;

            StockEntry* entry = nullptr;
            if (isAmmo) {
                entry = PickAmmoEntry(bucket, follower, requiredMask);
            } else {
                // En ucuz stoktan al (adet başı altın değeri)
                for (auto& e : bucket) {
                    if (e.stock <= 0 || (e.vendorMask & requiredMask) == 0) continue;
                    // Ruh taşı boş/dolu filtrelemesi (1 = Boş, 2 = Dolu)
                    if (gemFill == 1 && IsSoulGemFull(e.obj)) continue;
                    if (gemFill == 2 && !IsSoulGemFull(e.obj)) continue;
                    if (!entry || e.obj->GetGoldValue() < entry->obj->GetGoldValue()) entry = &e;
                }
            }
            if (!entry || entry->stock <= 0) return false;

            uint32_t qty = std::min<uint32_t>(desiredQty, static_cast<uint32_t>(entry->stock));
            const uint32_t purchasedQty = purchase(entry->obj, qty, entry->vendorName, entry->stockContainerID);
            if (purchasedQty > 0) {
                entry->stock -= static_cast<int32_t>(purchasedQty);
                purchasedByCategory[cat] += purchasedQty;
                if (cat == SPoisonHealth || cat == SPoisonMagicka || cat == SPoisonStamina || cat == SPoisonOther || cat == SPoison) {
                    ConsumableAI::GetSingleton()->NotifyPoisonAcquired(formID);
                }
                return true;
            }
            return false;
        };

        // Rotate across every enabled product so early entries (typically food)
        // cannot starve potions, ingredients, poisons, or utility supplies.
        const size_t start = needs.empty() ? 0 : (_purchaseCursor[formID] % needs.size());
        size_t nextCursor = needs.empty() ? 0 : (start + 1) % needs.size();
        for (size_t offset = 0; offset < needs.size(); ++offset) {
            if (purchasesThisRound >= kMaxPurchasesPerRound) break;
            const size_t index = (start + offset) % needs.size();
            const auto& n = needs[index];
            if (!isNeeded(n)) continue;
            const uint32_t outstanding = n.limit - static_cast<uint32_t>(n.current);
            // Fill the configured deficit in one transaction, bounded by the
            // seller's available stock and the follower's spendable gold.
            const uint32_t purchaseTarget = outstanding;
            attemptedCategories.insert(n.cat);
            if (tryBuyStock(n.cat, purchaseTarget,
                             n.isAmmo, n.vendMask, n.label, n.soulGemFill)) {
                purchasesThisRound++;
                nextCursor = (index + 1) % needs.size();
            }
        }

        if (!needs.empty()) _purchaseCursor[formID] = nextCursor;

        // If the sellers' available stock cannot satisfy this follower's full
        // configured target, tell the user once per follower/item window.
        // Report the stock observed at the start of this visit (not the
        // remaining local batch counter), and avoid repeating every poll.
        static std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> lastStockShortageNotice;
        if (settings->notifyShopping) {
            for (const auto& need : needs) {
                if (!attemptedCategories.contains(need.cat) || purchasedByCategory[need.cat] == 0 || !isNeeded(need)) continue;
                const int32_t availableStock = stockAtVisit[need.cat];
                const int32_t outstanding = static_cast<int32_t>(need.limit) - need.current;
                const auto& sources = sellersAtVisit[need.cat];
                if (sources.empty() || availableStock <= 0 || availableStock >= outstanding) continue;

                const uint64_t noticeKey = (static_cast<uint64_t>(formID) << 32) | static_cast<uint32_t>(need.cat);
                auto previous = lastStockShortageNotice.find(noticeKey);
                if (previous != lastStockShortageNotice.end() &&
                    std::chrono::duration<float>(now - previous->second).count() < 300.0f) continue;

                const std::string seller = sources.size() == 1 ? *sources.begin() : "Nearby sellers";
                const std::string stockMessage = std::format("{} had only {}x {} in stock; {}'s target is {}.",
                    seller, availableStock, need.label, follower->GetName(), need.limit);
                stockShortageNotices.push_back({ need.cat, stockMessage });
                lastStockShortageNotice[noticeKey] = now;
            }
        }

        if (purchasesThisRound > 0) {
            _lastShopTime[formID] = now;
            MaybePlayShoppingSound(follower);
        }
        for (const auto& notice : stockShortageNotices) {
            notifier->ShowHUDMessage(notice.message);
            logger::info("Shopping stock shortage: follower={} category={} message={}", follower->GetName(), static_cast<int>(notice.category), notice.message);
        }
    }

    void ShoppingAI::ProcessSelling(RE::Actor* follower, FollowerProfile& profile)
    {
        if (!follower || follower->IsDead() || follower->IsInCombat()) return;

        auto settings = Settings::GetSingleton();
        RestoreProtectedArmor(follower, profile);

        auto player = RE::PlayerCharacter::GetSingleton();
        if (player && player->IsInCombat()) return;

        // System toggle
        if (!settings->enableSellingSystem) return;

        // Dialogue check: Diyalog sırasında satış ve HUD bildirimi yapılmaz
        auto ui = RE::UI::GetSingleton();
        if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) return;

        RE::FormID formID = follower->GetFormID();
        auto now = std::chrono::steady_clock::now();

        // Aşırı yüklü mü? O zaman satış ACİLDİR: cooldown atlanır ve tam yığınlar satılır
        float maxCarry = 0.0f;
        bool overEncumbered = false;
        if (auto avOwner = follower->AsActorValueOwner()) {
            maxCarry = avOwner->GetActorValue(RE::ActorValue::kCarryWeight);
            overEncumbered = follower->GetWeightInContainer() > maxCarry;
        }

        // Takipçi başına cooldown: En az 3 saniye aralıkla satış yapar (acil durum hariç)
        if (!overEncumbered) {
            if (auto it = _lastSellTime.find(formID); it != _lastSellTime.end()) {
                float elapsed = std::chrono::duration<float>(now - it->second).count();
                if (elapsed < 3.0f) {
                    return;
                }
            }
        }

        // Yakındaki geçerli satıcıları topla (1500 birim)
        auto processLists = RE::ProcessLists::GetSingleton();
        if (!processLists) return;

        auto nearbyVendors = FindNearbyVendors(follower, processLists, 1500.0f);

        if (nearbyVendors.empty()) return;

        auto econ = EconomyManager::GetSingleton();
        auto notifier = NotificationManager::GetSingleton();
        auto evaluator = ItemEvaluator::GetSingleton();
        auto goldItem = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        if (!goldItem) return;

        // Envanterdeki satılabilir fazlalıkları tara
        auto inventory = follower->GetInventory();
        if (inventory.empty()) return;

        // Takipçinin şu an elinde/üzerinde kuşanmış olduğu eşyaları tespit et
        std::unordered_set<RE::TESBoundObject*> equippedObjects;
        if (auto rightWep = follower->GetEquippedObject(false)) {
            if (auto b = rightWep->As<RE::TESBoundObject>()) equippedObjects.insert(b);
        }
        if (auto leftWep = follower->GetEquippedObject(true)) {
            if (auto b = leftWep->As<RE::TESBoundObject>()) equippedObjects.insert(b);
        }

        // Automated follower sales mirror the player's current Skyrim barter price.
        RE::Actor* priceActor = RE::PlayerCharacter::GetSingleton();
        if (!priceActor) priceActor = follower;

        uint32_t batchMult = settings->transactionBatchMultiplier;
        int salesThisCall = 0;
        constexpr int kMaxSalesPerCall = 3; // acil (aşırı yüklü) durumda tek seferde 3 yığın
        constexpr float kPurchaseHoldSeconds = 120.0f; // yeni alınan eşya bu süre satılmaz

        // ── KATEGORİ SINIFLANDIRICI VE TOPLAMLARI ──
        // Satış tarafı, alışveriş tarafıyla BİREBİR AYNI sayımı kullanır:
        // fazlalık tek yığına göre değil KATEGORİ TOPLAMINA göre hesaplanır.
        std::unordered_map<int, int32_t> catTotal;
        for (const auto& [obj, inv] : inventory) {
            if (!obj || inv.first <= 0) continue;
            catTotal[static_cast<int>(ClassifySupply(evaluator, obj))] += inv.first;
        }
        int32_t soulGemTotal = 0;
        for (const auto& [obj, inv] : inventory) {
            if (obj && obj->As<RE::TESSoulGem>() && inv.first > 0) soulGemTotal += inv.first;
        }

        // Kategori toplamı - limit => satılabilir kategori fazlası (bu çağrıda
        // sattıkça toplamdan düşülür, böylece birden çok yığında taşırılmaz)
        auto categoryExcess = [&](SupplyCat cat, uint32_t limit, int32_t stackCount) -> int32_t {
            int32_t excess = catTotal[static_cast<int>(cat)] - static_cast<int32_t>(limit);
            if (excess <= 0) return 0;
            return std::min(stackCount, excess);
        };
        auto consumeExcess = [&](SupplyCat cat, int32_t amount) {
            catTotal[static_cast<int>(cat)] -= amount;
        };

        // Alış-satış döngüsü koruması: bu eşya az önce satın alındıysa satma
        auto recentlyPurchased = [&](RE::FormID itemID) -> bool {
            auto fit = _lastPurchaseTime.find(formID);
            if (fit == _lastPurchaseTime.end()) return false;
            auto pit = fit->second.find(itemID);
            if (pit == fit->second.end()) return false;
            return std::chrono::duration<float>(now - pit->second).count() < kPurchaseHoldSeconds;
        };

        for (const auto& [boundObj, invData] : inventory) {
            int32_t count = invData.first;
            if (count <= 0 || !boundObj) continue;

            RE::FormID itemID = boundObj->GetFormID();

            // TNG's internal coverage item must never enter follower selling.
            if (IsTngHiddenBodyItem(boundObj)) continue;

            // 1. KORUMALI EŞYALAR — ASLA SATILMAZ (kıyafet koruma ayarı + ruh taşları filtresi)
            if (itemID == 0x0000000F) continue; // Altın satılmaz
            const bool isGear = (boundObj->As<RE::TESObjectWEAP>() != nullptr || boundObj->As<RE::TESObjectARMO>() != nullptr);
            if (profile.IsProtected(itemID) && !isGear) continue; // Ekipman dışındaki korunan eşyalar satılmaz

            // Life Again treatment items remain in the follower's inventory
            // until Life Again itself consumes them to heal the follower.
            if (ClassInterface::GetSingleton()->IsTreatmentItem(itemID)) continue;
            if (auto* alchemy = boundObj->As<RE::AlchemyItem>();
                alchemy && alchemy->HasEffect(RE::EffectArchetype::kCurePoison) && count <= 1) {
                continue;  // Preserve the single emergency cure-poison reserve.
            }

            // SoulKeeper Mod Uyumluluğu: Ruh taşlarını koruma filtresi
            if (settings->protectSoulGems) {
                if (boundObj->As<RE::TESSoulGem>()) continue;
                std::string bName = boundObj->GetName();
                std::transform(bName.begin(), bName.end(), bName.begin(), ::tolower);
                if (bName.find("soul gem") != std::string::npos || bName.find("azura") != std::string::npos ||
                    bName.find("ruh taşı") != std::string::npos) {
                    continue;
                }
            }

            // Görev eşyası (Quest Item / Quest Alias) satılamaz
            if (invData.second && invData.second->extraLists) {
                bool isQuestItem = false;
                for (auto* xList : *invData.second->extraLists) {
                    if (xList && (xList->HasType(RE::ExtraDataType::kFromAlias) ||
                                  xList->HasType(RE::ExtraDataType::kAliasInstanceArray))) {
                        isQuestItem = true;
                        break;
                    }
                }
                if (isQuestItem) continue;
            }

            // Kuşanılmış silah ve giyilmiş zırh sayımı:
            // Üzerinde GİYİLİ/KUŞANILMIŞ olan hiçbir eşya satılmaz (çıplak gezmeleri önler).
            int32_t equippedWeaponCount = 0;
            if (equippedObjects.contains(boundObj)) {
                if (follower->GetEquippedObject(false) == boundObj) equippedWeaponCount++;
                if (follower->GetEquippedObject(true) == boundObj) equippedWeaponCount++;
            }

            int32_t wornArmorCount = 0;
            if (invData.second && invData.second->extraLists) {
                for (auto* xList : *invData.second->extraLists) {
                    if (xList && (xList->HasType(RE::ExtraDataType::kWorn) ||
                                  xList->HasType(RE::ExtraDataType::kWornLeft))) {
                        wornArmorCount++;
                    }
                }
            }

            int32_t totalEquipped = std::max(equippedWeaponCount, wornArmorCount);
            // Korunan veya giyilen eşyaların asıl kullanılan kopyası (en az 1 adet) korunur,
            // varsa fazladan olan kopyaları (örneğin 4 demir zırhtan 3 tanesi) satılabilir.
            int32_t protectedCopies = totalEquipped;
            if (profile.IsProtected(itemID)) {
                protectedCopies = std::max(protectedCopies, 1);
            }
            int32_t unequippedCount = count - protectedCopies;
            if (unequippedCount <= 0) continue; // Tüm kopyalar giyili/korumalı → satılmaz!

            int32_t baseVal = boundObj->GetGoldValue();
            if (baseVal <= 0) continue;

            // ── KATEGORİLER VE FAZLA MİKTARLAR (kategori toplamı üzerinden) ──
            const auto& pref = profile.preferences;
            bool isWeapon = settings->sellWeapons && pref.allowSellWeapons && (boundObj->As<RE::TESObjectWEAP>() != nullptr);
            bool isArmor = false;
            bool isClothing = false;
            bool isJewelry = false;

            if (auto armo = boundObj->As<RE::TESObjectARMO>()) {
                using Slot = RE::BIPED_MODEL::BipedObjectSlot;
                auto mask = static_cast<uint32_t>(armo->GetSlotMask());
                constexpr uint32_t kJewelryMask = static_cast<uint32_t>(Slot::kAmulet) |
                                                  static_cast<uint32_t>(Slot::kRing) |
                                                  static_cast<uint32_t>(Slot::kCirclet);
                if ((mask & kJewelryMask) != 0) {
                    if (pref.allowSellJewelry) isJewelry = true;
                } else if (armo->GetArmorRating() == 0.0f) {
                    if (pref.allowSellClothing) isClothing = true;
                } else {
                    if (settings->sellArmor && pref.allowSellArmor) isArmor = true;
                }
            }

            SupplyCat cat = ClassifySupply(evaluator, boundObj);

            bool isAmmo = false;
            int32_t ammoExcess = 0;
            if (settings->sellAmmo && pref.allowSellAmmo && (cat == SArrow || cat == SBolt)) {
                uint32_t limit = (cat == SBolt) ? pref.maxBolts : pref.maxArrows;
                ammoExcess = categoryExcess(cat, limit, count);
                if (ammoExcess > 0) {
                    isAmmo = true;
                    consumeExcess(cat, ammoExcess);
                }
            }

            bool isPotion = false;
            int32_t potionExcess = 0;
            if (settings->sellPotions && pref.allowSellPotions && cat != SOther) {
                if (cat >= SHealthMinor && cat <= SHealthUltimate) {
                    const uint32_t limits[] = { pref.maxHealthMinor, pref.maxHealthStandard, pref.maxHealthPlentiful,
                        pref.maxHealthVigorous, pref.maxHealthExtreme, pref.maxHealthUltimate };
                    potionExcess = categoryExcess(cat, limits[static_cast<size_t>(cat - SHealthMinor)], count);
                } else if (cat >= SMagickaMinor && cat <= SMagickaUltimate) {
                    const uint32_t limits[] = { pref.maxMagickaMinor, pref.maxMagickaStandard, pref.maxMagickaPlentiful,
                        pref.maxMagickaVigorous, pref.maxMagickaExtreme, pref.maxMagickaUltimate };
                    potionExcess = categoryExcess(cat, limits[static_cast<size_t>(cat - SMagickaMinor)], count);
                } else if (cat >= SStaminaMinor && cat <= SStaminaUltimate) {
                    const uint32_t limits[] = { pref.maxStaminaMinor, pref.maxStaminaStandard, pref.maxStaminaPlentiful,
                        pref.maxStaminaVigorous, pref.maxStaminaExtreme, pref.maxStaminaUltimate };
                    potionExcess = categoryExcess(cat, limits[static_cast<size_t>(cat - SStaminaMinor)], count);
                } else if (cat == SHealth) {
                    potionExcess = categoryExcess(cat, pref.maxHealthPotions, count);
                } else if (cat == SMagicka) {
                    potionExcess = categoryExcess(cat, pref.maxMagickaPotions, count);
                } else if (cat == SStamina) {
                    potionExcess = categoryExcess(cat, pref.maxStaminaPotions, count);
                } else if (cat == SPoisonHealth) {
                    potionExcess = categoryExcess(cat, pref.maxPoisonHealth, count);
                } else if (cat == SPoisonMagicka) {
                    potionExcess = categoryExcess(cat, pref.maxPoisonMagicka, count);
                } else if (cat == SPoisonStamina) {
                    potionExcess = categoryExcess(cat, pref.maxPoisonStamina, count);
                } else if (cat == SPoisonOther) {
                    potionExcess = categoryExcess(cat, pref.maxPoisonOther, count);
                } else if (cat == SPoison) {
                    potionExcess = categoryExcess(cat, pref.maxPoisons, count);
                } else if (cat == SInvisibility) {
                    potionExcess = categoryExcess(cat, pref.maxInvisibilityPotions, count);
                }
                if (potionExcess > 0) {
                    isPotion = true;
                    consumeExcess(cat, potionExcess);
                }
            }

            bool isIngredient = false;
            int32_t ingredientExcess = 0;
            if (pref.allowSellIngredients && cat == SIngredient) {
                // Simya malzemeleri kişisel kullanım içindir — 10 adedin fazlası satılır
                ingredientExcess = categoryExcess(cat, 10, count);
                if (ingredientExcess > 0) {
                    isIngredient = true;
                    consumeExcess(cat, ingredientExcess);
                }
            }

            // Yiyecek & içecek: kişisel stok limitinin KATEGORİsel fazlası satılır
            bool isFood = false;
            int32_t foodExcess = 0;
            if (settings->sellFood && pref.allowSellFood) {
                if (cat == SFoodL) {
                    foodExcess = categoryExcess(cat, pref.maxFoodLarge, count);
                } else if (cat == SFoodS) {
                    foodExcess = categoryExcess(cat, pref.maxFoodSmall, count);
                } else if (cat == SFoodM) {
                    foodExcess = categoryExcess(cat, pref.maxFoodMedium, count);
                }
                if (foodExcess > 0) {
                    isFood = true;
                    consumeExcess(cat, foodExcess);
                }
            }

            bool isDrink = false;
            int32_t drinkExcess = 0;
            if (settings->sellFood && pref.allowSellDrinks) {
                if (cat == SDrinkN) {
                    drinkExcess = categoryExcess(cat, pref.maxDrinkNormal, count);
                } else if (cat == SDrinkA) {
                    drinkExcess = categoryExcess(cat, pref.maxDrinkAlcohol, count);
                }
                if (drinkExcess > 0) {
                    isDrink = true;
                    consumeExcess(cat, drinkExcess);
                }
            }

            bool isGem = false;
            if (settings->sellGems && pref.allowSellGems) {
                if (auto misc = boundObj->As<RE::TESObjectMISC>()) {
                    if (!misc->As<RE::TESSoulGem>() && baseVal >= 40 && misc->GetWeight() < 1.0f && itemID != 0x0000000A) {
                        isGem = true;
                    }
                }
            }

            bool isBook = false;
            bool isScroll = false;
            if (boundObj->As<RE::ScrollItem>()) {
                if (pref.allowSellScrolls) isScroll = true;
            } else if (auto book = boundObj->As<RE::TESObjectBOOK>()) {
                if (book->IsNoteScroll()) {
                    if (pref.allowSellScrolls) isScroll = true;
                } else {
                    if (pref.allowSellBooks) isBook = true;
                }
            }

            bool isSoulGem = false;
            int32_t soulGemExcess = 0;
            if (!settings->protectSoulGems && pref.allowSellSoulGems && boundObj->As<RE::TESSoulGem>()) {
                soulGemExcess = std::min(count, std::max(0, soulGemTotal - static_cast<int32_t>(pref.maxSoulGems)));
                if (soulGemExcess > 0) {
                    isSoulGem = true;
                    soulGemTotal -= soulGemExcess;
                }
            }

            bool isConsumable = isFood || isDrink;
            int32_t consumableExcess = isFood ? foodExcess : drinkExcess;

            if (!isWeapon && !isArmor && !isClothing && !isJewelry && !isAmmo && !isPotion && !isIngredient && !isConsumable && !isGem && !isBook && !isScroll && !isSoulGem) continue;

            // Yeni satın alınan sarf malzemelerini hemen geri satma (döngü koruması)
            if ((isAmmo || isPotion || isConsumable || isSoulGem) && recentlyPurchased(itemID)) continue;

            // ── SATICI TİPİ EŞLEŞTİRME ──
            // Ok/cıvata → demirci; iksir/zehir/simya → simyacı; yemek/içecek → hancı;
            // silah/zırh → demirci; kıyafet/takı/mücevher/kitap/parşömen/ruh taşı → büyücü / genel tüccar.
            const CandidateVendor* chosenVendor = nullptr;
            uint32_t requiredMask = kVendNone;
            if (isWeapon || isArmor || isAmmo) requiredMask = kVendSmith | kVendGeneral;
            else if (isPotion || isIngredient) requiredMask = kVendApothecary | kVendGeneral;
            else if (isConsumable) requiredMask = kVendInn | kVendGeneral;
            else if (isGem || isJewelry || isClothing) requiredMask = kVendSmith | kVendApothecary | kVendInn | kVendGeneral | kVendWizard;
            else if (isBook || isScroll || isSoulGem) requiredMask = kVendWizard | kVendGeneral;

            for (const auto& cand : nearbyVendors) {
                uint32_t vmask = cand.vendorMask & ~kVendHealer; // Clergy only supplies cure potions.
                if (requiredMask & vmask) {
                    chosenVendor = &cand;
                    break;
                }
            }

            if (!chosenVendor || !chosenVendor->actor) continue;

            // Satılacak miktar: aşırı yüklüyse TAM yığın, değilse batch çarpanı kadar
            int32_t sellCount = 0;
            if (isWeapon || isArmor || isClothing || isJewelry) {
                sellCount = overEncumbered ? unequippedCount : std::min(unequippedCount, static_cast<int32_t>(batchMult));
            } else if (isAmmo) {
                sellCount = std::min(ammoExcess, static_cast<int32_t>(batchMult * 15));
            } else if (isPotion) {
                sellCount = std::min(potionExcess, static_cast<int32_t>(batchMult * 2));
            } else if (isIngredient) {
                sellCount = std::min(ingredientExcess, static_cast<int32_t>(batchMult * 3));
            } else if (isConsumable) {
                sellCount = std::min(consumableExcess, static_cast<int32_t>(batchMult * 2));
            } else if (isSoulGem) {
                sellCount = std::min(soulGemExcess, static_cast<int32_t>(batchMult * 2));
            } else if (isGem || isBook || isScroll) {
                sellCount = std::min(count, static_cast<int32_t>(batchMult * 2));
            }

            if (sellCount <= 0) continue;

            uint32_t unitPrice = CalculateGameSellPrice(priceActor, baseVal);
            uint32_t earnedGold = unitPrice * static_cast<uint32_t>(sellCount);

            // Eşyayı takipçinin envanterinden çıkar
            follower->RemoveItem(boundObj, sellCount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

            // Altını takipçinin hem envanterine hem profiline ekle
            follower->AddObjectToContainer(goldItem, nullptr, static_cast<int32_t>(earnedGold), nullptr);
            econ->AddGold(formID, static_cast<int32_t>(earnedGold));

            _lastSellTime[formID] = now;
            salesThisCall++;

            // Bildirim
            if (settings->notifySelling) {
                std::string msg;
                if (sellCount > 1) {
                    msg = std::format("{} sold {}x {} to {}. (+{} Gold)",
                        follower->GetName(), sellCount, boundObj->GetName(), chosenVendor->actor->GetName(), earnedGold);
                } else {
                    msg = std::format("{} sold {} to {}. (+{} Gold)",
                        follower->GetName(), boundObj->GetName(), chosenVendor->actor->GetName(), earnedGold);
                }
                notifier->ShowHUDMessage(msg);
            }
            logger::info("Selling: {} sold {}x {} to {} for {} gold.",
                follower->GetName(), sellCount, boundObj->GetName(), chosenVendor->actor->GetName(), earnedGold);

            // Normal durumda tek yığın satarak doğal davranır; aşırı yüklüyse hızlıca 3 yığına kadar satmaya devam eder
            if (!overEncumbered || salesThisCall >= kMaxSalesPerCall) {
                return;
            }
        }
    }
}
