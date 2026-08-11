#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkSchedulerDebuggerWindow;
class SDockTab;
#if WITH_EDITOR
struct FGraphPanelNodeFactory;
#endif

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
#if WITH_EDITOR
    TSharedPtr<FGraphPanelNodeFactory> _NodeFactory;
#endif

    uint64 _DebuggerToolRegistrationId = 0;

    static const FName _DebuggerTabName;
};
