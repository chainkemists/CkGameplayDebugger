using UnrealBuildTool;

public class CkAudioDebugger : CkModuleRules
{
    public CkAudioDebugger(ReadOnlyTargetRules Target) : base(Target)
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

            "CkCore",
            "CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs
            "CkRecord",  // an AudioDirector holds its tracks in a Record
            "CkEcsExt",  // track transforms come off the SceneNode/Transform layer
            "CkLabel",  // a track is named by its GameplayLabel
            "CkResourceLoader",  // a track's asset is an ObjectReference_Soft/Hard this window reads
            "CkTimer",  // the window copies the track's FCk_Handle_Timer
            "CkAudio",

            "CkDebuggerCommon",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "WorkspaceMenuStructure"
            });
        }
    }
}
