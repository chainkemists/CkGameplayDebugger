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
            "CkDebuggerCommon", "CkEditorTools"
        });

        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
            PublicDefinitions.Add("WITH_CK_INPUT_HUD=1");
        else
            PublicDefinitions.Add("WITH_CK_INPUT_HUD=0");
    }
}
