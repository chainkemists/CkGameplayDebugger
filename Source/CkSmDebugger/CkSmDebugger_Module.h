#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkSmDebuggerWindow;
class SDockTab;
struct FGraphPanelNodeFactory;

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

    TSharedPtr<SCkSmDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab> _DebuggerTab;
    TSharedPtr<FGraphPanelNodeFactory> _NodeFactory;

    static const FName _DebuggerTabName;
};
