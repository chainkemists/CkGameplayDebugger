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
    // Build the focus-card model for one entity. Layout-driven providers plus the
    // always-on set (StateMachine). When InHistory is non-null, each row's value is
    // observed into it (drives the card's history trail).
    CKENTITYDEBUGOVERLAY_API auto Build_EntityModel(
        const FCk_Handle&                                    InFocusEntity,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        const FCk_DebugOverlay_Layout&                       InLayout,
        FCk_DebugOverlay_History*                            InHistory,
        double                                               InNow) -> FCk_DebugOverlay_EntityModel;

    // Pure pre-budget cleanup of a freshly collected model. Build_EntityModel applies this
    // before handing the model on, so the focus-card budget spends its slots on rows the card
    // will actually draw:
    //   1. rows with nothing to render (no Value AND no ExplicitHistory) are dropped — Slate
    //      already skipped them at render time, but only AFTER they had consumed a budget slot;
    //   2. when InMergeDuplicateRows, rows identical by (FieldTag, Value) within one provider
    //      collapse into the FIRST section that carried them, bumping that row's MergedCount
    //      (subtree aggregation emits the same row once per lifetime descendant). The survivor
    //      stays in its original section, so its history bucket (SourceEntityId) is unchanged.
    // Sections left empty by either step are dropped.
    CKENTITYDEBUGOVERLAY_API auto Prepare_FocusCardModel(
        const FCk_DebugOverlay_EntityModel& InModel,
        bool                                InMergeDuplicateRows) -> FCk_DebugOverlay_EntityModel;

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
