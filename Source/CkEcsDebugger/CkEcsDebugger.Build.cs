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
            "AppFramework",

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
            "CkCompass",
            "CkLabel",
            "CkMinimap",
            "CkObjective",
            "CkPoi",
            // Direct consumer of ECk_Poi_OffscreenPolicy's StaticEnum via ck::Format_UE in CkInspector_Poi.cpp;
            // the enum moved to CkPoiDisplayDefinition in CkPoi v2 refactor Gate 2.
            "CkPoiDisplayDefinition",
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
            "CkUICore",
            "CkWidgets",
            "CkUnrealComponent",
            "CkVariables",
            "CkVat",
            "CkVfx",
            "CkWorldSpaceWidget",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "WorkspaceMenuStructure",
                "EditorStyle",
                "ToolMenus",
                "GraphEditor"
            });
        }
    }
}
