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

            "ApplicationCore",  // FPlatformApplicationMisc::ClipboardCopy in CkDebug_CopyMenu_Utils
            "InputCore",     // EKeys symbols referenced by templated SListView/SComboBox instantiations
            "Slate",
            "SlateCore",
            "GraphEditor",
            "EditorStyle",
            "AppFramework",

            // For UCk_Plugin_UserSettings_UE base class.
            "CkCore",
            "CkEcs",       // FCk_Handle in SCkDebug_EntityRef + the cross-debugger entity navigator
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
