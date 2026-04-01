using UnrealBuildTool;

public class CkUIDebugger : CkModuleRules
{
    public CkUIDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",

            "Slate",
            "SlateCore",
            "WorkspaceMenuStructure",

            "CommonUI",

            "CkCore",
            "CkEcs",
            "CkUI",
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
