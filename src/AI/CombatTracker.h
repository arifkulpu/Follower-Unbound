#pragma once
#include "PCH.h"

namespace FollowerUnbound
{
    // ── CombatTracker ──
    // Savaş participant'larını (hostile combatant) takip eder: kim oyuncuya/takipçiye
    // saldırdı, kim öldü. Amaç: takipçilerin SADECE gerçek düşman cesetlerini
    // yağmalaması — masum şehir NPC'leri, pasif hayvanlar ve dost birimler
    // asla otomatik yağma kuyruğuna girmesin.
    //
    // EventManager tarafından beslenir:
    //   TESCombatEvent (kCombat) → NotifyCombatStart
    //   TESCombatEvent (kNone)   → NotifyCombatEnd
    //   TESDeathEvent            → NotifyDeath (participant ise casualty kaydı)
    // LootManager::QueueCorpse öncesi ShouldAutoLoot() ile sorgulanır.
    class CombatTracker
    {
    public:
        static CombatTracker* GetSingleton()
        {
            static CombatTracker instance;
            return &instance;
        }

        // actor savaşa girdi; hedefi oyuncu veya takipçiyse actor düşman (hostile) sayılır.
        // Hedef oyuncu/takipçi DEĞİLSE ama actor oyuncu/takipçiyse hedef tarafı hostile sayılır.
        bool NotifyCombatStart(RE::Actor* actor, RE::Actor* target);

        // Oyuncu veya takipçi savaştan çıktı — kimse savaşta kalmadıysa
        // geçici participant listesi temizlenir (casualty/ceset listesi korunur).
        void NotifyCombatEnd(RE::Actor* actor);
        void NotifyCombatActivity(RE::Actor* actor, RE::Actor* target = nullptr);

        // Bir aktör öldü. Savaş participant'ı ise casualty listesine eklenir.
        void NotifyDeath(RE::Actor* deadActor);

        // Bu ceset otomatik yağmaya uygun mu?
        // - LootHostilesOnly kapalıysa: herkes uygun.
        // - Açıkken: kayıtlı savaş participant'ı VEYA ölüm anında oyuncuya
        //   hostile olan aktörler uygun. (Agresyon ölümde korunur; event
        //   kaçırılmış olsa bile oyuncuya saldıran düşman yakalanır.)
        bool ShouldAutoLoot(RE::Actor* deadActor) const;
        bool IsLootSafe() const;

    private:
        CombatTracker() = default;

        static bool IsTrackedFriendly(RE::Actor* actor); // oyuncu veya aktif takipçi

        // Savaşta düşman (hostile) olarak görülen aktörler — savaş bitince temizlenir
        std::unordered_set<RE::FormID> _hostileParticipants;
        // Ölen düşmanlar (ceset whitelist) — yağmalanana kadar korunur
        std::unordered_set<RE::FormID> _casualties;
        mutable std::mutex _mutex;
        std::chrono::steady_clock::time_point _lastCombatActivity{};

        static constexpr size_t kMaxCasualties = 1024;
    };
}
