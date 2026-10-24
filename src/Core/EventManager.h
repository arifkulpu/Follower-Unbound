#pragma once
#include "PCH.h"

namespace FollowerUnbound
{
    class EventManager : 
        public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
        public RE::BSTEventSink<RE::TESCombatEvent>,
        public RE::BSTEventSink<RE::TESContainerChangedEvent>,
        public RE::BSTEventSink<RE::TESHitEvent>,
        public RE::BSTEventSink<RE::TESWaitStopEvent>,
        public RE::BSTEventSink<RE::TESSleepStopEvent>,
        public RE::BSTEventSink<RE::TESFastTravelEndEvent>,
        public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>,
        public RE::BSTEventSink<RE::TESDeathEvent>
    {
    public:
        static EventManager* GetSingleton()
        {
            static EventManager instance;
            return &instance;
        }

        void Register();

        // BSTEventSink overrides
        virtual RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;
        virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>* a_eventSource) override;
        virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* a_event, RE::BSTEventSource<RE::TESContainerChangedEvent>* a_eventSource) override;
        virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>* a_eventSource) override;
        virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESWaitStopEvent* a_event, RE::BSTEventSource<RE::TESWaitStopEvent>* a_eventSource) override;
        virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESSleepStopEvent* a_event, RE::BSTEventSource<RE::TESSleepStopEvent>* a_eventSource) override;
        virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESFastTravelEndEvent* a_event, RE::BSTEventSource<RE::TESFastTravelEndEvent>* a_eventSource) override;
        virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>* a_eventSource) override;
        virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>* a_eventSource) override;
        // virtual RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

    private:
        EventManager() = default;
        RE::FormID _lastContainerOpened{ 0 };
    };
}
