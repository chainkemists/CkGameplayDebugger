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

		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// D7: Inspector gateway — registered via FCkDebuggerInspectorRegistry,
			// which lives in CkEcsDebugger. Kept private so consumers of CkGoapDebugger
			// don't transitively pull the ECS-debugger module.
			"CkEcsDebugger",
		});

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
