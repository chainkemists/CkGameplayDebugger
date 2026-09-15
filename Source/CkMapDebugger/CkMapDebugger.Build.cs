using UnrealBuildTool;
using System.IO;

public class CkMapDebugger : CkModuleRules
{
    public CkMapDebugger(ReadOnlyTargetRules Target) : base(Target)
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

            "CkCompass",
            "CkCore",
            "CkDebuggerCommon",
            "CkEcs",
            "CkEcsExt",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
            "CkSlateLayout",
            "CkEntityTag",
            "CkLabel",
            "CkMinimap",
            "CkPoi",
            "CkRecord",
            "CkVisibleRange",
        });

        PrivateDependencyModuleNames.Add("Projects");
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "MapDebugger.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "MapDebugger.ui.css"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "MapDebuggerPoiRow.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "MapDebuggerPoiRow.ui.css"), StagedFileType.NonUFS);

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "WorkspaceMenuStructure"
            });
        }
    }
}
