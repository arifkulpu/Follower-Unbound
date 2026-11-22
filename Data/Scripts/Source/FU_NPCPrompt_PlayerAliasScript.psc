Scriptname FU_NPCPrompt_PlayerAliasScript extends ReferenceAlias
{Player alias bootstraps/re-registers the SkyPrompt controller on loading saves.}

FU_NPCPrompt_QuestScript Property Controller Auto

Event OnInit()
    If Controller != None
        Controller.Arm()
    EndIf
EndEvent

Event OnPlayerLoadGame()
    If Controller != None
        Controller.Arm()
    EndIf
EndEvent
