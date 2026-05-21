#include "CkGoapDebugger_Module.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

// ====================================================================================================================

#define LOCTEXT_NAMESPACE "FCkGoapDebuggerModule"

// ====================================================================================================================
// Phase D0: bare module shell. Style set is registered up-front so later phases
// can reference brushes/text styles via FCkGoapDebuggerStyle::Get(). No tab
// spawners, no graph factories, no console commands — those re-appear in
// subsequent phases as the matching widgets/window come back online.
// ====================================================================================================================

auto
    FCkGoapDebuggerModule::
    StartupModule()
    -> void
{
    FCkGoapDebuggerStyle::Initialize();
}

auto
    FCkGoapDebuggerModule::
    ShutdownModule()
    -> void
{
    FCkGoapDebuggerStyle::Shutdown();
}

// ====================================================================================================================

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkGoapDebuggerModule, CkGoapDebugger)
