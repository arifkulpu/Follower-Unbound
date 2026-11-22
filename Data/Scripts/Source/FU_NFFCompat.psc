Scriptname FU_NFFCompat Hidden
{Optional Nether's Follower Framework dismissal bridge.
 The main FU_NPCPromptQuestScript has no direct NFF script class dependency.
 Do NOT ship any original NFF Papyrus script or overwrite its PEX.}

Bool Function DismissFollower(Actor npc) Global
    If npc == None || npc.IsDead()
        Return False
    EndIf
    ; Validated NFF controller quest local FormID (0x434F).
    Quest controllerQuest = Game.GetFormFromFile(0x0000434F, "nwsFollowerFramework.esp") as Quest
    If controllerQuest == None
        Return False
    EndIf
    ; Imported custom companions belong to their own follower framework.
    ; NFF's "Export" action is different from removing a normal follower.
    Faction imported = Game.GetFormFromFile(0x00016EB1, "nwsFollowerFramework.esp") as Faction
    If imported != None && npc.IsInFaction(imported)
        Debug.Trace("[FU_NPC] NFF companion is imported: use its original dismissal system.")
        Return False
    EndIf
    nwsFollowerControllerScript controller = controllerQuest as nwsFollowerControllerScript
    If controller == None
        Debug.Trace("[FU_NPC] NFF controller quest was found but script cast failed.")
        Return False
    EndIf
    ; NFF's own RemoveFollower(actor, -1, 0) performs silent follower release.
    ; It owns its aliases, factions and roster, not this mod.
    controller.RemoveFollower(npc, -1, 0)
    Debug.Trace("[FU_NPC] Sent NFF RemoveFollower request.")
    Return True
EndFunction
