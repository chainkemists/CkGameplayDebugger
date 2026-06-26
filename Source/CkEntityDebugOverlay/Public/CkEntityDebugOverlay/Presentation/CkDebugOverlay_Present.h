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

// One world-anchored plate, drawn on the canvas (worldspace — projected per-viewport via
// UCanvas::Project so it aligns exactly with the diamond markers in every camera state).
// Built distance-aware; projected + fanned at draw time by the subsystem.
struct FCk_DebugOverlay_CanvasPlate
{
    FVector WorldLocation = FVector::ZeroVector;
    uint32  EntityNum     = 0;
    float   Distance      = 0.0f;   // camera→entity (cm), drives the gradual fan
    bool    bIsNearPlate  = false;  // near: Header + Badges; far: FarText only
    bool    bIsFocus      = false;  // highlighted to match the emphasized diamond
    FString Header;                 // "Name [id]" / "[id]"
    FString FarText;                // behavioral tokens joined " | "
    TArray<FCk_DebugOverlay_WorldTagBadge> Badges;
};

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

    // Build distance-scaled world tags / near-plates for the on-screen candidates
    // (B1 — scale/fade/cull + near-plate badges + co-located de-overlap). Returns
    // empty when InPC is null or InIsEjected (PC projection reflects a frozen camera).
    // Callers must have set FCandidate::bIsOnScreen on each candidate.
    // Every on-screen co-located cluster (>1 plate sharing a screen cell) is fanned apart and
    // each member badged "[i/N]" so overlapping entities can be told apart (non-interactive).
    CKENTITYDEBUGOVERLAY_API auto Build_WorldTags(
        const TArray<FCk_Handle>&                            InHandles,
        const TArray<FCandidate>&                            InCandidates,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        const FCk_DebugOverlay_Layout&                       InLayout,
        APlayerController*                                   InPC,
        bool                                                 InIsEjected) -> TArray<FCk_DebugOverlay_WorldTagInfo>;

    // Build world-anchored canvas plates for the on-screen candidates. No projection here —
    // the subsystem projects each per-viewport via UCanvas::Project (same as the diamonds) so
    // they align in every camera state. Distance (from InViewLocation) drives the gradual fan
    // and the near/far split; InFocusEntity is flagged for highlight.
    CKENTITYDEBUGOVERLAY_API auto Build_CanvasPlates(
        const TArray<FCk_Handle>&                            InHandles,
        const TArray<FCandidate>&                            InCandidates,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        const FCk_DebugOverlay_Layout&                       InLayout,
        const FVector&                                       InViewLocation,
        const FCk_Handle&                                    InFocusEntity) -> TArray<FCk_DebugOverlay_CanvasPlate>;

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
