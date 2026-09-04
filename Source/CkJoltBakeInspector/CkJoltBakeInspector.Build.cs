using UnrealBuildTool;

public class CkJoltBakeInspector : CkModuleRules
{
    public CkJoltBakeInspector(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "Slate", "SlateCore", "RenderCore", "RHI", "InputCore", "UMG",
            "UnrealEd", "AssetRegistry", "WorkspaceMenuStructure",
            "DeveloperSettings", "PhysicsCore",
            "CkCore", "CkEcs", "CkEditorTools", "CkDebuggerCommon", "CkDebugScene", "CkJolt", "CkJoltEditor"
        });
    }
}
