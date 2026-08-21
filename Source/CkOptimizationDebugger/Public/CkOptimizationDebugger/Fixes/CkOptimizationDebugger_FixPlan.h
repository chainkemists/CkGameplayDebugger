#pragma once

// Depends on Fixes.h rather than the other way round: a plan's deferred write RETURNS an
// `FCkOptimizationDebugger_FixResult`, and `TFunction` needs that type complete. Fixes.h never includes this header,
// so the dependency stays one-way.
#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_Fixes.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"
#include "UObject/NameTypes.h"
#include "UObject/SoftObjectPath.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

/** ONE property a fix would write, with the value it holds now beside the value it would hold.
 *
 *  The exact before and after, per object, per property, computed WITHOUT writing anything — which is what lets the
 *  reader see what a fix does before it does it, and what lets a spec assert a fix's effect without mutating a
 *  branch. `Included` is the tick: a change the reader clears is a change the apply does not make. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_PlannedChange
{
    /** The object the property lives ON, which is frequently NOT the finding's target: a shared base material, one
     *  mesh component of an actor that carries several, the renderer settings object. */
    FSoftObjectPath ObjectPath;

    FString ObjectLabel;

    // The property's own display name, as the details panel spells it.
    FString PropertyLabel;

    FString BeforeText;
    FString AfterText;

    bool Included = true;
};

// --------------------------------------------------------------------------------------------------------------------

/** What a fix does that is NOT a property write, in one sentence.
 *
 *  Deleting actors, queuing a shader compile, writing an ini file, rebuilding Nanite data, leaving placements
 *  alone. None of these is a before/after pair, and a preview that showed only property rows would be a preview
 *  that hid the destructive half of the catalogue. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_PlannedEffect
{
    FString Description;

    /** Whether this effect is the reason the batch confirmation would fire — destruction, a config write, or a
     *  behaviour change. Drives emphasis in the preview, nothing else. */
    bool IsRisk = false;
};

// --------------------------------------------------------------------------------------------------------------------

/** What applying ONE finding's fix would do, computed without doing it.
 *
 *  The plan is also where every fix's re-validation now lives. An apply used to re-ask its check's whole condition
 *  itself; it now asks the planner, so the sentence the preview shows and the sentence a refusal prints are the
 *  same sentence by construction rather than by review. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_FixPlan
{
    // Carried whole, because `Apply_Plan` re-plans from it to detect drift between preview and press.
    FCkOptimizationDebugger_FindingRow Finding;

    FString FixVerb;

    /** False when the fix refuses — the world moved on since the scan, the condition no longer holds, a
     *  precondition rejects it. `RefusalReason` says which, in the words the status strip prints. */
    bool CanApply = false;

    FString RefusalReason;

    TArray<FCkOptimizationDebugger_PlannedChange> Changes;

    TArray<FCkOptimizationDebugger_PlannedEffect> Effects;

    /** The deferred write, set by the planner and invoked by the apply. It is handed the plan it belongs to, so it
     *  reads its own `Changes[].Included` ticks — which is what makes "a property the planner did not list is a
     *  property the apply cannot write" true by construction rather than by review. Unset on a refused plan.
     *
     *  It captures the objects the planner resolved, so an apply never re-resolves and never re-validates. That is
     *  safe because a plan is executed in the same press that built it: the preview path re-plans first (see
     *  `Apply_PreviewedPlan`), so the captured pointers are always this frame's. */
    TFunction<FCkOptimizationDebugger_FixResult(const FCkOptimizationDebugger_FixPlan&)> Execute;
};

// --------------------------------------------------------------------------------------------------------------------

/** The counts a preview header prints and a spec can pin without a world. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_PlanSummary
{
    int32 ApplicableFixCount = 0;
    int32 RefusedFixCount = 0;

    // Distinct objects at least one INCLUDED change would write to.
    int32 AffectedObjectCount = 0;

    int32 IncludedChangeCount = 0;
    int32 ExcludedChangeCount = 0;

    int32 RiskEffectCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_fixplan
{
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_IncludedChanges(
        const FCkOptimizationDebugger_FixPlan& InPlan) -> TArray<FCkOptimizationDebugger_PlannedChange>;

    /** Whether this plan would still write anything. A plan whose every change is unticked is not applied — and is
     *  not an error either; the reader said no to all of it. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_HasIncludedWork(
        const FCkOptimizationDebugger_FixPlan& InPlan) -> bool;

    /** Whether ONE named change is ticked. What each apply asks before writing a property, so a cleared tick is
     *  honoured by the code that writes rather than by the code that displays. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_IsChangeIncluded(
        const FCkOptimizationDebugger_FixPlan& InPlan,
        const FSoftObjectPath& InObjectPath,
        const FString& InPropertyLabel) -> bool;

    CKOPTIMIZATIONDEBUGGER_API auto
    Set_ChangeIncluded(
        FCkOptimizationDebugger_FixPlan& InOutPlan,
        int32 InChangeIndex,
        bool InIncluded) -> void;

    CKOPTIMIZATIONDEBUGGER_API auto
    Set_AllChangesIncluded(
        FCkOptimizationDebugger_FixPlan& InOutPlan,
        bool InIncluded) -> void;

    CKOPTIMIZATIONDEBUGGER_API auto
    Build_PlanSummary(
        const TArray<FCkOptimizationDebugger_FixPlan>& InPlans) -> FCkOptimizationDebugger_PlanSummary;

    /** Every distinct object an included change would write to, sorted by path. What the preview groups rows under
     *  and what the modified-package list is seeded from. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AffectedObjectPaths(
        const TArray<FCkOptimizationDebugger_FixPlan>& InPlans) -> TArray<FSoftObjectPath>;

    /** Whether the world still matches what the reader was shown.
     *
     *  Compares a plan built at preview time against one built now, on (object, property, BEFORE value). A preview
     *  is a promise about a moment; between that moment and the press an import can finish, another tool can write,
     *  or an undo can land — and applying the reader's ticks to a different starting value is exactly the silent
     *  wrong write the preview exists to prevent. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_HasDrifted(
        const FCkOptimizationDebugger_FixPlan& InPreviewed,
        const FCkOptimizationDebugger_FixPlan& InFresh) -> bool;

    /** The applied fixes as a commit message body: one line per fix, grouped by check, in the order they ran.
     *
     *  Plain language on purpose — the reader pastes it into a commit, and "Enable Nanite on SM_Rock" is what the
     *  next person reading `git log` needs, not a check id and a severity. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_CommitMessage(
        const TArray<FCkOptimizationDebugger_FixLogEntry>& InEntries) -> FString;

    // ----------------------------------------------------------------------------------------------------------------

    /** What applying this finding's fix WOULD do, computed without doing any of it.
     *
     *  Read-only: it loads what it must to answer (an asset target's package, as every fix already did), resolves
     *  the objects, re-asks the check's whole condition, and returns either a refusal or the change rows plus the
     *  deferred write. Outside the editor every finding plans to a refusal saying an editor session is needed. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Plan_Fix(
        const FCkOptimizationDebugger_FindingRow& InFinding,
        UWorld* InEditorWorld) -> FCkOptimizationDebugger_FixPlan;

    /** Every finding's plan, in the selection's own order. What the preview is built from. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Plan_Fixes(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings,
        UWorld* InEditorWorld) -> TArray<FCkOptimizationDebugger_FixPlan>;

    /** Applies a plan the reader has SEEN and edited.
     *
     *  Re-plans first and refuses on drift, because a preview is a promise about a moment: between the moment and
     *  the press an import can finish, another tool can write, or an undo can land. The reader's ticks are carried
     *  onto the fresh plan by (object, property) — never by index, which a drifted plan would have renumbered. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Apply_PreviewedPlan(
        const FCkOptimizationDebugger_FixPlan& InPreviewedPlan,
        UWorld* InEditorWorld) -> FCkOptimizationDebugger_FixResult;

    /** Applies a whole previewed selection: ONE outer transaction over the transactional part, the config writes
     *  after it and outside it, and the review actions last — the same partition `Apply_Fixes` uses, over plans the
     *  reader has edited rather than over raw findings. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Apply_PreviewedPlans(
        const TArray<FCkOptimizationDebugger_FixPlan>& InPreviewedPlans,
        UWorld* InEditorWorld,
        TArray<FCkOptimizationDebugger_FixLogEntry>& OutLogEntries) -> FCkOptimizationDebugger_BatchFixResult;
}

// --------------------------------------------------------------------------------------------------------------------
