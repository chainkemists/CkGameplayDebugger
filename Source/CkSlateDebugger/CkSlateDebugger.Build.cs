using System.IO;
using UnrealBuildTool;

public class CkSlateDebugger : CkModuleRules
{
    public CkSlateDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "InputCore",
            "UnrealEd",
            "ToolMenus",
            "EditorSubsystem",
            "DeveloperSettings",
            "GameplayTags",
            "InputCore",
            "Iris",
            "IrisCore",
            "Projects",
            "ToolWidgets",
            "EditorWidgets",
            "ApplicationCore",
            "Json",
            "JsonUtilities",
            "EditorStyle",
            "Kismet",

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
        });
    }
}
