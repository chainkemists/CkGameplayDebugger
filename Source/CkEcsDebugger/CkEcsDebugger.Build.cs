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
            "CkNet",
            "CkOverlapBody",
            "CkPhysics",
            "CkSettings",
            "CkTimer",

            "CogImgui",
            "CogWindow",
            "CogEngine",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
            });
        }
    }
}
