using System.IO;
using UnrealBuildTool;

public class CkDialogDebugger : CkModuleRules
{
    public CkDialogDebugger(ReadOnlyTargetRules Target) : base(Target)
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

            "CkCore",
            "CkEcs",
            "CkRecord",
            "CkDialog",
            "CkEntityTag",

            "CkDebuggerCommon",
            "CkEditorTools",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "PropertyEditor"
            });
        }
    }
}
