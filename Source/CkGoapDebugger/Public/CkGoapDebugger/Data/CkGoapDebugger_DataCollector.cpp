#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"

#include "Engine/World.h"

// ====================================================================================================================
//
// TODO(CkGoap-BundleTierRefactor): full data-collector rewrite pending.
//
// The CkGoap planner-per-entity model is being replaced by a Bundle/Tier model
// (see docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md).
// This data collector previously read FFragment_Goap_Current /
// FFragment_Goap_Params / FFragment_Goap_Actions / FFragment_Goap_Goals
// directly off planner entities. Those fragments are being removed in
// Phase 2 of the refactor.
//
// The debugger redesign (which will introduce Bundle catalog views, active-
// tier chain breadcrumbs, per-tier plan inspection, and the new WS-source-
// resolution view) is scoped to a separate follow-up spec that will be
// authored after the refactor implementation lands.
//
// Until then, this stub keeps the module compiling and the UI shells alive
// without functional data. All collectors return empty containers.
//
// ====================================================================================================================

auto
    FCkGoapDebugger_DataCollector::
    Collect(UWorld* /*InWorld*/)
    -> void
{
    _GoapEntities.Reset();
    // No-op until debugger redesign lands.
}

// ====================================================================================================================

auto
    FCkGoapDebugger_DataCollector::
    CollectGoapEntity(
        FCk_Handle /*InHandle*/)
    -> void
{
    // No-op until debugger redesign lands.
}

// ====================================================================================================================

auto
    FCkGoapDebugger_DataCollector::
    TrackPlanCompletion(
        const FCkGoapDebugger_GoapInfo& /*InInfo*/)
    -> void
{
    // No-op until debugger redesign lands.
}

// ====================================================================================================================

auto
    FCkGoapDebugger_DataCollector::
    TrackSearchProgress(
        const FCkGoapDebugger_GoapInfo& /*InInfo*/)
    -> void
{
    // No-op until debugger redesign lands.
}

// ====================================================================================================================
