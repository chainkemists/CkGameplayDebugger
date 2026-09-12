using UnrealBuildTool;
using System.IO;

public class CkSaveDebugger : CkModuleRules
{
    public CkSaveDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",

            "InputCore",

            "Slate",
            "SlateCore",

            // Neither is transitive through CkDebuggerCommon: DesktopPlatform drives the Open/Save file dialogs and
            // Json builds the deterministic export document.
            "DesktopPlatform",
            "Json",

            "CkCore",
            "CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs
            "CkSnapshot",  // the offline inspection API this window is a front end for

            "CkDebuggerCommon",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
            "CkSlateLayout",  // authored entity-navigation surface
        });

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "SaveDebugger.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "SaveDebugger.ui.css"), StagedFileType.NonUFS);

        PrivateDependencyModuleNames.Add("Projects");

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "WorkspaceMenuStructure",

                // The save visualizer bridge activates its Editor-companion EdMode through GLevelEditorModeTools.
                "LevelEditor",

                // The retained editor-world visuals (all transitive through CkDebuggerCommon; declared for direct
                // use): transform requests on preview entities, ISM mesh ghosts, and PMG gizmo shape types.
                "CkEcsExt",
                "CkIsmRenderer",
                "CkPmg",
            });
        }
    }
}
