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
            "Engine",
            "GameplayTags",
            "InputCore",

            "Slate",
            "SlateCore",
            "GraphEditor",
            "WorkspaceMenuStructure",
            "EditorStyle",
            "AppFramework",
            "ToolMenus",

            "CkAnimation",
            "CkAttribute",
            "CkCore",
            "CkDebuggerCommon",
            "CkEcs",
            "CkEcsExt",
            "CkEntityCollection",
            "CkEntityExtension",
            "CkGrid",
            "CkInteraction",
            "CkInventory",
            "CkIsmRenderer",
            "CkLabel",
            "CkObjective",
            "CkRecord",
            "CkRelationship",
            "CkShapes",
            "CkDynamic",
            "CkSpatialQuery",
            "CkTagSet",
            "CkVariables",
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
