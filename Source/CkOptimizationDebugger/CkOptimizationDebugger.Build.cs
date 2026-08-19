using UnrealBuildTool;

public class CkOptimizationDebugger : CkModuleRules
{
    public CkOptimizationDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",  // UCkOptimizationDebuggerSettings : UDeveloperSettings — must link explicitly, not via CkDebuggerCommon
            "PhysicsCore",        // UBodySetupCore::GetCollisionTraceFlag — the collision-complexity check's only read, and it lives BELOW UBodySetup

            "InputCore",

            // FImage::ChangeFormat, converting a decoded snapshot PNG to the BGRA the Slate brush wants. Engine
            // pulls ImageCore in transitively, but a transitive link is luck rather than policy.
            "ImageCore",

            "Slate",
            "SlateCore",

            // No ApplicationCore / DesktopPlatform / Json here on purpose. All three were declared for a report
            // export that is UNSCHEDULED (see PLAN.md) and never written, and clipboard copy goes through
            // CkDebuggerCommon's `ck::DebugCopyMenu` rather than FPlatformApplicationMisc directly. A dependency
            // whose comment describes a feature the module does not have is a dependency the next reader trusts.
            // Add them back in the change that actually lands the export.

            "CkCore",
            "CkEcs",  // CkCore's SharedPCH instantiates global ECS registrations — every CK module must link CkEcs

            "CkDebuggerCommon",
            "CkEditorTools",  // shared CkStyle:: tokens used directly by the window
        });

        if (Target.bBuildEditor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                // The analysis engine walks the editor world and the asset registry; both are editor-only, and the
                // module is a DeveloperTool (packaged Development/DebugGame included), so every use of them stays
                // behind WITH_EDITOR with a stated non-editor fallback.
                "UnrealEd",
                "AssetRegistry",

                // IAssetTools::FixupReferencers — the cleanup page's redirector action. The implementation
                // (FAssetFixUpRedirectors) is Private to this module, so the interface is the only supported route.
                "AssetTools",

                // ISettingsModule::ShowViewer — the Project Settings route of scope-aware navigation. Not transitive
                // through DeveloperSettings: that module declares settings objects, this one shows them.
                "Settings",

                // UTexture::GetBuiltTextureSize takes an ITargetPlatform, and the running one comes from this
                // module's manager. The texture checks are its only consumer.
                "TargetPlatform",

                "WorkspaceMenuStructure",
            });
        }
    }
}
