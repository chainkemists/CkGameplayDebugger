#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkMapDebuggerWindow;
class SDockTab;
struct FCkMapDebuggerModuleTestAccess;

class FCkMapDebuggerModule : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkMapDebuggerModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

private:
    friend struct FCkMapDebuggerModuleTestAccess;
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
    auto HandleEnginePreExit() -> void;

    TSharedPtr<SCkMapDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;

    uint64 _DebuggerToolRegistrationId = 0;
    FDelegateHandle _EnginePreExitHandle;

    static const FName _DebuggerTabName;
};
