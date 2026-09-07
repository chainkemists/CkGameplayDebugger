#pragma once

#include "Modules/ModuleManager.h"

class FCkHangMonitorController;
class SDockTab;
class SWindow;
class FSpawnTabArgs;

class FCkHangMonitorModule : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkHangMonitorModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;
    auto Get_Controller() -> FCkHangMonitorController&;

private:
    auto OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
    auto OnEnginePreExit() -> void;

    TUniquePtr<FCkHangMonitorController> _Controller;
    TSharedPtr<SDockTab> _DebuggerTab;
    uint64 _DebuggerToolRegistrationId = 0;
    FDelegateHandle _EnginePreExitHandle;

    static const FName _DebuggerTabName;
};
