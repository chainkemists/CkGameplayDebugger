using System.IO;
using UnrealBuildTool;

public class CkInputHudOverlay : CkModuleRules
{
    public CkInputHudOverlay(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "DeveloperSettings",
            "GameplayTags", "InputCore",
            "Slate", "SlateCore", "ApplicationCore",
            "EnhancedInput", "CommonInput",
            "CkCore", "CkEcs", "CkLog", "CkInput", "CkIntent",
            "CkDebuggerCommon", "CkSlateLayout", "CkEditorTools"
        });

        // The authored HUD view resolves its installed resource pair through IPluginManager.
        PrivateDependencyModuleNames.Add("Projects");

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "InputHudOverlay.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "InputHudOverlay.ui.css"), StagedFileType.NonUFS);

        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
            PublicDefinitions.Add("WITH_CK_INPUT_HUD=1");
        else
            PublicDefinitions.Add("WITH_CK_INPUT_HUD=0");
    }
}
