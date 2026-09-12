using System.IO;
using UnrealBuildTool;

public class CkObjectPoolingDebugger : CkModuleRules
{
    public CkObjectPoolingDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "Json",  // FJsonObject / serializer for the pool-tuning JSON report export

            "InputCore",  // SListView references EKeys (mouse/keyboard nav) — needs InputCore

            "Slate",
            "SlateCore",

            "CkCore",
            "CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs
            "CkDebuggerCommon",
            "CkSlateLayout",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
        });

        // The retained authored command/overview surface resolves its installed resource pair through IPluginManager.
        PrivateDependencyModuleNames.Add("Projects");

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "ObjectPoolingDebugger.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "ObjectPoolingDebugger.ui.css"), StagedFileType.NonUFS);

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
