using UnrealBuildTool;

public class CkAiDebugger : CkModuleRules
{
    public CkAiDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "GameplayTags",
            "Slate", "SlateCore", "InputCore",
            "CkCore", "CkEcs", "CkDebuggerCommon", "CkEntityDebugOverlay", "CkCrowdDebugger", "CkEditorTools"
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd", "WorkspaceMenuStructure"
            });
        }
    }
}
