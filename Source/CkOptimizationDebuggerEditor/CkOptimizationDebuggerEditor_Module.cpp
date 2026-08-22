#include "Modules/ModuleManager.h"

#include "CkOptimizationDebuggerEditor/Heatmap/CkPerfLab_HeatmapEdMode.h"

class FCkOptimizationDebuggerEditorModule final : public IModuleInterface
{
public:
    auto StartupModule() -> void override
    {
        // UAssetEditorSubsystem auto-discovers non-abstract UEdMode CDOs after modules load. This explicit
        // reference prevents the linker stripping the reflected class and therefore preserves discovery.
        (void)UCk_PerfLab_HeatmapEdMode::StaticClass();
    }
};

IMPLEMENT_MODULE(FCkOptimizationDebuggerEditorModule, CkOptimizationDebuggerEditor)
