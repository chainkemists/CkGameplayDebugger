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
            "AppFramework",

            "CkCore",
            "CkEcs",
            "CkRecord",
            "CkStateMachine",

            "CkDebuggerCommon",
            "CkEditorTools",
            "CkSlateLayout",
        });

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "SmDebuggerShell.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "SmDebuggerShell.ui.css"), StagedFileType.NonUFS);

        PrivateDependencyModuleNames.Add("Projects");

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "GraphEditor",
                "UnrealEd",
                "PropertyEditor",
                "WorkspaceMenuStructure",
                "EditorStyle",
                "ToolMenus"
            });
        }
    }
}
