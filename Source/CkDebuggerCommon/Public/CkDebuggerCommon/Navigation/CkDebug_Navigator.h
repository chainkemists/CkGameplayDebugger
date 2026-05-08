#pragma once

#include "CoreMinimal.h"

struct FCk_Handle;

// ====================================================================================================================
// Cross-debugger navigation hook.
//
// CkDebuggerCommon is the lowest-tier debugger module — it cannot directly
// depend on CkEcsDebugger (or any sibling debugger module) without inverting
// the dependency graph. To still let widgets in this module trigger
// "open the ECS Debugger and select this entity", the ECS Debugger module
// registers an implementation function on startup, and the widget calls
// Goto_Entity which delegates to that function (or no-ops if nothing is
// registered, e.g. in a cooked client where CkEcsDebugger is absent).
//
// This is the standard one-way registration pattern: low-tier module owns
// the slot, high-tier module fills it.
// ====================================================================================================================

namespace ck::DebugNav
{
    using FGotoEntityFn = TFunction<void(const FCk_Handle&)>;

    // Called by FCkEcsDebuggerModule::StartupModule to install the implementation.
    // Passing an unbound TFunction clears the registration (used in ShutdownModule).
    CKDEBUGGERCOMMON_API auto Register_EntityNavigator(FGotoEntityFn InFn) -> void;

    // Called by widgets (e.g. SCkDebug_EntityRef) when the user clicks an entity
    // reference. No-op when nothing is registered or the handle is invalid.
    CKDEBUGGERCOMMON_API auto Goto_Entity(const FCk_Handle& InEntity) -> void;

    // Lets a caller adjust UI (tooltip, hover state) when the click would be a no-op.
    CKDEBUGGERCOMMON_API auto Has_EntityNavigator() -> bool;
}

// ====================================================================================================================
