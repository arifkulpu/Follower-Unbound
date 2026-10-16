#include "Core/ClassInterface.h"
#include "Core/FollowerManager.h"
#include <algorithm>

namespace FollowerUnbound
{
    void ClassInterface::SetTreatmentItems(const KingCore::TreatmentRegistration* reg)
    {
        if (!reg) return;
        std::unordered_set<uint32_t> incoming;
        incoming.reserve(reg->itemCount);
        for (uint32_t i = 0; i < reg->itemCount && i < KingCore::TREATMENT_MAX_ITEMS; ++i) {
            if (reg->items[i].formID != 0) incoming.insert(reg->items[i].formID);
        }
        {
            std::lock_guard lock(_treatmentMutex);
            _treatmentItems = std::move(incoming);
        }
        logger::info("Follower Unbound: {} treatment item(s) registered via King Core (protected from autonomous selling/cleanup).", reg->itemCount);
    }

    bool ClassInterface::IsTreatmentItem(RE::FormID formID) const
    {
        if (formID == 0) return false;
        std::lock_guard lock(_treatmentMutex);
        return _treatmentItems.contains(formID);
    }

    void ClassInterface::OnLifeAgainInjuryState(const LifeAgain::API::FollowerInjuryStateEvent& event)
    {
        if (event.apiVersion != LifeAgain::API::kVersion) return;
        if (event.formID == 0 && event.state == LifeAgain::API::InjuryState::Cleared) {
            std::lock_guard lock(_injuryMutex);
            _lifeAgainInjuries.clear();
            return;
        }
        if (event.formID == 0) return;
        if (event.state == LifeAgain::API::InjuryState::Cleared) {
            std::lock_guard lock(_injuryMutex);
            _lifeAgainInjuries.erase(event.formID);
            logger::info("LifeAgain injuries cleared for follower={:08X}.", event.formID);
            return;
        }
        const auto type = static_cast<uint8_t>(event.injuryType);
        if (type >= 8) return;
        {
            std::lock_guard lock(_injuryMutex);
            _lifeAgainInjuries[event.formID][type] = event.injuryCount;
        }
        logger::info("LifeAgain treatment demand updated: follower={:08X}, total doses={}",
            event.formID, GetTreatmentPotionDemand(event.formID));
    }

    uint32_t ClassInterface::GetTreatmentPotionDemand(RE::FormID followerFormID) const
    {
        std::lock_guard lock(_injuryMutex);
        const auto it = _lifeAgainInjuries.find(followerFormID);
        if (it == _lifeAgainInjuries.end()) return 0;
        uint32_t needed = 0;
        for (std::size_t i = 0; i < it->second.size(); ++i) {
            needed += static_cast<uint32_t>(it->second[i]) * (i < 4 ? 1u : 3u);
        }
        return needed;
    }

    void ClassInterface::SetClassBoundAPI(KingCore::IClassBoundAPI* api)
    {
        if (_classBoundAPI) {
            _classBoundAPI->UnregisterEventListener(this);
        }

        _classBoundAPI = api;
        _externalProviderActive = (_classBoundAPI != nullptr);

        if (_classBoundAPI) {
            _classBoundAPI->RegisterEventListener(this);
            logger::info("Follower Unbound: Connected to King Core / ClassBound API! Event listener registered.");
        }
    }

    void ClassInterface::OnClassEvent(const KingCore::ClassEvent& ev)
    {
        // Event-driven cache invalidation (No polling!)
        // Guard the cache — OnClassEvent can be called from any thread after King Core's
        // BroadcastEvent releases its lock.
        {
            std::lock_guard lock(_cacheMutex);
            _profileCache.erase(ev.actorFormID);
            if (ev.actor) {
                _profileCache.erase(ev.actor->GetFormID());
                if (ev.actor->GetActorBase()) {
                    _profileCache.erase(ev.actor->GetActorBase()->GetFormID());
                }
            }
        }
        logger::info("Follower Unbound: ClassEvent received for Actor [0x{:X}]. Profile marked dirty.", ev.actorFormID);

        // Update active follower profile if loaded (check both reference ID and base FormID)
        auto* followerMgr = FollowerManager::GetSingleton();
        auto profile = followerMgr->GetProfile(ev.actorFormID);
        if (!profile && ev.actor) {
            profile = followerMgr->GetProfile(ev.actor->GetFormID());
            if (!profile && ev.actor->GetActorBase()) {
                profile = followerMgr->GetProfile(ev.actor->GetActorBase()->GetFormID());
            }
        }

        if (profile) {
            profile->role = FromKingCoreClass(ev.newClass);
            // ClassBound'un orijinal sınıf adını UI gösterimi için sakla
            if (ev.actor) {
                profile->classBoundDisplayName = GetClassDisplayName(ev.actor, profile->role);
            }
            logger::info("Follower Unbound: Updated follower {} to role {} (Class: {})", 
                profile->name, GetRoleName(profile->role), profile->classBoundDisplayName);
        }
    }

    FollowerRole ClassInterface::FromKingCoreClass(KingCore::ClassID id)
    {
        switch (id) {
        case KingCore::ClassID::Warrior:      return FollowerRole::Warrior;
        case KingCore::ClassID::Knight:       return FollowerRole::Knight;
        case KingCore::ClassID::Paladin:      return FollowerRole::Paladin;
        case KingCore::ClassID::Thief:        return FollowerRole::Thief;
        case KingCore::ClassID::Assassin:     return FollowerRole::Assassin;

        case KingCore::ClassID::Archer:
        case KingCore::ClassID::Hunter:
        case KingCore::ClassID::Marksman:
        case KingCore::ClassID::Gunslinger:   return FollowerRole::Archer;
        case KingCore::ClassID::Crossbowman:  return FollowerRole::Crossbowman;

        case KingCore::ClassID::Mage:
        case KingCore::ClassID::Battlemage:
        case KingCore::ClassID::Elementalist: return FollowerRole::Mage;
        case KingCore::ClassID::Necromancer:  return FollowerRole::Necromancer;
        case KingCore::ClassID::Druid:        return FollowerRole::Druid;

        case KingCore::ClassID::Merchant:     return FollowerRole::Merchant;
        case KingCore::ClassID::Bard:         return FollowerRole::Bard;
        case KingCore::ClassID::Alchemist:    return FollowerRole::Alchemist;
        case KingCore::ClassID::Blacksmith:   return FollowerRole::Warrior;
        default:                              return FollowerRole::None;
        }
    }

    const char* ClassInterface::GetRoleName(FollowerRole role)
    {
        switch (role) {
        case FollowerRole::None:        return "No Class (Unassigned)";
        case FollowerRole::Warrior:     return "Warrior";
        case FollowerRole::Archer:      return "Archer / Ranger";
        case FollowerRole::Assassin:    return "Assassin";
        case FollowerRole::Thief:       return "Thief / Rogue";
        case FollowerRole::Mage:        return "Mage / Spellcaster";
        case FollowerRole::Necromancer: return "Necromancer";
        case FollowerRole::Knight:      return "Knight";
        case FollowerRole::Paladin:     return "Paladin";
        case FollowerRole::Druid:       return "Druid";
        case FollowerRole::Crossbowman: return "Crossbowman";
        case FollowerRole::Alchemist:   return "Alchemist";
        case FollowerRole::Bard:        return "Bard";
        case FollowerRole::Merchant:    return "Merchant";
        case FollowerRole::Custom:      return "Custom";
        default:                        return "No Class (Unassigned)";
        }
    }

    std::string ClassInterface::GetClassDisplayName(RE::Actor* actor, FollowerRole role)
    {
        if (_externalProviderActive && _classBoundAPI && actor && _classBoundAPI->IsActorEligible(actor)) {
            KingCore::ClassInfo info;
            if (_classBoundAPI->GetFollowerClassInfo(actor, info)) {
                if (info.displayName && info.displayName[0] != '\0') {
                    return info.displayName;
                }
                if (info.identifierName && info.identifierName[0] != '\0') {
                    return info.identifierName;
                }
            }
            // ClassBound aktif ancak oyuncu henüz bu yoldaş için bir sınıf seçmedi
            return "No Class (Unassigned)";
        }

        return GetRoleName(role);
    }

    FollowerRole ClassInterface::ResolveRole(RE::Actor* actor)
    {
        if (!actor) {
            return FollowerRole::None;
        }

        if (_externalProviderActive && _classBoundAPI && _classBoundAPI->IsActorEligible(actor)) {
            KingCore::ClassID classID = _classBoundAPI->GetFollowerClass(actor);
            if (classID != KingCore::ClassID::Unknown) {
                return FromKingCoreClass(classID);
            }
            // ClassBound aktif ancak oyuncu henüz bu yoldaş için bir sınıf seçmedi!
            // Sınıf seçene kadar hiçbir sınıf atanmamalıdır.
            return FollowerRole::None;
        }

        return FallbackDetectRole(actor);
    }

    bool ClassInterface::GetBehaviorProfile(RE::Actor* actor, KingCore::BehaviorProfile& outProfile)
    {
        if (!actor) return false;

        uint32_t formID = actor->GetFormID();

        {
            std::lock_guard lock(_cacheMutex);
            auto it = _profileCache.find(formID);
            if (it != _profileCache.end()) {
                outProfile = it->second;
                return true;
            }
        }

        // Lazy evaluation (outside lock — external API calls must not be made under lock)
        if (_externalProviderActive && _classBoundAPI && _classBoundAPI->IsActorEligible(actor)) {
            if (_classBoundAPI->GetBehaviorProfile(actor, outProfile)) {
                std::lock_guard lock(_cacheMutex);
                _profileCache[formID] = outProfile;
                return true;
            }
        }

        GenerateFallbackProfile(actor, outProfile);
        {
            std::lock_guard lock(_cacheMutex);
            _profileCache[formID] = outProfile;
        }
        return true;
    }

    void ClassInterface::GenerateFallbackProfile(RE::Actor* actor, KingCore::BehaviorProfile& outProfile)
    {
        outProfile = KingCore::BehaviorProfile{};
        FollowerRole role = FallbackDetectRole(actor);

        switch (role) {
        case FollowerRole::Archer:
            outProfile.classID = KingCore::ClassID::Archer;
            outProfile.preferredArmor = KingCore::ArmorPreference::Light;
            outProfile.preferredWeapon = KingCore::WeaponPreference::Bow;
            outProfile.ammoWeight = 100;
            outProfile.targetAmmoCount = 60;
            outProfile.staminaPotionWeight = 80;
            break;
        case FollowerRole::Mage:
        case FollowerRole::Necromancer:
            outProfile.classID = KingCore::ClassID::Mage;
            outProfile.preferredArmor = KingCore::ArmorPreference::Cloth;
            outProfile.preferredWeapon = KingCore::WeaponPreference::PureMagic;
            outProfile.magickaPotionWeight = 95;
            outProfile.targetMagickaPotions = 5;
            outProfile.soulGemWeight = 80;
            break;
        case FollowerRole::Assassin:
        case FollowerRole::Thief:
            outProfile.classID = KingCore::ClassID::Assassin;
            outProfile.preferredArmor = KingCore::ArmorPreference::Light;
            outProfile.preferredWeapon = KingCore::WeaponPreference::OneHandedDagger;
            outProfile.prefersStealth = true;
            outProfile.poisonWeight = 85;
            break;
        case FollowerRole::Merchant:
            outProfile.classID = KingCore::ClassID::Merchant;
            outProfile.isMerchantMinded = true;
            outProfile.goldWeight = 100;
            break;
        default:
            outProfile.classID = KingCore::ClassID::Warrior;
            outProfile.preferredArmor = KingCore::ArmorPreference::Heavy;
            outProfile.preferredWeapon = KingCore::WeaponPreference::OneHandedSword;
            outProfile.healthPotionWeight = 80;
            outProfile.targetHealthPotions = 3;
            break;
        }
    }

    FollowerRole ClassInterface::FallbackDetectRole(RE::Actor* actor)
    {
        auto base = actor->GetActorBase();
        if (base && base->npcClass) {
            std::string className = base->npcClass->GetName();
            std::transform(className.begin(), className.end(), className.begin(), ::tolower);

            if (className.find("druid") != std::string::npos || className.find("shaman") != std::string::npos) {
                return FollowerRole::Druid;
            }
            if (className.find("arch") != std::string::npos || className.find("scout") != std::string::npos || className.find("ranger") != std::string::npos) {
                return FollowerRole::Archer;
            }
            if (className.find("mage") != std::string::npos || className.find("sorcer") != std::string::npos || className.find("wizard") != std::string::npos) {
                return FollowerRole::Mage;
            }
            if (className.find("thief") != std::string::npos || className.find("rogue") != std::string::npos) {
                return FollowerRole::Thief;
            }
            if (className.find("assassin") != std::string::npos) {
                return FollowerRole::Assassin;
            }
            if (className.find("paladin") != std::string::npos) {
                return FollowerRole::Paladin;
            }
            if (className.find("knight") != std::string::npos) {
                return FollowerRole::Knight;
            }
            if (className.find("necro") != std::string::npos) {
                return FollowerRole::Necromancer;
            }
            if (className.find("bard") != std::string::npos) {
                return FollowerRole::Bard;
            }
        }

        auto avOwner = actor->AsActorValueOwner();
        if (avOwner) {
            float marksman = avOwner->GetActorValue(RE::ActorValue::kArchery);
            float destruction = avOwner->GetActorValue(RE::ActorValue::kDestruction);
            float sneak = avOwner->GetActorValue(RE::ActorValue::kSneak);
            float oneHanded = avOwner->GetActorValue(RE::ActorValue::kOneHanded);
            float twoHanded = avOwner->GetActorValue(RE::ActorValue::kTwoHanded);

            if (marksman > 25.0f && marksman > oneHanded && marksman > twoHanded && marksman > destruction) {
                return FollowerRole::Archer;
            }
            // Sadece gerçek büyücüler Mage olarak tanımlansın (en az 25 destruction veya bariz üstünlük)
            if (destruction > 25.0f && destruction > oneHanded && destruction > twoHanded && destruction > marksman) {
                return FollowerRole::Mage;
            }
            if (sneak > 50.0f && marksman < 40.0f && destruction < 40.0f) {
                return FollowerRole::Assassin;
            }
        }

        return FollowerRole::Warrior;
    }
}
