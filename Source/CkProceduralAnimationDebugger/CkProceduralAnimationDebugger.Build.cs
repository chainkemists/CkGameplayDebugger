using UnrealBuildTool;

public class CkProceduralAnimationDebugger : CkModuleRules
{
    public CkProceduralAnimationDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore",
            "CkCore", "CkEcs", "CkEcsExt", "CkProceduralAnimation", "CkDebuggerCommon",
            "CkEditorTools", "CkDebugScene", "CkSlateLayout"
        });
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "WorkspaceMenuStructure" });
        }
    }
}
