#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

// --------------------------------------------------------------------------------------------------------------------

/** The three things the cleanup page can DO, as opposed to the four things it can show.
 *
 *  There is deliberately no "delete everything unreferenced" and no "tidy the project": every action here is one
 *  engine operation over a set the reader chose, and each one ends in a dialog the engine itself owns. */
enum class ECkOptimizationDebugger_CleanupAction : uint8
{
    /** `ObjectTools::DeleteAssets(Assets, bShowConfirmation = true)`. The confirmation is the ENGINE's own delete
     *  dialog — the one that lists referencers and offers Force Delete — and it is never bypassed. */
    DeleteAssets,

    /** `IAssetTools::FixupReferencers` over the selected redirectors: rewrites every referencing package to point at
     *  the real asset, then deletes the redirector. */
    FixUpRedirectors,

    /** `FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave = true, ...)` — the editor's own save-checklist dialog,
     *  which is where the reader picks what actually gets written. */
    SaveDirtyPackages,
};

// --------------------------------------------------------------------------------------------------------------------

/** What one cleanup action is, before it is run. Registry data, not behaviour: the page reads it to decide what to
 *  put on a button, which rows the button applies to, and how loudly to warn. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_CleanupActionInfo
{
    ECkOptimizationDebugger_CleanupAction Action = ECkOptimizationDebugger_CleanupAction::DeleteAssets;

    /** Stable identity, `Cleanup.<Verb>`. What the page keys a button by and what a spec names an action with. */
    FName Id;

    /** The verb a button names this action by. Imperative and specific — "Delete Selected", never "Clean". */
    FString DisplayVerb;

    /** What it does and what the reader will be asked before it happens. Becomes the button's hover text. */
    FString Description;

    /** Whether it removes content rather than editing or writing it. The page says so before it runs. */
    bool IsDestructive = false;

    /** Whether the action operates on the SELECTION. False for the save action alone: the editor's own save dialog is
     *  a checklist over every dirty package, so a selection here would promise a narrowing the engine does not offer.
     *  Saying so in the tooltip beats a button that quietly saves more than was selected. */
    bool OperatesOnSelection = true;

    /** The categories whose rows this action accepts. A selection that mixes categories is narrowed to these, never
     *  refused wholesale — the reader who rubber-banded a list is asking about the rows the button understands. */
    TArray<ECkOptimizationDebugger_CleanupCategory> Categories;
};

// --------------------------------------------------------------------------------------------------------------------

/** What running an action did. A failure is ordinary — outside an editor session there is nothing to act on, and a
 *  reader who cancelled the engine's confirmation dialog has not failed at anything. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_CleanupActionResult
{
    bool DidRun = false;

    FString Message;

    /** Whether the page should re-scan now. False when the action was HANDED OFF rather than completed —
     *  `FixupReferencers` defers to a completion callback while the asset registry is still loading, and re-scanning
     *  on top of an operation that has not started yet would produce a census that is wrong twice over. */
    bool ShouldRefresh = true;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_cleanup_commands
{
    // ---- Catalog ----

    /** Every action, in the order the page offers them. One immutable table. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_AllActions() -> const TArray<FCkOptimizationDebugger_CleanupActionInfo>&;

    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_Action(
        ECkOptimizationDebugger_CleanupAction InAction) -> const FCkOptimizationDebugger_CleanupActionInfo*;

    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_ActionById(
        FName InId) -> const FCkOptimizationDebugger_CleanupActionInfo*;

    /** The action a category's rows are acted on by, or null when the category has none. Exactly one action claims
     *  each category, which is what lets the page show ONE action button per sub-tab rather than three that mostly
     *  disable. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_ActionForCategory(
        ECkOptimizationDebugger_CleanupCategory InCategory) -> const FCkOptimizationDebugger_CleanupActionInfo*;

    CKOPTIMIZATIONDEBUGGER_API auto
    Accepts_Category(
        const FCkOptimizationDebugger_CleanupActionInfo& InAction,
        ECkOptimizationDebugger_CleanupCategory InCategory) -> bool;

    // ---- Pure rules ----

    /** The part of a selection this action understands, in the selection's own order. What the button counts. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_ApplicableRows(
        const FCkOptimizationDebugger_CleanupActionInfo& InAction,
        const TArray<FCkOptimizationDebugger_CleanupRow>& InRows) -> TArray<FCkOptimizationDebugger_CleanupRow>;

    /** Whether the button is live. Editor availability is deliberately NOT part of this: it is a session fact, not a
     *  property of the selection, and mixing the two would make a spec need an editor to assert a selection rule.
     *  The page ANDs this with `Get_CanRunActions()`. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Can_RunAction(
        const FCkOptimizationDebugger_CleanupActionInfo& InAction,
        const TArray<FCkOptimizationDebugger_CleanupRow>& InRows) -> bool;

    /** Why the button is disabled, in one sentence, or an empty string when it is not. A disabled control that cannot
     *  say why is a control the reader concludes is broken. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_ActionDisabledReason(
        const FCkOptimizationDebugger_CleanupActionInfo& InAction,
        const TArray<FCkOptimizationDebugger_CleanupRow>& InRows) -> FString;

    /** What the button says for a selection of this size: the verb alone when there is one row, a counted label when
     *  there are several, and the plain verb when the action does not read the selection at all. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_ActionButtonLabel(
        const FCkOptimizationDebugger_CleanupActionInfo& InAction,
        const TArray<FCkOptimizationDebugger_CleanupRow>& InApplicableRows) -> FString;

    // ---- Execution ----

    /** Whether there is an editor session to act in, and no play session in the way.
     *
     *  False in a packaged Development build, where this module still ships but has no Content Browser, no
     *  transaction buffer and nothing to delete — and false during PIE, where deleting an asset the running session
     *  has loaded, or saving packages out from under it, is a decision nobody asked for while the game is playing. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_CanRunActions() -> bool;

    /** Why actions are unavailable, in one sentence, or an empty string when they are not. A disabled button that
     *  cannot say why is a button the reader concludes is broken — and "not here" and "not while you are playing"
     *  are two different answers. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_ActionsUnavailableReason() -> FString;

    /** Runs the action over the applicable part of the rows it was handed.
     *
     *  **Deletion always goes through the engine's own confirmation dialog** — `bShowConfirmation` is passed `true`
     *  and there is no parameter here that can turn it off. That dialog is where the reader inspects references and
     *  decides; this module's job ends at putting the right set in front of it. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Run_Action(
        const FCkOptimizationDebugger_CleanupActionInfo& InAction,
        const TArray<FCkOptimizationDebugger_CleanupRow>& InRows) -> FCkOptimizationDebugger_CleanupActionResult;
}

// --------------------------------------------------------------------------------------------------------------------
