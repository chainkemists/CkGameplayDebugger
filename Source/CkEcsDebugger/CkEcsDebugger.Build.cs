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

            "CkAbility",
            "CkAttribute",
            "CkCore",
            "CkEcs",
            "CkEntityCollection",
            "CkLabel",
            "CkLog",
            "CkNet",
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
