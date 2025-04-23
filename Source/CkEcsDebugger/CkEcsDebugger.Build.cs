using System.IO;
using UnrealBuildTool;

public class CkEcsDebugger : CkModuleRules
{
    public CkEcsDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.AddRange(new string[] {
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "InputCore",

            "Iris",
            "IrisCore",

            "CkAbility",
            "CkAnimation",
            "CkAttribute",
            "CkCore",
            "CkEcs",
            "CkEntityCollection",
            "CkLabel",
            "CkLog",

            "CkOverlapBody",
            "CkPhysics",
            "CkRelationship",
            "CkSettings",
            "CkTimer",

            "CogImgui",
            "CogWindow",
            "CogEngine"
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
