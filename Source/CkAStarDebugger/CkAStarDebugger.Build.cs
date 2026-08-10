using System.IO;
using UnrealBuildTool;

public class CkAStarDebugger : CkModuleRules
{
	public CkAStarDebugger(ReadOnlyTargetRules Target) : base(Target)
	{
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

			"CkDebuggerCommon",
			"CkEditorTools", // CkStyle:: roles — reached from this module's PUBLIC headers, so declare it directly
		});

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
	}
}
