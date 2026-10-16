#pragma once
#include "PCH.h"

namespace FollowerUnbound
{
    // The New Gentleman uses this internal armor record as a genital-coverage
    // blocker. It is a hidden implementation item, not follower apparel.
    inline bool IsTngHiddenBodyItem(const RE::TESForm* form)
    {
        if (!form || form->GetLocalFormID() != 0x0AFF) return false;
        auto* file = form->GetFile(0);
        if (!file) return false;

        std::string filename(file->GetFilename());
        std::transform(filename.begin(), filename.end(), filename.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return filename == "thenewgentleman.esp";
    }

    inline RE::TESObjectARMO* GetTngHiddenBodyItem()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        return dataHandler ? dataHandler->LookupForm<RE::TESObjectARMO>(0x0AFF, "TheNewGentleman.esp") : nullptr;
    }
}
