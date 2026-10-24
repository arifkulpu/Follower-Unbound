#include "AI/ConsumableAI.h"
#include "AI/ItemEvaluator.h"
#include "AI/AnimationManager.h"
#include "UI/NotificationManager.h"
#include "Core/Settings.h"
#include "AI/CombatTracker.h"
#include "AI/LootManager.h"
#include "Core/FollowerManager.h"

namespace FollowerUnbound
{
    bool ConsumableAI::TryCurePoison(RE::Actor* follower)
    {
        if (!follower || follower->IsDead() || follower->IsOnMount()) return false;

        auto* magicTarget = follower->AsMagicTarget();
        auto* activeEffects = magicTarget ? magicTarget->GetActiveEffectList() : nullptr;
        if (!activeEffects) return false;
        bool isPoisoned = false;
        for (auto* active : *activeEffects) {
            if (!active) continue;
            if (active->spell && active->spell->IsPoison()) {
                isPoisoned = true;
                break;
            }
            if (active->source) {
                auto* sourcePotion = active->source->As<RE::AlchemyItem>();
                if (sourcePotion && sourcePotion->IsPoison()) {
                    isPoisoned = true;
                    break;
                }
            }
        }
        if (!isPoisoned) return false;

        const auto now = std::chrono::steady_clock::now();
        const RE::FormID actorID = follower->GetFormID();
        if (auto it = _lastCurePoisonTime.find(actorID); it != _lastCurePoisonTime.end() &&
            std::chrono::duration<float>(now - it->second).count() < 5.0f) {
            return false;
        }

        for (const auto& [item, invData] : follower->GetInventory()) {
            if (invData.first <= 0 || !item) continue;
            auto* potion = item->As<RE::AlchemyItem>();
            if (!potion || !potion->HasEffect(RE::EffectArchetype::kCurePoison)) continue;

            AnimationManager::GetSingleton()->PlayPotionAnimation(follower);
            if (!follower->DrinkPotion(potion, nullptr)) {
                logger::warn("CurePoison: {} has a cure potion but DrinkPotion failed.", follower->GetName());
                return false;
            }
            follower->RemoveItem(potion, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            _lastCurePoisonTime[actorID] = now;
            logger::info("CurePoison: {} consumed {} while poisoned.", follower->GetName(), potion->GetName());
            NotificationManager::GetSingleton()->ShowHUDMessage(
                std::format("{} used a Cure Poison potion.", follower->GetName()));
            return true;
        }
        return false;
    }

    void ConsumableAI::UpdateCombatConsumables(RE::Actor* follower, FollowerProfile& profile)
    {
        // Yalnızca savaş sırasında ve hayattayken iksir kullanılır; at sırtındayken animasyon oynatılmaz
        if (!follower || follower->IsDead() || !follower->IsInCombat() || follower->IsOnMount()) return;

        // Dinamik savaş silahı geçişi (hedef mesafesine göre yay <-> kılıç/asa geçişi)
        FollowerManager::GetSingleton()->UpdateCombatWeaponSwitching(follower, profile);

        if (TryCurePoison(follower)) return;

        // System toggle
        if (!Settings::GetSingleton()->enableCombatPotions) return;

        auto ui = RE::UI::GetSingleton();
        if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) return;

        RE::FormID actorID = follower->GetFormID();
        auto now = std::chrono::steady_clock::now();

        auto avOwner = follower->AsActorValueOwner();
        if (!avOwner) return;

        auto settings = Settings::GetSingleton();
        auto animMgr  = AnimationManager::GetSingleton();
        auto notifMgr = NotificationManager::GetSingleton();
        auto evaluator = ItemEvaluator::GetSingleton();
        auto inventory = follower->GetInventory();

        float curHealth = avOwner->GetActorValue(RE::ActorValue::kHealth);
        float maxHealth = avOwner->GetBaseActorValue(RE::ActorValue::kHealth);
        float healthPct = (maxHealth > 0.0f) ? ((curHealth / maxHealth) * 100.0f) : 100.0f;

        float curMagicka = avOwner->GetActorValue(RE::ActorValue::kMagicka);
        float maxMagicka = avOwner->GetBaseActorValue(RE::ActorValue::kMagicka);
        float magickaPct = (maxMagicka > 0.0f) ? ((curMagicka / maxMagicka) * 100.0f) : 100.0f;

        float curStamina = avOwner->GetActorValue(RE::ActorValue::kStamina);
        float maxStamina = avOwner->GetBaseActorValue(RE::ActorValue::kStamina);
        float staminaPct = (maxStamina > 0.0f) ? ((curStamina / maxStamina) * 100.0f) : 100.0f;

        // ── ACİL DURUM (KRİTİK) ──
        // İçme animasyonu sürerken yeni içme başlatma (acil durumda bile)
        if (auto it = _combatDrinkAnimTime.find(actorID); it != _combatDrinkAnimTime.end()) {
            float since = std::chrono::duration<float>(now - it->second).count();
            if (since < 1.5f) return;          // animasyon hâlâ oynuyor
            // Animasyon bitti: grafiği savaş durumuna geri döndür
            follower->NotifyAnimationGraph("IdleForceDefaultState");
            if (auto* state = follower->AsActorState(); !state || !state->IsWeaponDrawn()) {
                follower->DrawWeaponMagicHands(true);
            }
            follower->EvaluatePackage();
            _combatDrinkAnimTime.erase(it);
        }

        // Can %15, mana %5 veya dayanıklılık %5 altına düştüyse: cooldown YOK,
        // takipçi arka arkaya (seri şekilde) iksir içer.
        bool emergency = (healthPct < settings->emergencyHealthPct) ||
                         (maxMagicka > 0.0f && magickaPct < settings->emergencyMagickaPct) ||
                         (maxStamina > 0.0f && staminaPct < settings->emergencyStaminaPct);

        if (!emergency) {
            // Normal durumda iksirler arasında en az 4 saniye bekleme süresi
            if (auto it = _lastPotionTime.find(actorID); it != _lastPotionTime.end()) {
                float elapsed = std::chrono::duration<float>(now - it->second).count();
                if (elapsed < 4.0f) {
                    return;
                }
            }
        }

        // ── SINIFSIZ İKSİR SEÇİMİ ──
        // Tüm takipçiler aynı kurallara tabidir; sınıfa özel bonus/öncelik yoktur.
        // Puan = (Eşik - Mevcut Yüzde); acil durum puanı +100 alır ve mutlak önceliklidir.

        struct PotionCandidate {
            CombatPotionType type;
            float urgency; // Daha yüksek puan = daha acil
        };
        std::vector<PotionCandidate> candidates;

        if (healthPct <= settings->healthPotionThreshold) {
            float deficit = settings->healthPotionThreshold - healthPct;
            if (healthPct < settings->emergencyHealthPct) deficit += 100.0f;
            candidates.push_back({ CombatPotionType::Health, deficit });
        }

        if (maxMagicka > 0.0f && magickaPct <= settings->magickaPotionThreshold) {
            float deficit = settings->magickaPotionThreshold - magickaPct;
            if (magickaPct < settings->emergencyMagickaPct) deficit += 100.0f;
            candidates.push_back({ CombatPotionType::Magicka, deficit });
        }

        if (maxStamina > 0.0f && staminaPct <= settings->staminaPotionThreshold) {
            float deficit = settings->staminaPotionThreshold - staminaPct;
            if (staminaPct < settings->emergencyStaminaPct) deficit += 100.0f;
            candidates.push_back({ CombatPotionType::Stamina, deficit });
        }

        // Use invisibility as an emergency escape option only when critically
        // injured; ordinary healing potions remain the first choice.
        if (healthPct <= settings->emergencyHealthPct) {
            candidates.push_back({ CombatPotionType::Invisibility, 99.0f });
        }

        // En acil ihtiyaçtan başlayarak sırala
        std::sort(candidates.begin(), candidates.end(), [](const PotionCandidate& a, const PotionCandidate& b) {
            return a.urgency > b.urgency;
        });

        auto tryDrink = [&](CombatPotionType type, float statPct) -> bool {
            for (const auto& [item, invData] : inventory) {
                if (invData.first <= 0) continue;
                bool matches = (type == CombatPotionType::Health && evaluator->IsHealthPotion(item)) ||
                               (type == CombatPotionType::Magicka && evaluator->IsMagickaPotion(item)) ||
                               (type == CombatPotionType::Stamina && evaluator->IsStaminaPotion(item)) ||
                               (type == CombatPotionType::Invisibility && evaluator->IsInvisibilityPotion(item));
                if (!matches) continue;

                auto potion = item->As<RE::AlchemyItem>();
                if (!potion) continue;

                animMgr->PlayPotionAnimation(follower);
                follower->DrinkPotion(potion, nullptr);
                follower->RemoveItem(potion, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

                _lastPotionTime[actorID] = now;
                _combatDrinkAnimTime[actorID] = now;
                _lastPotionType[actorID] = type;

                const char* potionName = (type == CombatPotionType::Health) ? "Health" :
                                         (type == CombatPotionType::Magicka) ? "Magicka" :
                                         (type == CombatPotionType::Stamina) ? "Stamina" : "Invisibility";
                if ((type == CombatPotionType::Health && settings->notifyCombatHealthPotion) ||
                    (type == CombatPotionType::Magicka && settings->notifyCombatMagickaPotion) ||
                    (type == CombatPotionType::Stamina && settings->notifyCombatStaminaPotion)) {
                    std::string msg = std::format("{} drank a {} Potion! ({}: {:.0f}%)",
                        follower->GetName(), potionName, potionName, statPct);
                    notifMgr->ShowHUDMessage(msg);
                }
                logger::info("CombatPotion: {} used {} Potion at {:.1f}%{}",
                    follower->GetName(), potionName, statPct, emergency ? " [EMERGENCY]" : "");
                return true;
            }
            return false;
        };

        // Adayları aciliyet sırasına göre dene (eğer bir iksir envanterde yoksa sıradaki acil iksire geçer)
        for (const auto& candidate : candidates) {
            float statPct = (candidate.type == CombatPotionType::Health || candidate.type == CombatPotionType::Invisibility) ? healthPct :
                            (candidate.type == CombatPotionType::Magicka) ? magickaPct : staminaPct;
            if (tryDrink(candidate.type, statPct)) return;
        }
    }

    void ConsumableAI::UpdateOutOfCombatPoison(RE::Actor* follower, FollowerProfile& profile)
    {
        // Zehirleme işlemi SADECE savaş dışındayken, takipçi canlıyken ve at sırtında DEĞİLKEN yapılır!
        if (!follower || follower->IsDead() || follower->IsInCombat() || !follower->Is3DLoaded() || follower->IsOnMount()) return;

        auto settings = Settings::GetSingleton();
        if (!settings->enablePoisonSystem) return;

        RE::FormID actorID = follower->GetFormID();
        auto now = std::chrono::steady_clock::now();

        // 1. Savaş bittikten ve yağma işlemi tamamlandıktan 3 saniye sonra zehir basılır
        if (!CombatTracker::GetSingleton()->IsLootSafe() || LootManager::GetSingleton()->HasPendingLootWork()) {
            _lastLootActiveTime = now;
            return;
        }
        if (_lastLootActiveTime.time_since_epoch().count() > 0) {
            float sinceLoot = std::chrono::duration<float>(now - _lastLootActiveTime).count();
            if (sinceLoot < 3.0f) {
                return;
            }
        }

        // Diyalog sırasında animasyon veya mesaj basma
        auto ui = RE::UI::GetSingleton();
        if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) return;

        // 2. Oyun yüklendikten hemen sonra (10 saniye) zehir basılmasını engelle
        if (_gameLoadedTime.time_since_epoch().count() > 0) {
            float sinceLoad = std::chrono::duration<float>(now - _gameLoadedTime).count();
            if (sinceLoad < 10.0f) {
                return;
            }
        }

        // 3. Savaş bittikten en az 3 saniye sonra zehir basılsın
        if (auto it = _lastCombatEndTime.find(actorID); it != _lastCombatEndTime.end()) {
            float sinceCombat = std::chrono::duration<float>(now - it->second).count();
            if (sinceCombat < 3.0f) {
                return;
            }
        }

        // 4. Takipçi zehir satın aldığında veya oyuncu verdiğinde 3 saniye gecikme
        if (auto it = _poisonAcquiredTime.find(actorID); it != _poisonAcquiredTime.end()) {
            float sinceAcquired = std::chrono::duration<float>(now - it->second).count();
            if (sinceAcquired < 3.0f) {
                return;
            }
        }

        // 3. Daha önce zehir animasyonu başlatıldıysa ve 2.5 saniyelik animasyon süresi dolduysa, animasyonu durdur ve silahı kınına al
        if (auto it = _lastPoisonAnimTime.find(actorID); it != _lastPoisonAnimTime.end()) {
            float elapsed = std::chrono::duration<float>(now - it->second).count();
            if (elapsed >= 2.5f) {
                AnimationManager::GetSingleton()->StopPoisonWeaponAnimation(follower);
                AnimationManager::GetSingleton()->SheatheWeapon(follower);
                _lastPoisonAnimTime.erase(it);
            } else {
                // Animasyon hala oynuyor, başka işlem yapma
                return;
            }
        }

        // Takipçinin kuşanmış olduğu birincil silahı kontrol et
        auto invChanges = follower->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList) return;

        RE::InventoryEntryData* targetWeaponEntry = nullptr;
        bool isRanged = false;

        for (auto* entry : *invChanges->entryList) {
            if (!entry || !entry->object) continue;
            auto weap = entry->object->As<RE::TESObjectWEAP>();
            if (!weap) continue;

            // Kuşanılmış mı?
            if (entry->IsWorn()) {
                // Asalar için zehir uygulaması yapma; asa saldırıları zehir yükünü kullanmaz.
                if (weap->IsStaff()) {
                    logger::debug("Poison: {} carries a staff; skipping poison application.", follower->GetName());
                    return;
                }
                // Halihazırda zehirlenmiş mi kontrol et
                if (!entry->IsPoisoned()) {
                    targetWeaponEntry = entry;
                    isRanged = weap->IsRanged();
                    break;
                }
            }
        }

        if (!targetWeaponEntry) return;

        // Envanterden uygun bir zehir bul
        auto inventory = follower->GetInventory();
        auto evaluator = ItemEvaluator::GetSingleton();
        RE::AlchemyItem* chosenPoison = nullptr;

        for (const auto& [item, invData] : inventory) {
            if (invData.first <= 0 || !evaluator->IsPoison(item)) continue;
            const auto poisonType = evaluator->GetPoisonType(item);
            const bool enabled = (poisonType == ItemEvaluator::PoisonType::Health && profile.preferences.buyPoisonHealth) ||
                (poisonType == ItemEvaluator::PoisonType::Magicka && profile.preferences.buyPoisonMagicka) ||
                (poisonType == ItemEvaluator::PoisonType::Stamina && profile.preferences.buyPoisonStamina) ||
                (poisonType == ItemEvaluator::PoisonType::Other && profile.preferences.buyPoisonOther);
            if (!enabled) continue;
            chosenPoison = item->As<RE::AlchemyItem>();
            if (chosenPoison) break;
        }

        if (!chosenPoison) return;

        // Silaha zehri motor düzeyinde uygula (1 doz)
        targetWeaponEntry->PoisonObject(chosenPoison, 1);

        // Envanterden 1 adet zehir şişesini eksilt
        follower->RemoveItem(chosenPoison, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

        // Takipçi savaş dışındayken silah kınında durur. Zehir sürmek için silahı eline al
        follower->DrawWeaponMagicHands(true);

        // Animated Poisons Redone animasyonunu oynat
        AnimationManager::GetSingleton()->PlayPoisonWeaponAnimation(follower, isRanged);

        // Animasyon başlangıç zamanını kaydet (2.5 sn sonraki tick'te SheatheWeapon çağrılacak)
        _lastPoisonAnimTime[actorID] = now;

        logger::info("ConsumableAI: {} applied {} to {} (Out-of-combat)",
            follower->GetName(), chosenPoison->GetName(), targetWeaponEntry->object->GetName());

        if (settings->notifyPoisonApplied) {
            std::string msg = std::format("{} applied {} to their weapon!", follower->GetName(), chosenPoison->GetName());
            NotificationManager::GetSingleton()->ShowHUDMessage(msg);
        }
    }

    void ConsumableAI::NotifyGameLoaded()
    {
        _gameLoadedTime = std::chrono::steady_clock::now();
    }

    void ConsumableAI::NotifyCombatEnded(RE::FormID actorID)
    {
        _lastCombatEndTime[actorID] = std::chrono::steady_clock::now();
    }

    void ConsumableAI::NotifySleepEnded()
    {
        _sleepEndTime = std::chrono::steady_clock::now();
    }

    void ConsumableAI::NotifyWaitEnded()
    {
        _waitEndTime = std::chrono::steady_clock::now();
    }

    void ConsumableAI::NotifyInventoryClosed()
    {
        _inventoryClosedTime = std::chrono::steady_clock::now();
        const auto now = _inventoryClosedTime;
        for (const auto& [id, p] : FollowerManager::GetSingleton()->GetProfilesSnapshot()) {
            _poisonAcquiredTime[id] = now;
        }
    }

    void ConsumableAI::NotifyPoisonAcquired(RE::FormID actorID)
    {
        _poisonAcquiredTime[actorID] = std::chrono::steady_clock::now();
    }

    void ConsumableAI::UpdateNeedsConsumption(RE::Actor* follower, FollowerProfile& profile)
    {
        if (follower && !follower->IsDead() && TryCurePoison(follower)) return;
        // 1. Savaş sırasında ve AT SIRTIINDAYKEN yemek yeme ve içme davranışı YAPILMAMALI
        if (!follower || follower->IsDead() || follower->IsInCombat() || follower->IsOnMount()) return;

        // System toggle
        if (!Settings::GetSingleton()->enableNeedsSystem) return;

        auto player = RE::PlayerCharacter::GetSingleton();
        if (player && player->IsInCombat()) return;

        // 2. Diyalog sırasında Scaleform çökmesi veya animasyon kesintisi olmaması için kontrol:
        // Oyuncu bir NPC ile konuşuyorken veya DialogueMenu açıkken yeme/içme ve HUD mesajı tetiklenmez.
        auto ui = RE::UI::GetSingleton();
        if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) return;

        auto now = std::chrono::steady_clock::now();

        // 2b. Savaştan hemen sonra yemek yenmez: ayarlanan süre (varsayılan 6 sn) beklenir
        if (auto it = _lastCombatEndTime.find(follower->GetFormID()); it != _lastCombatEndTime.end()) {
            float sinceCombat = std::chrono::duration<float>(now - it->second).count();
            if (sinceCombat < Settings::GetSingleton()->postCombatConsumeDelaySeconds) {
                return;
            }
        }

        // 3. Uyuduktan veya bekledikten sonra hemen yememeli: 5 saniye bekleme süresi
        if (_sleepEndTime.time_since_epoch().count() > 0) {
            float elapsedSinceSleep = std::chrono::duration<float>(now - _sleepEndTime).count();
            if (elapsedSinceSleep < 5.0f) {
                return;
            }
        }

        if (_waitEndTime.time_since_epoch().count() > 0) {
            float elapsedSinceWait = std::chrono::duration<float>(now - _waitEndTime).count();
            if (elapsedSinceWait < 5.0f) {
                return;
            }
        }

        // 4. Envanter kapatıldıktan sonra 3 saniye bekleme süresi
        if (_inventoryClosedTime.time_since_epoch().count() > 0) {
            float elapsedSinceInv = std::chrono::duration<float>(now - _inventoryClosedTime).count();
            if (elapsedSinceInv < 3.0f) {
                return;
            }
        }

        auto settings  = Settings::GetSingleton();
        RE::FormID actorID = follower->GetFormID();

        // ── SIRALI YEME/İÇME KİLİDİ ──
        // Çoklu takipçide animasyon hatasını önlemek için aynı anda yalnızca BİR takipçi
        // yer/içer. 1. takipçinin yeme-içme seansı tamamen bitmeden 2. takipçi başlamaz.
        if (_activeConsumer != 0) {
            // Tıkanma koruması: aktif takipçi 30 saniyedir hiçbir işlem yapmadıysa kilidi bırak
            if (_activeLastActivity.time_since_epoch().count() > 0 &&
                std::chrono::duration<float>(now - _activeLastActivity).count() > 30.0f) {
                _activeConsumer = 0;
            } else if (_activeConsumer != actorID) {
                return; // Başka bir takipçi şu an yiyor/içiyor, sıranı bekle
            }
        }

        // Aynı takipçi için tek seferde tek işlem: iki lokma/yudum arasında
        // ConsumeIntervalSeconds (varsayılan 10 sn) beklenir — animasyon rahatça
        // biter, birbirini kesen kısa yeme/içme animasyonları olmaz.
        if (auto it = _lastConsumeTime.find(actorID); it != _lastConsumeTime.end()) {
            float elapsedSec = std::chrono::duration<float>(now - it->second).count();
            float interval = std::max(settings->consumeDelaySeconds, settings->consumeIntervalSeconds);
            if (elapsedSec < interval) {
                return; // Animasyonun bitmesini bekle
            }
        }

        // ── HİSTEREZİS (SEANS) YÖNETİMİ ──
        // Takipçi sadece susuzluk/açlık eşik değerinin altına düştüğünde tüketmeye başlar.
        // Başladıktan sonra ise %100 olana kadar tüketmeye devam eder.
        if (profile.needs.thirst <= settings->drinkThreshold) {
            _isDrinkingSession.insert(actorID);
        } else if (profile.needs.thirst >= 100.0f) {
            _isDrinkingSession.erase(actorID);
        }

        if (profile.needs.hunger <= settings->eatThreshold) {
            _isEatingSession.insert(actorID);
        } else if (profile.needs.hunger >= 100.0f) {
            _isEatingSession.erase(actorID);
        }

        bool wantsDrink = _isDrinkingSession.contains(actorID) && profile.needs.thirst < 100.0f;
        bool wantsFood  = _isEatingSession.contains(actorID) && profile.needs.hunger < 100.0f;

        if (!wantsDrink && !wantsFood) {
            // Seans tamamlandı (%100) → kilidi bırak, sıradaki takipçi başlayabilir
            if (_activeConsumer == actorID) {
                _activeConsumer = 0;
            }
            return;
        }

        // SADECE ve SADECE yemek yiyecek veya içecek olan BU takipçinin elinde silah varsa:
        // Silahı zorla kınına al ve animasyon durumunu hemen güncelle.
        // Diğer takipçilerin silahına asla dokunulmaz.
        if (auto* state = follower->AsActorState(); state && state->IsWeaponDrawn()) {
            // Engine'e kın alma komutunu gönder
            follower->DrawWeaponMagicHands(false);
            follower->NotifyAnimationGraph("weaponSheathe");
            follower->NotifyAnimationGraph("BeginWeaponSheathe");
            follower->NotifyAnimationGraph("WeaponSheathe");
            // Devam et — silah animasyonu arka planda tamamlanır, yeme/içme başlayabilir
        }

        auto animMgr   = AnimationManager::GetSingleton();
        auto notifMgr  = NotificationManager::GetSingleton();
        auto inventory = follower->GetInventory();
        auto evaluator = ItemEvaluator::GetSingleton();

        // Kilidi al ve aktiviteyi işaretle
        _activeConsumer = actorID;
        _activeLastActivity = now;

        // Lambda: İçecek tüketmeyi dene
        auto tryConsumeDrink = [&]() -> bool {
            for (const auto& [item, invData] : inventory) {
                if (invData.first > 0 && evaluator->IsDrink(item)) {
                    auto drink = item->As<RE::AlchemyItem>();
                    if (drink) {
                        if (!profile.isAnimal) {
                            animMgr->PlayDrinkAnimation(follower, drink);
                        }
                        follower->DrinkPotion(drink, nullptr);
                        follower->RemoveItem(drink, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

                        profile.needs.thirst = std::min(100.0f, profile.needs.thirst + 55.0f);
                        if (evaluator->IsAlcohol(item)) {
                            profile.needs.alcohol += 20.0f;
                        }

                        if (profile.needs.thirst >= 100.0f) {
                            _isDrinkingSession.erase(actorID);
                        }

                        _lastConsumeTime[actorID] = now;

                        if (settings->notifyNeedsDrink) {
                            notifMgr->ShowHUDMessage(
                                std::format("{} consumed {}. (Thirst: {:.0f}%)", follower->GetName(), drink->GetName(), profile.needs.thirst));
                        }
                        logger::info("ConsumableAI: {} drank {} (Thirst now: {:.1f}%)", follower->GetName(), drink->GetName(), profile.needs.thirst);
                        return true;
                    }
                }
            }
            return false;
        };

        // Lambda: Yemek tüketmeyi dene
        auto tryConsumeFood = [&]() -> bool {
            for (const auto& [item, invData] : inventory) {
                if (invData.first > 0 && evaluator->IsFood(item) && !evaluator->IsDrink(item)) {
                    // İnsanlar çiğ et yemez, ancak hayvanlar/evcil hayvanlar çiğ et yer
                    if (!profile.isAnimal && evaluator->IsRawMeat(item)) continue;
                    auto food = item->As<RE::AlchemyItem>();
                    if (food) {
                        if (!profile.isAnimal) {
                            animMgr->PlayEatAnimation(follower);
                        }
                        follower->DrinkPotion(food, nullptr);
                        follower->RemoveItem(food, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

                        float restoreAmount = 50.0f;
                        if (evaluator->IsFoodLarge(food)) {
                            restoreAmount = 70.0f;
                        } else if (evaluator->IsFoodSmall(food)) {
                            restoreAmount = 30.0f;
                        }

                        profile.needs.hunger = std::min(100.0f, profile.needs.hunger + restoreAmount);

                        // Çorba ve güveç hem açlık hem susuzluğu giderir
                        if (evaluator->IsSoupOrStew(item)) {
                            profile.needs.thirst = std::min(100.0f, profile.needs.thirst + 30.0f);
                            if (profile.needs.thirst >= 100.0f) {
                                _isDrinkingSession.erase(actorID);
                            }
                        }

                        if (profile.needs.hunger >= 100.0f) {
                            _isEatingSession.erase(actorID);
                        }

                        _lastConsumeTime[actorID] = now;

                        if (settings->notifyNeedsFood) {
                            notifMgr->ShowHUDMessage(
                                std::format("{} consumed {}. (Hunger: {:.0f}%)", follower->GetName(), food->GetName(), profile.needs.hunger));
                        }
                        logger::info("ConsumableAI: {} ate {} (Hunger now: {:.1f}%)", follower->GetName(), food->GetName(), profile.needs.hunger);
                        return true;
                    }
                }
            }
            return false;
        };

        // ── HER ZAMAN İÇECEK ÖNCELİĞİ ──
        // Takipçi önce susuzluğunu giderir (içecek bitene / %100 olana kadar),
        // içecek kalmadıysa yemek yer. Tüketim seansı %100 olana kadar sürer.
        bool consumedSomething = false;
        if (wantsDrink && tryConsumeDrink()) {
            consumedSomething = true;
        } else if (wantsFood && tryConsumeFood()) {
            consumedSomething = true;
        }

        if (consumedSomething) {
            return;
        }

        // Hiçbir şey tüketilemedi (envanterde içecek/yemek yok):
        // kilidi bırak ki sıradaki takipçi işlemeye başlayabilsin.
        _activeConsumer = 0;

        // Eğer takipçi içmek istiyor ama envanterinde içecek yoksa:
        // Sol üst köşede bildirim göster (60 saniyede en fazla 1 kez)
        if (wantsDrink) {
            bool canAlertDrink = true;
            if (auto it = _lastMissingDrinkAlertTime.find(actorID); it != _lastMissingDrinkAlertTime.end()) {
                if (std::chrono::duration<float>(now - it->second).count() < 60.0f) {
                    canAlertDrink = false;
                }
            }
            if (canAlertDrink && settings->notifyNeedsThirstyWarning) {
                _lastMissingDrinkAlertTime[actorID] = now;
                std::string msg = std::format("{} is thirsty, but has nothing to drink!", follower->GetName());
                notifMgr->ShowHUDMessage(msg);
                logger::info("ConsumableAI: {} wants to drink but has no drinks in inventory!", follower->GetName());
            }
        }

        // Eğer takipçi yemek istiyor ama envanterinde yiyecek yoksa:
        // Sol üst köşede bildirim göster (60 saniyede en fazla 1 kez)
        if (wantsFood) {
            bool canAlertFood = true;
            if (auto it = _lastMissingFoodAlertTime.find(actorID); it != _lastMissingFoodAlertTime.end()) {
                if (std::chrono::duration<float>(now - it->second).count() < 60.0f) {
                    canAlertFood = false;
                }
            }
            if (canAlertFood && settings->notifyNeedsHungryWarning) {
                _lastMissingFoodAlertTime[actorID] = now;
                std::string msg = std::format("{} is hungry, but has nothing to eat!", follower->GetName());
                notifMgr->ShowHUDMessage(msg);
                logger::info("ConsumableAI: {} wants to eat but has no food in inventory!", follower->GetName());
            }
        }
    }

    void ConsumableAI::ApplyNeedsDebuffs(RE::Actor* follower, FollowerProfile& profile)
    {
        if (!follower || follower->IsDead()) return;

        auto avOwner = follower->AsActorValueOwner();
        if (!avOwner) return;

        float hunger  = profile.needs.hunger;
        float thirst  = profile.needs.thirst;
        float fatigue = profile.needs.fatigue;

        // ÖNEMLİ: Bu fonksiyon asla SetActorValue ile KALICI değer yazmamalıdır.
        // Eski kod SetActorValue(kSpeedMult/kCarryWeight) kullanıyordu; her çağrıda
        // takipçinin taban hızı aşağıya "ratchet"lendiği için takipçiler oyun
        // yeniden başlatılana kadar sürünerek yürüyordu. Artık yalnızca GEÇİCİ
        // damage modifier (RestoreActorValue) kullanılır.

        // ── AÇLIK (HUNGER) ──
        if (hunger < 15.0f) {
            float cur = avOwner->GetActorValue(RE::ActorValue::kStamina);
            if (cur > 1.0f) avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, -cur * 0.005f);
            // Taşıma kapasitesi: geçici -20 (kalıcı yazım yok)
            float baseCarry = avOwner->GetBaseActorValue(RE::ActorValue::kCarryWeight);
            float curCarry  = avOwner->GetActorValue(RE::ActorValue::kCarryWeight);
            if (curCarry > baseCarry - 20.0f)
                avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, -20.0f);
        } else if (hunger < 30.0f) {
            float cur = avOwner->GetActorValue(RE::ActorValue::kStamina);
            if (cur > 1.0f) avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, -cur * 0.002f);
        } else {
            // Açlık cezası kalktı: geçici taşıma cezasını geri yükle
            avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kCarryWeight, 20.0f);
        }

        // ── SUSUZLUK (THIRST) ──
        if (thirst < 15.0f) {
            float curHp = avOwner->GetActorValue(RE::ActorValue::kHealth);
            float curMp = avOwner->GetActorValue(RE::ActorValue::kMagicka);
            if (curHp > 1.0f) avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -curHp * 0.003f);
            if (curMp > 1.0f) avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka, -curMp * 0.003f);
        } else if (thirst < 30.0f) {
            float curHp = avOwner->GetActorValue(RE::ActorValue::kHealth);
            if (curHp > 1.0f) avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, -curHp * 0.001f);
        }

        // ── YORGUNLUK (FATIGUE) ──
        // Hız cezası tamamen kaldırıldı: yorgunluk artık yalnızca stamina hasarı ile
        // etkiler. (Eski SetActorValue(kSpeedMult) kodu takipçilerin kalıcı olarak
        // aşırı yavaşlamasına yol açan hataydı.)
        if (fatigue < 15.0f) {
            float cur = avOwner->GetActorValue(RE::ActorValue::kStamina);
            if (cur > 1.0f) avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, -cur * 0.004f);
        } else if (fatigue < 30.0f) {
            float cur = avOwner->GetActorValue(RE::ActorValue::kStamina);
            if (cur > 1.0f) avOwner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, -cur * 0.001f);
        }
    }
}

