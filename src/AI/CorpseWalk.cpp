#include "AI/CorpseWalk.h"
#include "AI/AnimationManager.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace FollowerUnbound
{
    namespace
    {
        using Clock = std::chrono::steady_clock;
        constexpr const char* kConfigPath = "Data/SKSE/Plugins/FollowerUnboundLootWalk.json";

        float Secs(Clock::time_point from, Clock::time_point to)
        {
            return std::chrono::duration<float>(to - from).count();
        }

        std::string Name(RE::TESObjectREFR* a)
        {
            return a ? std::string(a->GetDisplayFullName()) : std::string("?");
        }

        // Actor.KeepOffsetFromActor / ClearKeepOffsetFromActor through the VM (game thread).
        bool CallOnActor(RE::Actor* actor, const char* function, RE::BSScript::IFunctionArguments* args)
        {
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
            if (!policy || !actor) {
                delete args;
                return false;
            }
            const auto handle = policy->GetHandleForObject(RE::FormType::ActorCharacter, actor);
            if (handle == policy->EmptyHandle()) {
                delete args;
                return false;
            }
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            return vm->DispatchMethodCall2(handle, "Actor", function, args, callback);
        }

        // Distance from the follower to the body's edge.
        float ReachDistance(RE::Actor* follower, RE::Actor* body)
        {
            if (!follower || !body) return 9999.0f;
            RE::NiPoint3 center = body->GetPosition();
            float radius = 0.0f;
            if (auto* root = body->Get3D()) {
                // If worldBound center is valid and not at origin (0,0,0)
                if (root->worldBound.center.Length() > 1.0f) {
                    center = root->worldBound.center;
                }
                radius = std::clamp(root->worldBound.radius * 0.4f, 0.0f, 60.0f);
            }
            return (std::max)(0.0f, follower->GetPosition().GetDistance(center) - radius);
        }

        void FaceTowards(RE::Actor* actor, RE::TESObjectREFR* target)
        {
            if (!actor || !target) return;
            const auto from = actor->GetPosition();
            const auto to = target->GetPosition();
            actor->data.angle.z = std::atan2(to.x - from.x, to.y - from.y);
        }
    }

    // ------------------------------------------------------------------ settings

    CorpseWalk::CorpseWalk()
    {
        LoadConfig();
    }

    void CorpseWalk::LoadConfig()
    {
        try {
            std::ifstream in(kConfigPath);
            if (!in) return;   // no file yet: defaults
            const auto j = nlohmann::json::parse(in, nullptr, true, true);
            auto&      c = _config;
            c.enabled = j.value("enabled", c.enabled);
            c.delaySeconds = std::clamp(j.value("delaySeconds", c.delaySeconds), 2.0f, 30.0f);
            c.maxStartDistance = std::clamp(j.value("maxStartDistance", c.maxStartDistance), 500.0f, 6000.0f);
            c.walkLimitSeconds = std::clamp(j.value("walkLimitSeconds", c.walkLimitSeconds), 3.0f, 20.0f);
            c.arrivedReach = std::clamp(j.value("arrivedReach", c.arrivedReach), 40.0f, 200.0f);
            logger::info("CorpseWalk: settings loaded from {}", kConfigPath);
        } catch (const std::exception& e) {
            logger::warn("CorpseWalk: could not read {} ({}), defaults are used", kConfigPath, e.what());
        }
    }

    void CorpseWalk::SaveConfig() const
    {
        try {
            const auto&    c = _config;
            nlohmann::json j = {
                { "enabled", c.enabled },
                { "delaySeconds", c.delaySeconds },
                { "maxStartDistance", c.maxStartDistance },
                { "walkLimitSeconds", c.walkLimitSeconds },
                { "arrivedReach", c.arrivedReach }
            };
            std::filesystem::create_directories(std::filesystem::path(kConfigPath).parent_path());
            std::ofstream out(kConfigPath, std::ios::trunc);
            out << j.dump(2);
        } catch (const std::exception& e) {
            logger::warn("CorpseWalk: could not write {}: {}", kConfigPath, e.what());
        }
    }

    // ------------------------------------------------------------------ gate

    bool CorpseWalk::WantsWalk(RE::TESObjectREFR* source) const
    {
        if (!_config.enabled || !source) return false;
        auto* body = source->As<RE::Actor>();
        return body && body->IsDead();
    }

    bool CorpseWalk::CanWalk(RE::Actor* follower, RE::Actor* body) const
    {
        if (!follower->Is3DLoaded() || follower->IsDead() || follower->IsInCombat()) return false;
        if (AnimationManager::IsRidingWithPlayer(follower)) return false;
        if (!body->Get3D() || follower->GetParentCell() != body->GetParentCell()) return false;
        return follower->GetPosition().GetDistance(body->GetPosition()) <= _config.maxStartDistance;
    }

    CorpseWalk::Gate CorpseWalk::Check(RE::Actor* follower, RE::TESObjectREFR* source)
    {
        if (!_config.enabled || !follower || !WantsWalk(source)) return Gate::Go;
        auto* body = source->As<RE::Actor>();
        const auto id = follower->GetFormID();
        const auto now = Clock::now();

        auto it = _walks.find(id);
        if (it != _walks.end() && it->second.body != body->GetFormID()) {   // on to another body
            if (it->second.state == State::Walking) Stop(follower);
            _walks.erase(it);
            it = _walks.end();
        }

        if (it == _walks.end()) {
            Walk walk;
            walk.body = body->GetFormID();
            walk.started = now;
            // not walkable / already directly next to body: loot goes ahead at once
            const float currentDist = ReachDistance(follower, body);
            if (!CanWalk(follower, body) || currentDist <= 60.0f) {
                walk.state = State::Done;
                walk.doneAt = now;
                _walks[id] = walk;
                return Gate::Go;
            }
            if (!StartWalk(follower, body)) {
                walk.state = State::Done;
                walk.doneAt = now;
                _walks[id] = walk;
                return Gate::Go;
            }
            _walks[id] = walk;
            return Gate::Wait;
        }

        auto& walk = it->second;
        if (walk.state == State::Done) return Gate::Go;
        return Evaluate(follower, body, walk) ? Gate::Go : Gate::Wait;
    }

    // ------------------------------------------------------------------ the walk

    // Sends the follower to a spot at the body as it lies, on the follower's side, fanned out a little when several
    // followers walk to the same body (an offset from the body in the body's own frame: x right, y ahead).
    bool CorpseWalk::StartWalk(RE::Actor* follower, RE::Actor* body)
    {
        auto* root = body->Get3D();
        if (!root) return false;

        std::size_t walking = 0;
        for (const auto& [_, w] : _walks) {
            if (w.state == State::Walking && w.body == body->GetFormID()) ++walking;
        }
        const float spread = (static_cast<int>(walking % 3) - 1) * 0.7f;   // -40, 0, +40 degrees

        const auto&  bound = root->worldBound;
        RE::NiPoint3 point = bound.center;
        RE::NiPoint3 toward = follower->GetPosition() - bound.center;
        toward.z = 0.0f;
        if (const float length = toward.Length(); length > 1.0f) {
            const float c = std::cos(spread), s = std::sin(spread);
            const RE::NiPoint3 fanned{ toward.x * c - toward.y * s, toward.x * s + toward.y * c, 0.0f };
            point += fanned * ((std::min)(bound.radius * 0.6f, 70.0f) / length);
        }
        const auto  d = point - body->GetPosition();
        const float heading = body->GetAngleZ();
        // Skyrim standard transform to local actor frame (X right, Y forward):
        const float x = d.x * std::cos(heading) - d.y * std::sin(heading);
        const float y = d.x * std::sin(heading) + d.y * std::cos(heading);

        // Turn follower towards corpse when walking so they move forward and face it
        FaceTowards(follower, body);

        // Calculate relative facing angle towards the corpse (in degrees)
        float targetAngleZ = 0.0f;
        const float distToPoint = std::sqrt(x * x + y * y);
        if (distToPoint > 1.0f) {
            // Local vector from offset spot back to body is (-x, -y)
            const float rad = std::atan2(-x, -y);
            targetAngleZ = rad * (180.0f / 3.14159265f);
        }

        auto* args = RE::MakeFunctionArguments(
            static_cast<RE::Actor*>(body),
            static_cast<float>(x),
            static_cast<float>(y),
            0.0f,
            0.0f,
            0.0f,
            static_cast<float>(targetAngleZ),
            400.0f,
            20.0f
        );
        if (!CallOnActor(follower, "KeepOffsetFromActor", args)) return false;
        logger::info("CorpseWalk: {} walks up to {} ({:.0f} from its body)", Name(follower), Name(body), ReachDistance(follower, body));
        return true;
    }

    bool CorpseWalk::Evaluate(RE::Actor* follower, RE::Actor* body, Walk& walk)
    {
        const auto now = Clock::now();
        const bool gone = !follower || !body || follower->IsDead() || follower->IsInCombat() || !follower->Is3DLoaded() || !body->Get3D();
        const float dist = (follower && body) ? ReachDistance(follower, body) : 9999.0f;
        const bool there = !gone && dist <= _config.arrivedReach;
        const bool tooSlow = Secs(walk.started, now) > _config.walkLimitSeconds;
        if (!gone && !there && !tooSlow) return false;

        walk.state = State::Done;
        walk.doneAt = now;
        if (follower) {
            Stop(follower);
            if (there) {
                FaceTowards(follower, body);
            }
            logger::info("CorpseWalk: {} at {}: {} ({:.0f} from its body)", Name(follower), Name(body),
                there ? "there" : (gone ? "interrupted, remote loot" : "too slow, takes it from where it stands"), body ? ReachDistance(follower, body) : 0.0f);
        }
        return true;
    }

    void CorpseWalk::Stop(RE::Actor* follower)
    {
        CallOnActor(follower, "ClearKeepOffsetFromActor", RE::MakeFunctionArguments());
    }

    void CorpseWalk::Update()
    {
        const auto now = Clock::now();
        for (auto it = _walks.begin(); it != _walks.end();) {
            auto* follower = RE::TESForm::LookupByID<RE::Actor>(it->first);
            auto* body = RE::TESForm::LookupByID<RE::Actor>(it->second.body);
            if (it->second.state == State::Walking) {
                if (!follower) {
                    it = _walks.erase(it);
                    continue;
                }
                Evaluate(follower, body, it->second);   // a walk nobody asks about any more still ends on time
            } else if (Secs(it->second.doneAt, now) > 60.0f) {
                it = _walks.erase(it);
                continue;
            }
            ++it;
        }
    }

    void CorpseWalk::Reset()
    {
        for (auto& [id, walk] : _walks) {
            if (walk.state == State::Walking) {
                if (auto* follower = RE::TESForm::LookupByID<RE::Actor>(id)) Stop(follower);
            }
        }
        _walks.clear();
    }
}
