using System.IO;
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
            "Projects",

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
            "CkSlateLayout",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
        });

        foreach (var Resource in new[]
        {
            "AudioDebuggerShell.ui.html", "AudioDebuggerShell.ui.css",
            "AudioDebuggerDirectors.ui.html", "AudioDebuggerDirectors.ui.css",
            "AudioDebuggerTracks.ui.html", "AudioDebuggerTracks.ui.css",
            "AudioDebuggerCrossfade.ui.html", "AudioDebuggerCrossfade.ui.css",
            "AudioDebuggerSpatial.ui.html", "AudioDebuggerSpatial.ui.css",
            "AudioDebuggerAttenuation.ui.html", "AudioDebuggerAttenuation.ui.css",
            "AudioDebuggerEventsToolbar.ui.html", "AudioDebuggerEventsToolbar.ui.css",
            "AudioDebuggerEvents.ui.html", "AudioDebuggerEvents.ui.css",
            "AudioDebuggerOverlay.ui.html", "AudioDebuggerOverlay.ui.css"
        })
        {
            RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", Resource), StagedFileType.NonUFS);
        }

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
