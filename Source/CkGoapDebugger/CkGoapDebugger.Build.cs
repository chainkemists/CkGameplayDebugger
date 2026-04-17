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
			"WorkspaceMenuStructure",
			"EditorStyle",
			"AppFramework",
			"ToolMenus",

			"CkCore",
			"CkEcs",
			"CkAStar",
			"CkGoap",
			"CkLabel",
			"CkDebuggerCommon",

			"GraphEditor",
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
