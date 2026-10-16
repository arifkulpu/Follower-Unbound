#pragma once

#include <cstdint>

namespace LifeAgain::API {

    inline constexpr std::uint32_t kVersion = 2;
    inline constexpr std::uint32_t kFollowerInjuryStateMessage = 'IFAL';
    inline constexpr std::uint32_t kRequestFollowerInjurySnapshotMessage = 'QFAL';

    enum class InjuryState : std::uint8_t {
        Cleared = 0,
        Light = 1,
        Heavy = 2
    };

    enum class InjuryType : std::uint8_t {
        Abrasion = 0,
        Cut = 1,
        MinorBurn = 2,
        Bruise = 3,
        DeepCut = 4,
        Puncture = 5,
        Crush = 6,
        SevereBurn = 7,
        None = 255
    };

    // Sent through SKSE MessagingInterface with kFollowerInjuryStateMessage.
    // A Cleared state removes the follower's injury visuals in listening add-ons.
    // formID == 0 with Cleared resets an add-on's complete cached state.
    struct FollowerInjuryStateEvent {
        std::uint32_t apiVersion = kVersion;
        std::uint32_t formID = 0;
        InjuryState state = InjuryState::Cleared;
        InjuryType injuryType = InjuryType::None;
        std::uint16_t injuryCount = 0;
    };

}  // namespace LifeAgain::API
