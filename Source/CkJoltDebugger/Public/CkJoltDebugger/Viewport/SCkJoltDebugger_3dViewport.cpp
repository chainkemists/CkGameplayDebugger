#include "CkJoltDebugger/Viewport/SCkJoltDebugger_3dViewport.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkJolt/Subsystem/CkJolt_DebugDrawTarget.h"

#include "Components/Viewport.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "PreviewScene.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_debugger_3d_viewport
{
    auto Get_DefaultPerspectiveRotation() -> FRotator
    { return FRotator{-25.0, 45.0, 0.0}; }

    auto Get_DefaultPerspectiveLocation() -> FVector
    { return FVector{-1600.0, -1600.0, 1200.0}; }

    auto Get_PresetRotation(
        ECkJoltDebugger_CameraPreset InPreset) -> FRotator
    {
        switch (InPreset)
        {
            case ECkJoltDebugger_CameraPreset::Top:    return FRotator{-90.0, 0.0, 0.0};
            case ECkJoltDebugger_CameraPreset::Bottom: return FRotator{90.0, 0.0, 0.0};
            case ECkJoltDebugger_CameraPreset::Left:   return FRotator{0.0, 180.0, 0.0};
            case ECkJoltDebugger_CameraPreset::Right:  return FRotator{0.0, 0.0, 0.0};
            case ECkJoltDebugger_CameraPreset::Front:  return FRotator{0.0, -90.0, 0.0};
            case ECkJoltDebugger_CameraPreset::Back:   return FRotator{0.0, 90.0, 0.0};
            default:                                   return Get_DefaultPerspectiveRotation();
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

class FCkJoltDebugger_3dViewportClient final : public FUMGViewportClient
{
public:
    explicit FCkJoltDebugger_3dViewportClient(
        FPreviewScene& InPreviewScene)
        : FUMGViewportClient(&InPreviewScene)
    {
        EngineShowFlags.SetEyeAdaptation(false);
        SetBackgroundColor(FLinearColor::Black);
        SetViewLocation(ck_jolt_debugger_3d_viewport::Get_DefaultPerspectiveLocation());
        SetViewRotation(ck_jolt_debugger_3d_viewport::Get_DefaultPerspectiveRotation());
    }

    auto Set_Viewport(const TSharedRef<FSceneViewport>& InViewport) -> void
    { _Viewport = InViewport; }

    auto Set_Target(TSharedPtr<FCk_Jolt_DebugDrawTarget> InTarget) -> void
    { _Target = InTarget; }

    auto Set_SelectionBounds(TOptional<FBox> InBounds) -> void
    { _SelectionBounds = MoveTemp(InBounds); }

    auto Set_OnBodyPicked(FOnCkJoltDebugger_BodyPicked InDelegate) -> void
    { _OnBodyPicked = MoveTemp(InDelegate); }

    auto Get_ProjectionMode() const -> ECameraProjectionMode::Type
    { return ViewInfo.ProjectionMode; }

    auto Invalidate() const -> void
    {
        if (const auto SceneViewport = _Viewport.Pin())
        { SceneViewport->Invalidate(); }
    }

    auto ApplyPreset(ECkJoltDebugger_CameraPreset InPreset) -> void
    {
        if (InPreset == ECkJoltDebugger_CameraPreset::FrameAll)
        {
            FrameBounds(Get_TargetContentBounds());
            return;
        }

        if (InPreset == ECkJoltDebugger_CameraPreset::FrameSelection)
        {
            FrameBounds(_SelectionBounds.Get(FBox{ForceInit}));
            return;
        }

        ViewInfo.ProjectionMode = InPreset == ECkJoltDebugger_CameraPreset::Perspective
            ? ECameraProjectionMode::Perspective
            : ECameraProjectionMode::Orthographic;
        SetViewRotation(ck_jolt_debugger_3d_viewport::Get_PresetRotation(InPreset));
        FrameBounds(Get_TargetContentBounds());
    }

    virtual auto InputKey(const FInputKeyEventArgs& InEventArgs) -> bool override
    {
        // A PLAIN left click only: while any camera button is down the left button is part of a drag
        // gesture, and picking mid-orbit would fight the camera.
        const auto IsPlainLeftClick = InEventArgs.Key == EKeys::LeftMouseButton &&
            InEventArgs.Event == IE_Pressed &&
            InEventArgs.Viewport != nullptr &&
            NOT InEventArgs.Viewport->KeyState(EKeys::RightMouseButton) &&
            NOT InEventArgs.Viewport->KeyState(EKeys::MiddleMouseButton);
        if (IsPlainLeftClick && _OnBodyPicked.IsBound())
        {
            auto RayOrigin = FVector::ZeroVector;
            auto RayDirection = FVector::ZeroVector;

            if (GetCursorWorldRay(InEventArgs.Viewport, RayOrigin, RayDirection))
            {
                const auto Target = _Target.Pin();
                _OnBodyPicked.Execute(Target.IsValid()
                    ? Target->TryPick_Body(RayOrigin, RayDirection)
                    : TOptional<uint64>{});
                return true;
            }
        }

        const auto IsFrameSelectionKey = (InEventArgs.Event == IE_Pressed || InEventArgs.Event == IE_Repeat) &&
            InEventArgs.Key == EKeys::F;
        if (IsFrameSelectionKey)
        {
            ApplyPreset(ECkJoltDebugger_CameraPreset::FrameSelection);
            return true;
        }

        const auto IsFrameAllKey = (InEventArgs.Event == IE_Pressed || InEventArgs.Event == IE_Repeat) &&
            InEventArgs.Key == EKeys::Home;
        if (IsFrameAllKey)
        {
            ApplyPreset(ECkJoltDebugger_CameraPreset::FrameAll);
            return true;
        }

        const auto IsMouseWheel = InEventArgs.Key == EKeys::MouseScrollUp ||
            InEventArgs.Key == EKeys::MouseScrollDown;
        const auto IsSpeedChange = ViewInfo.ProjectionMode == ECameraProjectionMode::Perspective &&
            InEventArgs.Event == IE_Pressed &&
            IsMouseWheel &&
            InEventArgs.Viewport != nullptr &&
            InEventArgs.Viewport->KeyState(EKeys::RightMouseButton);
        if (IsSpeedChange)
        {
            const auto Direction = InEventArgs.Key == EKeys::MouseScrollDown ? -1.0f : 1.0f;
            _CameraSpeed = FMath::Clamp(_CameraSpeed + _CameraSpeed * 0.1f * Direction, 0.00001f, 10000.0f);
            return true;
        }

        if (IsMouseWheel && InEventArgs.Event == IE_Pressed)
        {
            Zoom(InEventArgs.Key == EKeys::MouseScrollUp ? 1.1f : 1.0f / 1.1f);
            return true;
        }

        return InEventArgs.Key == EKeys::RightMouseButton || InEventArgs.Key == EKeys::MiddleMouseButton;
    }

    virtual auto InputAxis(const FInputKeyEventArgs& InEventArgs) -> bool override
    {
        if (InEventArgs.Viewport == nullptr || (InEventArgs.Key != EKeys::MouseX && InEventArgs.Key != EKeys::MouseY))
        { return false; }

        const auto IsOrbiting = InEventArgs.Viewport->KeyState(EKeys::RightMouseButton);
        const auto IsPanning = InEventArgs.Viewport->KeyState(EKeys::MiddleMouseButton);
        if (NOT IsOrbiting && NOT IsPanning)
        { return false; }

        if (IsOrbiting)
        {
            auto Rotation = GetViewRotation();
            if (InEventArgs.Key == EKeys::MouseX)
            { Rotation.Yaw += InEventArgs.AmountDepressed * 0.25f; }
            else
            { Rotation.Pitch = FMath::Clamp(Rotation.Pitch + InEventArgs.AmountDepressed * 0.25f, -89.0f, 89.0f); }

            SetViewRotation(Rotation);
            const auto Distance = (GetViewLocation() - GetLookAtLocation()).Size();
            SetViewLocation(GetLookAtLocation() - Rotation.Vector() * FMath::Max(Distance, 1.0f));
        }
        else
        {
            const auto Distance = FMath::Max((GetViewLocation() - GetLookAtLocation()).Size(), 1.0f);
            const auto PanScale = FMath::Clamp(_CameraSpeed / 1000.0f, 0.1f, 100.0f);
            const auto Delta = InEventArgs.Key == EKeys::MouseX
                ? GetViewRotation().RotateVector(FVector::RightVector) * (-InEventArgs.AmountDepressed * Distance * 0.001f * PanScale)
                : GetViewRotation().RotateVector(FVector::UpVector) * (InEventArgs.AmountDepressed * Distance * 0.001f * PanScale);
            SetViewLocation(GetViewLocation() + Delta);
            SetLookAtLocation(GetLookAtLocation() + Delta);
        }

        Invalidate();
        return true;
    }

    auto Tick_Navigation(float InDeltaTime) -> void
    {
        const auto SceneViewport = _Viewport.Pin();
        if (SceneViewport == nullptr || ViewInfo.ProjectionMode != ECameraProjectionMode::Perspective)
        { return; }

        auto Direction = FVector::ZeroVector;
        if (SceneViewport->KeyState(EKeys::W) || SceneViewport->KeyState(EKeys::Up))
        { Direction += GetViewRotation().Vector(); }
        if (SceneViewport->KeyState(EKeys::S) || SceneViewport->KeyState(EKeys::Down))
        { Direction -= GetViewRotation().Vector(); }
        if (SceneViewport->KeyState(EKeys::D) || SceneViewport->KeyState(EKeys::Right))
        { Direction += GetViewRotation().RotateVector(FVector::RightVector); }
        if (SceneViewport->KeyState(EKeys::A) || SceneViewport->KeyState(EKeys::Left))
        { Direction -= GetViewRotation().RotateVector(FVector::RightVector); }
        if (SceneViewport->KeyState(EKeys::E))
        { Direction += FVector::UpVector; }
        if (SceneViewport->KeyState(EKeys::Q))
        { Direction -= FVector::UpVector; }
        if (Direction.IsNearlyZero())
        { return; }

        const auto Delta = Direction.GetSafeNormal() * _CameraSpeed * 32.0f * InDeltaTime;
        SetViewLocation(GetViewLocation() + Delta);
        SetLookAtLocation(GetLookAtLocation() + Delta);
        Invalidate();
    }

private:
    auto GetCursorWorldRay(
        FViewport* InViewport,
        FVector&   OutRayOrigin,
        FVector&   OutRayDirection) const -> bool
    {
        if (InViewport == nullptr)
        { return false; }

        const auto Size = InViewport->GetSizeXY();

        if (Size.X <= 0 || Size.Y <= 0)
        { return false; }

        auto Mouse = FIntPoint{};
        InViewport->GetMousePos(Mouse);

        auto ViewInitOptions = FSceneViewInitOptions{};
        ViewInitOptions.SetViewRectangle(FIntRect(0, 0, Size.X, Size.Y));
        ViewInitOptions.ViewOrigin = GetViewLocation();
        ViewInitOptions.ViewRotationMatrix = FInverseRotationMatrix(GetViewRotation());
        ViewInitOptions.ViewRotationMatrix = ViewInitOptions.ViewRotationMatrix * FMatrix(
            FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));

        const auto AspectRatioAxisConstraint = GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint;
        auto ProjectionViewInfo = ViewInfo;
        FMinimalViewInfo::CalculateProjectionMatrixGivenView(
            ProjectionViewInfo, AspectRatioAxisConstraint, InViewport, ViewInitOptions);

        FSceneView::DeprojectScreenToWorld(
            FVector2D(Mouse), ViewInitOptions.ViewRect,
            ViewInitOptions.ViewRotationMatrix.InverseFast(), ViewInitOptions.ProjectionMatrix.InverseFast(),
            OutRayOrigin, OutRayDirection);

        return NOT OutRayDirection.IsNearlyZero();
    }

    auto Get_TargetContentBounds() const -> FBox
    {
        const auto Target = _Target.Pin();
        return Target.IsValid() ? Target->Get_ContentBounds() : FBox{ForceInit};
    }

    auto FrameBounds(const FBox& InBounds) -> void
    {
        if (InBounds.IsValid == 0)
        { return; }

        const auto Center = InBounds.GetCenter();
        auto Radius = FMath::Max(InBounds.GetExtent().Size(), 10.0);
        auto AspectRatio = 1.777777f;
        if (const auto SceneViewport = _Viewport.Pin())
        {
            const auto Size = SceneViewport->GetSizeXY();
            if (Size.X > 0 && Size.Y > 0)
            { AspectRatio = SceneViewport->GetDesiredAspectRatio(); }
        }

        if (ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
        {
            ViewInfo.OrthoWidth = Radius * 2.0f * FMath::Max(AspectRatio, 1.0f);
            SetLookAtLocation(Center);

            // Sitting the eye ON the centre puts half the content behind the near plane, so solid bodies get
            // sliced away. Back off past the bounding sphere before looking in.
            constexpr auto EyeClearanceScale = 2.0f;
            SetViewLocation(Center - GetViewRotation().Vector() * (Radius * EyeClearanceScale));
        }
        else
        {
            if (AspectRatio > 1.0f)
            { Radius *= AspectRatio; }

            const auto Distance = Radius / FMath::Tan(FMath::DegreesToRadians(ViewInfo.FOV * 0.5f));
            SetLookAtLocation(Center);
            SetViewLocation(Center - GetViewRotation().Vector() * Distance);
        }

        Invalidate();
    }

    auto Zoom(float InFactor) -> void
    {
        if (ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
        { ViewInfo.OrthoWidth = FMath::Clamp(ViewInfo.OrthoWidth / InFactor, 10.0f, 1000000.0f); }
        else
        {
            const auto Delta = GetViewLocation() - GetLookAtLocation();
            SetViewLocation(GetLookAtLocation() + Delta / InFactor);
        }

        Invalidate();
    }

private:
    TWeakPtr<FCk_Jolt_DebugDrawTarget> _Target;
    TWeakPtr<FSceneViewport> _Viewport;

    TOptional<FBox> _SelectionBounds;
    FOnCkJoltDebugger_BodyPicked _OnBodyPicked;

    float _CameraSpeed = 1.0f;
};

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkJoltDebugger_3dViewport::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _PreviewScene = MakeShared<FPreviewScene>(
        FPreviewScene::ConstructionValues()
            .SetCreateDefaultLighting(false)
            .SetCreatePhysicsScene(false)
            .SetEditor(false));

    auto ViewportArguments = SViewport::FArguments{};
    ViewportArguments.IgnoreTextureAlpha(false);
    ViewportArguments.EnableBlending(false);
    SViewport::Construct(ViewportArguments);

    _ViewportClient = MakeShared<FCkJoltDebugger_3dViewportClient>(*_PreviewScene);
    _ViewportClient->Set_OnBodyPicked(InArgs._OnBodyPicked);
    _SceneViewport = MakeShared<FSceneViewport>(_ViewportClient.Get(), SharedThis(this));
    _ViewportClient->Set_Viewport(_SceneViewport.ToSharedRef());
    SetViewportInterface(_SceneViewport.ToSharedRef());
}

auto
    SCkJoltDebugger_3dViewport::
    Set_SelectionBounds(
        TOptional<FBox> InBounds)
    -> void
{
    if (_ViewportClient.IsValid())
    { _ViewportClient->Set_SelectionBounds(MoveTemp(InBounds)); }
}

auto
    SCkJoltDebugger_3dViewport::
    Get_PreviewWorld() const
    -> UWorld*
{
    return _PreviewScene.IsValid() ? _PreviewScene->GetWorld() : nullptr;
}

auto
    SCkJoltDebugger_3dViewport::
    Set_Target(
        TSharedPtr<FCk_Jolt_DebugDrawTarget> InTarget)
    -> void
{
    if (_ViewportClient.IsValid())
    { _ViewportClient->Set_Target(MoveTemp(InTarget)); }
}

auto
    SCkJoltDebugger_3dViewport::
    ApplyPreset(
        ECkJoltDebugger_CameraPreset InPreset)
    -> void
{
    if (_ViewportClient.IsValid())
    { _ViewportClient->ApplyPreset(InPreset); }
}

auto
    SCkJoltDebugger_3dViewport::
    Get_ProjectionMode() const
    -> ECameraProjectionMode::Type
{
    return _ViewportClient.IsValid()
        ? _ViewportClient->Get_ProjectionMode()
        : ECameraProjectionMode::Perspective;
}

auto
    SCkJoltDebugger_3dViewport::
    Get_ViewRotation() const
    -> FRotator
{
    return _ViewportClient.IsValid() ? _ViewportClient->GetViewRotation() : FRotator::ZeroRotator;
}

auto
    SCkJoltDebugger_3dViewport::
    Get_ViewLocation() const
    -> FVector
{
    return _ViewportClient.IsValid() ? _ViewportClient->GetViewLocation() : FVector::ZeroVector;
}

auto
    SCkJoltDebugger_3dViewport::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SViewport::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (_SceneViewport.IsValid())
    { _SceneViewport->Invalidate(); }

    if (_ViewportClient.IsValid())
    {
        _ViewportClient->Tick_Navigation(InDeltaTime);
        _ViewportClient->Tick(InDeltaTime);
    }
}

// --------------------------------------------------------------------------------------------------------------------
