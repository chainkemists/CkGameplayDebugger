#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkIntentDebuggerWindow;
class SDockTab;

// --------------------------------------------------------------------------------------------------------------------
// CK Intent Debugger module. Registers a nomad tab, the `ck.IntentDebugger` console command, and the launcher
// descriptor. Mirrors FCkInputDebuggerModule's shape, plus the OnEnginePreExit teardown every handle-holding
// debugger owes the registry.
// --------------------------------------------------------------------------------------------------------------------

class FCkIntentDebuggerModule : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkIntentDebuggerModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

    // Used by SCkIntentDebuggerWindow::OpenForEntity to reach the live window after spawning the tab.
    auto Get_DebuggerWindow() const -> TSharedPtr<SCkIntentDebuggerWindow> { return _DebuggerWindow; }

    static auto Get_TabName() -> FName { return _DebuggerTabName; }

private:
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
    auto HandleEnginePreExit() -> void;

    TSharedPtr<SCkIntentDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;

    uint64 _DebuggerToolRegistrationId = 0;
    uint64 _EntityTargetRouteRegistrationId = 0;
    FDelegateHandle _EnginePreExitHandle;

    static const FName _DebuggerTabName;
};

// --------------------------------------------------------------------------------------------------------------------
