#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkSmDebuggerWindow;
class SDockTab;

class FCkSmDebuggerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static auto Get() -> FCkSmDebuggerModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

private:
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
    auto OnTabForegrounded(
        TSharedPtr<SDockTab> InNewForegroundTab,
        TSharedPtr<SDockTab>) -> void;

    TSharedPtr<SCkSmDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;

    uint64 _DebuggerToolRegistrationId = 0;
    uint64 _EntityTargetRouteRegistrationId = 0;
    FDelegateHandle _SelectionSyncHandle;
    FDelegateHandle _TabForegroundedHandle;

    static const FName _DebuggerTabName;
};
