using UnrealBuildTool;

public class CkDebuggerCommon : CkModuleRules
{
    public CkDebuggerCommon(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",

            "Slate",
            "SlateCore",
            "GraphEditor",
            "EditorStyle",
            "AppFramework",

            // For UCk_Plugin_UserSettings_UE base class.
            "CkSettings",
        });
    }
}
