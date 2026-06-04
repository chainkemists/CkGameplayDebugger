#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

// --------------------------------------------------------------------------------------------------------------------
// Pure (Slate-free, ECS-light) transforms over the GOAP debugger history event stream. Kept separate from the widgets
// so they can be unit-tested headlessly. The planner display name is resolved by a caller-supplied functor.
// --------------------------------------------------------------------------------------------------------------------

namespace ck_goap_debugger_history_model
{
    // mm:ss.mmm from absolute world seconds (matches the history timestamp column).
    CKGOAPDEBUGGER_API auto Format_Timestamp(double InWorldSeconds) -> FString;

    // Short kind tag for text surfaces: ACT / DEACT / PLAN / FAIL / RESET / ON / OFF / CHAIN.
    CKGOAPDEBUGGER_API auto KindTag(ECkGoapDebugger_HistoryEventKind InKind) -> FString;

    // Plain aligned log-line serialization of the given events (already in display order). Header line, then
    // one raw event per line: "  <ts>  <planner>  <KIND>  <action>  <meta>".
    CKGOAPDEBUGGER_API auto SerializeHistory(
        const FString& InHeaderLine,
        const TArray<FCkGoapDebugger_HistoryEvent>& InEvents,
        const TFunctionRef<FString(const FCk_Handle_Goap_Planner&)> InPlannerName) -> FString;

    // Minimum alternating ACT/DEACT events between exactly two actions to collapse into a flap row.
    inline constexpr int32 k_FlapMinRun = 4;

    // Group events by planner (ActionSetHandle), preserving chronological order within each group, and
    // run-length-collapse flap storms (a run of ACT/DEACT alternating between exactly two actions).
    // PlannerName resolved via the supplied functor.
    CKGOAPDEBUGGER_API auto BuildPlannerGroups(
        const TArray<FCkGoapDebugger_HistoryEvent>& InEvents,
        const TFunctionRef<FString(const FCk_Handle_Goap_Planner&)> InPlannerName)
        -> TArray<FCkGoapDebugger_PlannerGroup>;
}
