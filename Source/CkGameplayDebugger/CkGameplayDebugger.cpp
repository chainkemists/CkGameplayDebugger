#include "CkGameplayDebugger.h"

#include "CkGameplayDebugger/Category/CkDebugger_Category.h"

#define LOCTEXT_NAMESPACE "FCkGameplayDebuggerModule"

#if WITH_GAMEPLAY_DEBUGGER
#include <GameplayDebugger.h>
#endif

// --------------------------------------------------------------------------------------------------------------------

void FCkGameplayDebuggerModule::StartupModule()
{
#if WITH_GAMEPLAY_DEBUGGER
    //If the gameplay debugger is available, register the category and notify the editor about the changes
    IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();

    GameplayDebuggerModule.RegisterCategory
    (
        "CkGameplayDebugger",
        IGameplayDebugger::FOnGetCategory::CreateStatic(&FCk_GameplayDebugger_Category::MakeInstance),
        EGameplayDebuggerCategoryState::EnabledInGameAndSimulate
    );

    GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

auto
    FCkGameplayDebuggerModule::
    ShutdownModule()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    //If the gameplay debugger is available, unregister the category
    if (IGameplayDebugger::IsAvailable())
    {
        IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
        GameplayDebuggerModule.UnregisterCategory("CkGameplayDebugger");
    }
#endif
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkGameplayDebuggerModule, CkGameplayDebugger)
