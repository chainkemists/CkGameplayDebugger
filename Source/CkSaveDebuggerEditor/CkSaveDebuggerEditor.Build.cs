using UnrealBuildTool;

public class CkSaveDebuggerEditor : CkModuleRules
{
    public CkSaveDebuggerEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "SlateCore",

            "UnrealEd",
            "EditorFramework",

            "CkCore",
            "CkEcs",
            "CkEditorTools",
            "CkSaveDebugger",
        });
    }
}
