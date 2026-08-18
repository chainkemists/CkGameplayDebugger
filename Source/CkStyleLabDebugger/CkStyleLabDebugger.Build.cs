using UnrealBuildTool;

public class CkStyleLabDebugger : CkModuleRules
{
    public CkStyleLabDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",

            "InputCore",
            "AppFramework", // FColorPickerArgs / OpenColorPicker for feature-local semantic swatches
            "Slate",
            "SlateCore",

            "CkCore",
            "CkDebuggerCommon",
            "CkEcs",
            "CkEditorTools",

            // The sample renders the REAL overlay focus card from a hand-authored model, so an
            // axis flip is proven against the shipping widget instead of a lookalike.
            "CkEntityDebugOverlay",

            // The Input HUD preview below is the shipping runtime widget backed by a canned model.
            "CkInputHudOverlay",
        });

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
