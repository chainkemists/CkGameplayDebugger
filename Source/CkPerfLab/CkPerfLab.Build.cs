using UnrealBuildTool;

public class CkPerfLab : CkModuleRules
{
    public CkPerfLab(ReadOnlyTargetRules Target) : base(Target)
    {
        // So sources under Public/CkPerfLab/** can include the module-root headers
        // (CkPerfLab_Log.h, CkPerfLab_Module.h) by their bare name.
        PrivateIncludePaths.AddRange(new string[] { ModuleDirectory });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",

            // The session model, the request and the heartbeat are all JSON on disk; the codec uses
            // FJsonObject directly, matching CkInsightsAnalyzer and CkAssetExporter. There is no Ck
            // wrapper around FJsonObject and this module does not invent one.
            "Json",

            "CkCore",
            "CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs

            // UCk_Utils_Stats_UE::Get_ThreadTimings — the measurement the runner records.
            "CkProfile",

            // UCk_Utils_NavSurface_UE::Try_ProjectPoint — navmesh-seeded positions, which are
            // preferred over a grid because a spot no player can reach costs nobody anything.
            // Projection of a fixed lattice rather than random sampling, so the same map plans the
            // same position ids every run and two sessions of it stay comparable.
            "CkNavigation",

            // UNavigationSystemV1::GetWorldBounds — the lattice footprint, which the navigation
            // surface's own tile bounds cannot stand in for (see the survey builder's comment).
            "NavigationSystem",

            // RHIGetGPUFrameCycles and GUsingNullRHI, for the runner's environment assertion.
            "RHI",

            "CkLog"
        });
    }
}
