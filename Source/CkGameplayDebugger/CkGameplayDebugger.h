#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCkGameplayDebuggerModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

public:
#if ALLOW_CONSOLE
    // Callback function registered with Console to inject show debug auto complete command
    static auto PopulateAutoCompleteEntries(
        TArray<struct FAutoCompleteCommand>& AutoCompleteList) -> void;

private:
    FDelegateHandle _ConsoleAutoCompleteDelegateHandle;
#endif
};
