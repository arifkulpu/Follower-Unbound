#pragma once
#include "PCH.h"
#include <nlohmann/json.hpp>

namespace PRISMA_UI_API { class IVPrismaUI1; }
namespace SKSEMenuFramework::Model { class InputEvent; }

namespace FollowerUnbound
{
    class PrismaUIManager
    {
    public:
        static PrismaUIManager* GetSingleton()
        {
            static PrismaUIManager instance;
            return &instance;
        }

        bool Initialize();
        bool IsAvailable() const noexcept { return _api && _view != 0; }
        bool ShowNotification(std::string_view message);
        void ClearNotifications();
        void Open();
        void SendState();
        void ShowEquipPrompt(RE::FormID followerID, RE::FormID itemID, RE::FormID oldItemID,
            std::string_view followerName, std::string_view itemName, std::string_view slotLabel,
            std::string_view reason, std::string_view currentStats, std::string_view candidateStats);
        void ShowEquipmentPurchasePrompt(RE::FormID followerID, RE::FormID itemID, RE::FormID oldItemID,
            std::string_view followerName, std::string_view itemName, std::string_view slotLabel,
            std::string_view vendorName, uint32_t price, uint32_t spendableGold,
            std::string_view currentStats, std::string_view candidateStats);

    private:
        PrismaUIManager() = default;
        static bool __stdcall OnFrameworkInput(RE::InputEvent* event);
        static void OnAction(const char* data);
        static void OnClose(const char* data);
        void ShowView();
        void BeginFrameworkClose();
        static void WaitForFrameworkClose(uint32_t attemptsRemaining);
        void ApplyAction(const nlohmann::json& action);

        PRISMA_UI_API::IVPrismaUI1* _api{ nullptr };
        uint64_t _view{ 0 };
        bool _viewReady{ false };
        uint64_t _notificationView{ 0 };
        bool _notificationViewReady{ false };
        SKSEMenuFramework::Model::InputEvent* _hotkeyListener{ nullptr };
    };
}
