using UnrealBuildTool;

public class CkEntityDebugOverlay : CkModuleRules
{
    public CkEntityDebugOverlay(ReadOnlyTargetRules Target) : base(Target)
    {
        // Dev-only debugger module: each provider .cpp defines identically-named
        // file-local tag helpers (ProviderTag()/FieldTag_*()) in an anonymous
        // namespace, which collide when merged into a unity TU. Disable unity for
        // this small module to sidestep the collision (rather than per-file prefixing).
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "DeveloperSettings",
            "GameplayTags", "InputCore",
            "Slate", "SlateCore", "UMG", "ApplicationCore",
            "CkCore", "CkEcs", "CkEcsExt", "CkEntityExtension", "CkLog", "CkSettings", "CkDebuggerCommon",
            // AI vertical-slice feature deps (more added as providers are ported):
            "CkStateMachine", "CkGoap", "CkPhysics", "CkAnimation", "CkRecord",
            // Attribute providers (Float/Integer/Byte):
            "CkAttribute", "CkLabel",
            // Curated feature pill providers:
            "CkInventory", "CkInteraction", "CkAggro", "CkRelationship", "CkObjective",
            "CkTagSet", "CkEntityCollection", "CkTimer", "CkVariables",
            // B2 — in-world diamond markers:
            "CkPmg"
        });

        if (Target.bBuildEditor)
        {
            // Ejected-PIE camera support (GEditor / GCurrentLevelEditingViewportClient).
            PublicDependencyModuleNames.Add("UnrealEd");
        }

        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
            PublicDefinitions.Add("WITH_CK_DEBUG_OVERLAY=1");
        else
            PublicDefinitions.Add("WITH_CK_DEBUG_OVERLAY=0");
    }
}
