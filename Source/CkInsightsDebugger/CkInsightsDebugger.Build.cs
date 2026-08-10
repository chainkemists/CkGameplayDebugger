using UnrealBuildTool;

public class CkInsightsDebugger : CkModuleRules
{
    public CkInsightsDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "Core",
            "CoreUObject",
            "DesktopPlatform",
            "Engine",
            "ImageCore",
            "ImageWrapper",
            "InputCore",
            "Slate",
            "SlateCore",
            "TraceLog",
            "TraceServices",

            "CkCore",
            "CkDebuggerCommon",
            // CkModuleRules selects the shared CkEcs PCH for CK modules; list its
            // implementation module explicitly so PCH-emitted symbols link here.
            "CkEcs",
            "CkEditorTools",
            "CkInsightsAnalyzer",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CkProfile",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.Add("WorkspaceMenuStructure");
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
