using UnrealBuildTool;

public class CkDebugTools : ModuleRules
{
    public CkDebugTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[] { });
        PrivateIncludePaths.AddRange(new string[] { });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "Slate",
            "SlateCore",
            "EditorStyle",
            "EditorWidgets",
            "ToolMenus",
            "WorkspaceMenuStructure",
            "LevelEditor",
            "Projects",
            
            // CK Framework Dependencies
            "CkCore",
            "CkEcs",
            "CkEcsDebugger",
            "CkEcsExt",
            "CkRelationship"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DesktopPlatform",
            "ApplicationCore"
        });

        DynamicallyLoadedModuleNames.AddRange(new string[] { });

        // Only build in development builds
        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            PublicDefinitions.Add("WITH_CKDEBUGTOOLS=0");
        }
        else
        {
            PublicDefinitions.Add("WITH_CKDEBUGTOOLS=1");
        }
    }
}