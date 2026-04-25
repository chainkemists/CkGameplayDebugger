#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------
// In-world debug overlay for nav agents. Reads `Ck.NavDebugger.*` cvars to determine which
// layers to draw (paths, projections, capsules, status text, navmesh bounds).
//
// Gating (so we don't pollute the viewport when the user isn't debugging):
//   - Overlay draws when the debugger window is open (FCkNavDebuggerModule::IsDebuggerOpen).
//   - OR when `Ck.NavDebugger.OverlayAlwaysOn 1` is set (opt-in headless overlay).
// --------------------------------------------------------------------------------------------------------------------

class FCkNavDebugger_WorldDraw
{
public:
    static auto
    DrawAll(
        UWorld* InWorld) -> void;

    // Module flips this when the debugger window opens / closes. WorldDraw uses it
    // as one of the gates that decides whether to render the overlay.
    static auto
    Set_WindowOpen(
        bool InOpen) -> void;

    static auto
    Get_WindowOpen() -> bool;
};

// --------------------------------------------------------------------------------------------------------------------
