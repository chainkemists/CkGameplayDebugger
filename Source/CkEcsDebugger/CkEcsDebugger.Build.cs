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
            "CkEcsExt",
            "CkEntityExtension",
            "CkEntityCollection",
            "CkLabel",
            "CkLog",

            "CkOverlapBody",
            "CkPhysics",
            "CkShapes",
            "CkSpatialQuery",
            "CkRecord",
            "CkRelationship",
            "CkSettings",
            "CkTimer",

            "Cog",
            "CogCommon",
            "CogImgui",
            "CogEngine",
            "CogDebug"
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