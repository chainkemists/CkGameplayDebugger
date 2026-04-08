#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkSchedulerDebuggerWindow;
class SDockTab;
struct FGraphPanelNodeFactory;

class FCkSchedulerDebuggerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static auto Get() -> FCkSchedulerDebuggerModule&;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

private:
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

    TSharedPtr<SCkSchedulerDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;
    TSharedPtr<FGraphPanelNodeFactory> _NodeFactory;

    static const FName _DebuggerTabName;
};
