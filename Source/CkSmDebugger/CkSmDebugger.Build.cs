using System.IO;
using UnrealBuildTool;

public class CkSmDebugger : CkModuleRules
{
    public CkSmDebugger(ReadOnlyTargetRules Target) : base(Target)
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
            "CkRecord",
            "CkStateMachine",

            "CkDebuggerCommon",
            "CkEditorTools",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "PropertyEditor",
                "WorkspaceMenuStructure",
                "EditorStyle",
                "ToolMenus"
            });
        }
    }
}
