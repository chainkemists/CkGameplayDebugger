using System.IO;
using UnrealBuildTool;

public class CkNavDebugger : CkModuleRules
{
	public CkNavDebugger(ReadOnlyTargetRules Target) : base(Target)
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

			"NavigationSystem",

			"CkCore",
			"CkEcs",
			"CkEcsExt",
			"CkNavigation",
			"CkPmg",

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
