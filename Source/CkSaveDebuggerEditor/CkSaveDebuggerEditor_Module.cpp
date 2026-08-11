#include "Modules/ModuleManager.h"

#include "CkSaveDebuggerEditor/Visualizer/CkSaveDebugger_VisualizerEdMode.h"

class FCkSaveDebuggerEditorModule final : public IModuleInterface
{
public:
    auto StartupModule() -> void override
    {
        // UAssetEditorSubsystem auto-discovers non-abstract UEdMode CDOs after modules load. This explicit
        // reference prevents the linker stripping the reflected class and therefore preserves discovery.
        (void)UCk_SaveDebugger_VisualizerEdMode::StaticClass();
    }
};

IMPLEMENT_MODULE(FCkSaveDebuggerEditorModule, CkSaveDebuggerEditor)
