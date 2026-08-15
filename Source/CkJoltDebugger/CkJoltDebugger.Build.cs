using UnrealBuildTool;

public class CkJoltDebugger : CkModuleRules
{
    public CkJoltDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",

            // Preview-scene viewport: FSceneViewport + FUMGViewportClient + camera input.
            "RenderCore",
            "RHI",
            "InputCore",
            "UMG",

            "Slate",
            "SlateCore",

            "CkCore",
            "CkDebuggerCommon",
            "CkEcs",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
            "CkJolt",
            "CkSpatialQuery", // FFragment_Probe_Current — the sensor population's body key
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
