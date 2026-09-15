using System.IO;
using UnrealBuildTool;

public class CkDebuggerLauncher : CkModuleRules
{
    public CkDebuggerLauncher(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            // EKeys:: in the rail's OnKeyDown — a direct symbol reference, so the
            // dependency is declared here rather than inherited through Slate.
            "InputCore",
            "Projects",
            "Slate",
            "SlateCore",

            "CkCore",
            "CkDebuggerCommon",
            // CkModuleRules selects the shared CkEcs PCH for CK modules; list its
            // implementation module explicitly so PCH-emitted symbols link here.
            "CkEcs",
            "CkEditorTools",
            "CkSlateLayout",
        });

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "DebuggerLauncherShell.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "DebuggerLauncherShell.ui.css"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "DebuggerSuiteShell.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "DebuggerSuiteShell.ui.css"), StagedFileType.NonUFS);

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.Add("WorkspaceMenuStructure");
        }
    }
}
