#pragma once

#include "CoreMinimal.h"

#include "CkDebuggerCommon/Markers/CkDebug_EntityMarkers.h"

#include "CkEcs/Handle/CkHandle.h"

#include "GenericPlatform/ICursor.h"
#include "Engine/EngineBaseTypes.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkDebug_ViewportPickerInputProcessor;
class ICkDebug_PickerOverlayCards;
class UGameViewportClient;
class APlayerController;
class UCanvas;
struct FHitResult;

// --------------------------------------------------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE_OneParam(FCkDebug_OnPickModeChanged, bool /*IsActive*/);

// --------------------------------------------------------------------------------------------------------------------

/**
 * State + policy for a debugger's "click-to-select" viewport picker mode —
 * shared across the debugger suite. Any debugger window can own an instance;
 * the host supplies world resolution, the pick handler, and (optionally) a
 * target filter that restricts the preview to entities its debugger supports.
 *
 * Responsibilities:
 *   - Activate/deactivate pick mode, releasing and restoring mouse capture on toggle
 *   - Install a Slate input pre-processor that forwards clicks here
 *   - Deproject cursor positions into the host's world via ck::DebugViewportView
 *   - Preview entities via the shared FCkDebug_EntityMarkers system (depth-tinted
 *     diamonds + dashed parent→child links, gated by ck.Debug.EntityMarkers.MaxDepth —
 *     the same display and depth setting as the On-Screen Overlay). With a
 *     TargetFilter, the preview instead shows matching entities plus their
 *     lifetime-owner chain up to the top-most non-transient, non-ActorRelay
 *     ancestor (the conceptual NPC), and only those are pickable.
 *   - Hit-test ECS-ready actors (physics trace) and the previewed marker entries
 *     (inline ray-sphere) and pick the closest candidate; only previewed entities
 *     are pickable — what you see is what you can pick
 *   - Reuse the On-Screen Overlay's focus card + world tags via the
 *     ck::DebugPickerCards factory hook (no-op when the overlay module is absent)
 *   - Auto-exit on successful pick (invoking the host's OnEntityPicked);
 *     deactivate cleanly on session invalidation (BeginPIE/EndPIE)
 */
class CKDEBUGGERCOMMON_API FCkDebug_ViewportPicker
    : public TSharedFromThis<FCkDebug_ViewportPicker>
{
public:
    struct FParams
    {
        // Required: the world this picker operates on (the host's selected
        // world). Only PIE/Game worlds are accepted; returning null or a
        // non-playable world deactivates the picker on the next Tick.
        TFunction<UWorld*()> Get_TargetWorld;

        // Required: invoked with the picked entity AFTER the picker has
        // deactivated. The host owns selection, cross-debugger broadcast, and
        // tab focus (the game viewport took focus while picking).
        TFunction<void(const FCk_Handle&)> OnEntityPicked;

        // Optional: restrict the preview/pick set to entities this debugger
        // supports, plus their owner chain up to the representative root. See
        // FCkDebug_EntityMarkers::FGatherParams::TargetMatch for the exact
        // semantics. Unbound = every entity (the ECS debugger's behavior).
        TFunction<bool(const FCk_Handle&)> TargetFilter;
    };

public:
    FCkDebug_ViewportPicker();
    ~FCkDebug_ViewportPicker();

    auto
    Construct(
        FParams InParams) -> void;

    // ---- Activation control ---------------------------------------------

    auto
    Activate() -> bool;

    auto
    Deactivate() -> void;

    auto
    Toggle() -> void;

    auto
    IsActive() const -> bool;

    auto
    CanActivate() const -> bool;

    // ---- Per-frame work (called by the host window's Tick) --------------

    auto
    Tick(
        float InDeltaSeconds) -> void;

    // ---- Input processor callbacks --------------------------------------

    auto
    OnMouseMoved(
        const FVector2D& InAbsolutePos) -> void;

    auto
    OnMouseClicked(
        const FVector2D& InAbsolutePos) -> bool;

    auto
    OnEscapePressed() -> bool;

    // ---- Options ---------------------------------------------------------

    auto
    Get_IgnoreLocalPawn() const -> bool;

    auto
    Set_IgnoreLocalPawn(
        bool InValue) -> void;

    auto
    Get_CullRadius() const -> float;

    auto
    Set_CullRadius(
        float InValue) -> void;

    auto
    Get_BillboardSize() const -> float;

    auto
    Set_BillboardSize(
        float InValue) -> void;

    auto
    Get_MeshesFirst() const -> bool;

    auto
    Set_MeshesFirst(
        bool InValue) -> void;

    // ---- Multicast -------------------------------------------------------

    FCkDebug_OnPickModeChanged OnPickModeChanged;

private:

    // ---- Pick candidate --------------------------------------------------

    struct FPickCandidate
    {
        FCk_Handle Entity;
        float      RayT       = TNumericLimits<float>::Max();
        bool       IsActorHit = false;

        auto IsValid() const -> bool { return ck::IsValid(Entity); }
    };

    // ---- Mouse capture restore payload -----------------------------------

    struct FRestoreMouseState
    {
        TWeakObjectPtr<UGameViewportClient> Viewport;
        TWeakObjectPtr<APlayerController>   Controller;
        EMouseCaptureMode                   PriorCapture       = EMouseCaptureMode::NoCapture;
        EMouseLockMode                      PriorLock          = EMouseLockMode::DoNotLock;
        bool                                PriorCursorVisible = false;
    };

    // ---- Private helpers -------------------------------------------------

    auto
    DoResolveTargetWorld() const -> UWorld*;

    auto
    DoGet_LocalIgnoredActors(
        UWorld* InWorld) const -> TArray<TWeakObjectPtr<const AActor>>;

    auto
    DoIsEntityOwnedByIgnoredActor(
        const FCk_Handle& InEntity,
        const TArray<TWeakObjectPtr<const AActor>>& InIgnoredActors) const -> bool;

    auto
    DoGet_CameraLocation(
        UWorld* InWorld) const -> FVector;

    auto
    DoCaptureMouseState(
        UWorld* InWorld) -> void;

    auto
    DoRestoreMouseState() -> void;

    auto
    DoDeproject(
        UWorld*          InWorld,
        const FVector2D& InAbsolutePos,
        FVector&         OutOrigin,
        FVector&         OutDirection) const -> bool;

    auto
    DoRefreshMarkers(
        UWorld* InWorld) -> void;

    auto
    DoPickAtRay(
        UWorld*        InWorld,
        const FVector& InOrigin,
        const FVector& InDirection) const -> FCk_Handle;

    auto
    DoDrawBillboards(
        UCanvas*           InCanvas,
        APlayerController* InPC) -> void;

    auto
    DoResolveActorEntity(
        UWorld* InWorld,
        AActor* InHitActor) const -> FCk_Handle;

    // ISM-instance hit → proxy entity: the hit instance's world transform is
    // matched against the marker snapshot (nearest IsmProxy-carrying entry).
    auto
    DoResolveIsmInstanceEntity(
        const FHitResult& InHit) const -> FCk_Handle;

    // Pickable by rendered geometry (ISM-instance- or direct-actor-backed)?
    auto
    DoIsMeshResolvable(
        const FCk_Handle& InEntity) const -> bool;

    // ---- State -----------------------------------------------------------

    FParams                                          _Params;
    TSharedPtr<FCkDebug_ViewportPickerInputProcessor> _InputProcessor;
    FDelegateHandle                                  _SessionInvalidatedHandle;
    FDelegateHandle                                  _DebugDrawHandle_Game;
    FDelegateHandle                                  _DebugDrawHandle_Editor;
    FCkDebug_EntityMarkers                           _Markers;
    bool                                             _IsActive = false;

    // ---- Overlay cards (focus card + world tags), via the factory hook ----
    TSharedPtr<ICkDebug_PickerOverlayCards>          _OverlayCards;

    // ---- Options ---------------------------------------------------------
    bool  _IgnoreLocalPawn  = true;
    float _CullRadius       = 5000.0f;
    float _BillboardSizePx  = 32.0f;

    // "Meshes first": diamonds hidden (still gathered/pickable) for entities
    // pickable via their rendered geometry. Persisted in UCkDebuggerSettings.
    bool         _MeshesFirst = false;
    TSet<uint32> _MeshSuppressedNums;

    // Analytic pick volumes for geometry-backed entities, rebuilt with the marker
    // snapshot each tick. Rays test against these in ADDITION to the physics trace:
    // renderer-only ISMs (stress gyms run 500+ at NoCollision) are invisible to
    // traces, so hover/click on their meshes must resolve analytically.
    struct FMeshPickEntry
    {
        FCk_Handle Entity;
        FBox       Box = FBox{ForceInit};
    };
    TArray<FMeshPickEntry> _MeshPickBounds;

    // ---- Cached hover / focus state -------------------------------------
    // _FocusEntity is STICKY: it holds the last validly-hovered entity and is not
    // cleared between markers, so the focus card + emphasized diamond don't flicker
    // as the cursor sweeps empty space. Cleared only on deactivation.
    FCk_Handle _FocusEntity;
    FVector    _LastRayOrigin    = FVector::ZeroVector;
    FVector    _LastRayDirection = FVector::ForwardVector;
    bool       _HasRay           = false;

    TOptional<FRestoreMouseState> _MouseStateToRestore;
};
