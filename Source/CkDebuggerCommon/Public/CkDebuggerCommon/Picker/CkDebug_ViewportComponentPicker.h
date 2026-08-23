#pragma once

#include "CoreMinimal.h"

#include "GenericPlatform/ICursor.h"
#include "Engine/EngineBaseTypes.h"

// --------------------------------------------------------------------------------------------------------------------

class APlayerController;
class FCkDebug_ViewportComponentPickerInputProcessor;
class UGameViewportClient;
class UMaterialInterface;
class UPrimitiveComponent;
struct FHitResult;

// --------------------------------------------------------------------------------------------------------------------

/** Value/weak-reference result returned by the generic primitive-component picker. */
struct CKDEBUGGERCOMMON_API FCkDebug_ComponentPickResult
{
    TWeakObjectPtr<UPrimitiveComponent> Component;
    TWeakObjectPtr<UMaterialInterface>  CollisionMaterial;
    FVector                             ImpactPoint = FVector::ZeroVector;
    float                               Distance = 0.0f;
    int32                               FaceIndex = INDEX_NONE;
    int32                               InstanceIndex = INDEX_NONE;
    int32                               CollisionSectionIndex = INDEX_NONE;
    bool                                UsedBoundsFallback = false;

    auto IsValid() const -> bool;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FCkDebug_OnComponentPickModeChanged, bool /*IsActive*/);

// --------------------------------------------------------------------------------------------------------------------

/**
 * Runtime-safe click-to-select picker for arbitrary primitive components in Editor, PIE, and Game worlds.
 *
 * It mirrors the established ECS picker lifecycle and input behavior but returns a weak component result rather than
 * an FCk_Handle. A visibility trace supplies face/instance context when collision supports it; analytic ray-vs-bounds
 * is the collisionless fallback. The picker never guesses a material slot from the returned section index.
 */
class CKDEBUGGERCOMMON_API FCkDebug_ViewportComponentPicker
    : public TSharedFromThis<FCkDebug_ViewportComponentPicker>
{
public:
    struct FParams
    {
        TFunction<UWorld*()> Get_TargetWorld;
        TFunction<void(const FCkDebug_ComponentPickResult&)> OnComponentPicked;
        TFunction<bool(const UPrimitiveComponent&)> TargetFilter;
    };

public:
    FCkDebug_ViewportComponentPicker();
    ~FCkDebug_ViewportComponentPicker();

    auto Construct(FParams InParams) -> void;

    auto Activate() -> bool;
    auto Deactivate() -> void;
    auto Toggle() -> void;
    auto IsActive() const -> bool;
    auto CanActivate() const -> bool;

    /** Called every frame by the host window, outside its refresh gate. */
    auto Tick(float InDeltaSeconds) -> void;

    auto OnMouseMoved(const FVector2D& InAbsolutePos) -> void;
    auto OnMouseClicked(const FVector2D& InAbsolutePos) -> bool;
    auto OnEscapePressed() -> bool;

    auto Get_CullRadius() const -> float;
    auto Set_CullRadius(float InValue) -> void;

    auto Get_HoveredResult() const -> const FCkDebug_ComponentPickResult&;

    /** Pure geometry seam used by focused tests and collisionless fallback selection. */
    static auto TryIntersect_RayBounds(
        const FBox& InBox,
        const FVector& InOrigin,
        const FVector& InDirection) -> TOptional<float>;

    FCkDebug_OnComponentPickModeChanged OnPickModeChanged;

private:
    struct FRestoreMouseState
    {
        TWeakObjectPtr<UGameViewportClient> Viewport;
        TWeakObjectPtr<APlayerController>   Controller;
        EMouseCaptureMode                   PriorCapture = EMouseCaptureMode::NoCapture;
        EMouseLockMode                      PriorLock = EMouseLockMode::DoNotLock;
        bool                                PriorCursorVisible = false;
    };

    auto DoResolveTargetWorld() const -> UWorld*;
    auto DoCaptureMouseState(UWorld* InWorld) -> void;
    auto DoRestoreMouseState() -> void;
    auto DoDeproject(
        UWorld* InWorld,
        const FVector2D& InAbsolutePos,
        FVector& OutOrigin,
        FVector& OutDirection) const -> bool;
    auto DoPickAtRay(
        UWorld* InWorld,
        const FVector& InOrigin,
        const FVector& InDirection) const -> FCkDebug_ComponentPickResult;
    auto DoIsCandidate(
        UWorld* InWorld,
        const UPrimitiveComponent* InComponent) const -> bool;
    auto DoBuildTraceResult(const FHitResult& InHit) const -> FCkDebug_ComponentPickResult;

private:
    FParams _Params;
    TSharedPtr<FCkDebug_ViewportComponentPickerInputProcessor> _InputProcessor;
    TOptional<FRestoreMouseState> _MouseStateToRestore;
    FDelegateHandle _SessionInvalidatedHandle;
    FCkDebug_ComponentPickResult _HoveredResult;
    FVector2D _PendingMousePosition = FVector2D::ZeroVector;
    float _CullRadius = 100'000.0f;
    bool _HasPendingMousePosition = false;
    bool _IsActive = false;
};

// --------------------------------------------------------------------------------------------------------------------
