using UnrealBuildTool;

public class CkAggroDebugger : CkModuleRules
{
    public CkAggroDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",

            "InputCore",

            "Slate",
            "SlateCore",
            "WorkspaceMenuStructure",

            "CkCore",
            "CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs
            "CkRecord",  // the threat table is a record of AggroTarget entities
            "CkEcsExt",  // AggroTarget's TrackedEntity is an EntityHolder
            "CkAggro",

            "CkDebuggerCommon",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd"
            });
        }
    }
}
