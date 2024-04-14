using System.IO;
using UnrealBuildTool;

public class CkGameplayDebugger : CkModuleRules
{
    public CkGameplayDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.AddRange(new string[] {
            // ... add other private include paths required here ...
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "InputCore",
            "DeveloperSettings",
            "RenderCore",
            "UMG",

            "CkCore",
            "CkEcs",
            "CkInput",
            "CkLog",
            "CkSettings",

            "CogCommon",
            "CogImgui",
            "CogWindow",
            "CogEngine"
        });

        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
            PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=1");

            PrivateDependencyModuleNames.AddRange(new string[] { "GameplayDebugger" });
        }
        else
        {
            PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=0");
        };
    }
}
