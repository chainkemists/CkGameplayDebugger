using System.IO;
using UnrealBuildTool;

public class CkCrowdDebugger : CkModuleRules
{
	public CkCrowdDebugger(ReadOnlyTargetRules Target) : base(Target)
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
			"WorkspaceMenuStructure",
			"EditorStyle",
			"AppFramework",
			"ToolMenus",

			"GameplayTags",
			"NavigationSystem",
			"AIModule",

			"CkCore",
			"CkEcs",
			"CkLabel",
			"CkDebuggerCommon",

			"CkNavigation",
			"CkCrowd",
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
