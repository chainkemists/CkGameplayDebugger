#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkVisualLodDebuggerWindow;
class SDockTab;

class FCkVisualLodDebuggerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static auto Get() -> FCkVisualLodDebuggerModule&;

    static auto Get_TabName() -> FName;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

private:
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

    // The window holds FCk_Handles (arbiter + member snapshots). By ShutdownModule the ECS registry's
    // shared state is already freed and ~FCk_Handle would release a dangling shared reference, so the
    // Slate tree goes down here instead.
    auto HandleEnginePreExit() -> void;

    TSharedPtr<SCkVisualLodDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;

    uint64 _DebuggerToolRegistrationId = 0;
    uint64 _EntityTargetRouteRegistrationId = 0;

    FDelegateHandle _EnginePreExitHandle;

    static const FName _DebuggerTabName;
};
