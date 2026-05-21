#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SCkGoapDebuggerWindow;
class SDockTab;
struct FGraphPanelNodeFactory;

// ====================================================================================================================
// CkGoap Debugger module. D0 set up the module shell; D1 added the data layer;
// D2 wires the Slate window + tab spawner.
// ====================================================================================================================

class FCkGoapDebuggerModule : public IModuleInterface
{
public:
    auto StartupModule()  -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkGoapDebuggerModule&
    {
        return FModuleManager::GetModuleChecked<FCkGoapDebuggerModule>("CkGoapDebugger");
    }

    auto OpenDebugger()  -> void;
    auto CloseDebugger() -> void;
    auto ToggleDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

private:
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

    TSharedPtr<SCkGoapDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab>              _DebuggerTab;
    TSharedPtr<FGraphPanelNodeFactory> _NodeFactory;

    static const FName _DebuggerTabName;
};

// ====================================================================================================================
