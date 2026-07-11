#pragma once

#include "CoreMinimal.h"

#include "CkDebuggerCommon/Markers/CkDebug_EntityMarkers.h"

#include "CkEcs/Handle/CkHandle.h"

#include "GenericPlatform/ICursor.h"
#include "Engine/EngineBaseTypes.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class FCkDebuggerViewportPicker_InputProcessor;
class UGameViewportClient;
class APlayerController;
class UCanvas;
struct FHitResult;

// Overlay focus card + world tags reused from the On-Screen Overlay (CkEntityDebugOverlay).
class SCkDebugOverlay_Root;
class ICk_DebugOverlay_Provider;
class FCk_DebugOverlay_History;

// --------------------------------------------------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE_OneParam(FCkDebugger_OnPickModeChanged, bool /*IsActive*/);

// --------------------------------------------------------------------------------------------------------------------

/**
 * State + policy for the debugger's "click-to-select" viewport picker mode.
 *
 * Peer to FCkDebuggerModel_EntitySelection and FCkDebuggerModel_WorldContext; held
 * as a shared pointer by SCkDebuggerWindow_Main.
 *
 * Responsibilities:
 *   - Activate/deactivate pick mode, releasing and restoring mouse capture on toggle
 *   - Install a Slate input pre-processor that forwards clicks here
 *   - Deproject cursor positions into the selected world via ULocalPlayer::CalcSceneView
 *   - Preview entities via the shared FCkDebug_EntityMarkers system (depth-tinted
 *     diamonds + dashed parent→child links, gated by ck.Debug.EntityMarkers.MaxDepth —
 *     the same display and depth setting as the On-Screen Overlay)
 *   - Hit-test ECS-ready actors (physics trace) and the previewed marker entries
 *     (inline ray-sphere) and pick the closest candidate; only previewed entities
 *     are pickable — what you see is what you can pick
 *   - Auto-exit on successful pick (focusing the debugger tab); subscribe to world
 *     changes to deactivate cleanly
 */
class FCkDebuggerModel_ViewportPicker
    : public TSharedFromThis<FCkDebuggerModel_ViewportPicker>
{
public:
    FCkDebuggerModel_ViewportPicker();
    ~FCkDebuggerModel_ViewportPicker();

    auto
    Construct(
        TSharedPtr<FCkDebuggerModel_EntitySelection> InSelection,
        TSharedPtr<FCkDebuggerModel_WorldContext>    InWorld) -> void;

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

    // ---- Per-frame work (called by SCkDebuggerWindow_Main::Tick) --------

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

    FCkDebugger_OnPickModeChanged OnPickModeChanged;

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

    // Reuse the On-Screen Overlay's focus card + world tags for the picked/hovered entity.
    // No-op when the overlay subsystem is already showing them (ck.DebugOverlay on).
    auto
    DoActivateOverlayCards(
        UWorld* InWorld) -> void;

    auto
    DoDeactivateOverlayCards() -> void;

    auto
    DoUpdateOverlayCards(
        UWorld*            InWorld,
        APlayerController* InPC,
        bool               InIsEjected) -> void;

    // ---- State -----------------------------------------------------------

    TWeakPtr<FCkDebuggerModel_EntitySelection>            _SelectionModel;
    TWeakPtr<FCkDebuggerModel_WorldContext>               _WorldModel;
    TSharedPtr<FCkDebuggerViewportPicker_InputProcessor>  _InputProcessor;
    FDelegateHandle                                       _WorldChangedHandle;
    FDelegateHandle                                       _DebugDrawHandle_Game;
    FDelegateHandle                                       _DebugDrawHandle_Editor;
    FCkDebug_EntityMarkers                                _Markers;
    bool                                                  _IsActive = false;

    // ---- Overlay cards (focus card + world tags) -------------------------
    TSharedPtr<SCkDebugOverlay_Root>                      _OverlayRoot;
    TArray<TSharedPtr<ICk_DebugOverlay_Provider>>         _OverlayProviders;
    TUniquePtr<FCk_DebugOverlay_History>                  _OverlayHistory;
    int32                                                 _OverlayLayoutIndex = 0;
    bool                                                  _OverlayCardsActive = false;

    // ---- Options ---------------------------------------------------------
    bool  _IgnoreLocalPawn  = true;
    float _CullRadius       = 5000.0f;
    float _BillboardSizePx  = 32.0f;

    // "Meshes first": diamonds hidden (still gathered/pickable) for entities
    // pickable via their rendered geometry. Persisted in UCkEcsDebuggerSettings.
    bool         _MeshesFirst = false;
    TSet<uint32> _MeshSuppressedNums;

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
