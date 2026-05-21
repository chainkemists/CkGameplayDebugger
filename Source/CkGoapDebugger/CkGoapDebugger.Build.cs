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
			"CkEntityExtension",
			"CkRecord",
			"CkLabel",
			"CkDebuggerCommon",

			"GraphEditor",
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
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
