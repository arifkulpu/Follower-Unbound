#include "UI/PrismaUIManager.h"
#include "UI/EquipPrompt.h"
#include "PrismaUI_API.h"
#include "UI/SKSEMenuFramework.h"
#include "Core/FollowerManager.h"
#include "Core/Settings.h"
#include "Economy/EconomyManager.h"
#include "Economy/ShoppingAI.h"
#include "AI/ItemEvaluator.h"
#include "AI/AnimationManager.h"
#include "Core/TNGCompatibility.h"
#include <cctype>

namespace FollowerUnbound
{
    namespace
    {
        std::string GetPortraitRaceKey(RE::Actor* actor)
        {
            auto* race = actor ? actor->GetRace() : nullptr;
            const char* rawName = race ? race->GetName() : nullptr;
            if (!rawName) return {};

            std::string name(rawName);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (name.find("khajiit") != std::string::npos) return "khajiit";
            if (name.find("argonian") != std::string::npos) return "argonian";
            if (name.find("orc") != std::string::npos || name.find("orsimer") != std::string::npos) return "orc";
            if (name.find("wood elf") != std::string::npos || name.find("bosmer") != std::string::npos) return "wood_elf";
            if (name.find("dark elf") != std::string::npos || name.find("dunmer") != std::string::npos) return "dark_elf";
            if (name.find("high elf") != std::string::npos || name.find("altmer") != std::string::npos) return "high_elf";
            if (name.find("redguard") != std::string::npos) return "redguard";
            if (name.find("breton") != std::string::npos) return "breton";
            if (name.find("imperial") != std::string::npos) return "imperial";
            if (name.find("nord") != std::string::npos) return "nord";
            return {};
        }

        std::atomic_bool g_leftShiftDown{ false };
        std::atomic_bool g_rightShiftDown{ false };

        void PressEscapeForFramework()
        {
            INPUT events[2]{};
            for (auto& event : events) event.type = INPUT_KEYBOARD;
            events[0].ki.wVk = VK_ESCAPE;
            events[1].ki.wVk = VK_ESCAPE;
            events[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, events, sizeof(INPUT));
        }

        void AddPreferences(nlohmann::json& out, const FollowerPreferences& p)
        {
            out = {
                {"allowLoot",p.allowLoot},{"lootWeapons",p.allowLootWeapons},{"lootArmor",p.allowLootArmor},
                {"lootClothing",p.allowLootClothing},{"lootPotions",p.allowLootPotions},{"lootFood",p.allowLootFood},
                {"lootDrinks",p.allowLootDrinks},{"lootGold",p.allowLootGold},{"lootArrows",p.allowLootArrows},
                {"lootIngredients",p.allowLootIngredients},{"lootSoulGems",p.allowLootSoulGems},{"lootGems",p.allowLootGems},
                {"lootBooks",p.allowLootBooks},{"lootScrolls",p.allowLootScrolls},
                {"sellWeapons",p.allowSellWeapons},{"sellArmor",p.allowSellArmor},{"sellClothing",p.allowSellClothing},
                {"sellJewelry",p.allowSellJewelry},{"sellAmmo",p.allowSellAmmo},{"sellPotions",p.allowSellPotions},
                {"sellFood",p.allowSellFood},{"sellDrinks",p.allowSellDrinks},{"sellIngredients",p.allowSellIngredients},
                {"sellSoulGems",p.allowSellSoulGems},{"sellGems",p.allowSellGems},{"sellBooks",p.allowSellBooks},
                {"sellScrolls",p.allowSellScrolls},{"buyHealthPotions",p.buyHealthPotions},
                {"buyMagickaPotions",p.buyMagickaPotions},{"buyStaminaPotions",p.buyStaminaPotions},
                {"maxHealthPotions",p.maxHealthPotions},{"maxMagickaPotions",p.maxMagickaPotions},
                {"maxStaminaPotions",p.maxStaminaPotions},{"buyPoisons",p.buyPoisons},{"maxPoisons",p.maxPoisons},
                {"buyFoodLarge",p.buyFoodLarge},{"buyFoodMedium",p.buyFoodMedium},{"buyFoodSmall",p.buyFoodSmall},
                {"maxFoodLarge",p.maxFoodLarge},{"maxFoodMedium",p.maxFoodMedium},{"maxFoodSmall",p.maxFoodSmall},
                {"buyDrinkNormal",p.buyDrinkNormal},{"buyDrinkAlcohol",p.buyDrinkAlcohol},
                {"maxDrinkNormal",p.maxDrinkNormal},{"maxDrinkAlcohol",p.maxDrinkAlcohol},
                {"buyArrows",p.buyArrows},{"maxArrows",p.maxArrows},{"buyBolts",p.buyBolts},{"maxBolts",p.maxBolts},
                {"buySoulGems",p.buySoulGems},{"maxSoulGems",p.maxSoulGems},{"buyTorches",p.buyTorches},
                {"buyPettySoulGems",p.buyPettySoulGems},{"maxPettySoulGems",p.maxPettySoulGems},
                {"buyLesserSoulGems",p.buyLesserSoulGems},{"maxLesserSoulGems",p.maxLesserSoulGems},
                {"buyCommonSoulGems",p.buyCommonSoulGems},{"maxCommonSoulGems",p.maxCommonSoulGems},
                {"buyGreaterSoulGems",p.buyGreaterSoulGems},{"maxGreaterSoulGems",p.maxGreaterSoulGems},
                {"buyGrandSoulGems",p.buyGrandSoulGems},{"maxGrandSoulGems",p.maxGrandSoulGems},
                {"buyBlackSoulGems",p.buyBlackSoulGems},{"maxBlackSoulGems",p.maxBlackSoulGems},
                {"maxPotionFlowers",p.maxPotionFlowers},
                {"buyHealthMinor",p.buyHealthMinor},{"buyHealthStandard",p.buyHealthStandard},{"buyHealthPlentiful",p.buyHealthPlentiful},
                {"buyHealthVigorous",p.buyHealthVigorous},{"buyHealthExtreme",p.buyHealthExtreme},{"buyHealthUltimate",p.buyHealthUltimate},
                {"buyMagickaMinor",p.buyMagickaMinor},{"buyMagickaStandard",p.buyMagickaStandard},{"buyMagickaPlentiful",p.buyMagickaPlentiful},
                {"buyMagickaVigorous",p.buyMagickaVigorous},{"buyMagickaExtreme",p.buyMagickaExtreme},{"buyMagickaUltimate",p.buyMagickaUltimate},
                {"buyStaminaMinor",p.buyStaminaMinor},{"buyStaminaStandard",p.buyStaminaStandard},{"buyStaminaPlentiful",p.buyStaminaPlentiful},
                {"buyStaminaVigorous",p.buyStaminaVigorous},{"buyStaminaExtreme",p.buyStaminaExtreme},{"buyStaminaUltimate",p.buyStaminaUltimate},
                {"buyPoisonHealth",p.buyPoisonHealth},{"buyPoisonMagicka",p.buyPoisonMagicka},
                {"buyPoisonStamina",p.buyPoisonStamina},{"buyPoisonOther",p.buyPoisonOther},
                {"buyInvisibilityPotions",p.buyInvisibilityPotions},{"maxInvisibilityPotions",p.maxInvisibilityPotions},
                {"maxTorches",p.maxTorches},{"buyLanternOil",p.buyLanternOil},{"maxLanternOil",p.maxLanternOil},
                {"buySoap",p.buySoap},{"maxSoap",p.maxSoap},
                {"buyTreatmentPotions",p.buyTreatmentPotions},{"maxTreatmentPotions",p.maxTreatmentPotions},
                {"buyCurePoisonPotions",p.buyCurePoisonPotions},{"maxCurePoisonPotions",p.maxCurePoisonPotions},
                {"allowCraftArrows",p.allowCraftArrows},{"allowCraftBolts",p.allowCraftBolts},
                {"allowCraftTorches",p.allowCraftTorches},
                {"savingsEnabled",p.savingsEnabled},{"savingsTarget",p.savingsTarget},
                {"savingsPercent",p.savingsPercent},{"savedGold",p.savedGold},
                {"allowCraftPotions",p.allowCraftPotions},{"allowUpgradeEquipment",p.allowUpgradeEquipment},
                {"receiveQuestRewards",p.receiveQuestRewards},{"rewardSharePercent",p.rewardSharePercent}
            };
        }

        void SetPreference(FollowerPreferences& p, const std::string& key, const nlohmann::json& value)
        {
#define FU_BOOL_PREF(name, field) if (key == name) { p.field = value.get<bool>(); return; }
#define FU_UINT_PREF(name, field, lo, hi) if (key == name) { p.field = std::clamp(value.get<uint32_t>(), uint32_t(lo), uint32_t(hi)); return; }
            if (key == "savingsEnabled") {
                p.savingsEnabled = value.get<bool>();
                if (!p.savingsEnabled) { p.savedGold = 0; p.savingsRemainder = 0; } // Turning savings off releases the reserve.
                return;
            }
            if (key == "savingsTarget") {
                p.savingsTarget = std::clamp(value.get<uint32_t>(), 1u, 10000u);
                p.savedGold = std::min(p.savedGold, p.savingsTarget);
                if (p.savedGold >= p.savingsTarget) p.savingsRemainder = 0;
                return;
            }
            FU_BOOL_PREF("allowLoot", allowLoot) FU_BOOL_PREF("lootWeapons", allowLootWeapons)
            FU_BOOL_PREF("lootArmor", allowLootArmor) FU_BOOL_PREF("lootClothing", allowLootClothing)
            FU_BOOL_PREF("lootPotions", allowLootPotions) FU_BOOL_PREF("lootFood", allowLootFood)
            FU_BOOL_PREF("lootDrinks", allowLootDrinks) FU_BOOL_PREF("lootGold", allowLootGold)
            FU_BOOL_PREF("lootArrows", allowLootArrows) FU_BOOL_PREF("lootIngredients", allowLootIngredients)
            FU_BOOL_PREF("lootSoulGems", allowLootSoulGems) FU_BOOL_PREF("lootGems", allowLootGems)
            FU_BOOL_PREF("lootBooks", allowLootBooks) FU_BOOL_PREF("lootScrolls", allowLootScrolls)
            FU_BOOL_PREF("sellWeapons", allowSellWeapons) FU_BOOL_PREF("sellArmor", allowSellArmor)
            FU_BOOL_PREF("sellClothing", allowSellClothing) FU_BOOL_PREF("sellJewelry", allowSellJewelry)
            FU_BOOL_PREF("sellAmmo", allowSellAmmo) FU_BOOL_PREF("sellPotions", allowSellPotions)
            FU_BOOL_PREF("sellFood", allowSellFood) FU_BOOL_PREF("sellDrinks", allowSellDrinks)
            FU_BOOL_PREF("sellIngredients", allowSellIngredients) FU_BOOL_PREF("sellSoulGems", allowSellSoulGems)
            FU_BOOL_PREF("sellGems", allowSellGems) FU_BOOL_PREF("sellBooks", allowSellBooks)
            FU_BOOL_PREF("sellScrolls", allowSellScrolls) FU_BOOL_PREF("buyHealthPotions", buyHealthPotions)
            FU_BOOL_PREF("buyMagickaPotions", buyMagickaPotions) FU_BOOL_PREF("buyStaminaPotions", buyStaminaPotions)
            FU_UINT_PREF("maxHealthPotions", maxHealthPotions, 0, 25) FU_UINT_PREF("maxMagickaPotions", maxMagickaPotions, 0, 25)
            FU_UINT_PREF("maxStaminaPotions", maxStaminaPotions, 0, 25) FU_BOOL_PREF("buyPoisons", buyPoisons) FU_UINT_PREF("maxPoisons", maxPoisons, 0, 25)
            FU_BOOL_PREF("buyFoodLarge", buyFoodLarge) FU_BOOL_PREF("buyFoodMedium", buyFoodMedium) FU_BOOL_PREF("buyFoodSmall", buyFoodSmall)
            FU_UINT_PREF("maxFoodLarge", maxFoodLarge, 0, 25) FU_UINT_PREF("maxFoodMedium", maxFoodMedium, 0, 25)
            FU_UINT_PREF("maxFoodSmall", maxFoodSmall, 0, 25) FU_BOOL_PREF("buyDrinkNormal", buyDrinkNormal)
            FU_BOOL_PREF("buyDrinkAlcohol", buyDrinkAlcohol) FU_UINT_PREF("maxDrinkNormal", maxDrinkNormal, 0, 25)
            FU_UINT_PREF("maxDrinkAlcohol", maxDrinkAlcohol, 0, 25) FU_BOOL_PREF("buyArrows", buyArrows)
            FU_UINT_PREF("maxArrows", maxArrows, 0, 250) FU_BOOL_PREF("buyBolts", buyBolts)
            FU_UINT_PREF("maxBolts", maxBolts, 0, 250) FU_BOOL_PREF("buySoulGems", buySoulGems)
            FU_UINT_PREF("maxSoulGems", maxSoulGems, 0, 20) FU_BOOL_PREF("buyTorches", buyTorches)
            FU_BOOL_PREF("buyPettySoulGems", buyPettySoulGems) FU_UINT_PREF("maxPettySoulGems", maxPettySoulGems, 1, 20)
            FU_BOOL_PREF("buyLesserSoulGems", buyLesserSoulGems) FU_UINT_PREF("maxLesserSoulGems", maxLesserSoulGems, 1, 20)
            FU_BOOL_PREF("buyCommonSoulGems", buyCommonSoulGems) FU_UINT_PREF("maxCommonSoulGems", maxCommonSoulGems, 1, 20)
            FU_BOOL_PREF("buyGreaterSoulGems", buyGreaterSoulGems) FU_UINT_PREF("maxGreaterSoulGems", maxGreaterSoulGems, 1, 20)
            FU_BOOL_PREF("buyGrandSoulGems", buyGrandSoulGems) FU_UINT_PREF("maxGrandSoulGems", maxGrandSoulGems, 1, 20)
            FU_BOOL_PREF("buyBlackSoulGems", buyBlackSoulGems) FU_UINT_PREF("maxBlackSoulGems", maxBlackSoulGems, 1, 20)
            FU_UINT_PREF("maxPotionFlowers", maxPotionFlowers, 1, 50)
            FU_BOOL_PREF("buyHealthMinor", buyHealthMinor) FU_BOOL_PREF("buyHealthStandard", buyHealthStandard)
            FU_BOOL_PREF("buyHealthPlentiful", buyHealthPlentiful) FU_BOOL_PREF("buyHealthVigorous", buyHealthVigorous)
            FU_BOOL_PREF("buyHealthExtreme", buyHealthExtreme) FU_BOOL_PREF("buyHealthUltimate", buyHealthUltimate)
            FU_BOOL_PREF("buyMagickaMinor", buyMagickaMinor) FU_BOOL_PREF("buyMagickaStandard", buyMagickaStandard)
            FU_BOOL_PREF("buyMagickaPlentiful", buyMagickaPlentiful) FU_BOOL_PREF("buyMagickaVigorous", buyMagickaVigorous)
            FU_BOOL_PREF("buyMagickaExtreme", buyMagickaExtreme) FU_BOOL_PREF("buyMagickaUltimate", buyMagickaUltimate)
            FU_BOOL_PREF("buyStaminaMinor", buyStaminaMinor) FU_BOOL_PREF("buyStaminaStandard", buyStaminaStandard)
            FU_BOOL_PREF("buyStaminaPlentiful", buyStaminaPlentiful) FU_BOOL_PREF("buyStaminaVigorous", buyStaminaVigorous)
            FU_BOOL_PREF("buyStaminaExtreme", buyStaminaExtreme) FU_BOOL_PREF("buyStaminaUltimate", buyStaminaUltimate)
            FU_BOOL_PREF("buyPoisonHealth", buyPoisonHealth) FU_BOOL_PREF("buyPoisonMagicka", buyPoisonMagicka)
            FU_BOOL_PREF("buyPoisonStamina", buyPoisonStamina) FU_BOOL_PREF("buyPoisonOther", buyPoisonOther)
            FU_BOOL_PREF("buyInvisibilityPotions", buyInvisibilityPotions) FU_UINT_PREF("maxInvisibilityPotions", maxInvisibilityPotions, 0, 25)
            FU_UINT_PREF("maxTorches", maxTorches, 0, 20) FU_BOOL_PREF("buyLanternOil", buyLanternOil)
            FU_UINT_PREF("maxLanternOil", maxLanternOil, 0, 20) FU_BOOL_PREF("buySoap", buySoap)
            FU_UINT_PREF("maxSoap", maxSoap, 0, 20) FU_BOOL_PREF("allowCraftArrows", allowCraftArrows)
            FU_BOOL_PREF("buyTreatmentPotions", buyTreatmentPotions) FU_UINT_PREF("maxTreatmentPotions", maxTreatmentPotions, 0, 25)
            FU_BOOL_PREF("buyCurePoisonPotions", buyCurePoisonPotions) FU_UINT_PREF("maxCurePoisonPotions", maxCurePoisonPotions, 0, 25)
            FU_BOOL_PREF("allowCraftBolts", allowCraftBolts) FU_BOOL_PREF("allowCraftTorches", allowCraftTorches)
            FU_BOOL_PREF("savingsEnabled", savingsEnabled) FU_UINT_PREF("savingsPercent", savingsPercent, 0, 100)
            FU_BOOL_PREF("allowCraftPotions", allowCraftPotions) FU_BOOL_PREF("allowUpgradeEquipment", allowUpgradeEquipment)
            FU_BOOL_PREF("receiveQuestRewards", receiveQuestRewards)
            if (key == "rewardSharePercent") p.rewardSharePercent = std::clamp(value.get<float>(), 0.0f, 100.0f);
#undef FU_BOOL_PREF
#undef FU_UINT_PREF
        }
    }

    bool PrismaUIManager::Initialize()
    {
        if (_api) return true;
        auto* apiV2 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
        _api = apiV2 ? static_cast<PRISMA_UI_API::IVPrismaUI1*>(apiV2) :
            PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
        if (!_api) {
            logger::info("PrismaUI not found; keeping SKSE Menu Framework interface available.");
            return false;
        }
        _view = _api->CreateView("FollowerUnbound/index.html", [](PrismaView view) {
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([view]() {
                auto* self = GetSingleton();
                if (!self->_api || self->_view != view || !self->_api->IsValid(view)) return;
                self->_viewReady = true;
                self->_api->Hide(view);
                self->SendState();
            });
        });
        if (_view == 0 || !_api->IsValid(_view)) {
            logger::error("Could not create PrismaUI Follower Unbound view.");
            _api = nullptr;
            return false;
        }
        _api->RegisterJSListener(_view, "fuAction", OnAction);
        _api->RegisterJSListener(_view, "fuClose", OnClose);
        if (auto* apiV2 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>()) {
            apiV2->RegisterConsoleCallback(_view, [](PrismaView view, PRISMA_UI_API::ConsoleMessageLevel level, const char* msg) {
                if (level == PRISMA_UI_API::ConsoleMessageLevel::Error) logger::error("PrismaUI Follower Unbound {}: {}", view, msg ? msg : "(null)");
                else if (level == PRISMA_UI_API::ConsoleMessageLevel::Warning) logger::warn("PrismaUI Follower Unbound {}: {}", view, msg ? msg : "(null)");
            });
        }
        _api->Hide(_view);
        if (SKSEMenuFramework::IsInstalled()) {
            _hotkeyListener = SKSEMenuFramework::AddInputEvent(OnFrameworkInput);
        }
        logger::info("Registered Follower Unbound PrismaUI; shortcut is Shift+I.");
        return true;
    }

    bool __stdcall PrismaUIManager::OnFrameworkInput(RE::InputEvent* event)
    {
        bool requested = false;
        for (auto* current = event; current; current = current->next) {
            if (current->GetDevice() != RE::INPUT_DEVICE::kKeyboard) continue;
            auto* button = current->AsButtonEvent();
            if (!button) continue;
            const auto key = button->GetIDCode();
            if (key == RE::BSKeyboardDevice::Keys::kLeftShift) {
                if (button->IsUp()) g_leftShiftDown.store(false, std::memory_order_relaxed);
                else if (button->IsPressed()) g_leftShiftDown.store(true, std::memory_order_relaxed);
            } else if (key == RE::BSKeyboardDevice::Keys::kRightShift) {
                if (button->IsUp()) g_rightShiftDown.store(false, std::memory_order_relaxed);
                else if (button->IsPressed()) g_rightShiftDown.store(true, std::memory_order_relaxed);
            } else if (key == RE::BSKeyboardDevice::Keys::kI && button->IsDown() &&
                (g_leftShiftDown.load(std::memory_order_relaxed) || g_rightShiftDown.load(std::memory_order_relaxed))) {
                requested = true;
            }
        }
        if (!requested) return false;
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([]() { GetSingleton()->Open(); });
            return true;
        }
        return false;
    }

    void PrismaUIManager::Open()
    {
        if (!IsAvailable()) return;
        if (!_api->IsHidden(_view)) {
            if (EquipPrompt::HasActivePrompt()) EquipPrompt::Resolve(false);
            _api->Unfocus(_view);
            _api->Hide(_view);
            return;
        }
        if (SKSEMenuFramework::IsInstalled()) {
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([]() { GetSingleton()->BeginFrameworkClose(); });
                return;
            }
        }
        ShowView();
    }

    void PrismaUIManager::BeginFrameworkClose()
    {
        if (auto* window = SKSEMenuFramework::GetMainWindow()) {
            const bool wasOpen = window->IsOpen.exchange(false, std::memory_order_acq_rel);
            if (wasOpen) PressEscapeForFramework();
        } else {
            PressEscapeForFramework();
        }
        WaitForFrameworkClose(120);
    }

    void PrismaUIManager::WaitForFrameworkClose(uint32_t remaining)
    {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        tasks->AddTask([remaining]() {
            if (SKSEMenuFramework::IsInstalled() && SKSEMenuFramework::IsAnyBlockingWindowOpened()) {
                if (remaining == 0) {
                    RE::DebugNotification("Close the SKSE menu before opening Follower Unbound.");
                    return;
                }
                if (remaining % 20 == 0 && !SKSEMenuFramework::GetMainWindow()) PressEscapeForFramework();
                WaitForFrameworkClose(remaining - 1);
                return;
            }
            GetSingleton()->ShowView();
        });
    }

    void PrismaUIManager::ShowView()
    {
        if (!IsAvailable()) return;
        SendState();
        _api->Show(_view);
        _api->Focus(_view, true, false);
    }

    void PrismaUIManager::OnClose(const char*)
    {
        auto* self = GetSingleton();
        if (!self->IsAvailable()) return;
        // Closing the overlay while an equipment decision is active dismisses
        // that decision as declined. Never trap the player behind a modal.
        if (EquipPrompt::HasActivePrompt()) EquipPrompt::Resolve(false);
        self->_api->Unfocus(self->_view);
        self->_api->Hide(self->_view);
    }

    void PrismaUIManager::ShowEquipPrompt(RE::FormID followerID, RE::FormID itemID, RE::FormID oldItemID,
        std::string_view followerName, std::string_view itemName, std::string_view reason)
    {
        if (!IsAvailable() || !_viewReady) {
            logger::warn("EquipPrompt: PrismaUI is unavailable; skipped prompt for {} / {}.", followerName, itemName);
            EquipPrompt::Resolve(false);
            return;
        }

        ShowView();
        nlohmann::json prompt = {
            {"follower", followerID}, {"item", itemID}, {"oldItem", oldItemID},
            {"followerName", std::string(followerName)}, {"itemName", std::string(itemName)},
            {"reason", std::string(reason)}
        };
        const auto script = "window.fuShowEquipPrompt && window.fuShowEquipPrompt(" + prompt.dump() + ");";
        _api->Invoke(_view, script.c_str());
        logger::info("EquipPrompt: opened PrismaUI decision for {} / {}.", followerName, itemName);
    }

    void PrismaUIManager::OnAction(const char* data)
    {
        if (!data) return;
        try {
            const auto action = nlohmann::json::parse(data);
            if (action.value("kind", std::string{}) == "close") { OnClose(data); return; }
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([action]() {
                auto* self = GetSingleton();
                self->ApplyAction(action);
                self->SendState();
            });
        } catch (const std::exception& e) {
            logger::warn("Could not parse PrismaUI action: {}", e.what());
        }
    }

    void PrismaUIManager::ApplyAction(const nlohmann::json& a)
    {
        const auto kind = a.value("kind", std::string{});
        auto* manager = FollowerManager::GetSingleton();
        if (kind == "refresh") { manager->ScanForActiveFollowers(); return; }
        if (kind == "setting") {
            auto* s = Settings::GetSingleton();
            const auto key = a.value("key", std::string{});
            const auto& v = a.at("value");
#define FU_SET_BOOL(name, field) if (key == name) s->field = v.get<bool>(); else
            FU_SET_BOOL("autoLoot", autoLootCorpses) FU_SET_BOOL("autoEquip", enableAutoEquip)
            FU_SET_BOOL("selling", enableSellingSystem) FU_SET_BOOL("shopping", enableShoppingSystem)
            FU_SET_BOOL("combatPotions", enableCombatPotions) FU_SET_BOOL("needs", enableNeedsSystem)
            FU_SET_BOOL("poison", enablePoisonSystem) FU_SET_BOOL("protectOutfit", protectEquippedClothing)
            FU_SET_BOOL("promptBetterGear", promptOnBetterEquipment) FU_SET_BOOL("mageRobes", magePreferRobes)
            FU_SET_BOOL("lootHostilesOnly", lootHostilesOnly) FU_SET_BOOL("lootGold", lootGold)
            FU_SET_BOOL("lootWeapons", lootWeapons) FU_SET_BOOL("lootArmor", lootArmor)
            FU_SET_BOOL("lootArrows", lootArrows) FU_SET_BOOL("lootPotions", lootPotions)
            FU_SET_BOOL("lootFood", lootFood) FU_SET_BOOL("lootIngredients", lootIngredients)
            FU_SET_BOOL("lootSoulGems", lootSoulGems) FU_SET_BOOL("lootMisc", lootMisc)
            { logger::debug("Unknown PrismaUI setting key: {}", key); }
#undef FU_SET_BOOL
            if (key == "lootDelay") s->lootDelaySeconds = std::clamp(v.get<float>(), 0.0f, 60.0f);
            s->Save();
            return;
        }
        if (kind == "gold") {
            const auto id = a.value("follower", uint32_t{ 0 });
            const auto amount = a.value("amount", 0);
            if ((amount == 100 || amount == 500) && id) {
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
                if (actor) EconomyManager::GetSingleton()->PlayerGiveGold(actor, amount);
            }
            return;
        }
        if (kind == "preference") {
            auto profile = manager->GetProfile(a.value("follower", uint32_t{ 0 }));
            if (!profile) return;
            SetPreference(profile->preferences, a.value("key", std::string{}), a.at("value"));
            return;
        }
        if (kind == "equipPromptDecision") {
            EquipPrompt::Resolve(a.value("accept", false));
            return;
        }
        const auto followerID = a.value("follower", uint32_t{ 0 });
        auto profile = manager->GetProfile(followerID);
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(followerID);
        if (!profile || !actor) return;
        if (kind == "protect") {
            const auto itemID = a.value("item", uint32_t{ 0 });
            const bool protect = a.value("value", false);
            profile->ProtectItem(itemID, protect);
            // An item left equipped is intentionally excluded from sales. When the
            // player explicitly removes its protection, unequip it as well so it
            // becomes eligible for the next merchant sale without requiring a
            // second protected item to replace it first.
            if (!protect) {
                auto* object = RE::TESForm::LookupByID<RE::TESBoundObject>(itemID);
                if (object && (object->As<RE::TESObjectARMO>() || object->As<RE::TESObjectWEAP>())) {
                    if (auto* equip = RE::ActorEquipManager::GetSingleton()) {
                        equip->UnequipObject(actor, object, nullptr, 1, nullptr, false, false, false, true);
                        logger::info("EquipmentProtection: {} unprotected and unequipped {}; it is now eligible for sale.", actor->GetName(), object->GetName());
                    }
                }
            }
        } else if (kind == "equipSelected") {
            std::vector<uint32_t> ids = a.value("items", std::vector<uint32_t>{});
            auto* equip = RE::ActorEquipManager::GetSingleton();
            if (!equip) return;
            for (auto itemID : ids) {
                auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(itemID);
                if (!armor || IsTngHiddenBodyItem(armor)) continue;
                const auto mask = static_cast<uint32_t>(armor->GetSlotMask());
                std::vector<uint32_t> conflicts;
                for (auto protectedID : profile->protectedEquipment) {
                    if (std::find(ids.begin(), ids.end(), protectedID) != ids.end()) continue;
                    auto* oldArmor = RE::TESForm::LookupByID<RE::TESObjectARMO>(protectedID);
                    if (oldArmor && (static_cast<uint32_t>(oldArmor->GetSlotMask()) & mask)) conflicts.push_back(protectedID);
                }
                for (auto oldID : conflicts) profile->ProtectItem(oldID, false);
                profile->ProtectItem(itemID, true);
                equip->EquipObject(actor, armor, nullptr, 1, nullptr, false, false, false, true);
            }
            for (auto itemID : ids) {
                auto* weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(itemID);
                if (!weapon) continue;
                profile->ProtectItem(itemID, true);
                equip->EquipObject(actor, weapon, nullptr, 1, nullptr, false, false, false, true);
            }
            AnimationManager::GetSingleton()->SheatheWeapon(actor);
        }
    }

    void PrismaUIManager::SendState()
    {
        if (!_api || !_view || !_api->IsValid(_view) || !_viewReady) return;
        nlohmann::json state;
        auto* settings = Settings::GetSingleton();
        state["settings"] = {
            {"autoLoot",settings->autoLootCorpses},{"autoEquip",settings->enableAutoEquip},
            {"selling",settings->enableSellingSystem},{"shopping",settings->enableShoppingSystem},
            {"combatPotions",settings->enableCombatPotions},{"needs",settings->enableNeedsSystem},
            {"poison",settings->enablePoisonSystem},{"protectOutfit",settings->protectEquippedClothing},
            {"promptBetterGear",settings->promptOnBetterEquipment},{"mageRobes",settings->magePreferRobes},
            {"lootHostilesOnly",settings->lootHostilesOnly},{"lootGold",settings->lootGold},
            {"lootWeapons",settings->lootWeapons},{"lootArmor",settings->lootArmor},
            {"lootArrows",settings->lootArrows},{"lootPotions",settings->lootPotions},
            {"lootFood",settings->lootFood},{"lootIngredients",settings->lootIngredients},
            {"lootSoulGems",settings->lootSoulGems},{"lootMisc",settings->lootMisc},
            {"lootDelay",settings->lootDelaySeconds}
        };
        state["followers"] = nlohmann::json::array();
        auto gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0x0000000F);
        for (const auto& [id, p] : FollowerManager::GetSingleton()->GetProfilesSnapshot()) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            if (!actor) continue;
            nlohmann::json f = {
                {"id",id},{"name",p.name},{"animal",p.isAnimal},{"role",p.classBoundDisplayName},
                {"portraitRace",GetPortraitRaceKey(actor)},
                {"portraitFemale",actor->GetActorBase() && actor->GetActorBase()->GetSex() == RE::SEX::kFemale},
                {"hunger",p.needs.hunger},{"thirst",p.needs.thirst},{"fatigue",p.needs.fatigue},
                {"gold",gold ? ItemEvaluator::GetItemCount(actor, gold) : 0},
                {"carry",actor->GetWeightInContainer()},{"capacity",actor->GetTotalCarryWeight()}
            };
            AddPreferences(f["prefs"], p.preferences);
            const auto stock = ShoppingAI::GetSupplyCounts(actor);
            f["stock"] = {
                {"healthPotions",stock.health},{"magickaPotions",stock.magicka},{"staminaPotions",stock.stamina},
                {"poisons",stock.poison},{"foodLarge",stock.foodL},{"foodMedium",stock.foodM},{"foodSmall",stock.foodS},
                {"drinks",stock.drinkN},{"alcohol",stock.drinkA},{"arrows",stock.arrows},{"bolts",stock.bolts},
                {"soulGems",stock.soulGems},{"pettySoulGems",stock.pettySoulGems},{"lesserSoulGems",stock.lesserSoulGems},
                {"commonSoulGems",stock.commonSoulGems},{"greaterSoulGems",stock.greaterSoulGems},
                {"poisonHealth",stock.poisonHealth},{"poisonMagicka",stock.poisonMagicka},
                {"poisonStamina",stock.poisonStamina},{"poisonOther",stock.poisonOther},
                {"invisibilityPotions",stock.invisibilityPotions},
                {"healthMinor",stock.healthMinor},{"healthStandard",stock.healthStandard},{"healthPlentiful",stock.healthPlentiful},
                {"healthVigorous",stock.healthVigorous},{"healthExtreme",stock.healthExtreme},{"healthUltimate",stock.healthUltimate},
                {"magickaMinor",stock.magickaMinor},{"magickaStandard",stock.magickaStandard},{"magickaPlentiful",stock.magickaPlentiful},
                {"magickaVigorous",stock.magickaVigorous},{"magickaExtreme",stock.magickaExtreme},{"magickaUltimate",stock.magickaUltimate},
                {"staminaMinor",stock.staminaMinor},{"staminaStandard",stock.staminaStandard},{"staminaPlentiful",stock.staminaPlentiful},
                {"staminaVigorous",stock.staminaVigorous},{"staminaExtreme",stock.staminaExtreme},{"staminaUltimate",stock.staminaUltimate},
                {"grandSoulGems",stock.grandSoulGems},{"blackSoulGems",stock.blackSoulGems},
                {"blueMountainFlowers",stock.blueMountainFlowers},{"wheat",stock.wheat},
                {"redMountainFlowers",stock.redMountainFlowers},{"whiteCaps",stock.whiteCaps},
                {"purpleMountainFlowers",stock.purpleMountainFlowers},{"pineThrushEggs",stock.pineThrushEggs},
                {"torches",stock.torches},{"lanternOil",stock.lanternOil},{"soap",stock.soap},
                {"treatmentPotions",stock.treatmentPotions},{"curePoisonPotions",stock.curePoisonPotions}
            };
            f["weapons"] = nlohmann::json::array();
            f["apparel"] = nlohmann::json::array();
            for (const auto& [item, inv] : actor->GetInventory()) {
                if (!item || inv.first <= 0) continue;
                if (auto* weapon = item->As<RE::TESObjectWEAP>()) {
                    bool worn = false;
                    if (auto* right = actor->GetEquippedObject(false); right && right->GetFormID() == weapon->GetFormID()) worn = true;
                    if (auto* left = actor->GetEquippedObject(true); left && left->GetFormID() == weapon->GetFormID()) worn = true;
                    f["weapons"].push_back({{"id",weapon->GetFormID()},{"name",weapon->GetName() ? weapon->GetName() : "Weapon"},
                        {"damage",weapon->GetAttackDamage()},{"count",inv.first},{"worn",worn},{"protected",p.IsProtected(weapon->GetFormID())}});
                } else if (auto* armor = item->As<RE::TESObjectARMO>(); armor && !IsTngHiddenBodyItem(armor) &&
                    (armor->GetFormFlags() & RE::TESObjectARMO::RecordFlags::kNonPlayable) == 0) {
                    bool worn = false;
                    if (inv.second && inv.second->extraLists) for (auto* list : *inv.second->extraLists) {
                        if (list && (list->HasType(RE::ExtraDataType::kWorn) || list->HasType(RE::ExtraDataType::kWornLeft))) { worn = true; break; }
                    }
                    f["apparel"].push_back({{"id",armor->GetFormID()},{"name",armor->GetName() ? armor->GetName() : "Apparel"},
                        {"armor",armor->GetArmorRating()},{"count",inv.first},{"worn",worn},{"protected",p.IsProtected(armor->GetFormID())}});
                }
            }
            state["followers"].push_back(std::move(f));
        }
        const auto script = "window.fuSetState && window.fuSetState(" + state.dump() + ");";
        _api->Invoke(_view, script.c_str());
    }
}
