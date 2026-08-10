using System.IO;
using UnrealBuildTool;

public class CkSchedulerDebugger : CkModuleRules
{
    public CkSchedulerDebugger(ReadOnlyTargetRules Target) : base(Target)
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
            "AppFramework",

            "CkCore",
            "CkEcs",

            "CkDebuggerCommon",
            "CkEditorTools",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "WorkspaceMenuStructure",
                "EditorStyle",
                "ToolMenus"
            });
        }
    }
}
