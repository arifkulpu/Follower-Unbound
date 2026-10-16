#pragma once
#include "PCH.h"

namespace FollowerUnbound
{
    class Serialization
    {
    public:
        static constexpr uint32_t SerializationVersion = 17;
        static constexpr uint32_t RecordTypeFollowerProfile = 'FOLL';
        static constexpr uint32_t RecordTypeGlobalConfig    = 'CONF';

        static void SaveCallback(SKSE::SerializationInterface* a_intfc);
        static void LoadCallback(SKSE::SerializationInterface* a_intfc);
        static void ApplyPendingProfiles();
        static void RevertCallback(SKSE::SerializationInterface* a_intfc);
    };
}
