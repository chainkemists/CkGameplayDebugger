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
            "Slate",
            "SlateCore",
            "WorkspaceMenuStructure",

            "CkCore",
            "CkDebuggerCommon",
            "CkEcs",
            "CkEditorTools",

            // The sample renders the REAL overlay focus card from a hand-authored model, so an
            // axis flip is proven against the shipping widget instead of a lookalike.
            "CkEntityDebugOverlay",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd"
            });
        }
    }
}
