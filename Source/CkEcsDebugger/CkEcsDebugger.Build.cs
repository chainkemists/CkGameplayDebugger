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
            "CkCamera",
            "CkChaos",
            "CkCore",
            "CkCrowd",
            "CkDebuggerCommon",
            "CkEditorTools",
            "CkDynamic",
            "CkEcs",
            "CkEcsExt",
            "CkEntityCollection",
            "CkEntityDebugOverlay",
            "CkEntityExtension",
            "CkEntityTag",
            "CkEqs",
            "CkFx",
            "CkGoap",
            "CkGrid",
            "CkInteraction",
            "CkInventory",
            "CkIsmRenderer",
            "CkIskmRenderer",
            "CkJolt",
            "CkLabel",
            "CkObjective",
            "CkOverlapBody",
            "CkPathNetwork",
            "CkPhysics",
            "CkProjectile",
            "CkRaySense",
            "CkRecord",
            "CkRelationship",
            "CkRenderTarget",
            "CkResolver",
            "CkShapes",
            "CkSnapshot",
            "CkSpatialQuery",
            "CkSpline",
            "CkStateMachine",
            "CkTagSet",
            "CkTimer",
            "CkTween",
            "CkUI",
            "CkUnrealComponent",
            "CkVariables",
            "CkVat",
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
