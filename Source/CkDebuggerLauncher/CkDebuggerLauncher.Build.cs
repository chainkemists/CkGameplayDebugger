using UnrealBuildTool;

public class CkDebuggerLauncher : CkModuleRules
{
    public CkDebuggerLauncher(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "Projects",
            "Slate",
            "SlateCore",
            "WorkspaceMenuStructure",

            "CkCore",
            "CkDebuggerCommon",
            // CkModuleRules selects the shared CkEcs PCH for CK modules; list its
            // implementation module explicitly so PCH-emitted symbols link here.
            "CkEcs",
            "CkEditorTools",
        });
    }
}
