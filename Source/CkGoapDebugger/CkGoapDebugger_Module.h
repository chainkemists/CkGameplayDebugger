#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// ====================================================================================================================
// Minimal module shell for the rewritten CkGoap debugger. The pre-unification
// implementation (Goal-centric panels, graph factories, ViewModel, style-test
// tab) has been removed. Subsequent phases (D1+) re-add the data collector,
// window, panels, and any graph/visual factories incrementally.
// ====================================================================================================================

class FCkGoapDebuggerModule : public IModuleInterface
{
public:
    auto StartupModule() -> void override;
    auto ShutdownModule() -> void override;

    static auto Get() -> FCkGoapDebuggerModule&
    {
        return FModuleManager::GetModuleChecked<FCkGoapDebuggerModule>("CkGoapDebugger");
    }
};

// ====================================================================================================================
