using UnrealBuildTool;

public class CkEntityDebugOverlay : CkModuleRules
{
    public CkEntityDebugOverlay(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "DeveloperSettings",
            "GameplayTags", "InputCore",
            "Slate", "SlateCore", "UMG", "ApplicationCore",
            "CkCore", "CkEcs", "CkLog", "CkSettings", "CkDebuggerCommon",
            // AI vertical-slice feature deps (more added as providers are ported):
            "CkStateMachine", "CkGoap", "CkPhysics", "CkAnimation",
        });

        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
            PublicDefinitions.Add("WITH_CK_DEBUG_OVERLAY=1");
        else
            PublicDefinitions.Add("WITH_CK_DEBUG_OVERLAY=0");
    }
}
