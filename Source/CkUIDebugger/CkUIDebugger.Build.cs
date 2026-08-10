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

            "CommonUI",

            "CkCore",
            "CkDebuggerCommon",
            "CkEcs",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
            "CkUI",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "WorkspaceMenuStructure"
            });
        }
    }
}
