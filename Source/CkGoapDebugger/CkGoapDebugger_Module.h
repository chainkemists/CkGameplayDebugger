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

    // Used by SCkGoapDebuggerWindow::OpenForEntity (D7) to reach the live window
    // instance after spawning the tab.
    auto Get_DebuggerWindow() const -> TSharedPtr<SCkGoapDebuggerWindow> { return _DebuggerWindow; }

    // Tab name registered with the global tab manager.
    static auto Get_TabName() -> FName { return _DebuggerTabName; }

private:
    auto OnSpawnDebuggerTab(const class FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

    TSharedPtr<SCkGoapDebuggerWindow> _DebuggerWindow;
    TSharedPtr<SDockTab>              _DebuggerTab;
    TSharedPtr<FGraphPanelNodeFactory> _NodeFactory;

    uint64 _DebuggerToolRegistrationId = 0;

    static const FName _DebuggerTabName;
};

// ====================================================================================================================
