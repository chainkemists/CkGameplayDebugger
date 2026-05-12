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
            "CkIskmRenderer",
            "CkLabel",
            "CkObjective",
            "CkOverlapBody",
            "CkPhysics",
            "CkRecord",
            "CkRelationship",
            "CkResolver",
            "CkShapes",
            "CkSpatialQuery",
            "CkStateMachine",
            "CkStateTree",
            "CkTagSet",
            "CkTimer",
            "CkTween",
            "CkUI",
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
