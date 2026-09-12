using UnrealBuildTool;
using System.IO;

public class CkVisualLodDebugger : CkModuleRules
{
    public CkVisualLodDebugger(ReadOnlyTargetRules Target) : base(Target)
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
            "CkEcsExt",  // UCk_Utils_Ecs_Base_UE, the base of both VisualLod Utils classes
            "CkResourceLoader",  // the arbiter/member Current fragments hold RootedAssetBatch members
            "CkVisualLod",
            "CkIskmRenderer",  // batched-crowd actor + the promoted proxy's SKMC-backed rendering flags
            "CkPmg",  // the retained world markers call UCk_Utils_Pmg_{Basic,Flat}Shapes directly — a transitive link through CkDebuggerCommon would be luck, not policy

            "CkDebuggerCommon",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
            "CkSlateLayout",
        });

        // The authored arbiter-tuner surface resolves its installed resource pair through IPluginManager.
        PrivateDependencyModuleNames.Add("Projects");

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "VisualLodArbiterTuners.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "VisualLodArbiterTuners.ui.css"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "VisualLodCrowdTuners.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "VisualLodCrowdTuners.ui.css"), StagedFileType.NonUFS);

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
