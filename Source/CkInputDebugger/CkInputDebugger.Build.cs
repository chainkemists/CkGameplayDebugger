using System.IO;
using UnrealBuildTool;

public class CkInputDebugger : CkModuleRules
{
    public CkInputDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",

            "InputCore",
            "EnhancedInput",

            "Slate",
            "SlateCore",

            "CkCore",
            "CkInput",  // bindings pane: user-settings profile rows + Ck scope-tag settings
            "CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs
            "CkDebuggerCommon",
            "CkSlateLayout",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
        });

        // The authored controls resolve their installed resource pair through IPluginManager.
        PrivateDependencyModuleNames.Add("Projects");

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "InputDebuggerControls.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "InputDebuggerControls.ui.css"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "InputDebuggerShell.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "InputDebuggerShell.ui.css"), StagedFileType.NonUFS);

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "WorkspaceMenuStructure"
            });
        }
    }
}
