using System.IO;
using UnrealBuildTool;

public class CkGoapDebugger : CkModuleRules
{
	public CkGoapDebugger(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(new string[] {
		});

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
			"CkAStar",
			"CkGoap",
			"CkEntityExtension",
			"CkRecord",
			"CkLabel",
			"CkDebuggerCommon",
            "CkEditorTools",
			"CkSlateLayout",

		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects",
			// D7: Inspector gateway — registered via FCkDebuggerInspectorRegistry,
			// which lives in CkEcsDebugger. Kept private so consumers of CkGoapDebugger
			// don't transitively pull the ECS-debugger module.
			"CkEcsDebugger",
		});

		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerSquad.ui.html"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerSquad.ui.css"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerAgentColumn.ui.html"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerAgentColumn.ui.css"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerDecision.ui.html"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerDecision.ui.css"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerTimeline.ui.html"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerTimeline.ui.css"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerWorldState.ui.html"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerWorldState.ui.css"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerCatalog.ui.html"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerCatalog.ui.css"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerGraph.ui.html"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerGraph.ui.css"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerSearchTrace.ui.html"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerSearchTrace.ui.css"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerShell.ui.html"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapDebuggerShell.ui.css"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapInspectorGateway.ui.html"), StagedFileType.NonUFS);
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "GoapInspectorGateway.ui.css"), StagedFileType.NonUFS);

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.Add("GraphEditor");
			PrivateDependencyModuleNames.Add("UnrealEd");
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"WorkspaceMenuStructure",
				"EditorStyle",
				"ToolMenus",
			});
		}
	}
}
