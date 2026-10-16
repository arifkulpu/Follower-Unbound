#include "PCH.h"
#include "Core/FollowerManager.h"
#include "Core/Settings.h"
#include "Core/Serialization.h"
#include "Core/EventManager.h"
#include "AI/ConsumableAI.h"
#include "Economy/ShoppingAI.h"
#include "UI/MenuInterface.h"
#include "Core/ClassInterface.h"
#include "KingCore/KingCoreAPI.h"

using namespace FollowerUnbound;

namespace
{
    void InitializeLog()
    {
#ifndef NDEBUG
        auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
        auto path = logger::log_directory();
        if (!path) {
            return;
        }

        *path /= "FollowerUnbound.log"sv;
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

        auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
        log->set_level(spdlog::level::trace);
#else
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
#endif

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v"s);
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            {
                logger::info("Data loaded. Loading settings, registering event sinks and menu...");
                Settings::GetSingleton()->Load();
                EventManager::GetSingleton()->Register();
                FollowerManager::GetSingleton()->Initialize();
                MenuInterface::GetSingleton()->RegisterMenu();

                // Also query King Core API at kDataLoaded in case it registered after kPostLoad
                auto* messaging = SKSE::GetMessagingInterface();
                if (messaging) {
                    messaging->Dispatch(KingCore::MESSAGE_TYPE_REQUEST_API, nullptr, 0, "KingCore");
                    messaging->Dispatch(KingCore::MESSAGE_TYPE_REQUEST_API, nullptr, 0, nullptr);
                    messaging->Dispatch(LifeAgain::API::kRequestFollowerInjurySnapshotMessage, nullptr, 0, "LifeAgain");
                }
            }
            break;
        case KingCore::MESSAGE_TYPE_REGISTER_TREATMENT:
            {
                if (a_msg->data && a_msg->dataLen >= sizeof(KingCore::TreatmentRegistration)) {
                    const auto* reg = static_cast<const KingCore::TreatmentRegistration*>(a_msg->data);
                    const uint32_t providerMajor = (reg->apiVersion >> 16) & 0xFFFF;
                    if (providerMajor == KingCore::API_VERSION_MAJOR && reg->itemCount <= KingCore::TREATMENT_MAX_ITEMS) {
                        ClassInterface::GetSingleton()->SetTreatmentItems(reg);
                    } else {
                        logger::warn("Follower Unbound: Treatment registration rejected (major={}, itemCount={}).", providerMajor, reg->itemCount);
                    }
                }
            }
            break;
        case LifeAgain::API::kFollowerInjuryStateMessage:
            {
                if (a_msg->data && a_msg->dataLen >= sizeof(LifeAgain::API::FollowerInjuryStateEvent)) {
                    const auto* event = static_cast<const LifeAgain::API::FollowerInjuryStateEvent*>(a_msg->data);
                    ClassInterface::GetSingleton()->OnLifeAgainInjuryState(*event);
                }
            }
            break;
        case SKSE::MessagingInterface::kPostLoad:
            {
                // Request API from King Core if loaded
                auto* messaging = SKSE::GetMessagingInterface();
                if (messaging) {
                    messaging->Dispatch(KingCore::MESSAGE_TYPE_REQUEST_API, nullptr, 0, "KingCore");
                    messaging->Dispatch(KingCore::MESSAGE_TYPE_REQUEST_API, nullptr, 0, nullptr);
                }
            }
            break;
        case KingCore::MESSAGE_TYPE_PROVIDE_API:
            {
                if (a_msg->data && a_msg->dataLen >= sizeof(KingCore::KingCoreInterfaceExchange)) {
                    auto* exchange = static_cast<KingCore::KingCoreInterfaceExchange*>(a_msg->data);
                    if (exchange && exchange->classAPI) {
                        uint32_t providerVer   = exchange->classAPI->GetVersion();
                        uint32_t providerMajor = (providerVer >> 16) & 0xFFFF;

                        // Reject ABI-incompatible providers even if King Core is bypassed.
                        // A major version mismatch means vtable layouts may differ → crash.
                        if (providerMajor != KingCore::API_VERSION_MAJOR) {
                            logger::error("Follower Unbound: Version mismatch! Provider major={}, expected={}. Rejecting API.",
                                providerMajor, KingCore::API_VERSION_MAJOR);
                            break;
                        }

                        logger::info("Follower Unbound: Successfully connected to King Core API provider (version 0x{:X})", providerVer);
                        ClassInterface::GetSingleton()->SetClassBoundAPI(exchange->classAPI);
                    }
                }
            }
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            {
                logger::info("Game loaded / started. Performing initial follower scan...");
                Serialization::ApplyPendingProfiles();
                FollowerManager::GetSingleton()->ScanForActiveFollowers();
                // Oyun zamanını başlat — ilk Wait/Sleep'te delta hesaplanabilsin
                FollowerManager::GetSingleton()->InitGameTime();
                // Oyun açılışında ani zehirleme veya sarf tüketimini önle
                ConsumableAI::GetSingleton()->NotifyGameLoaded();
            }
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    InitializeLog();
    logger::info("Follower Unbound (Living Followers) initializing...");

    SKSE::Init(a_skse);

    // Serialization for Co-Save
    auto serialization = SKSE::GetSerializationInterface();
    if (serialization) {
        serialization->SetUniqueID('FLUN');
        serialization->SetSaveCallback(Serialization::SaveCallback);
        serialization->SetLoadCallback(Serialization::LoadCallback);
        serialization->SetRevertCallback(Serialization::RevertCallback);
        logger::info("Registered SKSE Serialization callbacks.");
    }

    // Messaging
    auto messaging = SKSE::GetMessagingInterface();
    if (messaging) {
        // SKSE 2.0+ mesaj sistemi gönderici bazlıdır: RegisterListener(callback)
        // yalnızca "SKSE" kaynaklı yaşam döngüsü mesajlarını alır.
        // Eklentiler arası mesajları almak için her gönderici için ayrı kayıt gerekir.
        if (!messaging->RegisterListener(MessageHandler)) {
            logger::error("Failed to register SKSE messaging listener.");
        }
        // King Core'dan gelen PROVIDE_API yayın/yanıt mesajları:
        if (!messaging->RegisterListener("KingCore", MessageHandler)) {
            logger::warn("KingCore messaging listener registration failed (King Core not loaded yet?).");
        }
        if (!messaging->RegisterListener("LifeAgain", MessageHandler)) {
            logger::warn("LifeAgain messaging listener registration failed (Life Again not loaded yet?).");
        }
        logger::info("Registered SKSE messaging listeners (SKSE + KingCore + LifeAgain).");
    }

    logger::info("Follower Unbound loaded successfully.");
    return true;
}
