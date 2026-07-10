using System.IO;
using UnrealBuildTool;

public class CkEqsDebugger : CkModuleRules
{
    public CkEqsDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",

            "Slate",
            "SlateCore",
            "WorkspaceMenuStructure",
            "EditorStyle",
            "AppFramework",
            "ToolMenus",
            "DeveloperSettings", // UCkEqsDebuggerSettings (UDeveloperSettings)

            "CkCore",
            "CkEcs",
            "CkEcsExt",       // FFragment_Transform on the querier (overlay reads its world location)
            "CkEqs",
            "CkGrid",         // 2dGridSystem + DebugDraw_Grid for the in-world grid lattice (SimpleGrid / Grid)
            "CkPmg",          // in-world sphere markers + debug lines for the overlay

            "CkDebuggerCommon",
            "CkEditorTools",
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
