using System.IO;
using UnrealBuildTool;

public class CkEcsDebugger : CkModuleRules
{
    public CkEcsDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "DeveloperSettings",
            "Engine",
            "GameplayTags",
            "InputCore",

            "ApplicationCore",
            "Slate",
            "SlateCore",
            "GraphEditor",
            "WorkspaceMenuStructure",
            "EditorStyle",
            "AppFramework",
            "ToolMenus",

            "CkActorRelay",
            "CkAggro",
            "CkAnimation",
            "CkAStar",
            "CkAttribute",
            "CkAudio",
            "CkCore",
            "CkDebuggerCommon",
            "CkDynamic",
            "CkEcs",
            "CkEcsExt",
            "CkEntityCollection",
            "CkEntityExtension",
            "CkGoap",
            "CkGrid",
            "CkInteraction",
            "CkInventory",
            "CkIsmRenderer",
            "CkLabel",
            "CkObjective",
            "CkOverlapBody",
            "CkRecord",
            "CkRelationship",
            "CkResolver",
            "CkShapes",
            "CkSpatialQuery",
            "CkStateMachine",
            "CkTagSet",
            "CkTimer",
            "CkTween",
            "CkVariables",
            "CkVfx",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd"
            });
        }
    }
}
