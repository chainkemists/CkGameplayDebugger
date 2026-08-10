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
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
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
