#pragma once
#include "PCH.h"

namespace FollowerUnbound
{
    class MenuInterface
    {
    public:
        static MenuInterface* GetSingleton()
        {
            static MenuInterface instance;
            return &instance;
        }

        void RegisterMenu();
        static void __stdcall RenderFollowerMenu();
        static void __stdcall RenderWeaponsMenu();
        static void __stdcall RenderApparelMenu();
        static void __stdcall RenderPurchaseMenu();
        static void __stdcall RenderLootMenu();
        static void __stdcall RenderSellMenu();
        static void __stdcall RenderSettingsMenu();
        static void __stdcall RenderBackgroundTick();

    private:
        MenuInterface() = default;
        void* _hudElement{ nullptr };
    };
}
