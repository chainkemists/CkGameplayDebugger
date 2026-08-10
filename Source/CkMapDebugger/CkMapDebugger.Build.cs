using UnrealBuildTool;

public class CkMapDebugger : CkModuleRules
{
    public CkMapDebugger(ReadOnlyTargetRules Target) : base(Target)
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

            "CkCompass",
            "CkCore",
            "CkDebuggerCommon",
            "CkEcs",
            "CkEcsExt",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
            "CkEntityTag",
            "CkLabel",
            "CkMinimap",
            "CkPoi",
            "CkRecord",
            "CkVisibleRange",
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
