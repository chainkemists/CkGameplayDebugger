using System.IO;
using UnrealBuildTool;

public class CkAiDebugger : CkModuleRules
{
    public CkAiDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "GameplayTags",
            "Slate", "SlateCore", "InputCore",
            "CkCore", "CkEcs", "CkDebuggerCommon", "CkEntityDebugOverlay", "CkCrowdDebugger", "CkEditorTools", "CkSlateLayout"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { "Projects", "CkCrowd" });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd", "WorkspaceMenuStructure"
            });
        }

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "AiDebuggerRoster.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "AiDebuggerRoster.ui.css"), StagedFileType.NonUFS);
    }
}
