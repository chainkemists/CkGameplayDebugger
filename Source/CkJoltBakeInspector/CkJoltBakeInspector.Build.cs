using UnrealBuildTool;
using System.IO;

public class CkJoltBakeInspector : CkModuleRules
{
    public CkJoltBakeInspector(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "Slate", "SlateCore", "RenderCore", "RHI", "InputCore", "UMG",
            "UnrealEd", "AssetRegistry", "WorkspaceMenuStructure",
            "DeveloperSettings", "PhysicsCore",
            "CkCore", "CkEcs", "CkEditorTools", "CkDebuggerCommon", "CkDebugScene", "CkJolt", "CkJoltEditor", "CkSlateLayout"
        });

        PrivateDependencyModuleNames.Add("Projects");

        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "JoltBakeInspector.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "JoltBakeInspector.ui.css"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "JoltBakeInspectorRow.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "JoltBakeInspectorRow.ui.css"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "JoltBakeInspectorLegend.ui.html"), StagedFileType.NonUFS);
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "UI", "JoltBakeInspectorLegend.ui.css"), StagedFileType.NonUFS);
    }
}
