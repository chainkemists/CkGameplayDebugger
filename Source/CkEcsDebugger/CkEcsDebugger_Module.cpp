#include "CkEcsDebugger_Module.h"

#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#define LOCTEXT_NAMESPACE "FCkEcsDebuggerModule"

// --------------------------------------------------------------------------------------------------------------------

auto FCkEcsDebuggerModule::StartupModule() -> void
{
    FCkDebuggerStyle::Initialize();
}

auto FCkEcsDebuggerModule::ShutdownModule() -> void
{
    FCkDebuggerStyle::Shutdown();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkEcsDebuggerModule, CkEcsDebugger)
