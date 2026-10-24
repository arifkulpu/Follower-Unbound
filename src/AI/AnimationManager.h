#pragma once
#include "PCH.h"

namespace FollowerUnbound
{
    class AnimationManager
    {
    public:
        static AnimationManager* GetSingleton()
        {
            static AnimationManager instance;
            return &instance;
        }

        // Yeme animasyonu oynatur.
        // Eating Animations and Sounds SE kuruluysa behavior graph override eder.
        void PlayEatAnimation(RE::Actor* actor);

        // Icme animasyonu oynatur (su/icecek).
        // SunHelm matara/sise (EAS - SunHelm patch 177926) veya Potion Drinking Animation SE override eder.
        void PlayDrinkAnimation(RE::Actor* actor, RE::TESBoundObject* item = nullptr);

        // Potion icme animasyonu (savaS iksiri).
        void PlayPotionAnimation(RE::Actor* actor);

        // Corpses search idle from Immersive Interactions; uses generic pickup
        // animation events as a graceful fallback when that idle is unavailable.
        bool PlayLootAnimation(RE::Actor* actor, RE::TESObjectREFR* target = nullptr);

        // Uyku animasyonu / durumu sifirla.
        void PlaySleepRestoreAnimation(RE::Actor* actor);

        // Kitap ve not okuma animasyonları (Immersive Interactions & Vanilla uyumlu)
        bool PlayReadAnimation(RE::Actor* actor, bool isSitting = false);
        void StopReadAnimation(RE::Actor* actor);

        // Silahi kinina alir ve animasyon grafini idle'a dondurur.
        // TryAutoEquip sonrasi cagrılarak "silah elde T-pose" sorununu onler.
        // Savaştaki aktörlerde çağrılmaz.
        void SheatheWeapon(RE::Actor* actor);

        // Zehir uygulama animasyonu:
        // Animated Poisons Redone (APR_PoisonStart / APR_PoisonAnim),
        // OAR / Immersive Interactions / Vanilla fallbackleri oynatir.
        void PlayPoisonWeaponAnimation(RE::Actor* follower, bool isRanged = false);
        void StopPoisonWeaponAnimation(RE::Actor* follower);

        // Animasyon grafine event gonder — temel yardimci fonksiyon
        static bool SendAnimEvent(RE::Actor* actor, const char* eventName);

    private:
        AnimationManager() = default;

        // Son kın alma çağrı zamanı (saniyede onlarca kez spam olmasını engeller)
        std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastSheatheTime;
        std::mutex _sheatheMutex;


        // Zehir şişesi mesh takibi: Actor FormID -> Eklenen şişe node'u
        std::unordered_map<RE::FormID, RE::NiPointer<RE::NiAVObject>> _attachedPoisonBottles;
        std::mutex _bottleMutex;
    };
}
