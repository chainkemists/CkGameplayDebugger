#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCkSlateDebuggerModule : public IModuleInterface
{
public:

public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    auto OpenDebugger() -> void;
    auto CloseDebugger() -> void;
    auto IsDebuggerOpen() const -> bool;

private:
    auto RegisterConsoleCommands() -> void;
    auto UnregisterConsoleCommands() -> void;

private:
    TSharedPtr<class SCkSlateDebuggerWindow> DebuggerWindow;
    TSharedPtr<class SWindow> SlateWindow;
    TArray<class IConsoleObject*> ConsoleCommands;
};
