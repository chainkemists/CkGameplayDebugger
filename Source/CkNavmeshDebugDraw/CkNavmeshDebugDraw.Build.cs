using UnrealBuildTool;

public class CkNavmeshDebugDraw : CkModuleRules
{
    public CkNavmeshDebugDraw(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "NavigationSystem",
            "RenderCore",
            "RHI",
            "CkCore",
            "CkLog",
        });

        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
            PublicDefinitions.Add("WITH_CK_NAVMESH_DEBUG_DRAW=1");
        else
            PublicDefinitions.Add("WITH_CK_NAVMESH_DEBUG_DRAW=0");
    }
}
