#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkSaveDebuggerWindow;
class SDockTab;
struct FCkSaveDebuggerAuthoredShellTestAccess;

class FCkSaveDebuggerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static auto Get() -> FCkSaveDebuggerModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;
    auto HandleEnginePreExit() -> void;

private:
    friend struct FCkSaveDebuggerAuthoredShellTestAccess;
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

    TSharedPtr<SCkSaveDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;

    uint64 _DebuggerToolRegistrationId = 0;
    FDelegateHandle _EnginePreExitHandle;

    static const FName _DebuggerTabName;
};
