// CkDebugTools.Build.cs - Add missing dependencies
using UnrealBuildTool;

public class CkDebugTools : ModuleRules
{
    public CkDebugTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "InputCore",
            "UnrealEd",
            "ToolMenus",
            "EditorStyle",
            "EditorWidgets",
            "WorkspaceMenuStructure",
            "ToolWidgets",
            "PropertyEditor",
            "GameplayTags",

            "CkEcs",
            "CkEcsExt",
            "CkCore",
            "CkEcsDebugger",
            "CkAttribute",
            "CkAbility",
            "CkEntityCollection",
            "CkTimer",
            "CkRelationship",
            "Projects",

            "AngelscriptCode",
        });
    }
}
