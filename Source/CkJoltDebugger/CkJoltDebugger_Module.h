#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkJoltDebuggerWindow;
class SDockTab;

class FCkJoltDebuggerModule : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkJoltDebuggerModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

private:
    /*
     * The nomad tab id — defined once, by the window, because the entity-target route, the launcher descriptor,
     * the chrome's Sync action and the tab spawner must all name the SAME tab. A function rather than a static
     * of its own: a second FName constant here could initialise before the window's and silently copy nothing.
     */
    static auto Get_DebuggerTabName() -> FName;

    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
    auto HandleEnginePreExit() -> void;

    TSharedPtr<SCkJoltDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;

    FDelegateHandle _EnginePreExitHandle;

    uint64 _DebuggerToolRegistrationId = 0;
    uint64 _EntityTargetRouteRegistrationId = 0;
};
