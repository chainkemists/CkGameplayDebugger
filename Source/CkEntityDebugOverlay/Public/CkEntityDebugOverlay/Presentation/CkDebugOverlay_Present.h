#pragma once

#include "CoreMinimal.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"
#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_Selection.h"   // ck_debugoverlay::FCandidate
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"       // FCk_DebugOverlay_WorldTagInfo

struct FCk_DebugOverlay_Layout;
class  FCk_DebugOverlay_History;
class  UCk_DebugOverlay_Settings;
class  APlayerController;

// ====================================================================================================================
// Shared presentation builders for the on-screen entity debug overlay.
//
// These are the two pure builders the overlay subsystem's per-frame pipeline used to
// own as private methods. Extracted here so the ECS Debugger's viewport picker can
// reuse the exact same focus card + world tags instead of reimplementing them — the
// overlay subsystem now delegates to these, so its behaviour is unchanged.
// ====================================================================================================================

namespace ck_debugoverlay
{
    // Detailed consumers (for example a debugger overview) retain every
    // provider/source section and its value-only topology. Focus cards keep
    // the established condensed presentation by default.
    struct FCk_DebugOverlay_EntityModelBuildOptions
    {
        bool bCondensePerSourceSections = true;
        /** Preserve a provider/source section even when its current rows are empty. */
        bool bRetainEmptySourceSections = false;
    };

    // Build the focus-card model for one entity. Layout-driven providers plus the
    // always-on set (StateMachine). When InHistory is non-null, each row's value is
    // observed into it (drives the card's history trail).
    CKENTITYDEBUGOVERLAY_API auto Build_EntityModel(
        const FCk_Handle&                                    InFocusEntity,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        const FCk_DebugOverlay_Layout&                       InLayout,
        FCk_DebugOverlay_History*                            InHistory,
        double                                               InNow,
        FCk_DebugOverlay_EntityModelBuildOptions             InOptions = {}) -> FCk_DebugOverlay_EntityModel;

    // Pure option gate used by Build_EntityModel and focused model tests.
    inline auto Should_CondensePerSourceSections(
        const FCk_DebugOverlay_EntityModelBuildOptions& InOptions,
        bool                                            InMergeDuplicateRows) -> bool
    {
        return InOptions.bCondensePerSourceSections && InMergeDuplicateRows;
    }

    // Builds the one summary row that stands in for a whole sub-source section during the
    // condense pass. Build_EntityModel routes this to the owning provider's
    // Get_CondensedSourceRow; specs pass a stub so the collapse is testable without providers.
    using FCondenseSourceRowFn = TFunction<FCk_DebugOverlay_Row(
        const FGameplayTag&                 InProviderTag,
        const FText&                        InSourceName,
        const TArray<FCk_DebugOverlay_Row>& InRows)>;

    // Pure collapse of a CondensePerSource provider's sections into: its FIRST section, kept
    // whole, plus ONE condensed section holding a summary row per section that followed. Every
    // other behavior's sections pass through untouched and in order, as does a provider that
    // contributed only one section (nothing to condense).
    //
    // "First section", not "the focus entity's section": a hierarchical provider never provides
    // on the focus entity itself — a GOAP planner and an SM root are both CHILD entities of the
    // NPC — so a SourceOrder-0 section usually does not exist at all. Sources are collected
    // breadth-first, i.e. nearest-first, so the provider's first section IS the NPC's primary
    // planner / state machine and is exactly the one worth keeping in full. When a focus section
    // does exist it is first anyway, so this is a strict superset of a SourceOrder-0 rule.
    //
    // The condensed section keeps the provider's tag/priority, buckets its history under
    // InFocusEntityId (stable across ticks, unlike any one sub-entity), takes the lowest
    // collapsed SourceOrder as its sort tie-break, and names its source chip "N sub-entities".
    // Collapsed sections that carry no rows contribute nothing.
    CKENTITYDEBUGOVERLAY_API auto Condense_ProviderSections(
        const TArray<FCk_DebugOverlay_Section>& InSections,
        uint32                                  InFocusEntityId,
        const FCondenseSourceRowFn&             InCondenseFn) -> TArray<FCk_DebugOverlay_Section>;

    // Pure pre-budget cleanup of a freshly collected model. Build_EntityModel applies this
    // before handing the model on, so the focus-card budget spends its slots on rows the card
    // will actually draw:
    //   1. rows with nothing to render (no Value AND no ExplicitHistory) are dropped — Slate
    //      already skipped them at render time, but only AFTER they had consumed a budget slot;
    //   2. when InMergeDuplicateRows, rows identical by (FieldTag, Value) collapse in two
    //      passes — first WITHIN each section (genuine redundancy, every behavior), then ACROSS
    //      sections but only between those the provider declared MergeAcrossSources. The
    //      survivor keeps its original section, so its history bucket (SourceEntityId) is
    //      unchanged, absorbs the duplicate's MergedCount, and keeps the loudest Severity.
    //      A row carrying ExplicitHistory never merges in either pass: two identical values
    //      with different trails are not the same fact.
    // Sections left empty by either step are dropped.
    CKENTITYDEBUGOVERLAY_API auto Prepare_FocusCardModel(
        const FCk_DebugOverlay_EntityModel& InModel,
        bool                                InMergeDuplicateRows,
        bool                                InRetainEmptySections = false) -> FCk_DebugOverlay_EntityModel;

    // Build distance-scaled world tags / near-plates for the on-screen candidates
    // (B1 — scale/fade/cull + near-plate badges + co-located de-overlap). Returns
    // empty when InPC is null or InIsEjected (PC projection reflects a frozen camera).
    // Callers must have set FCandidate::bIsOnScreen on each candidate.
    // Build Slate world-tag plates for the on-screen candidates. ScreenPos is in DPI-scaled
    // Slate units (ProjectWorldToScreen pixels / InDpiScale) so each plate lands on its
    // diamond marker. InFocusEntity's plate is flagged bIsFocus (highlighted). Every on-screen
    // co-located cluster fans apart gradually with camera proximity, each badged "[i/N]".
    CKENTITYDEBUGOVERLAY_API auto Build_WorldTags(
        const TArray<FCk_Handle>&                            InHandles,
        const TArray<FCandidate>&                            InCandidates,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        const FCk_DebugOverlay_Layout&                       InLayout,
        APlayerController*                                   InPC,
        bool                                                 InIsEjected,
        float                                                InDpiScale    = 1.0f,
        const FCk_Handle&                                    InFocusEntity = FCk_Handle{}) -> TArray<FCk_DebugOverlay_WorldTagInfo>;

    // Resolve a layout by index from settings (index clamped to range). Null when no
    // layouts are configured.
    CKENTITYDEBUGOVERLAY_API auto Resolve_Layout(
        const UCk_DebugOverlay_Settings* InSettings,
        int32                            InIndex) -> const FCk_DebugOverlay_Layout*;

    // Index of the StartingLayout tag in settings (0 fallback when not found / none set).
    CKENTITYDEBUGOVERLAY_API auto Get_StartingLayoutIndex(
        const UCk_DebugOverlay_Settings* InSettings) -> int32;
}

// ====================================================================================================================
