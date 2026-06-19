#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkInputDebuggerWindow;
class SDockTab;

// --------------------------------------------------------------------------------------------------------------------
// Enhanced Input debugger module. Registers a nomad tab + an `ck.InputDebugger`
// console command. Mirrors FCkUIDebuggerModule.
// --------------------------------------------------------------------------------------------------------------------

class FCkInputDebuggerModule : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkInputDebuggerModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

private:
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

    TSharedPtr<SCkInputDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;

    static const FName _DebuggerTabName;
};
