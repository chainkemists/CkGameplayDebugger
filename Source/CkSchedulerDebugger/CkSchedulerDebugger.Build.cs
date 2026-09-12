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
            "AppFramework",

            "CkCore",
            "CkEcs",

            "CkDebuggerCommon",
            "CkEditorTools",
            "CkSlateLayout",
            "Projects",
        });

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "SchedulerInspector.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "SchedulerInspector.ui.css"), StagedFileType.NonUFS);

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
