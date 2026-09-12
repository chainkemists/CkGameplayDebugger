using UnrealBuildTool;
using System.IO;

public class CkIntentDebugger : CkModuleRules
{
    public CkIntentDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",

            "InputCore",

            "Slate",
            "SlateCore",

            "CkCore",
            "CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs
            "CkInput",   // input sources, the layer stack, the button map
            "CkIntent",  // the frame record, the compiled set, the matcher

            "CkInputHudOverlay", // one-way DeveloperTool UI -> Runtime settings owner

            "CkDebuggerCommon",
            "CkSlateLayout",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
        });

        // The authored HUD view resolves the installed CkDebugger resource pair through IPluginManager.
        PrivateDependencyModuleNames.Add("Projects");

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "IntentInputHudControls.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "IntentInputHudControls.ui.css"), StagedFileType.NonUFS);
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
