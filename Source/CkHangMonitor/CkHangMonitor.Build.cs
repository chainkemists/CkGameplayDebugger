using UnrealBuildTool;

public class CkHangMonitor : CkModuleRules
{
    public CkHangMonitor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.Add(ModuleDirectory);

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

            "CkCore",
            "CkDebuggerCommon",
            // CkModuleRules selects the shared CkEcs PCH for CK modules; list its
            // implementation module explicitly so PCH-emitted symbols link here.
            "CkEcs",
            "CkEditorTools",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.Add("WorkspaceMenuStructure");
        }
    }
}
