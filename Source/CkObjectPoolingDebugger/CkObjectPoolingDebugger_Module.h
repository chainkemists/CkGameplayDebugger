#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkObjectPoolingDebuggerWindow;
class SDockTab;

// --------------------------------------------------------------------------------------------------------------------
// Object Pooling debugger module. Registers a nomad tab + an `ck.ObjectPoolingDebugger` console
// command. Mirrors FCkInputDebuggerModule. Shows the live state of the CkCore ObjectPooling
// subsystem's pools for the selected world.
// --------------------------------------------------------------------------------------------------------------------

class FCkObjectPoolingDebuggerModule : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkObjectPoolingDebuggerModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

private:
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

    TSharedPtr<SCkObjectPoolingDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;

    static const FName _DebuggerTabName;
};
