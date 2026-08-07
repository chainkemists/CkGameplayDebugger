using UnrealBuildTool;

public class CkInsightsDebugger : CkModuleRules
{
    public CkInsightsDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "Core",
            "CoreUObject",
            "DesktopPlatform",
            "Engine",
            "InputCore",
            "Slate",
            "SlateCore",
            "TraceServices",
            "WorkspaceMenuStructure",

            "CkCore",
            "CkDebuggerCommon",
            // CkModuleRules selects the shared CkEcs PCH for CK modules; list its
            // implementation module explicitly so PCH-emitted symbols link here.
            "CkEcs",
            "CkEditorTools",
            "CkInsightsAnalyzer",
        });
    }
}
