using UnrealBuildTool;

public class CkJoltDebugger : CkModuleRules
{
    public CkJoltDebugger(ReadOnlyTargetRules Target) : base(Target)
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

            "CkCore",
            "CkDebuggerCommon",
            "CkEcs",
            "CkJolt",
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
