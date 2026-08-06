#pragma once

#include "CoreMinimal.h"

// ====================================================================================================================
// Shared world-selection state for every CK debugger window.
//
// Holds nothing but "which UWorld is the user inspecting" + the list of worlds
// available to pick from. Deliberately ECS-free (no FCk_Handle / entity cache)
// so any debugger module can depend on it. The ECS debugger composes this inside
// FCkDebuggerModel_WorldContext and layers its entity cache on top.
//
// Pair with SCkDebug_WorldSelector for the button-strip UI. Worlds are tracked
// by identity (a PIE stop→restart can reuse the same UWorld* count with new
// pointers — see CkDebuggerCommon/CLAUDE.md "PIE lifecycle").
// ====================================================================================================================

DECLARE_MULTICAST_DELEGATE_OneParam(FCkDebugger_OnWorldChanged, UWorld*);

class CKDEBUGGERCOMMON_API FCkDebuggerModel_WorldSelector
{
public:
    FCkDebuggerModel_WorldSelector();
    ~FCkDebuggerModel_WorldSelector();
    FCkDebuggerModel_WorldSelector(const FCkDebuggerModel_WorldSelector&) = delete;
    auto operator=(const FCkDebuggerModel_WorldSelector&) -> FCkDebuggerModel_WorldSelector& = delete;
    FCkDebuggerModel_WorldSelector(FCkDebuggerModel_WorldSelector&&) = delete;
    auto operator=(FCkDebuggerModel_WorldSelector&&) -> FCkDebuggerModel_WorldSelector& = delete;

    auto Set_SelectedWorld(UWorld* InWorld) -> void;
    auto Get_SelectedWorld() const -> UWorld*;
    auto Get_AvailableWorlds() const -> TArray<UWorld*>;

    // If nothing is selected (fresh window) or the selection went stale (PIE
    // ended and the weak ptr is now null), pick the first available world.
    // Returns true if the selection changed as a result.
    auto Ensure_AutoSelect() -> bool;

    FCkDebugger_OnWorldChanged OnWorldChanged;

private:
    auto BroadcastWorldChanged() -> void;
    auto HandleWorldCleanup(UWorld* InWorld, bool InSessionEnded, bool InCleanupResources) -> void;

    TWeakObjectPtr<UWorld> SelectedWorld;
    FDelegateHandle WorldCleanupHandle;
};
