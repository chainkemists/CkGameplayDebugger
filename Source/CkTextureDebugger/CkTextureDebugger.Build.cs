using System.IO;
using UnrealBuildTool;

public class CkTextureDebugger : CkModuleRules
{
    public CkTextureDebugger(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Foliage",
            "InputCore",
            "RHI",
            "Slate",
            "SlateCore",

            // CkModuleRules selects the shared CkEcs PCH for CK modules; link its implementation module explicitly.
            "CkCore",
            "CkEcs",
            "CkDebuggerCommon",
            "CkEditorTools",
        });

        // The debugger hard-loads these cooked assets in Development/DebugGame. Staging is deliberately
        // per-package; broad content-directory staging would hide accidental asset additions.
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Content", "TextureDebugger", "Textures", "T_CkTextureChecker_ColorGrid_2K.uasset"), StagedFileType.UFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Content", "TextureDebugger", "Textures", "T_CkTextureChecker_ColorGrid_4K.uasset"), StagedFileType.UFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Content", "TextureDebugger", "Textures", "T_CkTextureChecker_GoldGray_4K.uasset"), StagedFileType.UFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Content", "TextureDebugger", "Textures", "T_CkTextureChecker_RoundedSpectrum_4K.uasset"), StagedFileType.UFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Content", "TextureDebugger", "Textures", "T_CkTextureChecker_DirectionalMono_4K.uasset"), StagedFileType.UFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Content", "TextureDebugger", "Materials", "M_CkTextureChecker.uasset"), StagedFileType.UFS);

        if (Target.bBuildEditor)
        {
            // The package-safe window uses FGlobalTabmanager directly. Only the editor's workspace-menu grouping
            // needs this module, so it must never enter a Development/DebugGame link graph.
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "WorkspaceMenuStructure",

                // Opt-in checker asset bootstrap and validation. These dependencies must never enter packaged builds.
                "Projects",
                "UnrealEd",
                "AssetTools",
                "AssetRegistry",
                "MaterialEditor",
            });
        }
    }
}
