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
			"RenderCore",
			"RHI",
			"DeveloperSettings",
			"InputCore",

			"Slate",
			"SlateCore",
			"AppFramework",
			"UMG",

			"GameplayTags",
			"NavigationSystem",
			"AIModule",

			"CkCore",
			"CkEcs",
			"CkEcsExt",
			"CkLabel",
			"CkDebuggerCommon",
            "CkEditorTools",

			"CkNavigation",
			"CkVoxelNav",
			"CkPathNetwork",
			"CkCrowd",
			"CkPmg",       // destination-ping ring in the in-world RMB command processor
		});

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"WorkspaceMenuStructure",
				"EditorStyle",
				"ToolMenus",
				"EditorWidgets",
			});
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("CkVoxelNavEditor");
		}
	}
}
