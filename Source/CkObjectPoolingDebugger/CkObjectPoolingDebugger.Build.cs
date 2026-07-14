using UnrealBuildTool;

public class CkObjectPoolingDebugger : CkModuleRules
{
    public CkObjectPoolingDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "Json",  // FJsonObject / serializer for the pool-tuning JSON report export

            "InputCore",  // SListView references EKeys (mouse/keyboard nav) — needs InputCore

            "Slate",
            "SlateCore",
            "WorkspaceMenuStructure",

            "CkCore",
            "CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs
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
