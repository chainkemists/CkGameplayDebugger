#pragma once

#include "CkEqsDebugger/Data/CkEqsDebugger_Types.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CkGrid/2dGridSystem/Grid/Ck2dGridSystem_Fragment_Data.h"   // FCk_Handle_2dGridSystem

#include "CoreMinimal.h"

class UWorld;
class UCkEqsDebuggerSettings;

// --------------------------------------------------------------------------------------------------------------------
// In-world PMG overlay for the currently-selected query. Owns a single transient parent entity per PIE world; child
// shape entities (spheres, line segments) cascade-destroy when the parent is destroyed (CkPmg/CLAUDE.md "live-tracking
// overlay" pattern).
//
// Per-feature visibility is gated by UCkEqsDebuggerSettings (per-user, EditorPerProjectUserSettings) so the toolbar's
// View-menu checkboxes give immediate, persistent control over what's shown.
//
// PIE lifetime: the parent entity lives in the editor's TransientOwner pool, so it cascade-destroys when PIE ends.
// We also explicitly Reset() in the window's EndPIE handler to release the handle BEFORE the registry tears down
// (CkSmDebugger handle-lifetime contract).
// --------------------------------------------------------------------------------------------------------------------

class FCkEqsDebugger_OverlayManager
{
public:
    // Rebuild the overlay from the given query info. Cheap to call per-frame for typical query sizes (256 candidates
    // × 2 sphere ops + 1 line ≈ < 1 ms). Per-frame rebuild is fine for v1.1; if perf becomes a problem with very
    // large queries, switch to incremental updates keyed on candidate count + selection delta.
    // InAllQueries is consulted only when InSettings->bShow_AllQueriesAlways is on — in that mode the overlay
    // ignores the selection entirely and draws every query in the registry. Pass nullptr (or an empty array)
    // when you don't have the all-queries list handy; the overlay just falls back to the selection-only path.
    auto
    Update(
        UWorld*                                       InWorld,
        const TArray<FCkEqsDebugger_QueryInfo>*       InAllQueries,
        const FCkEqsDebugger_QueryInfo*               InSelectedQuery,
        const UCkEqsDebuggerSettings*                 InSettings) -> void;

    // Tear down the overlay entity (and all its child shapes via cascade). Call from PIE end / window close /
    // selection cleared / overlay master-toggle off.
    auto
    Reset() -> void;

private:
    auto
    EnsureParentEntity(
        UWorld* InWorld) -> bool;

    auto
    Rebuild(
        const FCkEqsDebugger_QueryInfo& InQuery,
        const UCkEqsDebuggerSettings&   InSettings) -> void;

    // Hashes the settings fields that change what's drawn so Update() can skip rebuilding when nothing relevant
    // changed (selection same, candidate count same, settings same) — avoids per-tick destroy+recreate of the
    // overlay entity which would cost PMG procmesh allocations every frame and flicker.
    auto
    ComputeSettingsHash(
        const UCkEqsDebuggerSettings& InSettings) const -> uint32;

    // Single transient parent — destroying this cascade-destroys every child shape. Created lazily on the first
    // Update(World) call.
    FCk_Handle _OverlayParent;

    // CkGrid 2dGridSystem entities per query (SimpleGrid / Grid only). Created during Rebuild as children of
    // _OverlayParent; cascade-destroyed when the parent is destroyed. Update() walks this map every tick to
    // call DebugDraw_Grid (which is one-frame and needs to be re-issued each tick to stay visible).
    TMap<FCk_Handle_EqsQuery, FCk_Handle_2dGridSystem> _GridsPerQuery;

    // Cached "what's currently drawn" state. Update() compares against these; if everything matches we skip the
    // teardown + rebuild. Reset on Reset(). _LastShownAllMode discriminates selection-mode caches from
    // all-queries-mode caches so a mode toggle always forces a rebuild.
    bool                _LastShownAllMode        = false;
    FCk_Handle_EqsQuery _LastShownHandle;
    int32               _LastShownCandidateCount = -1;
    bool                _LastShownHasResults     = false;
    FVector             _LastShownBestLocation   = FVector::ZeroVector;
    uint32              _LastShownSettingsHash   = 0;
};

// --------------------------------------------------------------------------------------------------------------------
