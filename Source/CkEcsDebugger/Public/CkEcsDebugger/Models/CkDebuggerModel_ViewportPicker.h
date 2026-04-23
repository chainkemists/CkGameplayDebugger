#pragma once

#include "CoreMinimal.h"

#include "CkEcs/Handle/CkHandle.h"

#include "GenericPlatform/ICursor.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/StrongObjectPtr.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class FCkDebuggerViewportPicker_InputProcessor;
class UGameViewportClient;
class APlayerController;
class UCanvas;
class UTexture2D;

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
 *   - Hit-test ECS-ready actors (physics trace) and pure ECS entities with FCk_Transform
 *     (inline ray-sphere) and pick the closest candidate
 *   - Draw screen-space billboards (with hover swap) for transform entities while active
 *   - Auto-exit on successful pick; subscribe to world changes to deactivate cleanly
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

    // ---- State -----------------------------------------------------------

    TWeakPtr<FCkDebuggerModel_EntitySelection>            _SelectionModel;
    TWeakPtr<FCkDebuggerModel_WorldContext>               _WorldModel;
    TSharedPtr<FCkDebuggerViewportPicker_InputProcessor>  _InputProcessor;
    FDelegateHandle                                       _WorldChangedHandle;
    FDelegateHandle                                       _DebugDrawHandle_Game;
    FDelegateHandle                                       _DebugDrawHandle_Editor;
    TStrongObjectPtr<UTexture2D>                          _MarkerTexture;
    TStrongObjectPtr<UTexture2D>                          _MarkerHoverTexture;
    bool                                                  _IsActive = false;

    // ---- Options ---------------------------------------------------------
    bool  _IgnoreLocalPawn  = true;
    float _CullRadius       = 5000.0f;
    float _BillboardSizePx  = 32.0f;

    // ---- Cached hover state ---------------------------------------------
    FCk_Handle _HoveredEntity;
    FVector    _LastRayOrigin    = FVector::ZeroVector;
    FVector    _LastRayDirection = FVector::ForwardVector;
    bool       _HasRay           = false;

    TOptional<FRestoreMouseState> _MouseStateToRestore;
};
