#include "CkGoapDebugger_Module.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"

// ====================================================================================================================

#define LOCTEXT_NAMESPACE "FCkGoapDebuggerModule"

// ====================================================================================================================
// Phase D1: module shell + style set + data layer. The data collector's
// singleton lifecycle is anchored here so PIE delegate (re)binding tracks
// module lifetime exactly. No tab spawners / windows yet — D2 wires those.
// ====================================================================================================================

auto
    FCkGoapDebuggerModule::
    StartupModule()
    -> void
{
    FCkGoapDebuggerStyle::Initialize();
    FCkGoapDebugger_DataCollector::Initialize();
}

auto
    FCkGoapDebuggerModule::
    ShutdownModule()
    -> void
{
    FCkGoapDebugger_DataCollector::Shutdown();
    FCkGoapDebuggerStyle::Shutdown();
}

// ====================================================================================================================

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkGoapDebuggerModule, CkGoapDebugger)
