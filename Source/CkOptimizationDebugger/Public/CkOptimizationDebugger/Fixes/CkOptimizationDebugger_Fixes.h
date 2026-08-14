#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

/** What applying one fix did, said in the words the status strip will print. A failure is NOT an error condition —
 *  a finding whose world moved on since the scan is the ordinary case, and the reader is told which one it was. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_FixResult
{
    bool Succeeded = false;

    FString Message;

    // Whether anything on disk or in the world actually changed. A review-style fix succeeds by SHOWING the reader
    // something, and re-scanning after one would re-derive the same findings from the same unchanged level.
    bool ChangedState = false;
};

// --------------------------------------------------------------------------------------------------------------------

/** Where a fix's change lands, which is what decides both the undo story and the order the batch runs it in.
 *
 *  Three values rather than a `bool IsTransactional`, because "not transactional" was covering two genuinely
 *  different things: an ini write Undo cannot reach, and a review action that changes nothing at all. Folding them
 *  together put a selection action inside a bucket named `ConfigWrite`. */
enum class ECkOptimizationDebugger_FixExecution : uint8
{
    /** Mutates an asset or a level, inside an `FScopedTransaction`. Ctrl+Z reverses it. */
    Transactional,

    /** Writes a config file. Outside the transaction buffer, so it is applied after the record and never inside it —
     *  holding one in an undo record would promise a Ctrl+Z that silently does nothing. */
    ConfigWrite,

    /** Changes NOTHING. It shows the reader something — today, selecting the actors a finding is about. It runs LAST
     *  in a batch so the selection it leaves behind is the one the reader ends up looking at. */
    Review,
};

// --------------------------------------------------------------------------------------------------------------------

/** What a check's fix is, before it is applied. Registry data, not behaviour: the UI reads it to decide what to put
 *  on a button and how to warn, and the batch path reads it to decide what goes inside the undo record. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_FixInfo
{
    FName CheckId;

    // The verb a button or menu entry names this fix by. Imperative and specific — "Enable Nanite", never "Fix".
    FString DisplayVerb;

    // Where the change lands. Drives the batch partition, the batch's run order and whether a transaction is opened.
    ECkOptimizationDebugger_FixExecution Execution = ECkOptimizationDebugger_FixExecution::Transactional;

    // Whether the fix destroys or replaces existing objects rather than editing a property. `Build_BatchConfirmation`
    // turns this into the prompt a batch shows before it runs; "convert 40 actors into instances" is not the same
    // promise as "tick a checkbox".
    bool IsDestructive = false;
};

// --------------------------------------------------------------------------------------------------------------------

/** What applying a SET of fixes did. Counts rather than a bool because a batch that half-worked is the common case
 *  and the honest thing to report. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_BatchFixResult
{
    int32 SucceededCount = 0;
    int32 FailedCount = 0;

    // True when at least one applied fix actually changed something — the re-scan trigger.
    bool ChangedState = false;

    // One line per attempted fix, in the order they were attempted.
    TArray<FString> Messages;
};

// --------------------------------------------------------------------------------------------------------------------

/** A batch split by where each fix's change lands. Config writes are separated rather than smuggled inside the
 *  transaction: putting a `DefaultEngine.ini` edit inside an undo record would promise the reader a Ctrl+Z that
 *  silently does nothing. Review actions are separated from BOTH, because they are not writes at all — and because
 *  the one that exists replaces the editor selection, so it has to run last or it would wipe its own result.
 *
 *  `Apply_Fixes` runs them in field order: Transactional (one record), then ConfigWrite, then Review. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_FixBatchPartition
{
    TArray<FCkOptimizationDebugger_FindingRow> Transactional;
    TArray<FCkOptimizationDebugger_FindingRow> ConfigWrite;
    TArray<FCkOptimizationDebugger_FindingRow> Review;
};

// --------------------------------------------------------------------------------------------------------------------

/** What a batch must ask the reader before it runs, or an unset prompt when it may run unannounced.
 *
 *  A batch that deletes actors or rewrites a shared config file is not the same click as one that ticks a checkbox,
 *  and the difference has to be visible BEFORE it happens rather than in the status strip afterwards. Pure over the
 *  selection, so a spec can pin which selections prompt. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_FixConfirmation
{
    bool IsRequired = false;

    // The dialog's title and body. Empty when nothing is required.
    FString Title;
    FString Body;

    int32 DestructiveCount = 0;
    int32 ConfigWriteCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_fixes
{
    /** Every fix this module can apply, keyed by the check that produces the finding. The check sets `HasAutoFix`
     *  and describes the action in `FixDescription`; THIS is where the action actually lives, so a check id in one
     *  list and not the other is a defect `Ck.OptimizationDebugger.Fixes.RegistryCoverage` fails on. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllFixes() -> const TArray<FCkOptimizationDebugger_FixInfo>&;

    /** The registry entry for a check, or null when the check has no fix. Null is an ordinary answer: most of the
     *  twenty-eight checks explain something the reader acts on by hand. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_FixInfo(
        FName InCheckId) -> const FCkOptimizationDebugger_FixInfo*;

    CKOPTIMIZATIONDEBUGGER_API auto
    Has_Fix(
        FName InCheckId) -> bool;

    /** Whether THIS finding can be applied: the check must have a registered fix AND the finding must have been
     *  flagged by the check that produced it. Both halves matter — a finding built before a check learned to set
     *  `HasAutoFix` must not sprout a button because the registry grew one. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Can_ApplyFix(
        const FCkOptimizationDebugger_FindingRow& InFinding) -> bool;

    /** The applicable subset of a selection, in the selection's own order. What the Apply button counts. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_FixableFindings(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings) -> TArray<FCkOptimizationDebugger_FindingRow>;

    /** Splits the applicable findings by `Execution`, dropping anything that has no fix. Order inside each part is
     *  the input's own. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Partition_ForBatch(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings) -> FCkOptimizationDebugger_FixBatchPartition;

    /** What the reader must agree to before this selection is applied, or an unset confirmation when it may run
     *  unannounced. Pure: the window turns a required confirmation into a dialog, and a spec pins which selections
     *  need one without opening anything. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_BatchConfirmation(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings) -> FCkOptimizationDebugger_FixConfirmation;

    /** What the Apply button says for a selection of this size: the fix's own verb when there is exactly one, a
     *  counted label when there are several, and a disabled-state label when there are none. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_FixButtonLabel(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFixableFindings) -> FString;

    // ----------------------------------------------------------------------------------------------------------------

    /** Whether there is an editor session to apply a fix in, and no play session in the way. Same shape and the same
     *  two reasons as the cleanup page's gate. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CanApplyFixes() -> bool;

    /** Why fixes are unavailable, in one sentence, or an empty string when they are not. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_FixesUnavailableReason() -> FString;

    /** Applies one finding's fix, inside its own transaction when the registry says it is transactional.
     *
     *  Outside the editor every fix fails with a message saying so — the module ships in packaged
     *  Development/DebugGame targets, where there is no transaction buffer and no asset to edit. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Apply_Fix(
        const FCkOptimizationDebugger_FindingRow& InFinding,
        UWorld* InEditorWorld) -> FCkOptimizationDebugger_FixResult;

    /** Applies a whole selection: ONE outer transaction over the transactional part, then the config writes after
     *  it and outside it. A reader who applied six fixes with one click expects one Ctrl+Z to put the level back. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Apply_Fixes(
        const TArray<FCkOptimizationDebugger_FindingRow>& InFindings,
        UWorld* InEditorWorld) -> FCkOptimizationDebugger_BatchFixResult;
}

// --------------------------------------------------------------------------------------------------------------------
