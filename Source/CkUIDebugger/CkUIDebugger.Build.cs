using System.IO;
using UnrealBuildTool;

public class CkUIDebugger : CkModuleRules
{
    public CkUIDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",

            "Slate",
            "SlateCore",

            "CommonUI",

            "CkCore",
            "CkDebuggerCommon",
            "CkEcs",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
            "CkSlateLayout",
            "CkUI",
            "CkUICore",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "WorkspaceMenuStructure"
            });
        }

        PrivateDependencyModuleNames.Add("Projects");

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "UiDebuggerHistory.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "UiDebuggerHistory.ui.css"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "UiDebuggerLayers.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "UiDebuggerLayers.ui.css"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "UiDebuggerSummary.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "UiDebuggerSummary.ui.css"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "UiDebuggerCommands.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "UiDebuggerCommands.ui.css"), StagedFileType.NonUFS);
    }
}
