Scriptname FU_NPCPromptQuestScript extends Quest
{Follower Unbound NPC interaction prompts for SkyPrompt 2.x / Skyrim SE 1.5.97.
 Requires SkyPrompt, SKSE64 and powerofthree's Papyrus Extender.}

Quest Property DialogueFollower Auto
Faction Property PotentialFollowerFaction Auto
Faction Property CurrentFollowerFaction Auto
GlobalVariable Property PlayerFollowerCount Auto
Keyword Property ActorTypeNPC Auto
Outfit Property EmptyTributeOutfit Auto

Int _clientID = 0
Actor _focusedNPC = None
Bool _combatPromptMode = False
Bool _followerPromptMode = False
Bool _promptsHiddenForMenu = False
; SMF menu addon writes StorageUtilData/FU_NPC_Interactions.json.
; Poll only every 1.6 seconds, never for each crosshair event.
Bool _featureEnabled = True
Bool _restoreOnEnable = False
Float _nextFeatureCheckAt = 0.0
; Retained solely to release actors restrained by v0.5.1 on loaded saves.
Actor[] _pacifiedActors = None
Bool _legacyRestraintsReleased = False

Event OnInit()
    Arm()
EndEvent

Function Arm()
    ReleaseLegacyRestraints()
    RegisterForInteractionMenus()
    RegisterForCrosshairRef()
    Int id = SkyPrompt.RegisterForSkyPromptEvent(Self as Form, 2, 0)
    If id <= 0
        _clientID = 0
        Debug.Trace("[FU_NPC] SkyPrompt client registration failed; will retry.")
        RegisterForSingleUpdate(5.0)
        Return
    EndIf
    If _clientID > 0
        ClearCurrent()
    EndIf
    _clientID = id
    SkyPrompt.RequestTheme(_clientID, "FU_NPC_Diamond")
    SyncFeatureEnabled(True)
    RegisterForSingleUpdate(0.8)
    Debug.Trace("[FU_NPC] Registered SkyPrompt client " + _clientID)
EndFunction


; Only watch the UI that can overlap NPC interaction prompts.
; Re-register on save load via Arm(); no menu-close redraw handlers.
Function SyncFeatureEnabled(Bool force = False)
    Float now = Utility.GetCurrentRealTime()
    If !force && now < _nextFeatureCheckAt
        Return
    EndIf
    _nextFeatureCheckAt = now + 1.6
    ; JsonUtil keeps external JSON cached. Discard it without saving so
    ; changes made by the native SMF page are visible to Papyrus.
    JsonUtil.Unload("FU_NPC_Interactions", False)
    Bool shouldEnable = JsonUtil.GetPathIntValue("FU_NPC_Interactions", ".Enabled", 1) != 0
    If shouldEnable != _featureEnabled
        _featureEnabled = shouldEnable
        If !_featureEnabled
            ; Remove active prompts exactly once on disable.
            ClearCurrent()
            _promptsHiddenForMenu = False
            _restoreOnEnable = False
        Else
            ; Reacquire target once, not every OnUpdate or crosshair cycle.
            _promptsHiddenForMenu = False
            _restoreOnEnable = True
        EndIf
    EndIf
EndFunction

Function RegisterForInteractionMenus()
    RegisterForMenu("Dialogue Menu")
    RegisterForMenu("BarterMenu")
    RegisterForMenu("ContainerMenu")
    RegisterForMenu("GiftMenu")
EndFunction

Bool Function IsInteractionMenuOpen()
    Return UI.IsMenuOpen("Dialogue Menu") || UI.IsMenuOpen("BarterMenu") || UI.IsMenuOpen("ContainerMenu") || UI.IsMenuOpen("GiftMenu")
EndFunction

Function HideForInteractionMenu()
    If _promptsHiddenForMenu
        Return
    EndIf
    _promptsHiddenForMenu = True
    ; Remove exactly once when the menu first opens. Keep the target so
    ; the periodic mode check can recover after the menu is fully closed.
    If _focusedNPC != None
        ClearPrompts()
    EndIf
EndFunction

Event OnMenuOpen(String menuName)
    If !_featureEnabled
        Return
    EndIf
    If menuName == "Dialogue Menu" || menuName == "BarterMenu" || menuName == "ContainerMenu" || menuName == "GiftMenu"
        HideForInteractionMenu()
    EndIf
EndEvent

Event OnUpdate()
    ReleaseLegacyRestraints()
    If _clientID <= 0
        Arm()
        Return
    EndIf
    SyncFeatureEnabled()
    If !_featureEnabled
        RegisterForSingleUpdate(0.8)
        Return
    EndIf
    If _restoreOnEnable && !IsInteractionMenuOpen()
        _restoreOnEnable = False
        Actor currentTarget = Game.GetCurrentCrosshairRef() as Actor
        If IsValidNPC(currentTarget)
            _focusedNPC = currentTarget
            DrawPrompts(currentTarget, True)
        EndIf
    EndIf
    If IsInteractionMenuOpen()
        ; Fallback if a menu-open notification was missed.
        HideForInteractionMenu()
    ElseIf _promptsHiddenForMenu
        ; Resume only on the regular 0.8s update, never during OnMenuClose.
        ; This prevents the forced close/reopen redraw loop of v0.5.5.
        _promptsHiddenForMenu = False
        If _focusedNPC != None
            If IsValidNPC(_focusedNPC)
                DrawPrompts(_focusedNPC, True)
            Else
                ; Already removed when the menu opened.
                _focusedNPC = None
                _combatPromptMode = False
                _followerPromptMode = False
            EndIf
        EndIf
    ElseIf _focusedNPC != None
        If !IsValidNPC(_focusedNPC)
            ClearCurrent()
        ElseIf _combatPromptMode != IsCombatEnemy(_focusedNPC) || _followerPromptMode != IsFollowerTarget(_focusedNPC)
            DrawPrompts(_focusedNPC)
        EndIf
    EndIf
    RegisterForSingleUpdate(0.8)
EndEvent

Event OnCrosshairRefChange(ObjectReference akRef)
    If !_featureEnabled
        Return
    EndIf
    If _promptsHiddenForMenu
        If IsInteractionMenuOpen()
            Return
        EndIf
        ; A fresh crosshair event after menu close supersedes the saved target.
        ; The old prompt IDs were already removed on menu open.
        _promptsHiddenForMenu = False
        _focusedNPC = None
        _combatPromptMode = False
        _followerPromptMode = False
    EndIf
    Actor npc = akRef as Actor
    If npc == _focusedNPC
        Return
    EndIf

    ; Skip empty-queue cleanup when coming from no selected NPC.
    ; On a real target switch ClearCurrent() removes only the old mode's IDs.
    If _focusedNPC != None
        ClearCurrent()
    EndIf
    If IsValidNPC(npc)
        _focusedNPC = npc
        ; Do not clear twice: old prompts are already gone.
        ; Avoid expensive follower-status Debug.Trace before SendPrompt.
        DrawPrompts(npc, True)
    EndIf
EndEvent

Bool Function IsValidNPC(Actor npc)
    If npc == None
        Return False
    EndIf
    If npc == Game.GetPlayer() || npc.IsDead() || npc.IsDisabled()
        Return False
    EndIf
    If ActorTypeNPC == None || !npc.HasKeyword(ActorTypeNPC)
        Return False
    EndIf
    If Game.GetPlayer().GetDistance(npc) > 260.0
        Return False
    EndIf
    Return True
EndFunction

Bool Function IsCombatEnemy(Actor npc)
    If !IsValidNPC(npc)
        Return False
    EndIf
    Return npc.IsInCombat() && npc.IsHostileToActor(Game.GetPlayer())
EndFunction

Bool Function IsActiveVanillaFollower(Actor npc)
    If npc == None
        Return False
    EndIf
    ; The vanilla alias is authoritative, even if a framework has cleared
    ; IsPlayerTeammate on an otherwise valid follower.
    DialogueFollowerScript followerScript = DialogueFollower as DialogueFollowerScript
    If followerScript == None || followerScript.pFollowerAlias == None
        Return False
    EndIf
    Return followerScript.pFollowerAlias.GetActorRef() == npc
EndFunction

Faction Function GetCurrentFollowerFaction()
    ; Runtime fallback is essential on existing saves: newly added quest VMAD
    ; properties do not always initialize on already saved script instances.
    If CurrentFollowerFaction == None
        CurrentFollowerFaction = Game.GetFormFromFile(0x0005C84E, "Skyrim.esm") as Faction
    EndIf
    Return CurrentFollowerFaction
EndFunction

Bool Function IsFollowerTarget(Actor npc)
    If !IsValidNPC(npc) || npc.IsHostileToActor(Game.GetPlayer())
        Return False
    EndIf
    If IsActiveVanillaFollower(npc)
        Return True
    EndIf
    Faction current = GetCurrentFollowerFaction()
    If current != None && npc.IsInFaction(current)
        ; NFF can keep a dismissed NPC in the faction at rank -1.
        Return npc.GetFactionRank(current) >= 0
    EndIf
    ; A fallback teammate must also be a genuine follower candidate.
    ; Betrayed enemies / pacified tribute NPCs can be player teammates too.
    Return npc.IsPlayerTeammate() && PotentialFollowerFaction != None && npc.IsInFaction(PotentialFollowerFaction)
EndFunction

Function DrawPrompts(Actor npc, Bool abAlreadyCleared = False)
    If !_featureEnabled || _clientID <= 0 || !IsValidNPC(npc) || _promptsHiddenForMenu
        Return
    EndIf
    ; Periodic follower/combat mode switches still clear the old prompts.
    ; Crosshair acquisitions have already handled removal.
    If !abAlreadyCleared
        ClearPrompts()
    EndIf
    Int[] devices = new Int[1]
    Int[] keys = new Int[1]
    devices[0] = 0 ; keyboard
    _combatPromptMode = IsCombatEnemy(npc)
    _followerPromptMode = False
    If _combatPromptMode
        ; Three combat options. F1=Betray Allies, F2=Surrender, F3=Close.
        ; SkyPrompt diamond positions: bottom, right, left.
        keys[0] = 61 ; F3
        SkyPrompt.SendPrompt(_clientID, "Close", 203, 0, 0, npc as Form, devices, keys, 0.0)
        keys[0] = 60 ; F2
        SkyPrompt.SendPrompt(_clientID, "Surrender", 202, 0, 0, npc as Form, devices, keys, 0.0)
        keys[0] = 59 ; F1
        SkyPrompt.SendPrompt(_clientID, "Betray Allies", 201, 0, 0, npc as Form, devices, keys, 0.0)
    ElseIf IsFollowerTarget(npc)
        _followerPromptMode = True
        ; Vanilla follower commands. Keep the same diamond/F1-F4 layout.
        ; F3 Dismiss is a hold to avoid accidentally dismissing your follower.
        keys[0] = 61 ; F3
        SkyPrompt.SendPrompt(_clientID, "Dismiss", 303, 0, 1, npc as Form, devices, keys, 0.0)
        keys[0] = 62 ; F4
        SkyPrompt.SendPrompt(_clientID, "Follow Me", 304, 0, 0, npc as Form, devices, keys, 0.0)
        keys[0] = 60 ; F2
        SkyPrompt.SendPrompt(_clientID, "Exchange Items", 302, 0, 0, npc as Form, devices, keys, 0.0)
        keys[0] = 59 ; F1
        SkyPrompt.SendPrompt(_clientID, "Wait", 301, 0, 0, npc as Form, devices, keys, 0.0)
    Else
        ; Normal four-direction prompts: F1 Close, F2 Trade, F3 Fight, F4 Follow.
        keys[0] = 61 ; F3
        SkyPrompt.SendPrompt(_clientID, "Start Fight", 100, 0, 1, npc as Form, devices, keys, 0.0)
        keys[0] = 62 ; F4
        SkyPrompt.SendPrompt(_clientID, "Follow Me", 101, 0, 0, npc as Form, devices, keys, 0.0)
        keys[0] = 60 ; F2
        SkyPrompt.SendPrompt(_clientID, "Trade", 102, 0, 0, npc as Form, devices, keys, 0.0)
        keys[0] = 59 ; F1
        SkyPrompt.SendPrompt(_clientID, "Close", 103, 0, 0, npc as Form, devices, keys, 0.0)
    EndIf
EndFunction

Function ClearPrompts()
    If _clientID <= 0
        Return
    EndIf
    ; Normal removal path: only the IDs belonging to the active layout.
    ; v0.4 removed 4 IDs rather than all 11 on each crosshair change.
    If _focusedNPC != None
        If _combatPromptMode
            SkyPrompt.RemovePrompt(_clientID, 201, 0)
            SkyPrompt.RemovePrompt(_clientID, 202, 0)
            SkyPrompt.RemovePrompt(_clientID, 203, 0)
        ElseIf _followerPromptMode
            SkyPrompt.RemovePrompt(_clientID, 301, 0)
            SkyPrompt.RemovePrompt(_clientID, 302, 0)
            SkyPrompt.RemovePrompt(_clientID, 303, 0)
            SkyPrompt.RemovePrompt(_clientID, 304, 0)
        Else
            SkyPrompt.RemovePrompt(_clientID, 100, 0)
            SkyPrompt.RemovePrompt(_clientID, 101, 0)
            SkyPrompt.RemovePrompt(_clientID, 102, 0)
            SkyPrompt.RemovePrompt(_clientID, 103, 0)
        EndIf
    Else
        ; Safe fallback for unknown/stale prompt state on re-registration.
        SkyPrompt.RemovePrompt(_clientID, 100, 0)
        SkyPrompt.RemovePrompt(_clientID, 101, 0)
        SkyPrompt.RemovePrompt(_clientID, 102, 0)
        SkyPrompt.RemovePrompt(_clientID, 103, 0)
        SkyPrompt.RemovePrompt(_clientID, 201, 0)
        SkyPrompt.RemovePrompt(_clientID, 202, 0)
        SkyPrompt.RemovePrompt(_clientID, 203, 0)
        SkyPrompt.RemovePrompt(_clientID, 301, 0)
        SkyPrompt.RemovePrompt(_clientID, 302, 0)
        SkyPrompt.RemovePrompt(_clientID, 303, 0)
        SkyPrompt.RemovePrompt(_clientID, 304, 0)
    EndIf
EndFunction

Function ClearCurrent()
    ClearPrompts()
    _focusedNPC = None
    _combatPromptMode = False
    _followerPromptMode = False
EndFunction

Event OnSkyPromptEvent(Int clientID, Int eventType, Int eventID, Int actionID, Float deltaX, Float deltaY, Float progress)
    If !_featureEnabled || clientID != _clientID || eventType != 0 || actionID != 0 || _promptsHiddenForMenu
        Return
    EndIf
    Actor npc = _focusedNPC
    If !IsValidNPC(npc)
        ClearCurrent()
        Return
    EndIf
    Bool enemyInCombat = IsCombatEnemy(npc)
    ClearCurrent()
    ; Both Close buttons are pure dismiss actions, without activation,
    ; combat roll, inventory transfer or other NPC state changes.
    If eventID == 103 || eventID == 203
        Return
    EndIf
    If eventID == 201 || eventID == 202
        If enemyInCombat
            TryCombatAction(npc, eventID)
        EndIf
        Return
    EndIf
    ; Do not permit the peaceful commands to trigger during an enemy encounter.
    If enemyInCombat
        Return
    EndIf
    If eventID >= 301 && eventID <= 304
        ; Do not let stale buttons issue commands to an unrelated NPC.
        If IsFollowerTarget(npc)
            TryFollowerCommand(npc, eventID)
        EndIf
        Return
    EndIf
    If eventID == 102
        TryTrade(npc)
    ElseIf eventID == 100
        TryFight(npc)
    ElseIf eventID == 101
        TryFollow(npc)
    EndIf
EndEvent

; Success chances depend on the target's HEALTH percentage, not the player's.
; Betray 5-65%, surrender 10-80%. Close has no combat roll.
Bool Function RollCombatAction(Actor enemy, Int eventID)
    Float hp = enemy.GetActorValuePercentage("Health")
    If hp < 0.0
        hp = 0.0
    ElseIf hp > 1.0
        hp = 1.0
    EndIf
    Float injury = 1.0 - hp
    Int chance = 0
    If eventID == 201
        chance = 5 + ((60.0 * injury) as Int)
    ElseIf eventID == 202
        chance = 10 + ((70.0 * injury) as Int)
    EndIf
    Return Utility.RandomInt(1, 100) <= chance
EndFunction

Function ReleaseLegacyRestraints()
    If _legacyRestraintsReleased
        Return
    EndIf
    _legacyRestraintsReleased = True
    If _pacifiedActors == None
        Return
    EndIf
    ; A one-time v0.5.1 save migration: unfreeze only actors this quest restrained.
    ; Original aggression/teammate state was not saved by v0.5.1, so a pre-fix
    ; save remains safest for a clean test.
    Int index = 0
    While index < _pacifiedActors.Length
        Actor affected = _pacifiedActors[index]
        If affected != None && !affected.IsDead()
            affected.SetRestrained(False)
            affected.EvaluatePackage()
        EndIf
        _pacifiedActors[index] = None
        index += 1
    EndWhile
EndFunction

Function StartCowardFlee(Actor npc)
    ; Keep the enemy hostile and combat active, but let the engine decide when
    ; its existing package can be reevaluated naturally.
    ; A crash report for Surrender showed IPackageData::LoadPackage in the
    ; FollowTo procedure on a dynamically spawned Stormcloak Soldier.
    ; Forcing ResetAI/EvaluatePackage on this actor may invalidate its package.
    ; Do not force a package reset here or call StopCombat/SetPlayerTeammate.
    npc.SetRestrained(False)
    If npc.GetActorValue("Aggression") < 1.0
        npc.SetActorValue("Aggression", 1.0)
    EndIf
    npc.SetActorValue("Confidence", 0.0) ; Cowardly, normal AI updates only
EndFunction

Function TryCombatAction(Actor npc, Int eventID)
    If !IsCombatEnemy(npc)
        Debug.Notification("Only hostile humanoid NPCs in combat can respond.")
        Return
    EndIf
    If !RollCombatAction(npc, eventID)
        Debug.Notification("The enemy refuses and continues fighting.")
        Return
    EndIf
    If eventID == 201
        ; Experimental defection. This does not grant a follower alias or AI package.
        npc.SetPlayerTeammate(True, False)
        npc.StopCombatAlarm()
        npc.StopCombat()
        Debug.Notification("Enemy has switched to your side.")
    ElseIf eventID == 202
        ; Surrender now triggers Skyrim's Cowardly flee behavior; armor remains worn.
        ; We leave the encounter active so the actor can actually run away.
        StartCowardFlee(npc)
        Debug.Notification("Enemy morale broken; may flee.")
    EndIf
EndFunction

Function TryFollowerCommand(Actor npc, Int eventID)
    If !IsFollowerTarget(npc)
        Debug.Notification("This NPC is not your active follower.")
        Return
    EndIf
    If npc.IsInCombat()
        Debug.Notification("Follower commands are unavailable during combat.")
        Return
    EndIf
    ; Inventory transfer remains unchanged.
    If eventID == 302
        npc.OpenInventory(True)
        Return
    EndIf
    ; The user's NFF-managed follower may no longer occupy the vanilla
    ; DialogueFollower alias. Ask the actual NFF controller to dismiss it.
    ; This check runs ONLY for Dismiss, never for Wait/Trade/Follow.
    If eventID == 303
        Faction current = GetCurrentFollowerFaction()
        If current != None && npc.IsInFaction(current) && npc.GetFactionRank(current) >= 0
            If Game.GetFormFromFile(0x0000434F, "nwsFollowerFramework.esp") != None
                If FU_NFFCompat.DismissFollower(npc)
                    Debug.Notification("Dismiss request sent to NFF.")
                Else
                    Debug.Notification("NFF could not dismiss this follower.")
                EndIf
                Return
            EndIf
        EndIf
    EndIf
    DialogueFollowerScript followerScript = DialogueFollower as DialogueFollowerScript
    If IsActiveVanillaFollower(npc) && followerScript != None
        ; Only issue vanilla commands when THIS NPC occupies its alias.
        If eventID == 301
            If npc.GetActorValue("WaitingForPlayer") > 0.0
                Debug.Notification("Your follower is already waiting.")
            Else
                followerScript.FollowerWait()
                Debug.Notification("Your follower will wait here.")
            EndIf
        ElseIf eventID == 303
            followerScript.DismissFollower(0, 0)
        ElseIf eventID == 304
            TryFollow(npc)
        EndIf
        Return
    EndIf
    ; External management such as NFF: avoid hijacking someone else's alias.
    If eventID == 301
        npc.SetActorValue("WaitingForPlayer", 1.0)
        npc.EvaluatePackage()
        Debug.Notification("Wait requested; follower framework may override it.")
    ElseIf eventID == 304
        npc.SetActorValue("WaitingForPlayer", 0.0)
        npc.EvaluatePackage()
        Debug.Notification("Follow requested; follower framework may override it.")
    ElseIf eventID == 303
        Debug.Notification("Dismiss requires the follower's owning framework.")
        Debug.Trace("[FU_NPC v0.5.4.1] No safe follower controller found.")
    EndIf
EndFunction

Function TryTrade(Actor npc)
    If npc.IsInCombat() || npc.IsHostileToActor(Game.GetPlayer())
        Debug.Notification("Trading is not possible right now.")
        Return
    EndIf
    If PO3_SKSEFunctions.GetOffersServices(npc) && PO3_SKSEFunctions.GetVendorFaction(npc) != None
        npc.ShowBarterMenu()
    Else
        Debug.Notification("This NPC does not offer trading.")
    EndIf
EndFunction

Function TryFight(Actor npc)
    If npc.IsPlayerTeammate()
        Debug.Notification("You cannot fight your current follower.")
        Return
    EndIf
    If npc.IsInCombat()
        Debug.Notification("This NPC is already in combat.")
        Return
    EndIf
    npc.StartCombat(Game.GetPlayer())
EndFunction

Function TryFollow(Actor npc)
    Actor player = Game.GetPlayer()
    If npc.IsInCombat() || npc.IsHostileToActor(player)
        Debug.Notification("This NPC refuses to follow you.")
        Return
    EndIf
    DialogueFollowerScript followerScript = DialogueFollower as DialogueFollowerScript
    If followerScript == None
        Debug.Notification("Vanilla follower system is unavailable.")
        Return
    EndIf
    If followerScript.pFollowerAlias == None
        Debug.Notification("Vanilla follower slot is unavailable.")
        Return
    EndIf
    Actor currentVanillaFollower = followerScript.pFollowerAlias.GetActorRef()
    If currentVanillaFollower == npc
        If npc.GetActorValue("WaitingForPlayer") > 0.0
            followerScript.FollowerFollow()
            Debug.Notification("Your follower is following you again.")
        Else
            Debug.Notification("This NPC is already following you.")
        EndIf
        Return
    EndIf
    ; Keep external follower frameworks in charge of their own teammates.
    If npc.IsPlayerTeammate()
        Debug.Notification("Use this follower's dialogue commands.")
        Return
    EndIf
    If currentVanillaFollower != None
        Debug.Notification("Dismiss your current vanilla follower first.")
        Return
    EndIf
    If PlayerFollowerCount == None
        Debug.Notification("Vanilla follower count is unavailable.")
        Return
    EndIf
    If PlayerFollowerCount.GetValueInt() > 0
        Debug.Notification("Dismiss your current follower before recruiting another.")
        Return
    EndIf
    ; An NPC is not recruitable just because the player can interact with them.
    If PotentialFollowerFaction == None || !npc.IsInFaction(PotentialFollowerFaction)
        Debug.Notification("This NPC refuses to follow you.")
        Return
    EndIf
    ; Require ally disposition. This protects most unpaid hirelings and locked recruits.
    ; A generic Papyrus check cannot prove every custom quest/fee condition.
    If npc.GetRelationshipRank(player) < 3
        Debug.Notification("This NPC is not ready to join you.")
        Return
    EndIf
    ; Standard vanilla DialogueFollowerScript function sets the follower alias,
    ; player teammate flag and follower-count global without opening dialogue.
    followerScript.SetFollower(npc)
    If followerScript.pFollowerAlias.GetActorRef() == npc && npc.IsPlayerTeammate()
        Debug.Notification("Follower recruited.")
    Else
        Debug.Notification("Follow request could not be completed.")
    EndIf
EndFunction
