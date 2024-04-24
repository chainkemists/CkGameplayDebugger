using System.IO;
using UnrealBuildTool;

public class CkEcsDebugger : CkModuleRules
{
    public CkEcsDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.AddRange(new string[] {
            "UnrealEd"
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",

            "CkAbility",
            "CkAttribute",
            "CkCore",
            "CkEcs",
            "CkLabel",
            "CkLog",
            "CkSettings",
            "CkTimer",

            "CogImgui",
            "CogWindow",
            "CogEngine",
        });

        if (Target.bBuildDeveloperTools || (Target.Configuration != UnrealTargetConfiguration.Shipping))
        {
            if (Target.Type == TargetRules.TargetType.Editor)
            {
                PublicDependencyModuleNames.Add("UnrealEd");
            }
        }
    }
}
