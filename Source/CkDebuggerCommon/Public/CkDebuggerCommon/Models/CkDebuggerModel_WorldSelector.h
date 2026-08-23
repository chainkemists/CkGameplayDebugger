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

    auto
    Set_SelectedWorld(
        UWorld* InWorld) -> void;

    auto
    Get_SelectedWorld() const -> UWorld*;

    auto
    Get_AvailableWorlds() const -> TArray<UWorld*>;

    /** Opt-in for tools that can inspect the live Editor world as well as PIE/Game worlds. */
    auto
    Set_IncludeEditorWorld(
        bool InIncludeEditorWorld) -> void;

    // If nothing is selected or the selection went stale, pick an available
    // world. Editor-capable models promote Editor -> PIE/Game when play starts
    // and fall back to Editor when the playable world ends.
    // Returns true if the selection changed as a result.
    auto
    Ensure_AutoSelect() -> bool;

    FCkDebugger_OnWorldChanged OnWorldChanged;

private:
    auto
    Set_AutoSelectedWorld(
        UWorld* InWorld) -> void;

    auto
    BroadcastWorldChanged() -> void;

    auto
    HandleWorldCleanup(
        UWorld* InWorld,
        bool InSessionEnded,
        bool InCleanupResources) -> void;

    TWeakObjectPtr<UWorld> SelectedWorld;
    bool IncludeEditorWorld = false;
    bool SelectedWorldIsAutomatic = false;
    FDelegateHandle WorldCleanupHandle;
};
