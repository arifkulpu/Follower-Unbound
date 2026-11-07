#include "AI/TradeManager.h"
#include "Core/EventManager.h"
#include "AI/AnimationManager.h"
#include "AI/ItemEvaluator.h"
#include "Core/FollowerManager.h"
#include "Economy/ShoppingAI.h"
#include "RE/B/BGSRefAlias.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace FollowerUnbound
{
    namespace
    {
        using namespace std::chrono_literals;
        using Clock = std::chrono::steady_clock;

        constexpr std::string_view kPlugin = "FollowerUnboundTrade.esp";

        // local FormIDs in FollowerUnboundTrade.esp (same order as make_trade_esl.py allocates them):
        // factions (follower, merchant per slot), the cell, then the spots, quests, packages and scenes of all slots
        constexpr int        kSlotCount = 10;   // must match SLOTS in make_trade_esl.py
        constexpr RE::FormID kFirstId = 0x800;
        constexpr RE::FormID FollowerFactionId(int i) { return kFirstId + 2 * i; }
        constexpr RE::FormID MerchantFactionId(int i) { return kFirstId + 2 * i + 1; }
        constexpr RE::FormID kCellId = kFirstId + 2 * kSlotCount;
        constexpr RE::FormID SpotId(int i) { return kCellId + 1 + i; }
        constexpr RE::FormID QuestId(int i) { return kCellId + 1 + kSlotCount + i; }
        constexpr RE::FormID PackId(int i) { return kCellId + 1 + 2 * kSlotCount + i; }
        constexpr RE::FormID SceneId(int i) { return kCellId + 1 + 3 * kSlotCount + i; }

        // Skyrim.esm idles: pointing at the goods, handing over, taking, stop
        constexpr RE::FormID kIdlePointClose = 0x0FFA0C;
        constexpr RE::FormID kIdleGive = 0x0B5E20;
        constexpr RE::FormID kIdleTake = 0x0B5E1F;
        constexpr RE::FormID kIdleStop = 0x10D9EE;
        constexpr RE::FormID kGold = 0x00000F;

        constexpr auto kStagger = 3000ms;      // between two pairs setting off
        constexpr auto kQuestCalm = 1500ms;    // a slot is not reused right after its quest stopped
        constexpr float kGestureDelay = 1.4f;  // seconds into the talk: the money changes hands

        float Secs(Clock::time_point from, Clock::time_point to)
        {
            return std::chrono::duration<float>(to - from).count();
        }

        float Dist(RE::TESObjectREFR* a, RE::TESObjectREFR* b)
        {
            return a->GetPosition().GetDistance(b->GetPosition());
        }

        std::string Name(RE::Actor* a)
        {
            return a ? std::string(a->GetDisplayFullName()) : std::string("?");
        }

        // One place for the process access: if your CommonLib exposes it differently, fix it here only.
        void PlayIdleForm(RE::Actor* actor, RE::FormID idleId)
        {
            auto* idle = RE::TESForm::LookupByID<RE::TESIdleForm>(idleId);
            auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
            if (idle && process) {
                process->PlayIdle(actor, idle, nullptr);
            }
        }

        bool CanGesture(RE::Actor* a)
        {
            auto* state = a ? a->AsActorState() : nullptr;
            return a && a->Is3DLoaded() && !a->IsDead() && !a->IsInCombat() && state && !state->IsWeaponDrawn() &&
                   state->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal && !a->IsOnMount();
        }

        bool CanTurn(RE::Actor* a)
        {
            auto* state = a ? a->AsActorState() : nullptr;
            return state && a->Is3DLoaded() && !a->IsDead() && !a->IsInCombat() && !a->IsOnMount() &&
                   state->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal;
        }

        void FaceTowards(RE::Actor* actor, RE::TESObjectREFR* target)
        {
            if (!actor || !target) return;
            const auto from = actor->GetPosition();
            const auto to = target->GetPosition();
            actor->data.angle.z = std::atan2(to.x - from.x, to.y - from.y);
        }

        std::uint32_t ScenePhase(RE::BGSScene* scene)
        {
            return (scene && scene->isPlaying) ? 1 : 0;
        }
    }

    // ------------------------------------------------------------------ settings file

    namespace
    {
        constexpr const char* kConfigPath = "Data/SKSE/Plugins/FollowerUnboundTrade.json";
    }

    TradeManager::TradeManager()
    {
        LoadConfig();
    }

    void TradeManager::LoadConfig()
    {
        try {
            std::ifstream in(kConfigPath);
            if (!in) return;   // no file yet: defaults
            const auto j = nlohmann::json::parse(in, nullptr, true, true);
            auto&      c = _config;
            c.enabled = j.value("enabled", c.enabled);
            c.holdWhileWalking = j.value("holdWhileWalking", c.holdWhileWalking);
            c.voiceEnabled = j.value("voiceEnabled", c.voiceEnabled);
            c.noticeDistance = std::clamp(j.value("noticeDistance", c.noticeDistance), 300.0f, 1500.0f);
            c.nearbyStaySeconds = std::clamp(j.value("nearbyStaySeconds", c.nearbyStaySeconds), 2.0f, 20.0f);
            c.lingerSeconds = std::clamp(j.value("lingerSeconds", c.lingerSeconds), 1.0f, 10.0f);
            c.cooldownGameDays = std::clamp(j.value("cooldownGameDays", c.cooldownGameDays), 1.0f / 24.0f, 3.0f);
            c.noDealCooldownGameDays = std::clamp(j.value("noDealCooldownGameDays", c.noDealCooldownGameDays), 1.0f / 24.0f, 1.0f);
            logger::info("TradeManager: settings loaded from {}", kConfigPath);
        } catch (const std::exception& e) {
            logger::warn("TradeManager: could not read {} ({}), defaults are used", kConfigPath, e.what());
        }
    }

    void TradeManager::SaveConfig() const
    {
        try {
            const auto&    c = _config;
            nlohmann::json j = {
                { "enabled", c.enabled },
                { "holdWhileWalking", c.holdWhileWalking },
                { "voiceEnabled", c.voiceEnabled },
                { "noticeDistance", c.noticeDistance },
                { "nearbyStaySeconds", c.nearbyStaySeconds },
                { "lingerSeconds", c.lingerSeconds },
                { "cooldownGameDays", c.cooldownGameDays },
                { "noDealCooldownGameDays", c.noDealCooldownGameDays }
            };
            std::filesystem::create_directories(std::filesystem::path(kConfigPath).parent_path());
            std::ofstream out(kConfigPath, std::ios::trunc);
            out << j.dump(2);
        } catch (const std::exception& e) {
            logger::warn("TradeManager: could not write {}: {}", kConfigPath, e.what());
        }
    }

    // ------------------------------------------------------------------ forms / slots

    const TradeManager::SlotForms& TradeManager::Forms(int slot)
    {
        if (!_formsLoaded) {
            static_assert(kSlotCount == kSlots, "kSlots (TradeManager.h) and kSlotCount (TradeManager.cpp) must match");
            _formsLoaded = true;
            _formsOk = true;
            auto* data = RE::TESDataHandler::GetSingleton();
            for (int i = 0; i < kSlots && data; ++i) {
                _forms[i].quest = data->LookupForm<RE::TESQuest>(QuestId(i), kPlugin);
                _forms[i].followerFaction = data->LookupForm<RE::TESFaction>(FollowerFactionId(i), kPlugin);
                _forms[i].merchantFaction = data->LookupForm<RE::TESFaction>(MerchantFactionId(i), kPlugin);
                _forms[i].scene = data->LookupForm<RE::BGSScene>(SceneId(i), kPlugin);
                _forms[i].spot = data->LookupForm<RE::TESObjectREFR>(SpotId(i), kPlugin);
                const auto& f = _forms[i];
                if (!f.quest || !f.followerFaction || !f.merchantFaction || !f.scene || !f.spot) {
                    _formsOk = false;
                }
            }
            if (!data) _formsOk = false;
            logger::info("TradeManager: {} {}", kPlugin, _formsOk ? "found, walking to merchants is on" : "NOT found: instant trading stays");
        }
        return _forms[slot];
    }

    bool TradeManager::PluginAvailable()
    {
        Forms(0);
        return _formsOk;
    }

    int TradeManager::SlotOf(RE::FormID follower) const
    {
        for (int i = 0; i < kSlots; ++i) {
            if (follower != 0 && _slots[i].follower == follower && _slots[i].state != State::Free) return i;
        }
        return -1;
    }

    bool TradeManager::IsBusy(RE::FormID followerID) const
    {
        return SlotOf(followerID) >= 0;
    }

    int TradeManager::FreeSlot()
    {
        const auto now = Clock::now();
        if (now - _lastSlotStart < kStagger) return -1;
        for (int i = 0; i < kSlots; ++i) {
            const auto& f = Forms(i);
            if (_slots[i].state == State::Free && now >= _slots[i].calmUntil && f.quest && !f.quest->IsEnabled() && !f.quest->IsStopping()) {
                return i;
            }
        }
        return -1;
    }

    bool TradeManager::MerchantBusy(RE::Actor* merchant)
    {
        const auto id = merchant->GetFormID();
        if (const auto it = _merchantRest.find(id); it != _merchantRest.end()) {
            if (Clock::now() < it->second) return true;
            _merchantRest.erase(it);
        }
        for (const auto& s : _slots) {
            if (s.state != State::Free && s.merchantID == id) return true;
        }
        return false;
    }

    // ------------------------------------------------------------------ merchants

    bool TradeManager::OpenNow(RE::TESFaction* faction)
    {
        auto* calendar = RE::Calendar::GetSingleton();
        if (!calendar || !faction) return false;
        const auto& v = faction->vendorData.vendorValues;
        const float hour = calendar->GetHour();
        if (v.startHour == v.endHour) return true;
        return v.startHour < v.endHour ? (hour >= v.startHour && hour < v.endHour) : (hour >= v.startHour || hour < v.endHour);
    }

    bool TradeManager::VendorOpen(RE::Actor* merchant)
    {
        RE::TESFaction* vendor = nullptr;
        merchant->VisitFactions([&](RE::TESFaction* faction, std::int8_t rank) {
            if (faction && rank >= 0 && faction->IsVendor() && faction->vendorData.merchantContainer) {
                vendor = faction;
                return true;
            }
            return false;
        });
        return !vendor || OpenNow(vendor);
    }

    // The merchant's customer side (where a stall's or counter's keeper faces), else the follower's side.
    RE::NiPoint3 TradeManager::CustomerSide(RE::Actor* merchant, RE::Actor* follower, float ahead)
    {
        const auto at = merchant->GetPosition();
        float      dx = std::sin(merchant->GetAngleZ()), dy = std::cos(merchant->GetAngleZ());
        if (merchant->IsMoving()) {
            const auto  from = follower->GetPosition();
            const float apart = std::hypot(from.x - at.x, from.y - at.y);
            if (apart < 1.0f) return at;
            dx = (from.x - at.x) / apart;
            dy = (from.y - at.y) / apart;
        }
        return { at.x + dx * ahead, at.y + dy * ahead, at.z };
    }

    // Clearly behind the merchant (more than 120 degrees off its customer side): the back of a stall.
    bool TradeManager::BehindMerchant(const Config& c, RE::Actor* follower, RE::Actor* merchant)
    {
        const auto at = merchant->GetPosition();
        const auto ahead = CustomerSide(merchant, follower, c.spotAhead);
        const auto from = follower->GetPosition();
        const float fx = from.x - at.x, fy = from.y - at.y;
        const float apart = std::hypot(fx, fy);
        return apart >= 1.0f && (fx * (ahead.x - at.x) + fy * (ahead.y - at.y)) / (apart * c.spotAhead) < -0.5f;
    }

    bool TradeManager::InReach(RE::Actor* follower, RE::Actor* merchant, float reach) const
    {
        const auto from = follower->GetPosition();
        const auto to = merchant->GetPosition();
        return from.GetDistance(to) <= reach && std::abs(from.z - to.z) <= _config.floorHeight;
    }

    // ------------------------------------------------------------------ business check

    bool TradeManager::DefaultBusiness(RE::Actor* actor, FollowerProfile& profile) const
    {
        // looted loose gear waiting in the phantom backpack is exactly what gets sold
        if (!profile.backpackItems.empty() || !profile.backpackCustom.empty()) return true;

        auto* gold = RE::TESForm::LookupByID<RE::TESBoundObject>(kGold);
        if (gold && ItemEvaluator::GetItemCount(actor, gold) >= 100) return true;   // something to spend

        for (auto& [obj, data] : actor->GetInventory()) {
            if (!obj || data.first <= 0 || obj->GetFormID() == kGold || obj->GetGoldValue() <= 0) continue;
            if (profile.IsProtected(obj->GetFormID())) continue;
            if (data.second && (data.second->IsWorn() || data.second->IsQuestObject())) continue;
            return true;
        }
        return false;
    }

    // ------------------------------------------------------------------ the per-follower tick

    bool TradeManager::HandleTradeTick(RE::Actor* actor, FollowerProfile& profile)
    {
        if (EventManager::IsOstimBusy()) return true;
        if (!_config.enabled || !PluginAvailable()) return false;   // old instant path
        if (!actor || actor->IsDead() || !actor->Is3DLoaded()) return true;
        if (SlotOf(actor->GetFormID()) >= 0) return true;           // already on its way / talking

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* calendar = RE::Calendar::GetSingleton();
        if (!player || !calendar || player->IsInCombat() || actor->IsInCombat() || actor->IsOnMount()) return true;
        if (auto* state = actor->AsActorState(); state && state->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal) return true;

        const auto  now = Clock::now();
        const float today = calendar->GetCurrentGameTime();

        // cheap check first (when there is no custom check): nothing to sell and nothing to spend means no trip
        if (!_businessCheck && !DefaultBusiness(actor, profile)) return true;

        // the vendors ShoppingAI itself would trade with, nearest first
        for (auto* merchant : TradeVendorFilter::Nearby(actor, _config.noticeDistance)) {
            if (!merchant || merchant->IsDead() || merchant->IsInCombat() || merchant->IsPlayerTeammate()) continue;
            if (auto* state = merchant->AsActorState(); state && state->GetSitSleepState() == RE::SIT_SLEEP_STATE::kIsSleeping) continue;
            if (!VendorOpen(merchant)) continue;
            if (merchant->IsHostileToActor(actor)) continue;

            const PairKey key{ actor->GetFormID(), merchant->GetFormID() };
            if (const auto allowed = _tradeAllowedDay.find(key); allowed != _tradeAllowedDay.end() && today < allowed->second) continue;
            if (const auto failed = _failedAt.find(key); failed != _failedAt.end() && Secs(failed->second, now) < _config.retrySeconds) continue;
            if (MerchantBusy(merchant)) continue;   // busy with another follower: try another merchant

            // it has to stay near for a while, not just pass by
            auto& near = _nearSince[key];
            if (near.since == Clock::time_point{} || Secs(near.last, now) > 6.0f) near.since = now;
            near.last = now;
            if (Secs(near.since, now) < _config.nearbyStaySeconds) return true;

            // a scene of another mod (or the game's own chatter) cannot be mixed with ours
            if (actor->GetCurrentScene() || merchant->GetCurrentScene()) return true;
            if (Dist(actor, player) > _config.playerLeash) return true;

            if (_businessCheck && !_businessCheck(actor, profile, merchant)) {
                _failedAt[key] = now;   // looked at again after retrySeconds
                return true;
            }
            if (!StartWalk(actor, merchant)) {
                _failedAt[key] = now;
            }
            return true;   // one merchant at a time per follower
        }
        return true;
    }

    // ------------------------------------------------------------------ walk

    bool TradeManager::StartWalk(RE::Actor* follower, RE::Actor* merchant)
    {
        const int slot = FreeSlot();
        if (slot < 0) return false;
        const auto& f = Forms(slot);
        const PairKey key{ follower->GetFormID(), merchant->GetFormID() };
        const bool far = _farSpot.contains(key);
        const auto spot = CustomerSide(merchant, follower, far ? _config.farSpotAhead : _config.spotAhead);

        f.spot->MoveTo(merchant);
        f.spot->SetPosition(spot);

        follower->AddToFaction(f.followerFaction, 0);
        merchant->AddToFaction(f.merchantFaction, 0);
        if (!f.quest->Start()) {
            follower->RemoveFromFaction(f.followerFaction);
            merchant->RemoveFromFaction(f.merchantFaction);
            logger::info("TradeManager: slot {} quest did not start ({} -> {})", slot + 1, Name(follower), Name(merchant));
            return false;
        }

        auto& s = _slots[slot];
        s = Slot{};
        s.state = State::Walking;
        s.follower = follower->GetFormID();
        s.followerHandle = follower->GetHandle();
        s.merchantID = merchant->GetFormID();
        s.merchant = merchant->GetHandle();
        s.started = Clock::now();
        s.spotPos = spot;
        s.playerApart = Dist(follower, RE::PlayerCharacter::GetSingleton());
        _lastSlotStart = s.started;
        HoldFollower(follower, true);
        logger::info("TradeManager: {} walks over to {} (slot {}, {:.0f} away)", Name(follower), Name(merchant), slot + 1, Dist(follower, merchant));
        return true;
    }

    void TradeManager::AdvanceWalk(int slot)
    {
        auto&       s = _slots[slot];
        const auto& forms = Forms(slot);
        auto*       follower = RE::TESForm::LookupByID<RE::Actor>(s.follower);
        auto        mp = s.merchant.get();
        auto*       merchant = mp.get();
        const PairKey key{ s.follower, s.merchantID };
        const auto  now = Clock::now();

        const auto giveUp = [&](const std::string& why) {
            logger::info("TradeManager: {} could not get to {} ({}), tries again later", Name(follower), Name(merchant), why);
            _failedAt[key] = now;
            EndSlot(slot, true, why.c_str());
        };

        if (!follower || !merchant || follower->IsDead() || merchant->IsDead() || !follower->Is3DLoaded() || !merchant->Is3DLoaded()) return giveUp("gone");
        if (follower->IsInCombat() || merchant->IsInCombat()) return giveUp("a fight");

        const float apart = Dist(follower, merchant);
        const auto  phase = ScenePhase(forms.scene);
        const float toSpot = follower->GetPosition().GetDistance(s.spotPos);
        if (toSpot < s.closest - 20.0f) {
            s.closest = toSpot;
            s.closerAt = now;
        }
        // near but no nearer for a while: others stand at the counter, or the counter is in the way
        const bool stalled = s.closerAt != Clock::time_point{} && Secs(s.closerAt, now) > _config.stallSeconds && InReach(follower, merchant, _config.reachArrived);

        if ((phase >= 1 || stalled) && BehindMerchant(_config, follower, merchant)) {
            _farSpot.insert(key);   // next time a spot further out in front
            return giveUp("ended up behind the merchant");
        }

        const bool atSpot = toSpot <= 150.0f && InReach(follower, merchant, _config.reachArrived);
        if (atSpot || (InReach(follower, merchant, _config.reach) && !BehindMerchant(_config, follower, merchant)) ||
            (phase >= 1 && InReach(follower, merchant, _config.reachArrived)) || stalled) {
            OnArrive(slot, follower, merchant);
            return;
        }

        if (!forms.quest->IsEnabled()) return giveUp("the scene ended");
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            if (Dist(follower, player) > (std::max)(_config.playerLeash, s.playerApart + 600.0f)) return giveUp("you went off");
        }
        if (apart > _config.merchantGone) return giveUp("the merchant walked off");
        if (s.closerAt != Clock::time_point{} && Secs(s.closerAt, now) > _config.stuckSeconds) return giveUp("not getting nearer");
        if (Secs(s.started, now) > _config.walkTimeoutSeconds) return giveUp("took too long");
    }

    // ------------------------------------------------------------------ the trade and the talk

    void TradeManager::OnArrive(int slot, RE::Actor* follower, RE::Actor* merchant)
    {
        auto&      s = _slots[slot];
        const auto now = Clock::now();
        s.state = State::Talking;
        s.talkStart = now;

        // weapon away, face each other (the scene's packages keep both still)
        if (auto* state = follower->AsActorState(); state && state->IsWeaponDrawn()) {
            AnimationManager::GetSingleton()->SheatheWeapon(follower);
        }
        if (CanTurn(follower)) FaceTowards(follower, merchant);
        if (CanTurn(merchant)) FaceTowards(merchant, follower);
        if (CanGesture(follower)) PlayIdleForm(follower, kIdlePointClose);

        // the actual business: the existing ShoppingAI (it applies PROTECTED, the phantom backpack and the buy rules)
        auto* gold = RE::TESForm::LookupByID<RE::TESBoundObject>(kGold);
        const auto goldOf = [&]() { return gold ? ItemEvaluator::GetItemCount(follower, gold) : 0; };
        const int  before = goldOf();
        int        afterSell = before;
        if (auto profile = FollowerManager::GetSingleton()->GetProfile(follower->GetFormID())) {
            TradeVendorFilter::Scope onlyThisMerchant(merchant->GetFormID());   // ShoppingAI trades with this one only
            ShoppingAI::GetSingleton()->ProcessSelling(follower, *profile);
            afterSell = goldOf();
            ShoppingAI::GetSingleton()->ProcessShopping(follower, *profile);
        }
        const int after = goldOf();
        s.sold = afterSell > before;
        s.bought = after < afterSell;
        if (auto* calendar = RE::Calendar::GetSingleton()) {
            // a real deal: a day off with this merchant. Nothing changed hands: back in 6 game hours (new loot, new needs)
            _tradeAllowedDay[{ s.follower, s.merchantID }] =
                calendar->GetCurrentGameTime() + ((s.sold || s.bought) ? _config.cooldownGameDays : _config.noDealCooldownGameDays);
        }
        logger::info("TradeManager: {} reached {} ({:.0f} away): sold {}, bought {}", Name(follower), Name(merchant), Dist(follower, merchant), s.sold, s.bought);

        // the talk: the game's own voice lines (nothing configured / no fitting line: it stays quiet and short)
        s.lines.clear();
        if (_config.voiceEnabled) s.lines = TradeVoice::GetSingleton()->Plan(follower, merchant, s.sold, s.bought);
        s.talkSeconds = s.lines.empty() ? _config.lingerSeconds : (std::max)(_config.lingerSeconds, s.lines.back().at + TradeVoice::GetSingleton()->TailSeconds());

        if (_talkHook) _talkHook(follower, merchant, s.sold, s.bought);
    }

    void TradeManager::AdvanceTalk(int slot)
    {
        auto&      s = _slots[slot];
        const auto now = Clock::now();
        auto*      follower = RE::TESForm::LookupByID<RE::Actor>(s.follower);
        auto       mp = s.merchant.get();
        auto*      merchant = mp.get();
        if (!follower || !merchant || follower->IsDead() || merchant->IsDead() || follower->IsInCombat() || merchant->IsInCombat()) {
            return EndSlot(slot, true, "talk interrupted");
        }
        const float elapsed = Secs(s.talkStart, now);
        for (auto& line : s.lines) {
            if (line.done || elapsed < line.at) continue;
            line.done = true;
            if (auto speaker = line.speaker.get()) TradeVoice::Say(speaker.get(), line.topic);
        }
        if (!s.gestured && elapsed >= kGestureDelay) {
            s.gestured = true;
            // money changes hands: the one who pays gives, the other takes
            if (s.bought || s.sold) {
                auto* giver = s.bought ? follower : merchant;
                auto* taker = s.bought ? merchant : follower;
                if (CanGesture(giver)) PlayIdleForm(giver, kIdleGive);
                if (CanGesture(taker)) PlayIdleForm(taker, kIdleTake);
            }
        }
        if (elapsed >= s.talkSeconds) EndSlot(slot, false, "done");
    }

    // ------------------------------------------------------------------ end / cleanup

    void TradeManager::EndSlot(int slot, bool failed, const char* why)
    {
        auto&       s = _slots[slot];
        const auto& forms = Forms(slot);
        const auto  now = Clock::now();
        auto*       follower = RE::TESForm::LookupByID<RE::Actor>(s.follower);
        auto        mp = s.merchant.get();
        auto*       merchant = mp.get();

        if (follower) {
            HoldFollower(follower, false);
            follower->RemoveFromFaction(forms.followerFaction);
            if (CanGesture(follower)) PlayIdleForm(follower, kIdleStop);
        }
        if (merchant) {
            merchant->RemoveFromFaction(forms.merchantFaction);
            if (!failed) _merchantRest[merchant->GetFormID()] = now + std::chrono::milliseconds(static_cast<int>(_config.merchantRestSeconds * 1000));
        }
        // whoever the aliases still hold
        for (const std::uint32_t aliasIdx : { 0u, 1u }) {
            if (forms.quest && aliasIdx < forms.quest->aliases.size()) {
                auto* baseAlias = forms.quest->aliases[aliasIdx];
                auto* refAlias = baseAlias ? skyrim_cast<RE::BGSRefAlias*>(baseAlias) : nullptr;
                if (refAlias) {
                    if (auto* actor = refAlias->GetActorReference()) {
                        actor->RemoveFromFaction(aliasIdx == 0 ? forms.followerFaction : forms.merchantFaction);
                    }
                }
            }
        }
        if (forms.quest->IsEnabled()) forms.quest->Stop();

        if (!s.lines.empty()) {   // subtitles of the talk off the screen
            TradeVoice::ClearSubtitles(s.followerHandle);
            TradeVoice::ClearSubtitles(s.merchant);
        }

        QueueRepack(follower ? follower->GetHandle() : RE::ActorHandle{}, merchant ? merchant->GetHandle() : RE::ActorHandle{});
        logger::info("TradeManager: slot {} over ({})", slot + 1, why ? why : "");
        s = Slot{};
        s.calmUntil = now + kQuestCalm;
    }

    void TradeManager::AbortAll(const char* why)
    {
        for (int i = 0; i < kSlots; ++i) {
            if (_slots[i].state != State::Free) {
                EndSlot(i, true, why);
            } else if (PluginAvailable()) {
                const auto& f = Forms(i);
                if (f.quest && f.quest->IsEnabled()) f.quest->Stop();   // a quest a save left running
            }
        }
        for (const auto& [id, before] : _heldBefore) {
            if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
                if (auto* av = actor->AsActorValueOwner()) av->SetActorValue(RE::ActorValue::kWaitingForPlayer, before);
            }
        }
        _heldBefore.clear();
        _nearSince.clear();
        _repacks.clear();
        _merchantRest.clear();
    }

    // Optional: WaitingForPlayer keeps follower mods from teleporting it back mid-walk. It is NOT saved with the game
    // (see Config::holdWhileWalking), so it is off by default: the scene's packages already outrank follow packages.
    void TradeManager::HoldFollower(RE::Actor* actor, bool hold)
    {
        if (!actor || !_config.holdWhileWalking) return;
        auto* av = actor->AsActorValueOwner();
        if (!av) return;
        const auto id = actor->GetFormID();
        if (hold) {
            if (_heldBefore.contains(id)) return;
            const float before = av->GetActorValue(RE::ActorValue::kWaitingForPlayer);
            if (before != 0.0f) return;   // already waiting by your order
            av->SetActorValue(RE::ActorValue::kWaitingForPlayer, 1.0f);
            _heldBefore[id] = before;
        } else if (const auto it = _heldBefore.find(id); it != _heldBefore.end()) {
            av->SetActorValue(RE::ActorValue::kWaitingForPlayer, it->second);
            _heldBefore.erase(it);
        }
    }

    // After a scene both pick their package anew (one left in "movement blocked" would stand on the spot).
    void TradeManager::QueueRepack(RE::ActorHandle a, RE::ActorHandle b)
    {
        _repacks.push_back({ a, b, Clock::now() + 600ms });
    }

    void TradeManager::ProcessRepacks()
    {
        const auto now = Clock::now();
        for (auto it = _repacks.begin(); it != _repacks.end();) {
            if (now < it->at) {
                ++it;
                continue;
            }
            for (const auto& handle : { it->a, it->b }) {
                if (auto actor = handle.get(); actor && !actor->IsDead()) actor->EvaluatePackage(true, false);
            }
            it = _repacks.erase(it);
        }
    }

    // A quest left running with nobody in it (a save made mid-scene) is stopped after 20 s.
    void TradeManager::StopIdleQuests()
    {
        const auto now = Clock::now();
        bool       anyIdleRunning = false;
        for (int i = 0; i < kSlots; ++i) {
            const auto& f = Forms(i);
            if (_slots[i].state == State::Free && f.quest && f.quest->IsEnabled()) anyIdleRunning = true;
        }
        if (!anyIdleRunning) {
            _idleQuestSince = {};
            return;
        }
        if (_idleQuestSince == Clock::time_point{}) {
            _idleQuestSince = now;
        } else if (Secs(_idleQuestSince, now) > 20.0f) {
            _idleQuestSince = {};
            for (int i = 0; i < kSlots; ++i) {
                const auto& f = Forms(i);
                if (_slots[i].state == State::Free && f.quest && f.quest->IsEnabled()) {
                    logger::info("TradeManager: slot {} quest running with nobody in it, stopped", i + 1);
                    f.quest->Stop();
                }
            }
        }
    }

    // ------------------------------------------------------------------ update

    void TradeManager::Update()
    {
        const auto now = Clock::now();
        if (now - _lastUpdate < 400ms) return;
        _lastUpdate = now;
        if (!PluginAvailable()) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        const bool playerFighting = player && player->IsInCombat();
        for (int i = 0; i < kSlots; ++i) {
            switch (_slots[i].state) {
            case State::Walking:
                if (playerFighting) EndSlot(i, true, "you are fighting");
                else AdvanceWalk(i);
                break;
            case State::Talking:
                AdvanceTalk(i);
                break;
            default:
                break;
            }
        }
        ProcessRepacks();
        StopIdleQuests();
    }

    void TradeManager::ScheduleUpdate()
    {
        bool expected = false;
        if (!_updateScheduled.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            _updateScheduled.store(false, std::memory_order_release);
            return;
        }
        tasks->AddUITask([this]() {
            Update();
            _updateScheduled.store(false, std::memory_order_release);
        });
    }

    namespace TradeVendorFilter
    {
        namespace { RE::FormID g_vendorFilter = 0; }
        void       Set(RE::FormID vendorID) { g_vendorFilter = vendorID; }
        RE::FormID Get() { return g_vendorFilter; }
    }
}
