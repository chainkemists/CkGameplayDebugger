using UnrealBuildTool;

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
            "WorkspaceMenuStructure",

            "CkCompass",
            "CkCore",
            "CkDebuggerCommon",
            "CkEcs",
            "CkEcsExt",
            "CkEntityTag",
            "CkLabel",
            "CkMinimap",
            "CkPoi",
            "CkRecord",
            "CkVisibleRange",
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
