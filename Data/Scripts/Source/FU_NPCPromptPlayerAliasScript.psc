Scriptname FU_NPCPromptPlayerAliasScript extends ReferenceAlias
{Player alias bootstraps/re-registers the SkyPrompt controller on loading saves.}

FU_NPCPromptQuestScript Property Controller Auto

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
