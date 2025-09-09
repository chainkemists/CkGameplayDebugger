using System.IO;
using UnrealBuildTool;

public class CkEcsDebugger : CkModuleRules
{
    public CkEcsDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
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
            "CkAudio",
            "CkCore",
            "CkEcs",
            "CkEcsExt",
            "CkEntityCollection",
            "CkEntityExtension",
            "CkInteraction",
            "CkLabel",
            "CkLog",
            "CkObjective",
            "CkOverlapBody",
            "CkPhysics",
            "CkRecord",
            "CkRelationship",
            "CkSettings",
            "CkShapes",
            "CkSpatialQuery",
            "CkTimer",

            "Cog",
            "CogCommon",
            "CogDebug",
            "CogEngine",
            "CogImgui"
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
