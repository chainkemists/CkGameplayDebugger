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

            // UCkJoltDebuggerSettings — the per-user preferences the window restores at construct.
            "DeveloperSettings",

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

            // UCk_Utils_Transform_TypeUnsafe_UE — the world location of an entity a selected probe is
            // overlapping, which is the far end of the probe-results line. Nothing else here needs it.
            "CkEcsExt",

            // The module's own log category (ck::jolt_debugger). Named explicitly like every other Ck module
            // that logs: CkJolt re-exports it publicly, but the import lib does not reach a consumer plugin.
            "CkLog",
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
