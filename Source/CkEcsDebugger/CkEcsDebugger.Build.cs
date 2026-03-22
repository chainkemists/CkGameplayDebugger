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

            "Slate",
            "SlateCore",
            "WorkspaceMenuStructure",
            "EditorStyle",
            "AppFramework",

            "CkAnimation",
            "CkAttribute",
            "CkCore",
            "CkEcs",
            "CkEcsExt",
            "CkEntityCollection",
            "CkLabel",
            "CkRelationship",
            "CkSpatialQuery",
            "CkTagSet",
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
