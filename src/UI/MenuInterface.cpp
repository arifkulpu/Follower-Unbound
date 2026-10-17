#include "UI/MenuInterface.h"
#include "UI/SKSEMenuFramework.h"
#include "UI/NotificationManager.h"
#include "Core/FollowerManager.h"
#include "Core/TNGCompatibility.h"
#include "Core/ClassInterface.h"
#include "Core/Settings.h"
#include "Economy/EconomyManager.h"
#include "AI/ItemEvaluator.h"
#include "AI/ConsumableAI.h"
#include "AI/LootManager.h"
#include "AI/DroppedItemRecovery.h"
#include "AI/AnimationManager.h"
#include "UI/PrismaUIManager.h"
#include "UI/EquipPrompt.h"

namespace FollowerUnbound
{
    static void __stdcall RenderPrismaLauncher()
    {
        using namespace ImGuiMCP;
        TextColored(ImVec4{ 0.75f, 0.86f, 1.0f, 1.0f }, "FOLLOWER UNBOUND | PRISMA UI");
        Text("Open the modern follower management interface.");
        Spacing();
        if (Button("OPEN FOLLOWER UNBOUND (SHIFT + I)", ImVec2(320.0f, 44.0f))) {
            PrismaUIManager::GetSingleton()->Open();
        }
    }

    // Helper: push ImGui color for needs bar based on percentage (0-100)
    static void PushNeedsColor(float pct)
    {
        ImGuiMCP::ImVec4 color;
        if (pct >= 75.0f) {
            color = ImGuiMCP::ImVec4{ 0.2f, 0.8f, 0.2f, 1.0f };       // Green
        } else if (pct >= 50.0f) {
            color = ImGuiMCP::ImVec4{ 0.9f, 0.9f, 0.0f, 1.0f };       // Yellow
        } else if (pct >= 25.0f) {
            color = ImGuiMCP::ImVec4{ 0.9f, 0.5f, 0.0f, 1.0f };       // Orange
        } else {
            color = ImGuiMCP::ImVec4{ 0.9f, 0.1f, 0.1f, 1.0f };       // Red
        }
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PlotHistogram, color);
    }

    void MenuInterface::RegisterMenu()
    {
        const bool prismaReady = PrismaUIManager::GetSingleton()->Initialize();
        if (!SKSEMenuFramework::IsInstalled()) {
            logger::info("SKSEMenuFramework is not installed; PrismaUI remains available through Shift+I.");
            return;
        }

        SKSEMenuFramework::SetSection("Follower Unbound");
        if (prismaReady) SKSEMenuFramework::AddSectionItem("Open Prisma UI (Shift+I)", RenderPrismaLauncher);
        SKSEMenuFramework::AddSectionItem("Follower Overview", RenderFollowerMenu);
        SKSEMenuFramework::AddSectionItem("Weapons", RenderWeaponsMenu);
        SKSEMenuFramework::AddSectionItem("Apparel", RenderApparelMenu);
        SKSEMenuFramework::AddSectionItem("Follower Purchase Management", RenderPurchaseMenu);
        SKSEMenuFramework::AddSectionItem("Items to be looted", RenderLootMenu);
        SKSEMenuFramework::AddSectionItem("Items to be sold", RenderSellMenu);
        SKSEMenuFramework::AddSectionItem("Settings", RenderSettingsMenu);
        _hudElement = SKSEMenuFramework::AddHudElement(RenderBackgroundTick);
        logger::info("Registered Follower Unbound PrismaUI launcher and legacy SKSEMenuFramework pages.");
    }

    void __stdcall MenuInterface::RenderFollowerMenu()
    {
        using namespace ImGuiMCP;

        auto manager = FollowerManager::GetSingleton();

        // Thread-safe snapshot — mutex kilitli değilken render edilir
        // ScanForActiveFollowers / UpdateGameTimeNeeds her frame çağrılmaz;
        // bunlar zaten event-based (wait/sleep/fast travel/cell load) tetikleniyor.
        auto profiles = manager->GetProfilesSnapshot();

        if (profiles.empty()) {
            Text("No active followers detected.");
            Text("Tip: Ensure your follower is nearby, recruited, or in your party.");
            if (Button("Force Scan Followers Now")) {
                manager->ScanForActiveFollowers();
            }
            return;
        }

        if (Button("Refresh / Scan Followers")) {
            // Sadece buton tıklandığında scan — her frame değil
            manager->ScanForActiveFollowers();
        }

        ImGuiMCP::Separator();

        // Gold item form (Gold001 = 0x0000000F)
        auto goldItem = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);

        auto settings = Settings::GetSingleton();

        for (auto& [formID, profile] : profiles) {
            auto actorForm = RE::TESForm::LookupByID(formID);
            auto actor = actorForm ? actorForm->As<RE::Actor>() : nullptr;
            bool isAnimal = profile.isAnimal || (actor && FollowerManager::IsAnimalActor(actor));

            // Eğer evcil hayvan sistemi kapalıysa hayvanları menüde gösterme
            if (isAnimal && !settings->enablePetSystem) {
                continue;
            }

            // Read real gold from actor inventory
            int32_t realGold = 0;
            if (actor && goldItem) {
                realGold = ItemEvaluator::GetItemCount(actor, goldItem);
            }

            std::string displayClass;
            if (isAnimal) {
                displayClass = "Animal";
            } else if (!profile.classBoundDisplayName.empty()) {
                displayClass = profile.classBoundDisplayName;
            } else {
                displayClass = ClassInterface::GetSingleton()->GetClassDisplayName(actor, profile.role);
            }

            float currentWeight = actor ? actor->GetWeightInContainer() : 0.0f;
            float maxWeight = actor ? actor->GetTotalCarryWeight() : 0.0f;

            std::string header;
            if (isAnimal) {
                header = std::format("{} [Animal] [Carry: {:.0f}/{:.0f}]", profile.name, currentWeight, maxWeight);
            } else if (displayClass == "No Class (Unassigned)") {
                header = std::format("{} [No Class] [Gold: {}] [Carry: {:.0f}/{:.0f}]", profile.name, realGold, currentWeight, maxWeight);
            } else {
                header = std::format("{} [{}] [Gold: {}] [Carry: {:.0f}/{:.0f}]", profile.name, displayClass, realGold, currentWeight, maxWeight);
            }

            if (CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (isAnimal) {
                    TextColored(ImVec4{ 0.3f, 0.9f, 0.7f, 1.0f }, "Type: Pet / Animal Companion");
                    TextDisabled("Animals do not have class perks, barter or equipment. Only needs (hunger, thirst, fatigue) are tracked.");
                } else if (displayClass == "No Class (Unassigned)") {
                    TextColored(ImVec4{ 0.9f, 0.7f, 0.2f, 1.0f }, "Class: Not Assigned (Select a class in ClassBound menu)");
                } else {
                    Text("Class: %s", displayClass.c_str());
                }

                Text("Gold: %d", realGold);
                Text("Inventory Capacity: %.0f / %.0f (Weight)", currentWeight, maxWeight);
                float weightPct = maxWeight > 0.0f ? std::clamp(currentWeight / maxWeight, 0.0f, 1.0f) : 0.0f;
                ProgressBar(weightPct, ImVec2(-1.0f, 12.0f), std::format("{:.0f} / {:.0f}", currentWeight, maxWeight).c_str());

                Separator();

                Text("Needs Status:");
                Spacing();

                // Hunger bar
                Text("Hunger: %.0f%%", profile.needs.hunger);
                PushNeedsColor(profile.needs.hunger);
                ProgressBar(profile.needs.hunger / 100.0f, ImVec2(-1.0f, 14.0f), "");
                ImGuiMCP::PopStyleColor();
                Spacing();

                // Thirst bar
                Text("Thirst: %.0f%%", profile.needs.thirst);
                PushNeedsColor(profile.needs.thirst);
                ProgressBar(profile.needs.thirst / 100.0f, ImVec2(-1.0f, 14.0f), "");
                ImGuiMCP::PopStyleColor();
                Spacing();

                // Fatigue bar
                Text("Fatigue: %.0f%%", profile.needs.fatigue);
                PushNeedsColor(profile.needs.fatigue);
                ProgressBar(profile.needs.fatigue / 100.0f, ImVec2(-1.0f, 14.0f), "");
                ImGuiMCP::PopStyleColor();
                Spacing();

                // Hayvanlar için altın verme gösterilmez
                if (!isAnimal) {
                    Separator();
                    Text("Give Gold:");
                    if (Button(std::format("Give 100 Gold##{:08X}", formID).c_str())) {
                        if (actorForm && actorForm->As<RE::Actor>()) {
                            EconomyManager::GetSingleton()->PlayerGiveGold(actorForm->As<RE::Actor>(), 100);
                        }
                    }
                    SameLine();
                    if (Button(std::format("Give 500 Gold##{:08X}", formID).c_str())) {
                        if (actorForm && actorForm->As<RE::Actor>()) {
                            EconomyManager::GetSingleton()->PlayerGiveGold(actorForm->As<RE::Actor>(), 500);
                        }
                    }
                }
            }
        }
    }

    void __stdcall MenuInterface::RenderPurchaseMenu()
    {
        using namespace ImGuiMCP;

        auto manager = FollowerManager::GetSingleton();
        auto profiles = manager->GetProfilesSnapshot();

        std::vector<std::pair<RE::FormID, std::string>> followerList;
        for (const auto& [fID, prof] : profiles) {
            if (!prof.isAnimal) {
                followerList.emplace_back(fID, prof.name);
            }
        }

        if (followerList.empty()) {
            TextColored(ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f }, "No humanoid followers detected.");
            return;
        }

        static RE::FormID s_selectedPurchaseFollower{ 0 };
        bool followerValid = false;
        for (const auto& [fID, name] : followerList) {
            if (fID == s_selectedPurchaseFollower) {
                followerValid = true;
                break;
            }
        }
        if (!followerValid) {
            s_selectedPurchaseFollower = followerList.front().first;
        }

        TextColored(ImVec4{ 0.4f, 0.85f, 1.0f, 1.0f }, "Follower Purchase Management (Alisveris & Stok Yonetimi)");
        TextDisabled("Takipciler sehirlerdeki veya hanlardaki tuccarlardan kendi parasiyla bu limitlere kadar alisveris yapar.");
        Separator();

        Text("Select Follower:");
        for (size_t i = 0; i < followerList.size(); ++i) {
            auto [fID, name] = followerList[i];
            bool isSelected = (fID == s_selectedPurchaseFollower);

            if (isSelected) {
                TextColored(ImVec4{ 0.3f, 1.0f, 0.4f, 1.0f }, "[ %s ]", name.c_str());
            } else {
                if (Button(std::format("{}##PurchSel_{:08X}", name, fID).c_str())) {
                    s_selectedPurchaseFollower = fID;
                }
            }
            if (i + 1 < followerList.size()) SameLine();
        }

        Separator();

        auto profile = manager->GetProfile(s_selectedPurchaseFollower);
        auto* actorForm = RE::TESForm::LookupByID(s_selectedPurchaseFollower);
        auto* actor = actorForm ? actorForm->As<RE::Actor>() : nullptr;

        if (!profile || !actor) {
            TextColored(ImVec4{ 1.0f, 0.5f, 0.2f, 1.0f }, "Follower not loaded in 3D.");
            return;
        }

        auto& pref = profile->preferences;
        auto formID = s_selectedPurchaseFollower;

        auto goldItem = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        int32_t realGold = actor && goldItem ? ItemEvaluator::GetItemCount(actor, goldItem) : 0;
        TextColored(ImVec4{ 1.0f, 0.85f, 0.3f, 1.0f }, "Follower Gold: %d", realGold);
        Spacing();

        // ── 1. EXTRA UTILITY ITEMS (Soul Gems, Meşale, Fener Yağı, Sabun) ──
        TextColored(ImVec4{ 0.9f, 0.6f, 1.0f, 1.0f }, "Extra Utility Items (Aralik: 1 - 20 | Default: 5)");
        TextDisabled("Ruh taslari genel tuccarlardan ve saray buyuculerinden satin alinir.");
        Spacing();

        auto soulGemPreference = [&](const char* label, bool& enabled, uint32_t& target, const char* id) {
            bool value = enabled;
            int limit = static_cast<int>(target);
            if (Checkbox(std::format("{}##{}_{}", label, id, formID).c_str(), &value)) enabled = value;
            SameLine();
            if (SliderInt(std::format("Limit 1-20##{}_{}", id, formID).c_str(), &limit, 1, 20)) {
                target = static_cast<uint32_t>(std::clamp(limit, 1, 20));
            }
        };
        soulGemPreference("Petty Soul Gems", pref.buyPettySoulGems, pref.maxPettySoulGems, "PettySoul");
        soulGemPreference("Lesser Soul Gems", pref.buyLesserSoulGems, pref.maxLesserSoulGems, "LesserSoul");
        soulGemPreference("Common Soul Gems", pref.buyCommonSoulGems, pref.maxCommonSoulGems, "CommonSoul");
        soulGemPreference("Greater Soul Gems", pref.buyGreaterSoulGems, pref.maxGreaterSoulGems, "GreaterSoul");
        soulGemPreference("Grand Soul Gems", pref.buyGrandSoulGems, pref.maxGrandSoulGems, "GrandSoul");
        soulGemPreference("Black Soul Gems", pref.buyBlackSoulGems, pref.maxBlackSoulGems, "BlackSoul");

        // Torches (Meşale)
        bool buyTorch = pref.buyTorches;
        int maxTorch = static_cast<int>(pref.maxTorches);
        if (Checkbox(std::format("Buy Torches (Mesale)##{:08X}", formID).c_str(), &buyTorch)) {
            pref.buyTorches = buyTorch;
        }
        SameLine();
        if (SliderInt(std::format("Limit##TORCH_{:08X}", formID).c_str(), &maxTorch, 1, 20)) {
            pref.maxTorches = static_cast<uint32_t>(std::clamp(maxTorch, 1, 20));
        }
        if (IsItemHovered()) SetTooltip("Karanlik zindanlar ve geceleri aydinlatma icin tasinan mesaleler.");

        // Lantern Oil (Fener Yağı)
        bool buyOil = pref.buyLanternOil;
        int maxOil = static_cast<int>(pref.maxLanternOil);
        if (Checkbox(std::format("Buy Lantern Oil (Fener Yagi)##{:08X}", formID).c_str(), &buyOil)) {
            pref.buyLanternOil = buyOil;
        }
        SameLine();
        if (SliderInt(std::format("Limit##OIL_{:08X}", formID).c_str(), &maxOil, 1, 20)) {
            pref.maxLanternOil = static_cast<uint32_t>(std::clamp(maxOil, 1, 20));
        }
        if (IsItemHovered()) SetTooltip("Fener modlari icin (Wearable Lanterns / Quick Light vb.) fener yagi.");

        // Soap (Sabun)
        bool buySoap = pref.buySoap;
        int maxSoap = static_cast<int>(pref.maxSoap);
        if (Checkbox(std::format("Buy Soap (Sabun)##{:08X}", formID).c_str(), &buySoap)) {
            pref.buySoap = buySoap;
        }
        SameLine();
        if (SliderInt(std::format("Limit##SOAP_{:08X}", formID).c_str(), &maxSoap, 1, 20)) {
            pref.maxSoap = static_cast<uint32_t>(std::clamp(maxSoap, 1, 20));
        }
        if (IsItemHovered()) SetTooltip("Hijyen ve banyo modlari icin (Dirt and Blood / Bathing in Skyrim vb.) sabun.");

        Separator();

        // ── 2. POTIONS ──
        TextColored(ImVec4{ 0.3f, 0.85f, 1.0f, 1.0f }, "Potions (Simyaci / Apothecary - Limit: 0-25)");

        bool buyHp = pref.buyHealthPotions;
        int maxHp = static_cast<int>(pref.maxHealthPotions);
        if (Checkbox(std::format("Buy Health Potions##{:08X}", formID).c_str(), &buyHp)) pref.buyHealthPotions = buyHp;
        SameLine();
        if (SliderInt(std::format("Limit##HP_{:08X}", formID).c_str(), &maxHp, 0, 25)) pref.maxHealthPotions = static_cast<uint32_t>(std::clamp(maxHp, 0, 25));

        bool buyMp = pref.buyMagickaPotions;
        int maxMp = static_cast<int>(pref.maxMagickaPotions);
        if (Checkbox(std::format("Buy Magicka Potions##{:08X}", formID).c_str(), &buyMp)) pref.buyMagickaPotions = buyMp;
        SameLine();
        if (SliderInt(std::format("Limit##MP_{:08X}", formID).c_str(), &maxMp, 0, 25)) pref.maxMagickaPotions = static_cast<uint32_t>(std::clamp(maxMp, 0, 25));

        bool buySp = pref.buyStaminaPotions;
        int maxSp = static_cast<int>(pref.maxStaminaPotions);
        if (Checkbox(std::format("Buy Stamina Potions##{:08X}", formID).c_str(), &buySp)) pref.buyStaminaPotions = buySp;
        SameLine();
        if (SliderInt(std::format("Limit##SP_{:08X}", formID).c_str(), &maxSp, 0, 25)) pref.maxStaminaPotions = static_cast<uint32_t>(std::clamp(maxSp, 0, 25));

        int maxPoisons = static_cast<int>(pref.maxPoisons);
        if (SliderInt(std::format("Max Poisons to Carry##POI_{:08X}", formID).c_str(), &maxPoisons, 0, 25)) {
            pref.maxPoisons = static_cast<uint32_t>(std::clamp(maxPoisons, 0, 25));
        }

        Separator();

        // ── 3. FOOD & DRINKS ──
        TextColored(ImVec4{ 1.0f, 0.8f, 0.4f, 1.0f }, "Food & Drinks (Hanlar / Inns / Taverns - Limit: 0-25)");

        bool buyFL = pref.buyFoodLarge;
        int maxFL = static_cast<int>(pref.maxFoodLarge);
        if (Checkbox(std::format("Large Food (Meat/Fish)##{:08X}", formID).c_str(), &buyFL)) pref.buyFoodLarge = buyFL;
        SameLine();
        if (SliderInt(std::format("Limit##FL_{:08X}", formID).c_str(), &maxFL, 0, 25)) pref.maxFoodLarge = static_cast<uint32_t>(std::clamp(maxFL, 0, 25));

        bool buyFM = pref.buyFoodMedium;
        int maxFM = static_cast<int>(pref.maxFoodMedium);
        if (Checkbox(std::format("Medium Food (Bread/Stew)##{:08X}", formID).c_str(), &buyFM)) pref.buyFoodMedium = buyFM;
        SameLine();
        if (SliderInt(std::format("Limit##FM_{:08X}", formID).c_str(), &maxFM, 0, 25)) pref.maxFoodMedium = static_cast<uint32_t>(std::clamp(maxFM, 0, 25));

        bool buyFS = pref.buyFoodSmall;
        int maxFS = static_cast<int>(pref.maxFoodSmall);
        if (Checkbox(std::format("Small Food (Fruits/Vegetables)##{:08X}", formID).c_str(), &buyFS)) pref.buyFoodSmall = buyFS;
        SameLine();
        if (SliderInt(std::format("Limit##FS_{:08X}", formID).c_str(), &maxFS, 0, 25)) pref.maxFoodSmall = static_cast<uint32_t>(std::clamp(maxFS, 0, 25));

        bool buyDN = pref.buyDrinkNormal;
        int maxDN = static_cast<int>(pref.maxDrinkNormal);
        if (Checkbox(std::format("Water / Milk##{:08X}", formID).c_str(), &buyDN)) pref.buyDrinkNormal = buyDN;
        SameLine();
        if (SliderInt(std::format("Limit##DN_{:08X}", formID).c_str(), &maxDN, 0, 25)) pref.maxDrinkNormal = static_cast<uint32_t>(std::clamp(maxDN, 0, 25));

        bool buyDA = pref.buyDrinkAlcohol;
        int maxDA = static_cast<int>(pref.maxDrinkAlcohol);
        if (Checkbox(std::format("Alcohol (Ale/Mead/Wine)##{:08X}", formID).c_str(), &buyDA)) pref.buyDrinkAlcohol = buyDA;
        SameLine();
        if (SliderInt(std::format("Limit##DA_{:08X}", formID).c_str(), &maxDA, 0, 25)) pref.maxDrinkAlcohol = static_cast<uint32_t>(std::clamp(maxDA, 0, 25));

        Separator();

        // ── 4. AMMUNITION ──
        TextColored(ImVec4{ 1.0f, 0.5f, 0.5f, 1.0f }, "Ammunition (Demirci / Blacksmith - Limit: 0-250)");

        bool buyArr = pref.buyArrows;
        int maxArr = static_cast<int>(pref.maxArrows);
        if (Checkbox(std::format("Buy Arrows##{:08X}", formID).c_str(), &buyArr)) pref.buyArrows = buyArr;
        SameLine();
        if (SliderInt(std::format("Limit##ARR_{:08X}", formID).c_str(), &maxArr, 0, 250)) pref.maxArrows = static_cast<uint32_t>(std::clamp(maxArr, 0, 250));

        bool buyBlt = pref.buyBolts;
        int maxBlt = static_cast<int>(pref.maxBolts);
        if (Checkbox(std::format("Buy Crossbow Bolts##{:08X}", formID).c_str(), &buyBlt)) pref.buyBolts = buyBlt;
        SameLine();
        if (SliderInt(std::format("Limit##BLT_{:08X}", formID).c_str(), &maxBlt, 0, 250)) pref.maxBolts = static_cast<uint32_t>(std::clamp(maxBlt, 0, 250));

        Separator();

        // ── 5. CRAFTING & ECONOMY ──
        TextColored(ImVec4{ 0.6f, 0.9f, 0.6f, 1.0f }, "Economy & Crafting Preferences:");
        Checkbox(std::format("Receive Quest Reward Share##{:08X}", formID).c_str(), &pref.receiveQuestRewards);
        SameLine();
        SliderFloat(std::format("Share %##{:08X}", formID).c_str(), &pref.rewardSharePercent, 0.0f, 100.0f, "%.0f%%");

        Checkbox(std::format("Allow Crafting Arrows##{:08X}", formID).c_str(), &pref.allowCraftArrows);
        SameLine();
        Checkbox(std::format("Allow Crafting Bolts##{:08X}", formID).c_str(), &pref.allowCraftBolts);
        SameLine();
        Checkbox(std::format("Allow Crafting Potions##{:08X}", formID).c_str(), &pref.allowCraftPotions);
    }

    void __stdcall MenuInterface::RenderLootMenu()
    {
        using namespace ImGuiMCP;

        auto manager = FollowerManager::GetSingleton();
        auto profiles = manager->GetProfilesSnapshot();

        std::vector<std::pair<RE::FormID, std::string>> followerList;
        for (const auto& [fID, prof] : profiles) {
            if (!prof.isAnimal) {
                followerList.emplace_back(fID, prof.name);
            }
        }

        if (followerList.empty()) {
            TextColored(ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f }, "No humanoid followers detected.");
            return;
        }

        static RE::FormID s_selectedLootFollower{ 0 };
        bool followerValid = false;
        for (const auto& [fID, name] : followerList) {
            if (fID == s_selectedLootFollower) {
                followerValid = true;
                break;
            }
        }
        if (!followerValid) {
            s_selectedLootFollower = followerList.front().first;
        }

        TextColored(ImVec4{ 0.4f, 0.85f, 1.0f, 1.0f }, "Items to be looted (Yagmalanacak Esyalar)");
        TextDisabled("Her takipcinin cesetlerden veya yerden hangi kategorideki esyalari toplayacagini belirleyin.\nIsaretlenen kategoriler toplanir, isaretlenmeyenler es gecilir.");
        Separator();

        Text("Select Follower:");
        for (size_t i = 0; i < followerList.size(); ++i) {
            auto [fID, name] = followerList[i];
            bool isSelected = (fID == s_selectedLootFollower);

            if (isSelected) {
                TextColored(ImVec4{ 0.3f, 1.0f, 0.4f, 1.0f }, "[ %s ]", name.c_str());
            } else {
                if (Button(std::format("{}##LootSel_{:08X}", name, fID).c_str())) {
                    s_selectedLootFollower = fID;
                }
            }
            if (i + 1 < followerList.size()) SameLine();
        }

        Separator();

        auto profile = manager->GetProfile(s_selectedLootFollower);
        if (!profile) {
            TextColored(ImVec4{ 1.0f, 0.5f, 0.2f, 1.0f }, "Follower profile not found.");
            return;
        }

        auto& pref = profile->preferences;
        auto formID = s_selectedLootFollower;

        // Master Loot Toggle for this follower
        Checkbox(std::format("Allow Looting (Genel Yagma Izni)##Loot_{:08X}", formID).c_str(), &pref.allowLoot);
        if (IsItemHovered()) SetTooltip("Bu secenek kapaliysa takipci hicbir sekilde yagma yapmaz.");

        SameLine();
        if (Button(std::format("Select All (Hepsini Sec)##LootAll_{:08X}", formID).c_str())) {
            pref.allowLootGold = true;
            pref.allowLootWeapons = true;
            pref.allowLootArmor = true;
            pref.allowLootClothing = true;
            pref.allowLootJewelry = true;
            pref.allowLootPotions = true;
            pref.allowLootFood = true;
            pref.allowLootDrinks = true;
            pref.allowLootArrows = true;
            pref.allowLootIngredients = true;
            pref.allowLootSoulGems = true;
            pref.allowLootGems = true;
            pref.allowLootBooks = true;
            pref.allowLootScrolls = true;
        }
        SameLine();
        if (Button(std::format("Deselect All (Hepsini Kaldir)##LootNone_{:08X}", formID).c_str())) {
            pref.allowLootGold = false;
            pref.allowLootWeapons = false;
            pref.allowLootArmor = false;
            pref.allowLootClothing = false;
            pref.allowLootJewelry = false;
            pref.allowLootPotions = false;
            pref.allowLootFood = false;
            pref.allowLootDrinks = false;
            pref.allowLootArrows = false;
            pref.allowLootIngredients = false;
            pref.allowLootSoulGems = false;
            pref.allowLootGems = false;
            pref.allowLootBooks = false;
            pref.allowLootScrolls = false;
        }

        Spacing();
        TextColored(ImVec4{ 1.0f, 0.8f, 0.3f, 1.0f }, "Equipment & Combat Items:");
        Checkbox(std::format("Weapons (Silahlar)##Loot_{:08X}", formID).c_str(), &pref.allowLootWeapons);
        if (IsItemHovered()) SetTooltip("Kilic, balta, yay, asa gibi tum silahlar.");
        SameLine();
        Checkbox(std::format("Armor (Zirhlar)##Loot_{:08X}", formID).c_str(), &pref.allowLootArmor);
        if (IsItemHovered()) SetTooltip("Agir ve hafif zirh parcalari.");
        SameLine();
        Checkbox(std::format("Ammunition (Oklar & Civatalar)##Loot_{:08X}", formID).c_str(), &pref.allowLootArrows);
        if (IsItemHovered()) SetTooltip("Oklar ve tatar yayi civatalari.");

        Checkbox(std::format("Clothing & Robes (Kiyafetler & Cubbeler)##Loot_{:08X}", formID).c_str(), &pref.allowLootClothing);
        if (IsItemHovered()) SetTooltip("Zirh degeri olmayan cübbeler, giysiler, pelerinler.");
        SameLine();
        Checkbox(std::format("Jewelry (Kolyeler, Yuzukler, Taclar)##Loot_{:08X}", formID).c_str(), &pref.allowLootJewelry);
        if (IsItemHovered()) SetTooltip("Kolyeler, yuzukler ve taclar.");

        Spacing();
        TextColored(ImVec4{ 0.4f, 0.9f, 0.5f, 1.0f }, "Consumables & Survival:");
        Checkbox(std::format("Potions & Poisons (Iksirler & Zehirler)##Loot_{:08X}", formID).c_str(), &pref.allowLootPotions);
        if (IsItemHovered()) SetTooltip("Can, mana, stamina iksirleri ve silah zehirleri.");
        SameLine();
        Checkbox(std::format("Food (Yemekler)##Loot_{:08X}", formID).c_str(), &pref.allowLootFood);
        if (IsItemHovered()) SetTooltip("Pismis et, ekmek, corba ve yiyecekler.");
        SameLine();
        Checkbox(std::format("Drinks (Icecekler)##Loot_{:08X}", formID).c_str(), &pref.allowLootDrinks);
        if (IsItemHovered()) SetTooltip("Su, sut ve icki cesitleri.");

        Spacing();
        TextColored(ImVec4{ 0.8f, 0.6f, 1.0f, 1.0f }, "Valuables & Magic:");
        Checkbox(std::format("Gold / Money (Altin / Para)##Loot_{:08X}", formID).c_str(), &pref.allowLootGold);
        if (IsItemHovered()) SetTooltip("Cesetlerdeki veya konteynerlerdeki altin payi.");
        SameLine();
        Checkbox(std::format("Gems & Valuables (Mucevherler & Degerli Taslar)##Loot_{:08X}", formID).c_str(), &pref.allowLootGems);
        if (IsItemHovered()) SetTooltip("Yakut, zumrut, elmas ve degerli sus esyalari.");
        SameLine();
        Checkbox(std::format("Soul Gems (Ruh Taslari)##Loot_{:08X}", formID).c_str(), &pref.allowLootSoulGems);
        if (IsItemHovered()) SetTooltip("Bos veya dolu ruh taslari.");

        Checkbox(std::format("Ingredients (Craft & Simya Malzemeleri)##Loot_{:08X}", formID).c_str(), &pref.allowLootIngredients);
        if (IsItemHovered()) SetTooltip("Bitkiler, mantarlar ve simya malzemeleri.");
        SameLine();
        Checkbox(std::format("Books & Spell Tomes (Kitaplar)##Loot_{:08X}", formID).c_str(), &pref.allowLootBooks);
        if (IsItemHovered()) SetTooltip("Normal kitaplar ve buyu kitaplari.");
        SameLine();
        Checkbox(std::format("Scrolls (Parsomenler)##Loot_{:08X}", formID).c_str(), &pref.allowLootScrolls);
        if (IsItemHovered()) SetTooltip("Buyu parsomenleri.");
    }

    void __stdcall MenuInterface::RenderSellMenu()
    {
        using namespace ImGuiMCP;

        auto manager = FollowerManager::GetSingleton();
        auto profiles = manager->GetProfilesSnapshot();

        std::vector<std::pair<RE::FormID, std::string>> followerList;
        for (const auto& [fID, prof] : profiles) {
            if (!prof.isAnimal) {
                followerList.emplace_back(fID, prof.name);
            }
        }

        if (followerList.empty()) {
            TextColored(ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f }, "No humanoid followers detected.");
            return;
        }

        static RE::FormID s_selectedSellFollower{ 0 };
        bool followerValid = false;
        for (const auto& [fID, name] : followerList) {
            if (fID == s_selectedSellFollower) {
                followerValid = true;
                break;
            }
        }
        if (!followerValid) {
            s_selectedSellFollower = followerList.front().first;
        }

        TextColored(ImVec4{ 0.4f, 0.85f, 1.0f, 1.0f }, "Items to be sold (Satilacak Esyalar)");
        TextDisabled("Her takipcinin sehirlerde veya hanlardaki tuccarlara neleri satacagini belirleyin.\nIsaretli kategorilerdeki FAZLALIK esyalar satilir. Isaretlenmeyenler ASLA satilmaz.\nNOT: Uzerinde giyili veya koruma altina alinmis kiyafet/silahlar hicbir zaman satilmaz.");
        Separator();

        Text("Select Follower:");
        for (size_t i = 0; i < followerList.size(); ++i) {
            auto [fID, name] = followerList[i];
            bool isSelected = (fID == s_selectedSellFollower);

            if (isSelected) {
                TextColored(ImVec4{ 0.3f, 1.0f, 0.4f, 1.0f }, "[ %s ]", name.c_str());
            } else {
                if (Button(std::format("{}##SellSel_{:08X}", name, fID).c_str())) {
                    s_selectedSellFollower = fID;
                }
            }
            if (i + 1 < followerList.size()) SameLine();
        }

        Separator();

        auto profile = manager->GetProfile(s_selectedSellFollower);
        if (!profile) {
            TextColored(ImVec4{ 1.0f, 0.5f, 0.2f, 1.0f }, "Follower profile not found.");
            return;
        }

        auto& pref = profile->preferences;
        auto formID = s_selectedSellFollower;

        if (Button(std::format("Select All (Hepsini Sec)##SellAll_{:08X}", formID).c_str())) {
            pref.allowSellWeapons = true;
            pref.allowSellArmor = true;
            pref.allowSellClothing = true;
            pref.allowSellJewelry = true;
            pref.allowSellAmmo = true;
            pref.allowSellPotions = true;
            pref.allowSellFood = true;
            pref.allowSellDrinks = true;
            pref.allowSellIngredients = true;
            pref.allowSellSoulGems = true;
            pref.allowSellGems = true;
            pref.allowSellBooks = true;
            pref.allowSellScrolls = true;
        }
        SameLine();
        if (Button(std::format("Deselect All (Hepsini Kaldir)##SellNone_{:08X}", formID).c_str())) {
            pref.allowSellWeapons = false;
            pref.allowSellArmor = false;
            pref.allowSellClothing = false;
            pref.allowSellJewelry = false;
            pref.allowSellAmmo = false;
            pref.allowSellPotions = false;
            pref.allowSellFood = false;
            pref.allowSellDrinks = false;
            pref.allowSellIngredients = false;
            pref.allowSellSoulGems = false;
            pref.allowSellGems = false;
            pref.allowSellBooks = false;
            pref.allowSellScrolls = false;
        }

        Spacing();
        TextColored(ImVec4{ 1.0f, 0.8f, 0.3f, 1.0f }, "Excess Equipment & Gear:");
        Checkbox(std::format("Weapons (Yedek Silahlar)##Sell_{:08X}", formID).c_str(), &pref.allowSellWeapons);
        if (IsItemHovered()) SetTooltip("Kusanilmamis, fazlalik silahlar satilabilir.");
        SameLine();
        Checkbox(std::format("Armor (Yedek Zirhlar)##Sell_{:08X}", formID).c_str(), &pref.allowSellArmor);
        if (IsItemHovered()) SetTooltip("Giyilmemis veya koruma altina alinmamis fazlalik zirhlar satilabilir.");
        SameLine();
        Checkbox(std::format("Ammunition (Fazla Oklar)##Sell_{:08X}", formID).c_str(), &pref.allowSellAmmo);
        if (IsItemHovered()) SetTooltip("Takipcinin ok/civata limitini asan fazlalik muhimmat satilabilir.");

        Checkbox(std::format("Clothing & Robes (Kiyafetler)##Sell_{:08X}", formID).c_str(), &pref.allowSellClothing);
        if (IsItemHovered()) SetTooltip("Giyilmemis ve koruma listesinde olmayan kiyafet ve cubbeler.");
        SameLine();
        Checkbox(std::format("Jewelry (Takilar & Kolyeler)##Sell_{:08X}", formID).c_str(), &pref.allowSellJewelry);
        if (IsItemHovered()) SetTooltip("Kusanilmamis kolyeler, yuzukler ve taclar.");

        Spacing();
        TextColored(ImVec4{ 0.4f, 0.9f, 0.5f, 1.0f }, "Excess Consumables:");
        Checkbox(std::format("Potions & Poisons (Fazla Iksirler)##Sell_{:08X}", formID).c_str(), &pref.allowSellPotions);
        if (IsItemHovered()) SetTooltip("Kisisel iksir/zehir stok limitini asan fazlalik iksirler satilabilir.");
        SameLine();
        Checkbox(std::format("Food (Fazla Yemekler)##Sell_{:08X}", formID).c_str(), &pref.allowSellFood);
        if (IsItemHovered()) SetTooltip("Kisisel yiyecek limitini asan fazlalik yemekler satilabilir.");
        SameLine();
        Checkbox(std::format("Drinks (Fazla Icecekler)##Sell_{:08X}", formID).c_str(), &pref.allowSellDrinks);
        if (IsItemHovered()) SetTooltip("Kisisel icecek limitini asan fazlalik icecekler satilabilir.");

        Spacing();
        TextColored(ImVec4{ 0.8f, 0.6f, 1.0f, 1.0f }, "Valuables, Magic & Miscellaneous:");
        Checkbox(std::format("Gems & Valuables (Mucevherler)##Sell_{:08X}", formID).c_str(), &pref.allowSellGems);
        if (IsItemHovered()) SetTooltip("Yakut, zumrut, elmas vb. degerli taslar tuccarlara satilabilir.");
        SameLine();
        Checkbox(std::format("Soul Gems (Ruh Taslari)##Sell_{:08X}", formID).c_str(), &pref.allowSellSoulGems);
        if (IsItemHovered()) SetTooltip("Limit fazlasi ruh taslari buyuculere veya tuccarlara satilabilir (SoulKeeper korumasi aciksa asla satilmaz).");

        Checkbox(std::format("Ingredients (Fazla Simya Malzemeleri)##Sell_{:08X}", formID).c_str(), &pref.allowSellIngredients);
        if (IsItemHovered()) SetTooltip("10 adetten fazla biriken simya malzemeleri simyacilara satilir.");
        SameLine();
        Checkbox(std::format("Books (Kitaplar)##Sell_{:08X}", formID).c_str(), &pref.allowSellBooks);
        if (IsItemHovered()) SetTooltip("Envanterdeki kitaplar ve buyu kitaplari tuccarlara satilir.");
        SameLine();
        Checkbox(std::format("Scrolls (Parsomenler)##Sell_{:08X}", formID).c_str(), &pref.allowSellScrolls);
        if (IsItemHovered()) SetTooltip("Kullanilmayan buyu parsomenleri satilir.");
    }

    struct CachedWeaponItem
    {
        RE::FormID formID{ 0 };
        std::string name;
        std::string typeName;
        float damage{ 0.0f };
        bool isEnchanted{ false };
        bool isWorn{ false };
        bool isProtected{ false };
        int32_t count{ 1 };
    };

    struct CachedApparelItem
    {
        RE::FormID formID{ 0 };
        std::string name;
        std::string slotName;
        float armorRating{ 0.0f };
        bool isEnchanted{ false };
        bool isWorn{ false };
        bool isProtected{ false };
        int32_t count{ 1 };
    };

    static std::string GetWeaponTypeName(RE::TESObjectWEAP* wep)
    {
        if (!wep) return "Weapon";
        using Type = RE::WEAPON_TYPE;
        switch (wep->GetWeaponType()) {
        case Type::kHandToHandMelee: return "Unarmed";
        case Type::kOneHandSword: return "Sword (1H)";
        case Type::kOneHandDagger: return "Dagger";
        case Type::kOneHandAxe: return "War Axe (1H)";
        case Type::kOneHandMace: return "Mace";
        case Type::kTwoHandSword: return "Greatsword (2H)";
        case Type::kTwoHandAxe: return "Battleaxe (2H)";
        case Type::kBow: return "Bow";
        case Type::kStaff: return "Staff";
        case Type::kCrossbow: return "Crossbow";
        default: return "Weapon";
        }
    }

    static std::string GetArmorSlotName(RE::TESObjectARMO* armor)
    {
        if (!armor) return "Apparel";
        using Slot = RE::BIPED_MODEL::BipedObjectSlot;
        auto mask = static_cast<uint32_t>(armor->GetSlotMask());
        if (mask & static_cast<uint32_t>(Slot::kBody)) {
            return armor->GetArmorRating() > 0.0f ? "Body Armor" : "Robes / Clothing";
        }
        if ((mask & static_cast<uint32_t>(Slot::kHead)) || (mask & static_cast<uint32_t>(Slot::kHair))) return "Helmet / Hood";
        if (mask & static_cast<uint32_t>(Slot::kHands)) return "Gauntlets / Gloves";
        if (mask & static_cast<uint32_t>(Slot::kFeet)) return "Boots / Shoes";
        if (mask & static_cast<uint32_t>(Slot::kShield)) return "Shield";
        if (mask & static_cast<uint32_t>(Slot::kAmulet)) return "Amulet";
        if (mask & static_cast<uint32_t>(Slot::kRing)) return "Ring";
        if (mask & static_cast<uint32_t>(Slot::kCirclet)) return "Circlet";
        return "Apparel";
    }

    static RE::FormID s_selectedWeaponFollower{ 0 };
    static std::vector<CachedWeaponItem> s_cachedWeapons;
    static RE::FormID s_lastScannedWeaponFollower{ 0 };
    static std::unordered_set<RE::FormID> s_selectedWeaponItems;

    static RE::FormID s_selectedApparelFollower{ 0 };
    static std::vector<CachedApparelItem> s_cachedApparel;
    static RE::FormID s_lastScannedApparelFollower{ 0 };
    static std::unordered_set<RE::FormID> s_selectedApparelItems;

    static void ScanWeaponsForFollower(RE::Actor* actor, FollowerProfile* profile)
    {
        s_cachedWeapons.clear();
        if (!actor || !profile) return;
        if (s_lastScannedWeaponFollower != actor->GetFormID()) s_selectedWeaponItems.clear();

        auto* equippedObj = actor->GetEquippedObject(false);
        RE::FormID equippedID = equippedObj ? equippedObj->GetFormID() : 0;

        for (const auto& [item, inv] : actor->GetInventory()) {
            if (!item || inv.first <= 0) continue;
            auto* wep = item->As<RE::TESObjectWEAP>();
            if (!wep) continue;

            CachedWeaponItem entry;
            entry.formID = wep->GetFormID();
            entry.name = wep->GetName();
            if (entry.name.empty()) entry.name = "Unnamed Weapon";
            entry.typeName = GetWeaponTypeName(wep);
            entry.damage = static_cast<float>(wep->GetAttackDamage());
            entry.isEnchanted = (wep->formEnchanting != nullptr);
            entry.isProtected = profile->IsProtected(entry.formID);
            entry.count = inv.first;

            bool worn = (entry.formID == equippedID);
            if (!worn && inv.second && inv.second->extraLists) {
                for (auto* xList : *inv.second->extraLists) {
                    if (xList && (xList->HasType(RE::ExtraDataType::kWorn) || xList->HasType(RE::ExtraDataType::kWornLeft))) {
                        worn = true;
                        break;
                    }
                }
            }
            entry.isWorn = worn;

            s_cachedWeapons.push_back(entry);
        }

        std::sort(s_cachedWeapons.begin(), s_cachedWeapons.end(), [](const CachedWeaponItem& a, const CachedWeaponItem& b) {
            if (a.isProtected != b.isProtected) return a.isProtected > b.isProtected;
            if (a.isWorn != b.isWorn) return a.isWorn > b.isWorn;
            return a.name < b.name;
        });

        s_lastScannedWeaponFollower = actor->GetFormID();
    }

    static void ScanApparelForFollower(RE::Actor* actor, FollowerProfile* profile)
    {
        s_cachedApparel.clear();
        if (!actor || !profile) return;
        if (s_lastScannedApparelFollower != actor->GetFormID()) s_selectedApparelItems.clear();

        for (const auto& [item, inv] : actor->GetInventory()) {
            if (!item || inv.first <= 0) continue;
            auto* armor = item->As<RE::TESObjectARMO>();
            if (!armor || IsTngHiddenBodyItem(armor) ||
                (armor->GetFormFlags() & RE::TESObjectARMO::RecordFlags::kNonPlayable) != 0) continue;

            CachedApparelItem entry;
            entry.formID = armor->GetFormID();
            entry.name = armor->GetName();
            if (entry.name.empty()) entry.name = "Unnamed Apparel";
            entry.slotName = GetArmorSlotName(armor);
            entry.armorRating = armor->GetArmorRating();
            entry.isEnchanted = (armor->formEnchanting != nullptr);
            entry.isProtected = profile->IsProtected(entry.formID);
            entry.count = inv.first;

            bool worn = false;
            if (inv.second && inv.second->extraLists) {
                for (auto* xList : *inv.second->extraLists) {
                    if (xList && (xList->HasType(RE::ExtraDataType::kWorn) || xList->HasType(RE::ExtraDataType::kWornLeft))) {
                        worn = true;
                        break;
                    }
                }
            }
            entry.isWorn = worn;

            s_cachedApparel.push_back(entry);
        }

        std::sort(s_cachedApparel.begin(), s_cachedApparel.end(), [](const CachedApparelItem& a, const CachedApparelItem& b) {
            if (a.isProtected != b.isProtected) return a.isProtected > b.isProtected;
            if (a.isWorn != b.isWorn) return a.isWorn > b.isWorn;
            return a.name < b.name;
        });

        s_lastScannedApparelFollower = actor->GetFormID();
    }

    void __stdcall MenuInterface::RenderWeaponsMenu()
    {
        using namespace ImGuiMCP;

        auto manager = FollowerManager::GetSingleton();
        auto profiles = manager->GetProfilesSnapshot();

        // İnsansı takipçileri filtrele
        std::vector<std::pair<RE::FormID, std::string>> followerList;
        for (const auto& [fID, prof] : profiles) {
            if (!prof.isAnimal) {
                followerList.emplace_back(fID, prof.name);
            }
        }

        if (followerList.empty()) {
            TextColored(ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f }, "No humanoid followers detected.");
            Text("Tip: Ensure your follower is recruited and nearby, then press Refresh.");
            if (Button("Force Scan Followers Now")) {
                manager->ScanForActiveFollowers();
            }
            return;
        }

        // Eğer seçili takipçi listede yoksa ilkini seç
        bool followerValid = false;
        for (const auto& [fID, name] : followerList) {
            if (fID == s_selectedWeaponFollower) {
                followerValid = true;
                break;
            }
        }
        if (!followerValid) {
            s_selectedWeaponFollower = followerList.front().first;
        }

        TextColored(ImVec4{ 0.4f, 0.8f, 1.0f, 1.0f }, "Follower Weapons Management (Silah Yonetimi)");
        TextColored(ImVec4{ 0.55f, 0.9f, 1.0f, 1.0f }, "SECIM kutusu: Bu silahi toplu dugme icin sec.  |  KORUMA kutusu (esya adi): Satisi ve modun otomatik kusanmasini engeller.");
        Separator();

        // Takipçi Seçim Butonları
        Text("Select Follower:");
        for (size_t i = 0; i < followerList.size(); ++i) {
            auto [fID, name] = followerList[i];
            bool isSelected = (fID == s_selectedWeaponFollower);

            if (isSelected) {
                TextColored(ImVec4{ 0.3f, 1.0f, 0.4f, 1.0f }, "[ %s ]", name.c_str());
            } else {
                if (Button(std::format("{}##WepSel_{:08X}", name, fID).c_str())) {
                    s_selectedWeaponFollower = fID;
                }
            }
            if (i + 1 < followerList.size()) SameLine();
        }

        Separator();

        auto* actorForm = RE::TESForm::LookupByID(s_selectedWeaponFollower);
        auto* actor = actorForm ? actorForm->As<RE::Actor>() : nullptr;
        auto profile = manager->GetProfile(s_selectedWeaponFollower);

        if (!actor || !profile) {
            TextColored(ImVec4{ 1.0f, 0.5f, 0.2f, 1.0f }, "Follower not loaded in 3D.");
            return;
        }

        // PERFORMANS: Sadece takipçi değiştiğinde veya liste boşken veya kullanıcı refresh dediğinde tara!
        if (s_lastScannedWeaponFollower != s_selectedWeaponFollower || s_cachedWeapons.empty()) {
            ScanWeaponsForFollower(actor, profile.get());
        }

        if (Button("Refresh Inventory (Envanteri Yenile)")) {
            ScanWeaponsForFollower(actor, profile.get());
        }
        SameLine();
        if (Button("Clear All Weapon Protections (Tum Korumalari Kaldir)")) {
            for (auto& item : s_cachedWeapons) {
                profile->ProtectItem(item.formID, false);
                item.isProtected = false;
            }
            NotificationManager::GetSingleton()->ShowHUDMessage(
                std::format("{}: All weapon protections cleared.", profile->name));
        }
        if (Button("Equip & Protect Selected Weapons (Secilen Silahlari Giy ve Koru)")) {
            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            size_t applied = 0;
            if (equipManager) {
                for (const auto formID : s_selectedWeaponItems) {
                    auto* weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(formID);
                    auto* object = weapon ? weapon->As<RE::TESBoundObject>() : nullptr;
                    if (!object) continue;
                    profile->ProtectItem(formID, true);
                    equipManager->EquipObject(actor, object, nullptr, 1, nullptr, false, false, false, true);
                    ++applied;
                }
            }
            if (applied > 0) AnimationManager::GetSingleton()->SheatheWeapon(actor);
            s_selectedWeaponItems.clear();
            ScanWeaponsForFollower(actor, profile.get());
            NotificationManager::GetSingleton()->ShowHUDMessage(
            std::format("{}: {} selected weapon(s) equipped and protected.", profile->name, applied));
        }

        Spacing();

        if (s_cachedWeapons.empty()) {
            TextDisabled("This follower has no weapons in their inventory.");
            return;
        }

        TextColored(ImVec4{ 1.0f, 0.85f, 0.3f, 1.0f }, "Weapons in Inventory (%zu):", s_cachedWeapons.size());
        Separator();

        for (auto& item : s_cachedWeapons) {
            bool selected = s_selectedWeaponItems.contains(item.formID);
            if (Checkbox(std::format("Sec##WEPSEL_{:08X}", item.formID).c_str(), &selected)) {
                if (selected) s_selectedWeaponItems.insert(item.formID);
                else s_selectedWeaponItems.erase(item.formID);
            }
            SameLine();
            bool isProtected = item.isProtected;
            std::string label = std::format("{}##WP_{:08X}", item.name, item.formID);

            if (Checkbox(label.c_str(), &isProtected)) {
                profile->ProtectItem(item.formID, isProtected);
                item.isProtected = isProtected;
                NotificationManager::GetSingleton()->ShowHUDMessage(isProtected
                    ? std::format("{}: {} protected (not equipped).", profile->name, item.name)
                    : std::format("{}: {} protection removed.", profile->name, item.name));
            }

            SameLine();
            TextDisabled("[%s | Dmg: %.0f%s%s]",
                item.typeName.c_str(),
                item.damage,
                item.isEnchanted ? " | Enchanted" : "",
                item.isWorn ? " | Worn" : "");

            if (item.count > 1) {
                SameLine();
                TextColored(ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f }, "(x%d)", item.count);
            }
        }
    }

    void __stdcall MenuInterface::RenderApparelMenu()
    {
        using namespace ImGuiMCP;

        auto manager = FollowerManager::GetSingleton();
        auto profiles = manager->GetProfilesSnapshot();

        std::vector<std::pair<RE::FormID, std::string>> followerList;
        for (const auto& [fID, prof] : profiles) {
            if (!prof.isAnimal) {
                followerList.emplace_back(fID, prof.name);
            }
        }

        if (followerList.empty()) {
            TextColored(ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f }, "No humanoid followers detected.");
            if (Button("Force Scan Followers Now")) {
                manager->ScanForActiveFollowers();
            }
            return;
        }

        bool followerValid = false;
        for (const auto& [fID, name] : followerList) {
            if (fID == s_selectedApparelFollower) {
                followerValid = true;
                break;
            }
        }
        if (!followerValid) {
            s_selectedApparelFollower = followerList.front().first;
        }

        TextColored(ImVec4{ 0.4f, 0.8f, 1.0f, 1.0f }, "Follower Apparel & Outfit Management (Kiyafet & Zirh)");
        TextColored(ImVec4{ 0.55f, 0.9f, 1.0f, 1.0f }, "SECIM kutusu: Bu kiyafeti toplu dugme icin sec.  |  KORUMA kutusu (esya adi): Satisi engeller; baska sistem degistirirse geri giydirir.");
        Separator();

        Text("Select Follower:");
        for (size_t i = 0; i < followerList.size(); ++i) {
            auto [fID, name] = followerList[i];
            bool isSelected = (fID == s_selectedApparelFollower);

            if (isSelected) {
                TextColored(ImVec4{ 0.3f, 1.0f, 0.4f, 1.0f }, "[ %s ]", name.c_str());
            } else {
                if (Button(std::format("{}##ArmSel_{:08X}", name, fID).c_str())) {
                    s_selectedApparelFollower = fID;
                }
            }
            if (i + 1 < followerList.size()) SameLine();
        }

        Separator();

        auto* actorForm = RE::TESForm::LookupByID(s_selectedApparelFollower);
        auto* actor = actorForm ? actorForm->As<RE::Actor>() : nullptr;
        auto profile = manager->GetProfile(s_selectedApparelFollower);

        if (!actor || !profile) {
            TextColored(ImVec4{ 1.0f, 0.5f, 0.2f, 1.0f }, "Follower not loaded in 3D.");
            return;
        }

        if (s_lastScannedApparelFollower != s_selectedApparelFollower || s_cachedApparel.empty()) {
            ScanApparelForFollower(actor, profile.get());
        }

        if (Button("Refresh Inventory (Envanteri Yenile)")) {
            ScanApparelForFollower(actor, profile.get());
        }
        SameLine();
        if (Button("Capture Current Worn Outfit (Giydiklerini Koru)")) {
            FollowerManager::CaptureEquippedProtection(actor, *profile);
            ScanApparelForFollower(actor, profile.get());
            NotificationManager::GetSingleton()->ShowHUDMessage(
                std::format("{}: Giydiği tüm kıyafetler koruma altına alındı.", profile->name));
        }
        SameLine();
        if (Button("Clear All Apparel Protections (Korumalari Kaldir)")) {
            for (auto& item : s_cachedApparel) {
                profile->ProtectItem(item.formID, false);
                item.isProtected = false;
            }
            NotificationManager::GetSingleton()->ShowHUDMessage(
                std::format("{}: All apparel protections cleared.", profile->name));
        }
        if (Button("Equip & Protect Selected Apparel (Secilen Kiyafetleri Giy ve Koru)")) {
            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            size_t applied = 0;
            std::vector<RE::FormID> selectedIDs(s_selectedApparelItems.begin(), s_selectedApparelItems.end());
            // The selected outfit becomes authoritative for its slots. Remove
            // stale protection on conflicting armor so it cannot be restored
            // over the player's current selection after fast travel.
            for (const auto formID : selectedIDs) {
                auto* selectedArmor = RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
                if (!selectedArmor) continue;
                const auto selectedMask = static_cast<uint32_t>(selectedArmor->GetSlotMask());
                std::vector<RE::FormID> staleProtections;
                for (const auto protectedID : profile->protectedEquipment) {
                    if (s_selectedApparelItems.contains(protectedID)) continue;
                    auto* protectedArmor = RE::TESForm::LookupByID<RE::TESObjectARMO>(protectedID);
                    if (protectedArmor &&
                        (static_cast<uint32_t>(protectedArmor->GetSlotMask()) & selectedMask) != 0) {
                        staleProtections.push_back(protectedID);
                    }
                }
                for (const auto staleID : staleProtections) profile->ProtectItem(staleID, false);
            }
            if (equipManager) {
                for (const auto formID : selectedIDs) {
                    auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
                    auto* object = armor ? armor->As<RE::TESBoundObject>() : nullptr;
                    if (!object || IsTngHiddenBodyItem(armor)) continue;
                    profile->ProtectItem(formID, true);
                    equipManager->EquipObject(actor, object, nullptr, 1, nullptr, false, false, false, true);
                    ++applied;
                }
            }
            s_selectedApparelItems.clear();
            ScanApparelForFollower(actor, profile.get());
            NotificationManager::GetSingleton()->ShowHUDMessage(
                std::format("{}: {} selected apparel item(s) equipped and protected.", profile->name, applied));
        }

        Spacing();

        if (s_cachedApparel.empty()) {
            TextDisabled("This follower has no apparel or armor in their inventory.");
            return;
        }

        TextColored(ImVec4{ 1.0f, 0.85f, 0.3f, 1.0f }, "Apparel in Inventory (%zu):", s_cachedApparel.size());
        Separator();

        for (auto& item : s_cachedApparel) {
            bool selected = s_selectedApparelItems.contains(item.formID);
            if (Checkbox(std::format("Sec##APPSEL_{:08X}", item.formID).c_str(), &selected)) {
                if (selected) s_selectedApparelItems.insert(item.formID);
                else s_selectedApparelItems.erase(item.formID);
            }
            SameLine();
            bool isProtected = item.isProtected;
            std::string label = std::format("{}##AP_{:08X}", item.name, item.formID);

            if (Checkbox(label.c_str(), &isProtected)) {
                profile->ProtectItem(item.formID, isProtected);
                item.isProtected = isProtected;
                NotificationManager::GetSingleton()->ShowHUDMessage(isProtected
                    ? std::format("{}: {} protected (not equipped).", profile->name, item.name)
                    : std::format("{}: {} protection removed.", profile->name, item.name));
            }

            SameLine();
            TextDisabled("[%s | Armor: %.0f%s%s]",
                item.slotName.c_str(),
                item.armorRating,
                item.isEnchanted ? " | Enchanted" : "",
                item.isWorn ? " | Worn" : "");

            if (item.count > 1) {
                SameLine();
                TextColored(ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f }, "(x%d)", item.count);
            }
        }
    }

    void __stdcall MenuInterface::RenderSettingsMenu()
    {
        using namespace ImGuiMCP;

        auto settings = Settings::GetSingleton();

        Text("Follower Unbound - Mod Settings");
        Separator();

        // ═══════════════════════════════════════════════════════════
        // SECTION 1: SYSTEM TOGGLES
        // ═══════════════════════════════════════════════════════════
        TextColored(ImVec4{ 0.4f, 1.0f, 0.4f, 1.0f }, "System Toggles");
        TextDisabled("Enable or disable entire subsystems. When a system is OFF it does nothing.");
        Spacing();

        Checkbox("Combat Potion System", &settings->enableCombatPotions);
        if (IsItemHovered()) SetTooltip("Followers automatically drink Health/Magicka/Stamina potions during combat.");

        SameLine();
        Checkbox("Needs System (Hunger/Thirst)", &settings->enableNeedsSystem);
        if (IsItemHovered()) SetTooltip("Followers consume food and drink over time based on game hours played.");

        Checkbox("Auto-Equip System", &settings->enableAutoEquip);
        if (IsItemHovered()) SetTooltip("Followers automatically equip the best available weapon or armor from their inventory.");

        SameLine();
        if (Checkbox("Prompt on Better Equipment (Onay Kutusu)", &settings->promptOnBetterEquipment)) {
            settings->Save();
        }
        if (IsItemHovered()) SetTooltip("Takipçi daha iyi veya büyülü bir silah/zırh bulduğunda onay kutusu açılır.\nEvet derseniz kuşanır ve korur, eskiyi satışa bırakır.\nHayır derseniz kuşanmaz ve pazarda satar.");

        Checkbox("Mage Prefer Robes (Büyücü Cübbe Tercihi)", &settings->magePreferRobes);
        if (IsItemHovered()) SetTooltip("Büyücü takipçiler (Mage/Sorcerer/Priest) ağır zırh yerine büyülü cübbe ve kıyafetleri tercih eder.");

        Checkbox("Shopping System (Buy)", &settings->enableShoppingSystem);
        if (IsItemHovered()) SetTooltip("Followers automatically purchase supplies (potions, food, ammo) from nearby vendors.");

        SameLine();
        Checkbox("Selling System (Sell)", &settings->enableSellingSystem);
        if (IsItemHovered()) SetTooltip("Followers automatically sell excess weapons, armor, and gems to nearby vendors.");

        Checkbox("Poison System", &settings->enablePoisonSystem);
        if (IsItemHovered()) SetTooltip("Followers automatically apply poisons to their equipped weapon in combat.");

        Separator();

        // ═══════════════════════════════════════════════════════════
        // SECTION: LOOTING (YAĞMA)
        // ═══════════════════════════════════════════════════════════
        TextColored(ImVec4{ 0.5f, 0.9f, 1.0f, 1.0f }, "Looting (Yagma)");
        if (Checkbox("Share Leftover Container Loot (Kalan Esyalari Paylastir)", &settings->autoLootCorpses)) {
            settings->Save();
        }
        if (IsItemHovered()) SetTooltip("Ceset, sandik veya konteyner envanterini kapattiginda sende kalan uygun esyalari 20m icindeki takipcilere esit dagitir.");

        // ── KIYAFET KORUMA ──
        TextColored(ImVec4{ 0.6f, 0.9f, 1.0f, 1.0f }, "Equipment Protection:");
        if (Checkbox("Protect Equipped Clothing (per follower)", &settings->protectEquippedClothing)) {
            // Ayar açıldığı anda TÜM takipçilerin üzerindeki kıyafet/ekipman kaydedilir
            if (settings->protectEquippedClothing) {
                auto followers = FollowerManager::GetSingleton()->GetActiveFollowers();
                int captured = 0;
                for (auto* a : followers) {
                    if (!a) continue;
                    auto p = FollowerManager::GetSingleton()->GetProfile(a->GetFormID());
                    if (p && !p->isAnimal) {
                        FollowerManager::CaptureEquippedProtection(a, *p);
                        captured++;
                    }
                }
                NotificationManager::GetSingleton()->ShowHUDMessage(
                    std::format("Outfit protection enabled — captured for {} follower(s).", captured));
            }
            settings->Save();
        }
        if (IsItemHovered()) SetTooltip("Each follower's equipped clothes/armor/weapons are saved personally and NEVER sold.\nTo change an outfit: give new clothes, then press 'Reset && Capture Current Outfit' in the follower menu.");

        SameLine();
        if (Checkbox("Pet & Animal Needs System", &settings->enablePetSystem)) {
            settings->Save();
            FollowerManager::GetSingleton()->ScanForActiveFollowers();
        }
        if (IsItemHovered()) SetTooltip("Enables hunger, thirst and fatigue needs for non-humanoid companions (dogs, cats, horses). No looting, trading, or classes.");

        Separator();

        // ═══════════════════════════════════════════════════════════
        // SECTION 2: SELLING CATEGORY FILTERS
        // ═══════════════════════════════════════════════════════════
        TextColored(ImVec4{ 0.3f, 0.9f, 0.7f, 1.0f }, "Selling Category Filters");
        TextDisabled("Configure which excess item categories followers are allowed to sell to vendors.");
        Spacing();

        Checkbox("Sell Excess Weapons", &settings->sellWeapons);
        SameLine();
        Checkbox("Sell Excess Armor", &settings->sellArmor);
        SameLine();
        Checkbox("Sell Excess Ammo", &settings->sellAmmo);

        Checkbox("Sell Excess Potions", &settings->sellPotions);
        SameLine();
        Checkbox("Sell Excess Food & Drinks", &settings->sellFood);

        SameLine();
        Checkbox("Sell Gems & Jewelry", &settings->sellGems);

        Spacing();
        TextColored(ImVec4{ 0.9f, 0.4f, 1.0f, 1.0f }, "Soul Gem Protection (SoulKeeper Mod Compatibility):");
        Checkbox("NEVER Sell Soul Gems (Protect SoulKeeper Gems)", &settings->protectSoulGems);
        if (IsItemHovered()) SetTooltip("When checked, followers will NEVER sell any empty or filled soul gems.");

        Spacing();
        TextColored(ImVec4{ 1.0f, 0.75f, 0.2f, 1.0f }, "Sale Batch Size (Satis Carpani):");
        TextDisabled("Controls how many excess items are sold per transaction. Purchases fill each enabled stock target's shortage in one transaction.");
        if (Button(" 1x ##batch1")) { settings->transactionBatchMultiplier = 1; settings->Save(); }
        SameLine();
        if (Button(" 3x ##batch3")) { settings->transactionBatchMultiplier = 3; settings->Save(); }
        SameLine();
        if (Button(" 5x ##batch5")) { settings->transactionBatchMultiplier = 5; settings->Save(); }
        SameLine();
        if (Button(" 10x ##batch10")) { settings->transactionBatchMultiplier = 10; settings->Save(); }
        SameLine();
        Text("Current: %ux", settings->transactionBatchMultiplier);

        int batchInt = static_cast<int>(settings->transactionBatchMultiplier);
        if (SliderInt("Batch Multiplier##batchslider", &batchInt, 1, 10, "%d x")) {
            settings->transactionBatchMultiplier = static_cast<uint32_t>(batchInt);
            settings->Save();
        }

        Separator();

        // ═══════════════════════════════════════════════════════════
        // SECTION 2: NOTIFICATION TOGGLES
        // ═══════════════════════════════════════════════════════════
        TextColored(ImVec4{ 1.0f, 0.85f, 0.3f, 1.0f }, "Notification Toggles");
        TextDisabled("The underlying system still runs. Only the HUD message is suppressed when OFF.");
        Spacing();

        // Quick controls
        if (Button("Enable All Notifications")) {
            settings->notifyCombatHealthPotion  = true;
            settings->notifyCombatMagickaPotion = true;
            settings->notifyCombatStaminaPotion = true;
            settings->notifyPoisonApplied       = true;
            settings->notifyNeedsFood           = true;
            settings->notifyNeedsDrink          = true;
            settings->notifyNeedsHungryWarning  = true;
            settings->notifyNeedsThirstyWarning = true;
            settings->notifyLoot                = true;
            settings->notifyShopping            = true;
            settings->notifySelling             = true;
            settings->notifyAutoEquip           = true;
            settings->enableNotifications       = true;
        }
        SameLine();
        if (Button("Disable All Notifications")) {
            settings->notifyCombatHealthPotion  = false;
            settings->notifyCombatMagickaPotion = false;
            settings->notifyCombatStaminaPotion = false;
            settings->notifyPoisonApplied       = false;
            settings->notifyNeedsFood           = false;
            settings->notifyNeedsDrink          = false;
            settings->notifyNeedsHungryWarning  = false;
            settings->notifyNeedsThirstyWarning = false;
            settings->notifyLoot                = false;
            settings->notifyShopping            = false;
            settings->notifySelling             = false;
            settings->notifyAutoEquip           = false;
            settings->enableNotifications       = false;
            NotificationManager::GetSingleton()->Clear();
        }
        Spacing();
        Checkbox("Master: Enable All Notifications", &settings->enableNotifications);
        Spacing();

        // Combat
        TextColored(ImVec4{ 0.9f, 0.4f, 0.4f, 1.0f }, "  Combat:");
        Checkbox("Health Potion Notification", &settings->notifyCombatHealthPotion);
        SameLine();
        Checkbox("Magicka Potion Notification", &settings->notifyCombatMagickaPotion);
        SameLine();
        Checkbox("Stamina Potion Notification", &settings->notifyCombatStaminaPotion);

        Checkbox("Poison Applied Notification", &settings->notifyPoisonApplied);

        // Needs
        TextColored(ImVec4{ 1.0f, 0.8f, 0.4f, 1.0f }, "  Needs:");
        Checkbox("Food Consumed Notification", &settings->notifyNeedsFood);
        SameLine();
        Checkbox("Drink Consumed Notification", &settings->notifyNeedsDrink);

        Checkbox("Hungry Warning Notification", &settings->notifyNeedsHungryWarning);
        SameLine();
        Checkbox("Thirsty Warning Notification", &settings->notifyNeedsThirstyWarning);

        // Other systems
        TextColored(ImVec4{ 0.5f, 0.8f, 1.0f, 1.0f }, "  Other Systems:");
        Checkbox("Loot Notification", &settings->notifyLoot);
        SameLine();
        Checkbox("Shopping (Buy) Notification", &settings->notifyShopping);
        SameLine();
        Checkbox("Selling Notification", &settings->notifySelling);

        Checkbox("Auto-Equip Notification", &settings->notifyAutoEquip);

        Separator();

        // ═══════════════════════════════════════════════════════════
        // SECTION 3: NEEDS RATES
        // ═══════════════════════════════════════════════════════════
        TextColored(ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f }, "Needs Depletion Rates (Game Hours)");
        TextDisabled("Number of game hours for a need to drop to 0%.");

        SliderFloat("Thirst Depletion Rate (Hours)", &settings->thirstDepletionHours, 1.0f, 48.0f, "%.1f Hours");
        SliderFloat("Hunger Depletion Rate (Hours)", &settings->hungerDepletionHours, 1.0f, 48.0f, "%.1f Hours");
        SliderFloat("Fatigue Depletion Rate (Hours)", &settings->fatigueDepletionHours, 1.0f, 48.0f, "%.1f Hours");

        Separator();

        // ═══════════════════════════════════════════════════════════
        // SECTION 4: CONSUMPTION THRESHOLDS
        // ═══════════════════════════════════════════════════════════
        TextColored(ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f }, "Follower Auto-Consumption Thresholds (%)");
        TextDisabled("Followers will start consuming items when their need drops below this percentage.");

        SliderFloat("Eat Threshold (%)", &settings->eatThreshold, 5.0f, 95.0f, "Below %% %.0f");
        SliderFloat("Drink Threshold (%)", &settings->drinkThreshold, 5.0f, 95.0f, "Below %% %.0f");
        SliderFloat("Delay Between Consuming (Seconds)", &settings->consumeDelaySeconds, 1.0f, 20.0f, "%.1f Seconds");
        SliderFloat("Eat/Drink Animation Interval (Seconds)", &settings->consumeIntervalSeconds, 2.0f, 60.0f, "%.0f Seconds");
        if (IsItemHovered()) SetTooltip("Ayni takipcinin iki lokma/yudum arasindaki sure.\nYeme-icme animasyonu rahatca bitsin diye arttirildi (varsayilan 10 sn).");
        SliderFloat("Post-Combat Eat/Drink Delay (Seconds)", &settings->postCombatConsumeDelaySeconds, 0.0f, 60.0f, "%.0f Seconds");
        if (IsItemHovered()) SetTooltip("Followers wait this long after combat ends before they start eating or drinking.");

        Separator();

        // ═══════════════════════════════════════════════════════════
        // SECTION 5: COMBAT POTION THRESHOLDS
        // ═══════════════════════════════════════════════════════════
        TextColored(ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f }, "Combat Potion Usage Thresholds (%)");
        TextDisabled("Companions drink potions ONLY in combat when their stat drops below these percentages.");

        SliderFloat("Health Potion Threshold (%)", &settings->healthPotionThreshold, 5.0f, 95.0f, "Below %% %.0f");
        SliderFloat("Magicka Potion Threshold (%)", &settings->magickaPotionThreshold, 5.0f, 95.0f, "Below %% %.0f");
        SliderFloat("Stamina Potion Threshold (%)", &settings->staminaPotionThreshold, 5.0f, 95.0f, "Below %% %.0f");

        Spacing();
        TextColored(ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f }, "Emergency Thresholds (No Cooldown - Back to Back Drinking)");
        TextDisabled("When a stat drops below these values, followers drink potions immediately without any cooldown, one after another.");
        SliderFloat("Emergency Health (%%)", &settings->emergencyHealthPct, 1.0f, 50.0f, "Below %% %.0f");
        SliderFloat("Emergency Magicka (%%)", &settings->emergencyMagickaPct, 1.0f, 50.0f, "Below %% %.0f");
        SliderFloat("Emergency Stamina (%%)", &settings->emergencyStaminaPct, 1.0f, 50.0f, "Below %% %.0f");

        Separator();
        Text("Rest & Sleep Info:");
        TextDisabled("When the player rests or sleeps, active companions also rest and their fatigue restores to 100%.");

        Separator();
        if (Button("Save Settings (INI)")) {
            settings->Save();
        }
        SameLine();
        if (Button("Reset to Defaults")) {
            // Systems
            settings->enableCombatPotions  = true;
            settings->enableNeedsSystem    = true;
            settings->enableAutoEquip      = true;
            settings->enableShoppingSystem = true;
            settings->enableSellingSystem  = true;
            settings->enablePoisonSystem   = true;
            settings->enablePetSystem      = true;
            settings->protectEquippedClothing = true;
            settings->autoLootCorpses      = true;
            settings->lootDelaySeconds     = 10.0f;
            settings->lootMaxSpareWeapons  = 3;
            settings->lootMaxSpareArmor    = 2;
            settings->lootHostilesOnly     = true;
            settings->lootGold             = true;
            settings->lootWeapons          = true;
            settings->lootArmor            = true;
            settings->lootArrows           = true;
            settings->lootPotions          = true;
            settings->lootFood             = true;
            settings->lootIngredients      = false;
            settings->lootSoulGems         = false;
            settings->lootMisc             = true;
            // Selling category filters
            settings->sellWeapons     = true;
            settings->sellArmor       = true;
            settings->sellAmmo        = true;
            settings->sellPotions     = true;
            settings->sellFood        = true;
            settings->sellGems        = true;
            settings->protectSoulGems = true;
            // Notifications
            settings->notifyCombatHealthPotion  = true;
            settings->notifyCombatMagickaPotion = true;
            settings->notifyCombatStaminaPotion = true;
            settings->notifyPoisonApplied       = true;
            settings->notifyNeedsFood           = true;
            settings->notifyNeedsDrink          = true;
            settings->notifyNeedsHungryWarning  = true;
            settings->notifyNeedsThirstyWarning = true;
            settings->notifyLoot                = true;
            settings->notifyShopping            = true;
            settings->notifySelling             = true;
            settings->notifyAutoEquip           = true;
            // Rates & thresholds
            settings->thirstDepletionHours  = 6.0f;
            settings->hungerDepletionHours  = 8.0f;
            settings->fatigueDepletionHours = 10.0f;
            settings->eatThreshold          = 50.0f;
            settings->drinkThreshold        = 50.0f;
            settings->consumeDelaySeconds   = 3.0f;
            settings->consumeIntervalSeconds = 10.0f;
            settings->postCombatConsumeDelaySeconds = 6.0f;
            settings->healthPotionThreshold = 40.0f;
            settings->magickaPotionThreshold= 30.0f;
            settings->staminaPotionThreshold= 30.0f;
            settings->emergencyHealthPct  = 15.0f;
            settings->emergencyMagickaPct = 5.0f;
            settings->emergencyStaminaPct = 5.0f;
            settings->Save();
        }
    }

    void __stdcall MenuInterface::RenderBackgroundTick()
    {
        // SKSEMenuFramework her frame HUD çiziminde bu fonksiyonu çağırır.
        // Delayed equipment prompts are polled here so they wait until the
        // player closes any blocking game or SKSE menu before opening PrismaUI.
        EquipPrompt::Update();
        static auto lastConsumptionTime = std::chrono::steady_clock::now();
        static auto lastNeedsTickTime = std::chrono::steady_clock::now();
        static std::chrono::steady_clock::time_point lastPausedTime{};
        auto now = std::chrono::steady_clock::now();

        // ── SİSTEM DURAKLATMA ──
        // Yükleme ekranı, takipçi envanteri (container), diyalog ve bekleme/uyuma
        // menüleri açıkken tüm arka plan sistemleri DURUR. Menüler kapandıktan
        // ~3 saniye sonra yeniden başlar — böylece yükleme/ışınlanma/envanter
        // sırasında takipçiler işlem yapmaz, mesajlar birikmez.
        {
            auto ui = RE::UI::GetSingleton();
            bool paused = !ui ||
                          ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME) ||
                          ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
                          ui->IsMenuOpen(RE::SleepWaitMenu::MENU_NAME) ||
                          ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) ||
                          ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME);
            if (paused) {
                lastPausedTime = now;
                return;
            }
            if (std::chrono::duration<float>(now - lastPausedTime).count() < 3.0f) {
                if (LootManager::GetSingleton()->HasPendingLootWork()) {
                    LootManager::GetSingleton()->ScheduleProcessing();
                }
                return; // menüler kapandıktan 3 saniye sonra sisteme devam
            }
        }

        // Bildirim kuyruğunu işle: en fazla 1 mesaj / 2 saniye (yığılma önlenir)
        NotificationManager::GetSingleton()->Update();

        // 1. Her saniye: İhtiyaç seansındaki veya aç/susuz takipçilerin tüketimlerini dene
        //    + savaş bitmiş ölüleri otomatik yağlama kuyruğunu işle
        if (std::chrono::duration<float>(now - lastConsumptionTime).count() >= 1.0f) {
            lastConsumptionTime = now;
            FollowerManager::GetSingleton()->TriggerFollowerConsumption();
            LootManager::GetSingleton()->ScheduleProcessing();
            DroppedItemRecovery::GetSingleton()->ScheduleProcessing();

            // ── PERİYODİK TAKİPÇİ TARAMASI (~5 sn) ──
            // Bu CommonLib sürümünde takip-etme eventi (TESFollowEvent) YOK; eski
            // kodda tarama yalnızca load anında ve menü butonuyla yapılıyordu.
            // Sonuç: oyuncu yoldaş alırsa takipçi 0 profil olarak dakikalarca
            // kayıt dışı kalıyordu (ihtiyaçlar %100 takılı kalma raporu bu yüzden).
            // Artık her ~5 saniyede bir hafif tarama yeni yoldaşları yakalar.
            static int scanDivider = 0;
            if (++scanDivider >= 5) {
                scanDivider = 0;
                FollowerManager::GetSingleton()->ScanForActiveFollowers();
            }
        }

        // 2. Her 3 saniyede bir: Oyun zamanı delta'sını hesapla ve normal oynanışta da ihtiyaçları düşür
        if (std::chrono::duration<float>(now - lastNeedsTickTime).count() >= 3.0f) {
            lastNeedsTickTime = now;
            auto ui = RE::UI::GetSingleton();
            if (!ui || !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
                FollowerManager::GetSingleton()->UpdateGameTimeNeeds();

                // İhtiyaç cezalarını uygula
                if (Settings::GetSingleton()->enableNeedsSystem) {
                    auto followers = FollowerManager::GetSingleton()->GetActiveFollowers();
                    for (auto* actor : followers) {
                        if (!actor || actor->IsDead()) continue;
                        auto prof = FollowerManager::GetSingleton()->GetProfile(actor->GetFormID());
                        if (prof) {
                            ConsumableAI::GetSingleton()->ApplyNeedsDebuffs(actor, *prof);
                        }
                    }
                }
            }
        }

    }
}

