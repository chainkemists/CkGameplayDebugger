#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkOptimizationDebuggerWindow;
class SDockTab;

class FCkOptimizationDebuggerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static auto Get() -> FCkOptimizationDebuggerModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

private:
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

    TSharedPtr<SCkOptimizationDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;

    uint64 _DebuggerToolRegistrationId = 0;

    static const FName _DebuggerTabName;
};
