#pragma once

#include <CoreMinimal.h>
#include <Modules/ModuleManager.h>

class SCkProceduralAnimationDebuggerWindow;
class SDockTab;

class CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebuggerModule : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;
    static auto Get() -> FCkProceduralAnimationDebuggerModule&;
    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;
    auto Get_DebuggerWindow() const -> TSharedPtr<SCkProceduralAnimationDebuggerWindow> { return _Window; }
    static auto Get_TabName() -> FName { return TEXT("CkProceduralAnimationDebugger"); }

private:
    auto SpawnTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
    auto HandleEnginePreExit() -> void;
    TSharedPtr<SCkProceduralAnimationDebuggerWindow> _Window;
    TSharedPtr<SDockTab> _Tab;
    uint64 _ToolRegistration = 0;
    uint64 _RouteRegistration = 0;
    FDelegateHandle _PreExitHandle;
};
