using System.IO;
using UnrealBuildTool;

public class CkDialogDebugger : CkModuleRules
{
    public CkDialogDebugger(ReadOnlyTargetRules Target) : base(Target)
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
            "AppFramework",

            "CkCore",
            "CkEcs",
            "CkRecord",
            "CkDialog",
            "CkEntityTag",

            "CkDebuggerCommon",
            "CkEditorTools",
            "CkSlateLayout",
        });

        PrivateDependencyModuleNames.Add("Projects");

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "WorkspaceMenuStructure",
                "EditorStyle",
                "ToolMenus"
            });
        }

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "DialogDebugger.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "DialogDebugger.ui.css"), StagedFileType.NonUFS);
    }
}
