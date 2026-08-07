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
            "CkEditorTools",
            "CkInsightsAnalyzer",
        });
    }
}
