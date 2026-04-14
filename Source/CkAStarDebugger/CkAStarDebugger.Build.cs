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
			"WorkspaceMenuStructure",
			"EditorStyle",
			"AppFramework",
			"ToolMenus",

			"CkCore",
			"CkEcs",
			"CkAStar",

			"CkDebuggerCommon",
		});

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd"
			});
		}
	}
}
