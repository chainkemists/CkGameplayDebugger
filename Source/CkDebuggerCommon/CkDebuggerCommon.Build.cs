using System.IO;
using UnrealBuildTool;

public class CkDebuggerCommon : CkModuleRules
{
    public CkDebuggerCommon(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",

            "ApplicationCore",  // FPlatformApplicationMisc::ClipboardCopy in CkDebug_CopyMenu_Utils
            "InputCore",     // EKeys symbols referenced by templated SListView/SComboBox instantiations
            "Projects",      // IPluginManager — FCkDebuggerCommonStyle resolves the Resources content root
            "Slate",
            "SlateCore",
            "AppFramework",

            // For UCk_Plugin_UserSettings_UE base class.
            "CkActorRelay", // ACk_ActorRelay_UE check behind the shared depth-transparency predicate
            "CkCore",
            "CkEcs",       // FCk_Handle in SCkDebug_EntityRef + the cross-debugger entity navigator
            "CkEcsExt",    // FFragment_Transform view in the shared entity-marker preview
            "CkEditorTools", // Shared CkStyle:: tokens + UCk_Style_UserSettings_UE (migrated from this module)
            "CkIsmRenderer", // ISM-proxy mesh bounds in the shared focus-entity helper (CkDebug_Focus)
            "CkPmg",       // PMG debug-shape exclusion in the shared entity-marker preview + gizmo set
            "CkSettings",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "WorkspaceMenuStructure",
                "ToolMenus",
                // Editor-only graph plumbing. GraphEditor transitively pulls
                // EditorWidgets -> UnrealEd, so it must NOT be unconditional:
                // a Runtime/DeveloperTool consumer (CkEntityDebugOverlay) would
                // otherwise drag UnrealEd into a non-editor client target and
                // fail with "Unable to instantiate UnrealEd for non-editor".
                // Only the graph debuggers (editor-only) use these; the lone
                // consumer file CkDebugConnectionPolicyBase is WITH_EDITOR-gated.
                "GraphEditor",
                "EditorStyle",
            });
        }

        foreach (var GraphEditorResource in Directory.EnumerateFiles(
            Path.Combine(PluginDirectory, "Resources", "GraphEditor"),
            "*.png",
            SearchOption.AllDirectories))
        {
            RuntimeDependencies.Add(GraphEditorResource, StagedFileType.NonUFS);
        }

        // Common owns the runtime icon registries consumed by every debugger surface.
        // Stage the source SVGs from here as well as from the launcher so a packaged
        // debugger never depends on another feature module to provide its brushes.
        foreach (var IconResource in Directory.EnumerateFiles(
            Path.Combine(PluginDirectory, "Resources", "Icons"),
            "*.svg",
            SearchOption.AllDirectories))
        {
            RuntimeDependencies.Add(IconResource, StagedFileType.NonUFS);
        }
    }
}
