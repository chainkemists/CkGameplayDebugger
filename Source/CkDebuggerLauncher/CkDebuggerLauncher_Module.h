#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SCkDebuggerLauncher;
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

    static const FName& LauncherTabName;

private:
    auto OnSpawnLauncherTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>;

    TSharedPtr<SCkDebuggerLauncher> _LauncherWindow;
    TSharedPtr<SDockTab> _LauncherTab;
};
