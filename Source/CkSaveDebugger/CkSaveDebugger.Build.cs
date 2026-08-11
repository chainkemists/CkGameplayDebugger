using UnrealBuildTool;

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
            "WorkspaceMenuStructure",

            // Neither is transitive through CkDebuggerCommon: DesktopPlatform drives the Open/Save file dialogs and
            // Json builds the deterministic export document.
            "DesktopPlatform",
            "Json",

            "CkCore",
            "CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs
            "CkSnapshot",  // the offline inspection API this window is a front end for

            "CkDebuggerCommon",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window

            // The retained editor-world visuals (all transitive through CkDebuggerCommon; declared for direct use):
            // Transform requests on preview entities, ISM mesh ghosts, and the PMG gizmo set's shape types.
            "CkEcsExt",
            "CkIsmRenderer",
            "CkPmg",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",

                // The save visualizer's hidden EdMode (UBaseLegacyWidgetEdMode + GLevelEditorModeTools).
                "EditorFramework",
                "LevelEditor",
            });
        }
    }
}
