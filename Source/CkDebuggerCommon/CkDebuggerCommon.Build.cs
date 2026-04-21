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

            "InputCore",     // EKeys symbols referenced by templated SListView/SComboBox instantiations
            "Slate",
            "SlateCore",
            "GraphEditor",
            "EditorStyle",
            "AppFramework",

            // For UCk_Plugin_UserSettings_UE base class.
            "CkCore",
            "CkSettings",
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "WorkspaceMenuStructure",
                "ToolMenus",
            });
        }
    }
}
