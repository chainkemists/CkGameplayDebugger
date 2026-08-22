using UnrealBuildTool;

public class CkOptimizationDebuggerEditor : CkModuleRules
{
    public CkOptimizationDebuggerEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.AddRange(new string[] { ModuleDirectory });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "SlateCore",

            // UBaseLegacyWidgetEdMode and the level-editor viewport this draws into. Editor-only, which is why this
            // module exists separately from the DeveloperTool one that ships in packaged builds.
            "UnrealEd",
            "EditorFramework",

            "CkCore",
            "CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs
            "CkEditorTools",

            // ck::debug_axes::Get_HeatColor — the suite's one heat ramp, so this heatmap cannot drift from the
            // colours every other debugger uses.
            "CkDebuggerCommon",

            // The published marker snapshot. The arrow points this way only: CkPerfLab knows nothing about drawing.
            "CkPerfLab",
        });
    }
}
