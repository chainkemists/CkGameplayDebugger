#include "CkEcsDebugger.h"

#define LOCTEXT_NAMESPACE "FCkEcsDebuggerModule"

// --------------------------------------------------------------------------------------------------------------------

void FCkEcsDebuggerModule::StartupModule()
{
}

auto
    FCkEcsDebuggerModule::
    ShutdownModule()
    -> void
{
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkEcsDebuggerModule, CkEcsDebugger)
