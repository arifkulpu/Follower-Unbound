#include "AI/TradeVoice.h"
#include <algorithm>
#include <fstream>
#include <random>
#include <nlohmann/json.hpp>

namespace FollowerUnbound
{
    namespace
    {
        constexpr const char* kVoicePath = "Data/SKSE/Plugins/FollowerUnboundTradeVoice.json";

        std::mt19937& Rng()
        {
            static std::mt19937 rng{ std::random_device{}() };
            return rng;
        }

        // Logs when the Say call has run in the VM, so a silent line can be told from a call that never ran.
        class SayDone : public RE::BSScript::IStackCallbackFunctor
        {
        public:
            explicit SayDone(std::string line) : _line(std::move(line)) {}
            void operator()(RE::BSScript::Variable) override { logger::info("trade voice: {} ran", _line); }
            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

        private:
            std::string _line;
        };
    }

    // ------------------------------------------------------------------ config

    void TradeVoice::EnsureLoaded()
    {
        if (_loaded) return;
        _loaded = true;
        _followerGreet.clear();
        _merchantGreet.clear();
        _merchantPays.clear();
        _merchantSells.clear();
        _followerThanks.clear();
        try {
            std::ifstream in(kVoicePath);
            if (!in) {
                logger::info("trade voice: {} not found, trades are silent", kVoicePath);
                return;
            }
            const auto j = nlohmann::json::parse(in, nullptr, true, true);
            _gapSeconds = std::clamp(j.value("gapSeconds", _gapSeconds), 1.0f, 8.0f);
            _tailSeconds = std::clamp(j.value("tailSeconds", _tailSeconds), 0.5f, 8.0f);
            const auto read = [&](const char* key, std::vector<TopicRef>& out) {
                if (!j.contains(key) || !j[key].is_array()) return;
                for (const auto& e : j[key]) {
                    TopicRef ref;
                    ref.plugin = e.value("plugin", std::string("Skyrim.esm"));
                    const auto idText = e.value("id", std::string("0"));
                    ref.id = static_cast<RE::FormID>(std::stoul(idText, nullptr, 16)) & 0x00FFFFFF;   // local id: the load order does not matter
                    if (ref.id != 0) out.push_back(std::move(ref));
                }
            };
            read("followerGreet", _followerGreet);
            read("merchantGreet", _merchantGreet);
            read("merchantPays", _merchantPays);
            read("merchantSells", _merchantSells);
            read("followerThanks", _followerThanks);
            logger::info("trade voice: topics read: followerGreet {}, merchantGreet {}, merchantPays {}, merchantSells {}, followerThanks {}",
                _followerGreet.size(), _merchantGreet.size(), _merchantPays.size(), _merchantSells.size(), _followerThanks.size());
        } catch (const std::exception& e) {
            logger::warn("trade voice: could not read {} ({}), trades are silent", kVoicePath, e.what());
        }
    }

    float TradeVoice::TailSeconds()
    {
        EnsureLoaded();
        return _tailSeconds;
    }

    // ------------------------------------------------------------------ choosing lines

    // Whether the topic holds a line the speaker may say (its conditions pass for the speaker).
    // A topic whose lines are not loaded yet counts as "maybe": Say then loads and decides on its own.
    bool TradeVoice::HasLine(RE::TESTopic* topic, RE::Actor* speaker)
    {
        if (!topic || !speaker) return false;
        if (topic->ownerQuest && !topic->ownerQuest->IsEnabled()) return false;   // its dialogue quest is not running: it cannot speak
        if (!topic->topicInfos) return topic->numTopicInfos > 0;
        for (std::uint32_t i = 0; i < topic->numTopicInfos; ++i) {
            auto* info = topic->topicInfos[i];
            if (info && info->objConditions.IsTrue(speaker, speaker)) return true;
        }
        return false;
    }

    RE::TESTopic* TradeVoice::Pick(std::vector<TopicRef>& refs, RE::Actor* speaker, const char* role)
    {
        if (refs.empty() || !speaker) return nullptr;
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) return nullptr;

        std::vector<RE::TESTopic*> fit;
        std::vector<RE::TESTopic*> fallbackForms;
        for (auto& ref : refs) {
            if (!ref.resolved) {
                ref.resolved = true;
                ref.form = data->LookupForm<RE::TESTopic>(ref.id, ref.plugin);
                if (!ref.form) logger::warn("trade voice: {} {:06X} is not a topic in this load order (role {})", ref.plugin, ref.id, role);
            }
            if (ref.form) {
                fallbackForms.push_back(ref.form);
                if (HasLine(ref.form, speaker)) fit.push_back(ref.form);
            }
        }
        // Eger katı CTDA condition filtresi bazi ozel/modlu takipcilerde eslesmezse,
        // sessiz kalmak yerine en genel uygun topic formunu (ornek DialogueGenericHello/Goodbye) fallback olarak dener
        if (fit.empty() && !fallbackForms.empty()) {
            fit = fallbackForms;
        }
        if (fit.empty()) {
            auto* base = speaker->GetActorBase();
            auto* voice = base ? base->GetVoiceType() : nullptr;
            logger::info("trade voice: {} (voice {:08X}) has no line for {}, stays silent", speaker->GetDisplayFullName(), voice ? voice->GetFormID() : 0, role);
            return nullptr;
        }
        std::uniform_int_distribution<std::size_t> pick(0, fit.size() - 1);
        return fit[pick(Rng())];
    }

    std::vector<TradeVoice::Line> TradeVoice::Plan(RE::Actor* follower, RE::Actor* merchant, bool sold, bool bought)
    {
        EnsureLoaded();
        std::vector<Line> out;
        float             at = 0.0f;
        const auto        add = [&](std::vector<TopicRef>& refs, RE::Actor* who, const char* role) {
            if (auto* topic = Pick(refs, who, role)) {
                Line line;
                line.at = at;
                line.speaker = who->GetHandle();
                line.topic = topic;
                out.push_back(line);
                at += _gapSeconds;
            }
        };
        add(_followerGreet, follower, "followerGreet");
        add(_merchantGreet, merchant, "merchantGreet");
        if (sold) add(_merchantPays, merchant, "merchantPays");
        else if (bought) add(_merchantSells, merchant, "merchantSells");   // one merchant line per visit
        if (sold || bought) add(_followerThanks, follower, "followerThanks");
        return out;
    }

    // ------------------------------------------------------------------ speaking

    void TradeVoice::Say(RE::Actor* speaker, RE::TESTopic* topic)
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (!policy || !speaker || !topic || !speaker->Is3DLoaded() || speaker->IsDead() || speaker->IsInCombat()) return;

        const auto handle = policy->GetHandleForObject(RE::FormType::ActorCharacter, speaker);
        if (handle == policy->EmptyHandle()) {
            logger::info("trade voice: {} has no script handle, says nothing", speaker->GetDisplayFullName());
            return;
        }
        const auto line = std::format("{} saying topic {:08X}", speaker->GetDisplayFullName(), topic->GetFormID());
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new SayDone(line) };
        auto*      args = RE::MakeFunctionArguments(static_cast<RE::TESTopic*>(topic), static_cast<RE::Actor*>(nullptr), false);
        const bool sent = vm->DispatchMethodCall2(handle, "Actor", "Say", args, callback);
        logger::info("trade voice: {} ({})", line, sent ? "sent" : "the call failed");
    }

    // A line's subtitle can stay on screen long after the line; taken off when the visit ends.
    void TradeVoice::ClearSubtitles(RE::ActorHandle speaker)
    {
        auto* manager = RE::SubtitleManager::GetSingleton();
        auto  who = speaker.get();
        if (!manager || !who) return;
        RE::BSSpinLockGuard guard(manager->lock);
        for (auto it = manager->subtitles.begin(); it != manager->subtitles.end();) {
            if (it->speaker.get().get() == who.get()) {
                it = manager->subtitles.erase(it);
            } else {
                ++it;
            }
        }
    }
}
