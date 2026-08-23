#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkAiDebuggerWindow;
class SDockTab;
struct FCk_Handle;

/** Launcher and entity-target route owner for the concise, cross-system AI overview. */
class FCkAiDebuggerModule final : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkAiDebuggerModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;
    auto Get_DebuggerWindow() const -> TSharedPtr<SCkAiDebuggerWindow> { return _DebuggerWindow; }

    static auto Get_TabName() -> FName { return _DebuggerTabName; }

private:
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
    auto HandleEnginePreExit() -> void;
    auto HandleGlobalSelection(const FCk_Handle& InEntity, FName InSource) -> void;

    TSharedPtr<SCkAiDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;
    uint64 _DebuggerToolRegistrationId = 0;
    uint64 _EntityTargetRouteRegistrationId = 0;
    FDelegateHandle _SelectionSyncHandle;
    FDelegateHandle _EnginePreExitHandle;

    static const FName _DebuggerTabName;
};
