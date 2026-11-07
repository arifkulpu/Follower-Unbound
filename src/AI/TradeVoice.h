#pragma once
#include "PCH.h"
#include <vector>

namespace FollowerUnbound
{
    // The game's own voiced lines for a trade, with no audio shipped: the vanilla topics named in
    // Data/SKSE/Plugins/FollowerUnboundTradeVoice.json are spoken with Actor.Say. For each role the first topic that
    // has a line the speaker's voice type can say is used; with none, that speaker stays silent.
    //
    // Roles, in the order of a visit:  followerGreet -> merchantGreet -> merchantPays (the follower sold) or
    //                                  merchantSells (it only bought) -> followerThanks
    //
    // Use topics of plain barks/replies. Do NOT use service topics whose lines open menus (e.g. OfferServicesTopic):
    // their result scripts may run. Tools/find_vanilla_topics.py marks topics that carry scripts.
    class TradeVoice
    {
    public:
        static TradeVoice* GetSingleton()
        {
            static TradeVoice instance;
            return &instance;
        }

        struct Line
        {
            float          at{ 0.0f };   // seconds after the trade, when it is spoken
            RE::ActorHandle speaker;
            RE::TESTopic*  topic{ nullptr };
            bool           done{ false };
        };

        // The lines of one visit (empty: nothing to say / nothing configured).
        std::vector<Line> Plan(RE::Actor* follower, RE::Actor* merchant, bool sold, bool bought);

        float TailSeconds();                 // time to let the last line finish
        void  Reload() { _loaded = false; }  // re-read the json on next use

        // Actor.Say(topic) through the VM (game thread).
        static void Say(RE::Actor* speaker, RE::TESTopic* topic);
        // The speaker's subtitles taken off the screen.
        static void ClearSubtitles(RE::ActorHandle speaker);

    private:
        TradeVoice() = default;

        struct TopicRef
        {
            std::string   plugin;
            RE::FormID    id{ 0 };
            RE::TESTopic* form{ nullptr };
            bool          resolved{ false };
        };

        void          EnsureLoaded();
        RE::TESTopic* Pick(std::vector<TopicRef>& refs, RE::Actor* speaker, const char* role);
        static bool   HasLine(RE::TESTopic* topic, RE::Actor* speaker);

        bool  _loaded{ false };
        float _gapSeconds{ 2.6f };
        float _tailSeconds{ 2.4f };
        std::vector<TopicRef> _followerGreet, _merchantGreet, _merchantPays, _merchantSells, _followerThanks;
    };
}
