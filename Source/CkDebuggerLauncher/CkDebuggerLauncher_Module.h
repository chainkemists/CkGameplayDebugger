#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SCkDebuggerLauncher;
class SCkDebuggerSuiteWindow;
class SDockTab;

// --------------------------------------------------------------------------------------------------------------------

class FCkDebuggerLauncherModule : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkDebuggerLauncherModule&;

    auto OpenLauncher() -> void;
    auto CloseLauncher() -> void;
    auto ToggleLauncher() -> void;
    auto IsLauncherOpen() const -> bool;

    auto OpenSuite() -> void;
    auto CloseSuite() -> void;
    auto ToggleSuite() -> void;
    auto IsSuiteOpen() const -> bool;

    static const FName& LauncherTabName;

    // Deliberately NOT an FCkDebuggerToolDescriptor: the catalog spec pins the exact census of
    // registered tools, and the suite is a host for those tools rather than one of them. It gets
    // the same treatment as the launcher itself — a bare nomad spawner plus its Tools > Debug entry.
    // Aliases ck::debugger_tabs::SuiteTabId (Common owns tab ids so widget code can invoke them).
    static const FName& SuiteTabName;

private:
    auto OnSpawnLauncherTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;
    auto OnSpawnSuiteTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

    auto HandleEnginePreExit() -> void;

    TSharedPtr<SCkDebuggerLauncher> _LauncherWindow;
    TSharedPtr<SDockTab> _LauncherTab;

    TSharedPtr<SCkDebuggerSuiteWindow> _SuiteWindow;
    TSharedPtr<SDockTab> _SuiteTab;

    FDelegateHandle _EnginePreExitHandle;
};
