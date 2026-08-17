#include "CkDebuggerCommon/Viewport/SCkDebug_3dPreviewViewport.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_OrientationCube.h"

#include "CkCore/Macros/CkMacros.h"

#include "Components/Viewport.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"
#include "UnrealClient.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_debug_3d_preview_viewport
{
const auto DefaultLocation = FVector{-1600.0, -1600.0, 1200.0};
const auto DefaultRotation = FRotator{-25.0, 45.0, 0.0};
constexpr float DefaultOrbitDistance = 1600.0f;
constexpr int32 BookmarkCount = 10;

auto
TryGetBookmarkSlot(const FKey& InKey) -> TOptional<int32>
{
    static const FKey Keys[BookmarkCount] = {EKeys::Zero, EKeys::One, EKeys::Two,   EKeys::Three, EKeys::Four,
                                             EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
    for (auto Index = 0; Index < BookmarkCount; ++Index)
    {
        if (InKey == Keys[Index])
        {
            return Index;
        }
    }
    return {};
}

auto
GetPresetRotation(ECkDebug3dCameraPreset InPreset) -> FRotator
{
    switch (InPreset)
    {
    case ECkDebug3dCameraPreset::Top:
        return {-90.0, 0.0, 0.0};
    case ECkDebug3dCameraPreset::Bottom:
        return {90.0, 0.0, 0.0};
    case ECkDebug3dCameraPreset::Left:
        return {0.0, 180.0, 0.0};
    case ECkDebug3dCameraPreset::Right:
        return {0.0, 0.0, 0.0};
    case ECkDebug3dCameraPreset::Front:
        return {0.0, -90.0, 0.0};
    case ECkDebug3dCameraPreset::Back:
        return {0.0, 90.0, 0.0};
    default:
        return DefaultRotation;
    }
}
} // namespace ck_debug_3d_preview_viewport

class FCkDebug3dPreviewViewportClient final : public FUMGViewportClient
{
  public:
    friend class SCkDebug_3dPreviewViewport;
    FCkDebug3dPreviewViewportClient(FPreviewScene& InScene, FCkDebug3dPreviewScenePolicy InPolicy,
                                    TSharedPtr<ICkDebug3dPreviewAdapter> InAdapter,
                                    TFunction<void(int32)> InStoreBookmark, TFunction<void(int32)> InRecallBookmark)
        : FUMGViewportClient(&InScene), _Policy(InPolicy), _Adapter(InAdapter),
          _InteractionRouter(MakeUnique<FCkDebug3dInteractionRouter>(InAdapter, FCkDebug3dInteractionConfig{})),
          _StoreBookmark(MoveTemp(InStoreBookmark)), _RecallBookmark(MoveTemp(InRecallBookmark))
    {
        EngineShowFlags.EnableAdvancedFeatures();
        EngineShowFlags.SetLighting(_Policy._Lighting);
        EngineShowFlags.SetPostProcessing(_Policy._PostProcessing);
        EngineShowFlags.SetAntiAliasing(_Policy._AntiAliasing);
        EngineShowFlags.SetTemporalAA(_Policy._TemporalAA);
        EngineShowFlags.SetDynamicShadows(_Policy._DynamicShadows);
        EngineShowFlags.SetMotionBlur(_Policy._MotionBlur);
        EngineShowFlags.SetDepthOfField(_Policy._DepthOfField);
        EngineShowFlags.SetEyeAdaptation(_Policy._EyeAdaptation);
        SetBackgroundColor(FLinearColor::Black);
        SetViewLocation(ck_debug_3d_preview_viewport::DefaultLocation);
        SetViewRotation(ck_debug_3d_preview_viewport::DefaultRotation);
        _OrbitDistance = ck_debug_3d_preview_viewport::DefaultOrbitDistance;
        ResyncLookAt();
    }
    auto
    SetViewport(const TSharedRef<FSceneViewport>& InViewport) -> void
    {
        _Viewport = InViewport;
    }
    virtual auto
    CalcSceneView(FSceneViewFamily* InViewFamily) -> FSceneView* override
    {
        auto* View = FUMGViewportClient::CalcSceneView(InViewFamily);
        if (View != nullptr && _Policy._TemporalAA)
        {
            View->AntiAliasingMethod = EAntiAliasingMethod::AAM_TemporalAA;
        }
        return View;
    }
    auto
    GetFeatures() const -> FCkDebug3dViewportRenderFeatures
    {
        return {false,
                false,
                static_cast<bool>(EngineShowFlags.Lighting),
                static_cast<bool>(EngineShowFlags.PostProcessing),
                static_cast<bool>(EngineShowFlags.AntiAliasing),
                static_cast<bool>(EngineShowFlags.TemporalAA),
                static_cast<bool>(EngineShowFlags.DynamicShadows),
                static_cast<bool>(EngineShowFlags.MotionBlur),
                static_cast<bool>(EngineShowFlags.DepthOfField),
                static_cast<bool>(EngineShowFlags.EyeAdaptation)};
    }
    auto
    GetState() const -> FCkDebug3dCameraState
    {
        return {GetViewLocation(), GetViewRotation(), _LookAt, ViewInfo.OrthoWidth, ViewInfo.ProjectionMode};
    }
    auto
    GetLookAt() const -> FVector
    {
        return _LookAt;
    }
    auto
    GetSpeed() const -> float
    {
        return _CameraSpeed;
    }
    auto
    GetPanScale() const -> float
    {
        return static_cast<float>(GestureScale());
    }
    auto
    SetAdapter(TSharedPtr<ICkDebug3dPreviewAdapter> InAdapter) -> void
    {
        _Adapter = InAdapter;
        _InteractionRouter = InAdapter.IsValid()
                                 ? MakeUnique<FCkDebug3dInteractionRouter>(InAdapter, FCkDebug3dInteractionConfig{})
                                 : TUniquePtr<FCkDebug3dInteractionRouter>{};
    }
    auto
    Frame(ECkDebug3dFrameTarget InTarget) -> void
    {
        if (const auto Adapter = _Adapter.Pin())
        {
            FrameBounds(Adapter->Get_FrameBounds(InTarget));
        }
    }
    auto
    ApplyPreset(ECkDebug3dCameraPreset InPreset) -> void
    {
        if (InPreset == ECkDebug3dCameraPreset::FrameAll)
        {
            Frame(ECkDebug3dFrameTarget::All);
            return;
        }
        if (InPreset == ECkDebug3dCameraPreset::FrameSelection)
        {
            Frame(ECkDebug3dFrameTarget::Selection);
            return;
        }
        ViewInfo.ProjectionMode = InPreset == ECkDebug3dCameraPreset::Perspective ? ECameraProjectionMode::Perspective
                                                                                  : ECameraProjectionMode::Orthographic;
        SetViewRotation(ck_debug_3d_preview_viewport::GetPresetRotation(InPreset));
        ResyncLookAt();
        Frame(ECkDebug3dFrameTarget::All);
    }
    virtual auto
    InputKey(const FInputKeyEventArgs& InEventArgs) -> bool override
    {
        return HandleKey(InEventArgs.Viewport, InEventArgs.Key, InEventArgs.Event);
    }
    virtual auto
    InputAxis(const FInputKeyEventArgs& InEventArgs) -> bool override
    {
        return HandleAxis(InEventArgs.Viewport, InEventArgs.Key, InEventArgs.AmountDepressed);
    }
    virtual auto
    MouseMove(FViewport* InViewport, int32 InX, int32 InY) -> void override
    {
        FUMGViewportClient::MouseMove(InViewport, InX, InY);
        if (NOT _InteractionRouter.IsValid())
        {
            return;
        }
        auto Ray = FCkDebug3dCursorRay{};
        if (TryGetCursorRay(InViewport, Ray))
        {
            _InteractionRouter->TickHover(Ray, GetModifiers(), FPlatformTime::Seconds());
        }
    }
    virtual auto
    MouseLeave(FViewport* InViewport) -> void override
    {
        FocusLost();
        FUMGViewportClient::MouseLeave(InViewport);
    }
    virtual auto
    LostFocus(FViewport* InViewport) -> void override
    {
        FocusLost();
        FUMGViewportClient::LostFocus(InViewport);
    }
    auto
    HandleKey(FViewport* InViewport, const FKey& InKey, EInputEvent InEvent) -> bool
    {
        const auto Pressed = InEvent == IE_Pressed;
        const auto Released = InEvent == IE_Released;
        Track(InKey, Pressed, Released);
        const auto Modifiers = GetModifiers();
        if (Pressed && _InteractionRouter.IsValid() &&
            _InteractionRouter->OnKey(InKey, Modifiers, FPlatformTime::Seconds()))
        {
            return true;
        }
        if (Pressed)
        {
            const auto BookmarkSlot = ck_debug_3d_preview_viewport::TryGetBookmarkSlot(InKey);
            if (BookmarkSlot.IsSet())
            {
                if ((_ControlDown || _CommandDown) && NOT _AltDown && NOT _ShiftDown)
                {
                    if (_StoreBookmark)
                    {
                        _StoreBookmark(*BookmarkSlot);
                    }
                    return true;
                }
                if (NOT Modifiers.HasAny())
                {
                    if (_RecallBookmark)
                    {
                        _RecallBookmark(*BookmarkSlot);
                    }
                    return true;
                }
            }
        }
        if (Pressed && NOT Modifiers.HasAny() && InKey == EKeys::F)
        {
            ApplyPreset(ECkDebug3dCameraPreset::FrameSelection);
            return true;
        }
        if (Pressed && InKey == EKeys::Home)
        {
            ApplyPreset(ECkDebug3dCameraPreset::FrameAll);
            return true;
        }
        const auto Wheel = InKey == EKeys::MouseScrollUp || InKey == EKeys::MouseScrollDown;
        if (Pressed && Wheel)
        {
            const auto Sign = InKey == EKeys::MouseScrollUp ? 1.0f : -1.0f;
            if (_InteractionRouter.IsValid() && _InteractionRouter->OnWheel(Sign, Modifiers))
            {
                return true;
            }
            if (_RightMouseDown && ViewInfo.ProjectionMode == ECameraProjectionMode::Perspective)
            {
                _CameraSpeed = FMath::Clamp(_CameraSpeed * (Sign > 0 ? 1.1f : 1.0f / 1.1f), 0.00001f, 10000.0f);
            }
            else
            {
                Dolly(Sign);
            }
            return true;
        }
        if (InKey == EKeys::LeftMouseButton && _InteractionRouter.IsValid())
        {
            auto ScreenPosition = FVector2D::ZeroVector;
            auto Ray = FCkDebug3dCursorRay{};
            TryGetPointerContext(InViewport, ScreenPosition, Ray);
            if (Pressed && NOT _AltDown && NOT _RightMouseDown && NOT _MiddleMouseDown)
            {
                return _InteractionRouter->OnPointerPressed(ECkDebug3dPointerButton::Left, ScreenPosition, Modifiers,
                                                            Ray);
            }
            if (Released && _InteractionRouter->HasActiveGesture())
            {
                return _InteractionRouter->OnPointerReleased(ECkDebug3dPointerButton::Left, ScreenPosition, Modifiers,
                                                             Ray);
            }
        }
        return InKey == EKeys::LeftMouseButton || InKey == EKeys::RightMouseButton || InKey == EKeys::MiddleMouseButton;
    }
    auto
    HandleAxis(FViewport* InViewport, const FKey& InAxis, float InDelta) -> bool
    {
        if (InAxis != EKeys::MouseX && InAxis != EKeys::MouseY)
        {
            return false;
        }
        if (_InteractionRouter.IsValid() && _InteractionRouter->HasActiveDrag())
        {
            auto Ray = FCkDebug3dCursorRay{};
            if (TryGetCursorRay(InViewport, Ray))
            {
                _InteractionRouter->OnDragRay(Ray);
            }
            return true;
        }
        const auto Horizontal = InAxis == EKeys::MouseX;
        if (_AltDown && _LeftMouseDown && ViewInfo.ProjectionMode == ECameraProjectionMode::Perspective)
        {
            Orbit(Horizontal, InDelta);
            return true;
        }
        if (_RightMouseDown)
        {
            if (ViewInfo.ProjectionMode == ECameraProjectionMode::Perspective)
            {
                Look(Horizontal, InDelta);
            }
            else
            {
                Pan(Horizontal, InDelta);
            }
            return true;
        }
        if (_MiddleMouseDown)
        {
            Pan(Horizontal, InDelta);
            return true;
        }
        if (_LeftMouseDown && ViewInfo.ProjectionMode == ECameraProjectionMode::Perspective)
        {
            TrackAndYaw(Horizontal, InDelta);
            return true;
        }
        return false;
    }
    auto
    FocusLost() -> void
    {
        if (_InteractionRouter.IsValid())
        {
            _InteractionRouter->OnFocusLost();
        }
        _AltDown = _ControlDown = _ShiftDown = _CommandDown = _LeftMouseDown = _RightMouseDown = _MiddleMouseDown =
            false;
    }
    auto
    HasGesture() const -> bool
    {
        return (_InteractionRouter.IsValid() && _InteractionRouter->HasActiveGesture()) || _AltDown || _ControlDown ||
               _LeftMouseDown || _RightMouseDown || _MiddleMouseDown;
    }
    auto
    TickNavigation(float InDeltaTime) -> void
    {
        if (NOT _RightMouseDown || ViewInfo.ProjectionMode != ECameraProjectionMode::Perspective)
        {
            return;
        }
        const auto SceneViewport = _Viewport.Pin();
        if (NOT SceneViewport.IsValid())
        {
            return;
        }
        auto Direction = FVector::ZeroVector;
        if (SceneViewport->KeyState(EKeys::W) || SceneViewport->KeyState(EKeys::Up))
        {
            Direction += GetViewRotation().Vector();
        }
        if (SceneViewport->KeyState(EKeys::S) || SceneViewport->KeyState(EKeys::Down))
        {
            Direction -= GetViewRotation().Vector();
        }
        if (SceneViewport->KeyState(EKeys::D) || SceneViewport->KeyState(EKeys::Right))
        {
            Direction += GetViewRotation().RotateVector(FVector::RightVector);
        }
        if (SceneViewport->KeyState(EKeys::A) || SceneViewport->KeyState(EKeys::Left))
        {
            Direction -= GetViewRotation().RotateVector(FVector::RightVector);
        }
        if (SceneViewport->KeyState(EKeys::E))
        {
            Direction += FVector::UpVector;
        }
        if (SceneViewport->KeyState(EKeys::Q))
        {
            Direction -= FVector::UpVector;
        }
        if (NOT Direction.IsNearlyZero())
        {
            Translate(Direction.GetSafeNormal() * _CameraSpeed * 32.0f * InDeltaTime);
        }
    }
    auto
    Follow(TOptional<FVector> InCenter, bool InEnabled) -> void
    {
        if (NOT InEnabled || NOT InCenter.IsSet())
        {
            _LastFollowCenter.Reset();
            return;
        }
        if (_LastFollowCenter.IsSet())
        {
            Translate(*InCenter - *_LastFollowCenter);
        }
        _LastFollowCenter = InCenter;
    }
    auto
    Store(TArray<FCkDebug3dCameraBookmark>& InOutBookmarks, int32 InSlot) const -> void
    {
        if (NOT InOutBookmarks.IsValidIndex(InSlot))
        {
            return;
        }
        InOutBookmarks[InSlot] = {GetViewLocation(), GetViewRotation(), ViewInfo.OrthoWidth,
                                  ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic, true};
    }
    auto
    Recall(const TArray<FCkDebug3dCameraBookmark>& InBookmarks, int32 InSlot) -> void
    {
        if (NOT InBookmarks.IsValidIndex(InSlot) || NOT InBookmarks[InSlot]._IsSet)
        {
            return;
        }
        const auto& Bookmark = InBookmarks[InSlot];
        ViewInfo.ProjectionMode =
            Bookmark._IsOrthographic ? ECameraProjectionMode::Orthographic : ECameraProjectionMode::Perspective;
        ViewInfo.OrthoWidth = Bookmark._OrthoWidth;
        SetViewLocation(Bookmark._Location);
        SetViewRotation(Bookmark._Rotation);
        ResyncLookAt();
    }

  private:
    auto
    Translate(const FVector& InDelta) -> void
    {
        if (InDelta.IsNearlyZero())
        {
            return;
        }
        SetViewLocation(GetViewLocation() + InDelta);
        _LookAt += InDelta;
        SetLookAtLocation(_LookAt);
    }
    auto
    ResyncLookAt() -> void
    {
        _LookAt = GetViewLocation() + GetViewRotation().Vector() * FMath::Max(_OrbitDistance, 1.0f);
        SetLookAtLocation(_LookAt);
    }
    auto
    GestureScale() const -> double
    {
        const auto Extent = ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic
                                ? FMath::Max(static_cast<double>(ViewInfo.OrthoWidth), 1.0)
                                : FMath::Max(static_cast<double>(_OrbitDistance), 1.0);
        return Extent * 0.001 * FMath::Clamp(_CameraSpeed / 1000.0f, 0.1f, 100.0f);
    }
    auto
    Look(bool InHorizontal, float InDelta) -> void
    {
        auto Rotation = GetViewRotation();
        if (InHorizontal)
        {
            Rotation.Yaw += InDelta * 0.25f;
        }
        else
        {
            Rotation.Pitch = FMath::Clamp(Rotation.Pitch + InDelta * 0.25f, -89.0f, 89.0f);
        }
        SetViewRotation(Rotation);
        ResyncLookAt();
    }
    auto
    Orbit(bool InHorizontal, float InDelta) -> void
    {
        const auto Pivot = _LookAt;
        auto Rotation = GetViewRotation();
        if (InHorizontal)
        {
            Rotation.Yaw += InDelta * 0.25f;
        }
        else
        {
            Rotation.Pitch = FMath::Clamp(Rotation.Pitch + InDelta * 0.25f, -89.0f, 89.0f);
        }
        SetViewRotation(Rotation);
        SetViewLocation(Pivot - Rotation.Vector() * FMath::Max(_OrbitDistance, 1.0f));
    }
    auto
    Pan(bool InHorizontal, float InDelta) -> void
    {
        const auto Axis = InHorizontal ? GetViewRotation().RotateVector(FVector::RightVector) * -1.0f
                                       : GetViewRotation().RotateVector(FVector::UpVector);
        Translate(Axis * (InDelta * GestureScale()));
    }
    auto
    Dolly(float InDelta) -> void
    {
        if (ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
        {
            ViewInfo.OrthoWidth =
                FMath::Clamp(ViewInfo.OrthoWidth / (InDelta > 0 ? 1.1f : 1.0f / 1.1f), 10.0f, 1000000.0f);
            return;
        }
        const auto Delta = GetViewRotation().Vector() * (_OrbitDistance * 0.1f * InDelta);
        Translate(Delta);
    }
    auto
    TrackAndYaw(bool InHorizontal, float InDelta) -> void
    {
        if (InHorizontal)
        {
            constexpr auto IsYawInput = true;
            Look(IsYawInput, InDelta);
        }
        else
        {
            Translate(GetViewRotation().Vector() * (-InDelta * GestureScale()));
        }
    }
    auto
    FrameBounds(const FBox& InBounds) -> void
    {
        if (InBounds.IsValid == 0)
        {
            return;
        }
        const auto Center = InBounds.GetCenter();
        auto Radius = FMath::Max(InBounds.GetExtent().Size(), 10.0);
        auto AspectRatio = 1.777777f;
        if (const auto SceneViewport = _Viewport.Pin())
        {
            const auto Size = SceneViewport->GetSizeXY();
            if (Size.X > 0 && Size.Y > 0)
            {
                AspectRatio = SceneViewport->GetDesiredAspectRatio();
            }
        }
        if (ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
        {
            ViewInfo.OrthoWidth = Radius * 2.0f * FMath::Max(AspectRatio, 1.0f);
            SetViewLocation(Center - GetViewRotation().Vector() * (Radius + 10.0));
            _LookAt = Center;
            SetLookAtLocation(Center);
        }
        else
        {
            if (AspectRatio > 1.0f)
            {
                Radius *= AspectRatio;
            }
            _OrbitDistance = Radius / FMath::Tan(FMath::DegreesToRadians(ViewInfo.FOV * 0.5f));
            _LookAt = Center;
            SetLookAtLocation(Center);
            SetViewLocation(Center - GetViewRotation().Vector() * _OrbitDistance);
        }
    }
    auto
    TryGetCursorRay(FViewport* InViewport, FCkDebug3dCursorRay& OutRay) const -> bool
    {
        if (InViewport == nullptr)
        {
            return false;
        }
        const auto Size = InViewport->GetSizeXY();
        if (Size.X <= 0 || Size.Y <= 0)
        {
            return false;
        }
        auto Mouse = FIntPoint{};
        InViewport->GetMousePos(Mouse);
        auto ViewInit = FSceneViewInitOptions{};
        ViewInit.SetViewRectangle(FIntRect{0, 0, Size.X, Size.Y});
        ViewInit.ViewOrigin = GetViewLocation();
        ViewInit.ViewRotationMatrix =
            FInverseRotationMatrix(GetViewRotation()) *
            FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
        auto ProjectionInfo = ViewInfo;
        FMinimalViewInfo::CalculateProjectionMatrixGivenView(
            ProjectionInfo, GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint, InViewport, ViewInit);
        FSceneView::DeprojectScreenToWorld(
            FVector2D{Mouse}, ViewInit.ViewRect,
            ck::Debug3dViewport::Make_InverseViewMatrix(ViewInit.ViewOrigin, ViewInit.ViewRotationMatrix),
            ViewInit.ProjectionMatrix.InverseFast(), OutRay._Origin, OutRay._Direction);
        return NOT OutRay._Direction.IsNearlyZero();
    }
    auto
    TryProjectWorldToLocal(const FVector& InWorldPosition, const FVector2D& InLocalSize,
                           FVector2D& OutLocalPosition) const -> bool
    {
        const auto SceneViewport = _Viewport.Pin();
        if (NOT SceneViewport.IsValid())
        {
            return false;
        }
        const auto Size = SceneViewport->GetSizeXY();
        if (Size.X <= 0 || Size.Y <= 0 || InLocalSize.X <= 0.0 || InLocalSize.Y <= 0.0)
        {
            return false;
        }
        auto ViewInit = FSceneViewInitOptions{};
        ViewInit.SetViewRectangle(FIntRect{0, 0, Size.X, Size.Y});
        ViewInit.ViewOrigin = GetViewLocation();
        ViewInit.ViewRotationMatrix =
            FInverseRotationMatrix(GetViewRotation()) *
            FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
        auto ProjectionInfo = ViewInfo;
        FMinimalViewInfo::CalculateProjectionMatrixGivenView(
            ProjectionInfo, GetDefault<ULocalPlayer>()->AspectRatioAxisConstraint, SceneViewport.Get(), ViewInit);
        auto ScreenPosition = FVector2D::ZeroVector;
        const auto ViewProjection =
            FTranslationMatrix(-ViewInit.ViewOrigin) * ViewInit.ViewRotationMatrix * ViewInit.ProjectionMatrix;
        if (NOT FSceneView::ProjectWorldToScreen(InWorldPosition, ViewInit.ViewRect, ViewProjection, ScreenPosition))
        {
            return false;
        }
        OutLocalPosition = ck::Debug3dViewport::ViewPixelsToLocalSlate(ScreenPosition, FVector2D{Size}, InLocalSize);
        return true;
    }
    auto
    TryGetPointerContext(FViewport* InViewport, FVector2D& OutPosition, FCkDebug3dCursorRay& OutRay) const -> bool
    {
        if (InViewport == nullptr)
        {
            return false;
        }
        auto Mouse = FIntPoint{};
        InViewport->GetMousePos(Mouse);
        OutPosition = FVector2D{Mouse};
        return TryGetCursorRay(InViewport, OutRay);
    }
    auto
    GetModifiers() const -> FCkDebug3dInteractionModifiers
    {
        return {_ControlDown, _ShiftDown, _AltDown, _CommandDown};
    }
    auto
    Track(const FKey& InKey, bool InPressed, bool InReleased) -> void
    {
        const auto IsDown = InPressed ? true : InReleased ? false : false;
        if (InKey == EKeys::LeftAlt || InKey == EKeys::RightAlt)
        {
            _AltDown = IsDown;
        }
        else if (InKey == EKeys::LeftControl || InKey == EKeys::RightControl)
        {
            _ControlDown = IsDown;
        }
        else if (InKey == EKeys::LeftShift || InKey == EKeys::RightShift)
        {
            _ShiftDown = IsDown;
        }
        else if (InKey == EKeys::LeftCommand || InKey == EKeys::RightCommand)
        {
            _CommandDown = IsDown;
        }
        else if (InKey == EKeys::LeftMouseButton)
        {
            _LeftMouseDown = IsDown;
        }
        else if (InKey == EKeys::RightMouseButton)
        {
            _RightMouseDown = IsDown;
        }
        else if (InKey == EKeys::MiddleMouseButton)
        {
            _MiddleMouseDown = IsDown;
        }
    }
    FCkDebug3dPreviewScenePolicy _Policy;
    TWeakPtr<FSceneViewport> _Viewport;
    TWeakPtr<ICkDebug3dPreviewAdapter> _Adapter;
    TUniquePtr<FCkDebug3dInteractionRouter> _InteractionRouter;
    TFunction<void(int32)> _StoreBookmark;
    TFunction<void(int32)> _RecallBookmark;
    FVector _LookAt = FVector::ZeroVector;
    TOptional<FVector> _LastFollowCenter;
    float _OrbitDistance = 1.0f;
    float _CameraSpeed = 1.0f;
    bool _AltDown = false;
    bool _ControlDown = false;
    bool _ShiftDown = false;
    bool _CommandDown = false;
    bool _LeftMouseDown = false;
    bool _RightMouseDown = false;
    bool _MiddleMouseDown = false;
};

auto
    FCkDebug3dCameraState::
    Equals(const FCkDebug3dCameraState& InOther) const
    -> bool
{
    return _Location.Equals(InOther._Location) && _Rotation.Equals(InOther._Rotation) &&
           _LookAt.Equals(InOther._LookAt) && FMath::IsNearlyEqual(_OrthoWidth, InOther._OrthoWidth) &&
           _ProjectionMode == InOther._ProjectionMode;
}
auto
    ck::Debug3dViewport::
    Make_InverseViewMatrix(const FVector& InViewOrigin, const FMatrix& InViewRotationMatrix)
    -> FMatrix
{
    return InViewRotationMatrix.GetTransposed() * FTranslationMatrix(InViewOrigin);
}
auto
    ck::Debug3dViewport::
    ViewPixelsToLocalSlate(const FVector2D& InPixels, const FVector2D& InViewSize,
        const FVector2D& InLocalSize)
    -> FVector2D
{
    return InViewSize.X > 0 && InViewSize.Y > 0
               ? FVector2D{InPixels.X * InLocalSize.X / InViewSize.X, InPixels.Y * InLocalSize.Y / InViewSize.Y}
               : FVector2D::ZeroVector;
}

SCkDebug_3dPreviewViewport::~SCkDebug_3dPreviewViewport()
{
    Teardown();
}
auto
    SCkDebug_3dPreviewViewport::
    Construct(const FArguments& InArgs)
    -> void
{
    _Descriptor = InArgs._Descriptor;
    _Adapter = InArgs._Adapter;
    _CameraBookmarks = InArgs._CameraBookmarks;
    _CameraBookmarks.SetNum(ck_debug_3d_preview_viewport::BookmarkCount);
    _OnCameraBookmarksChanged = InArgs._OnCameraBookmarksChanged;
    _ShowOrientationCube = _Descriptor._ShowOrientationCube;
    constexpr auto CreatePhysicsScene = false;
    constexpr auto IsEditorScene = false;
    _PreviewScene = MakeShared<FPreviewScene>(FPreviewScene::ConstructionValues()
                                                  .SetCreateDefaultLighting(_Descriptor._PreviewPolicy._DefaultLighting)
                                                  .SetSkyBrightness(_Descriptor._PreviewPolicy._SkyBrightness)
                                                  .SetLightBrightness(_Descriptor._PreviewPolicy._LightBrightness)
                                                  .SetCreatePhysicsScene(CreatePhysicsScene)
                                                  .SetEditor(IsEditorScene));
    auto Args = SViewport::FArguments{};
    constexpr auto IgnoreTextureAlpha = false;
    constexpr auto EnableBlending = false;
    Args.IgnoreTextureAlpha(IgnoreTextureAlpha);
    Args.EnableBlending(EnableBlending);
    SViewport::Construct(Args);
    _ViewportClient = MakeShared<FCkDebug3dPreviewViewportClient>(
        *_PreviewScene, _Descriptor._PreviewPolicy, InArgs._Adapter, [this](int32 InSlot)
        {
            Store_CameraBookmark(InSlot);
        }, [this](int32 InSlot)
        {
            Recall_CameraBookmark(InSlot);
        });
    _SceneViewport = MakeShared<FSceneViewport>(_ViewportClient.Get(), SharedThis(this));
    _ViewportClient->SetViewport(_SceneViewport.ToSharedRef());
    SetViewportInterface(_SceneViewport.ToSharedRef());
    const auto WeakAdapter = _Adapter;
    auto Actions = TArray<FCkDebug_IconToggleAction>{
        {TEXT("Viewport3d.Grid"), TEXT("Net"), FText::FromString(TEXT("Grid")),
         FText::FromString(TEXT("Toggle preview grid.")),
         TAttribute<bool>::CreateLambda(
             [WeakAdapter]()
             {
                 const auto Adapter = WeakAdapter.Pin();
                 return Adapter.IsValid() && Adapter->Get_ShowGrid();
             }),
         FOnCkDebug_IconToggleChanged::CreateLambda(
             [WeakAdapter](bool InIsOn)
             {
                 if (const auto Adapter = WeakAdapter.Pin())
                 {
                     Adapter->Set_ShowGrid(InIsOn);
                 }
             })},
        {TEXT("Viewport3d.RenderMode"), TEXT("Grid"), FText::FromString(TEXT("Render mode")),
         FText::FromString(TEXT("Cycle None, Transparent Only, All.")),
         TAttribute<bool>::CreateLambda(
             [WeakAdapter]()
             {
                 const auto Adapter = WeakAdapter.Pin();
                 return Adapter.IsValid() && Adapter->Get_RenderMode() != ECkDebug3dRenderMode::None;
             }),
         FOnCkDebug_IconToggleChanged::CreateLambda(
             [WeakAdapter](bool)
             {
                 if (const auto Adapter = WeakAdapter.Pin())
                 {
                     const auto Current = Adapter->Get_RenderMode();
                     Adapter->Set_RenderMode(
                         Current == ECkDebug3dRenderMode::None              ? ECkDebug3dRenderMode::TransparentOnly
                         : Current == ECkDebug3dRenderMode::TransparentOnly ? ECkDebug3dRenderMode::All
                                                                            : ECkDebug3dRenderMode::None);
                 }
             })}};
    if (EnumHasAnyFlags(Get_Capabilities(), ECkDebug3dViewportCapability::Labels))
    {
        Actions.Emplace(TEXT("Viewport3d.Labels"), TEXT("Label"), FText::FromString(TEXT("Labels")),
                        FText::FromString(TEXT("Toggle content labels.")),
                        TAttribute<bool>::CreateLambda(
                            [WeakAdapter]()
                            {
                                const auto Adapter = WeakAdapter.Pin();
                                return Adapter.IsValid() && Adapter->Get_ShowLabels();
                            }),
                        FOnCkDebug_IconToggleChanged::CreateLambda(
                            [WeakAdapter](bool InIsOn)
                            {
                                if (const auto Adapter = WeakAdapter.Pin())
                                {
                                    Adapter->Set_ShowLabels(InIsOn);
                                }
                            }));
    }
    auto CompleteControls = SNew(SHorizontalBox);
    for (const auto ControlId : Build_CommonControlDescriptors())
    {
        auto Label = ControlId.ToString();
        Label.RemoveFromStart(TEXT("Viewport3d."));
        Label.ReplaceInline(TEXT("."), TEXT(" / "));
        if (Label.StartsWith(TEXT("Bookmark / Store / ")))
        {
            Label = TEXT("Store B") + Label.RightChop(19);
        }
        else if (Label.StartsWith(TEXT("Bookmark / ")))
        {
            Label = TEXT("B") + Label.RightChop(11);
        }
        CompleteControls->AddSlot()
            .AutoWidth()[SNew(SButton)
                             .Text(FText::FromString(Label))
                             .ToolTipText(FText::FromString(FString::Printf(TEXT("%s control"), *Label)))
                             .OnClicked_Lambda(
                                 [this, ControlId]()
                                 {
                                     Invoke_CommonControl(ControlId);
                                     return FReply::Handled();
                                 })];
    }
    _CommonControls = CompleteControls;
    {
        ChildSlot[SNew(SOverlay) +
                  SOverlay::Slot()
                      .HAlign(HAlign_Fill)
                      .VAlign(VAlign_Top)[SNew(SScrollBox).Orientation(Orient_Horizontal) +
                                          SScrollBox::Slot()[_CommonControls.ToSharedRef()]] +
                  SOverlay::Slot()
                      .HAlign(HAlign_Left)
                      .VAlign(VAlign_Bottom)[SNew(SCkDebug_OrientationCube)
                                                 .Visibility_Lambda(
                                                     [this]()
                                                     {
                                                         return _ShowOrientationCube ? EVisibility::HitTestInvisible
                                                                                     : EVisibility::Collapsed;
                                                     })
                                                 .Rotation_Lambda(
                                                     [this]()
                                                     {
                                                         return _ViewportClient.IsValid()
                                                                    ? _ViewportClient->GetViewRotation()
                                                                          .Quaternion()
                                                                          .Inverse()
                                                                    : FQuat::Identity;
                                                     })]];
    }
}
auto
    SCkDebug_3dPreviewViewport::
    Teardown()
    -> void
{
    if (_TornDown)
    {
        return;
    }
    _TornDown = true;
    if (const auto Adapter = _Adapter.Pin())
    {
        Adapter->On_ViewportTeardown();
    }
    _Adapter.Reset();
    if (_ViewportClient.IsValid())
    {
        _ViewportClient->SetAdapter({});
    }
    _ViewportClient.Reset();
    _SceneViewport.Reset();
    _PreviewScene.Reset();
    _CommonControls.Reset();
}
auto
    SCkDebug_3dPreviewViewport::
    Get_PreviewWorld() const
    -> UWorld*
{
    return _PreviewScene.IsValid() ? _PreviewScene->GetWorld() : nullptr;
}
auto
    SCkDebug_3dPreviewViewport::
    Get_RenderFeatures() const
    -> FCkDebug3dViewportRenderFeatures
{
    return _ViewportClient.IsValid() ? _ViewportClient->GetFeatures() : FCkDebug3dViewportRenderFeatures{};
}
auto
    SCkDebug_3dPreviewViewport::
    Apply_CameraPreset(ECkDebug3dCameraPreset InPreset)
    -> void
{
    if (_ViewportClient.IsValid())
    {
        _ViewportClient->ApplyPreset(InPreset);
    }
}
auto
    SCkDebug_3dPreviewViewport::
    Get_ProjectionMode() const
    -> ECameraProjectionMode::Type
{
    return _ViewportClient.IsValid() ? _ViewportClient->GetState()._ProjectionMode : ECameraProjectionMode::Perspective;
}
auto
    SCkDebug_3dPreviewViewport::
    Get_ViewLocation() const
    -> FVector
{
    return _ViewportClient.IsValid() ? _ViewportClient->GetState()._Location : FVector::ZeroVector;
}
auto
    SCkDebug_3dPreviewViewport::
    Get_ViewRotation() const
    -> FRotator
{
    return _ViewportClient.IsValid() ? _ViewportClient->GetState()._Rotation : FRotator::ZeroRotator;
}
auto
    SCkDebug_3dPreviewViewport::
    Get_LookAtLocation() const
    -> FVector
{
    return _ViewportClient.IsValid() ? _ViewportClient->GetLookAt() : FVector::ZeroVector;
}
auto
    SCkDebug_3dPreviewViewport::
    Get_CameraState() const
    -> FCkDebug3dCameraState
{
    return _ViewportClient.IsValid() ? _ViewportClient->GetState() : FCkDebug3dCameraState{};
}
auto
    SCkDebug_3dPreviewViewport::
    Get_CameraSpeed() const
    -> float
{
    return _ViewportClient.IsValid() ? _ViewportClient->GetSpeed() : 0.0f;
}
auto
    SCkDebug_3dPreviewViewport::
    Get_EffectivePanScale() const
    -> float
{
    return _ViewportClient.IsValid() ? _ViewportClient->GetPanScale() : 0.0f;
}
auto
    SCkDebug_3dPreviewViewport::
    TryProject_WorldToLocal(const FVector& InWorldPosition, const FVector2D& InLocalSize,
        FVector2D& OutLocalPosition) const
    -> bool
{
    return _ViewportClient.IsValid() &&
           _ViewportClient->TryProjectWorldToLocal(InWorldPosition, InLocalSize, OutLocalPosition);
}
auto
    SCkDebug_3dPreviewViewport::
    Input_Key(const FKey& InKey, EInputEvent InEvent)
    -> bool
{
    return _ViewportClient.IsValid() && _ViewportClient->HandleKey(nullptr, InKey, InEvent);
}
auto
    SCkDebug_3dPreviewViewport::
    Input_MouseAxis(const FKey& InAxisKey, float InDelta)
    -> bool
{
    return _ViewportClient.IsValid() && _ViewportClient->HandleAxis(nullptr, InAxisKey, InDelta);
}
auto
    SCkDebug_3dPreviewViewport::
    Handle_FocusLost()
    -> void
{
    if (_ViewportClient.IsValid())
    {
        _ViewportClient->FocusLost();
    }
}

auto
    SCkDebug_3dPreviewViewport::
    Get_HasActiveCameraGesture() const
    -> bool
{
    return _ViewportClient.IsValid() && _ViewportClient->HasGesture();
}
auto
    SCkDebug_3dPreviewViewport::
    Get_ShowOrientationCube() const
    -> bool
{
    return _ShowOrientationCube;
}
auto
    SCkDebug_3dPreviewViewport::
    Store_CameraBookmark(int32 InSlot)
    -> void
{
    if (_ViewportClient.IsValid())
    {
        _ViewportClient->Store(_CameraBookmarks, InSlot);
        _OnCameraBookmarksChanged.ExecuteIfBound(_CameraBookmarks);
    }
}
auto
    SCkDebug_3dPreviewViewport::
    Recall_CameraBookmark(int32 InSlot)
    -> void
{
    if (_ViewportClient.IsValid())
    {
        _ViewportClient->Recall(_CameraBookmarks, InSlot);
    }
}
auto
    SCkDebug_3dPreviewViewport::
    Get_CameraBookmarks() const
    -> const TArray<FCkDebug3dCameraBookmark>&
{
    return _CameraBookmarks;
}
auto
    SCkDebug_3dPreviewViewport::
    Import_CameraBookmarks(TArray<FCkDebug3dCameraBookmark> InBookmarks)
    -> void
{
    _CameraBookmarks = MoveTemp(InBookmarks);
    _CameraBookmarks.SetNum(ck_debug_3d_preview_viewport::BookmarkCount);
}
auto
    SCkDebug_3dPreviewViewport::
    Set_FollowSelection(bool InIsEnabled)
    -> void
{
    _FollowSelection = InIsEnabled;
}
auto
    SCkDebug_3dPreviewViewport::
    Tick_FollowSelection()
    -> void
{
    if (_ViewportClient.IsValid())
    {
        const auto Adapter = _Adapter.Pin();
        _ViewportClient->Follow(Adapter.IsValid() ? Adapter->Get_SelectionCenter() : TOptional<FVector>{},
                                _FollowSelection);
    }
}
auto
    SCkDebug_3dPreviewViewport::
    Set_IsolateSelection(bool InIsEnabled)
    -> void
{
    _IsolateSelection = InIsEnabled;
    Set_SelectionKeys(_SelectionKeys);
}
auto
    SCkDebug_3dPreviewViewport::
    Set_SelectionKeys(TArray<uint64> InKeys)
    -> void
{
    _SelectionKeys = MoveTemp(InKeys);
    _IsolatedKeys = _IsolateSelection ? _SelectionKeys : TArray<uint64>{};
    if (const auto Adapter = _Adapter.Pin())
    {
        Adapter->Set_IsolatedKeys(_IsolatedKeys);
    }
}
auto
    SCkDebug_3dPreviewViewport::
    Get_IsolatedKeys() const
    -> const TArray<uint64>&
{
    return _IsolatedKeys;
}
auto
    SCkDebug_3dPreviewViewport::
    Get_Capabilities() const
    -> ECkDebug3dViewportCapability
{
    const auto Adapter = _Adapter.Pin();
    return Adapter.IsValid() ? Adapter->Get_Capabilities() : ECkDebug3dViewportCapability::None;
}
auto
    SCkDebug_3dPreviewViewport::
    Build_CommonControlDescriptors() const
    -> TArray<FName>
{
    auto Ids = TArray<FName>{TEXT("Viewport3d.Camera"),
                             TEXT("Viewport3d.Camera.Perspective"),
                             TEXT("Viewport3d.Camera.Top"),
                             TEXT("Viewport3d.Camera.Bottom"),
                             TEXT("Viewport3d.Camera.Left"),
                             TEXT("Viewport3d.Camera.Right"),
                             TEXT("Viewport3d.Camera.Front"),
                             TEXT("Viewport3d.Camera.Back"),
                             TEXT("Viewport3d.FrameAll"),
                             TEXT("Viewport3d.Grid"),
                             TEXT("Viewport3d.RenderMode"),
                             TEXT("Viewport3d.RenderMode.None"),
                             TEXT("Viewport3d.RenderMode.TransparentOnly"),
                             TEXT("Viewport3d.RenderMode.All"),
                             TEXT("Viewport3d.OrientationCube"),
                             TEXT("Viewport3d.Bookmarks")};
    for (auto Index = 0; Index < ck_debug_3d_preview_viewport::BookmarkCount; ++Index)
    {
        Ids.Add(FName{*FString::Printf(TEXT("Viewport3d.Bookmark.%d"), Index)});
        Ids.Add(FName{*FString::Printf(TEXT("Viewport3d.Bookmark.Store.%d"), Index)});
    }
    const auto Caps = Get_Capabilities();
    if (EnumHasAnyFlags(Caps, ECkDebug3dViewportCapability::FrameSelection))
    {
        Ids.Add(TEXT("Viewport3d.FrameSelection"));
    }
    if (EnumHasAnyFlags(Caps, ECkDebug3dViewportCapability::Labels))
    {
        Ids.Add(TEXT("Viewport3d.Labels"));
    }
    if (EnumHasAnyFlags(Caps, ECkDebug3dViewportCapability::DirectionGlyphScale))
    {
        Ids.Add(TEXT("Viewport3d.DirectionGlyphScale"));
    }
    if (EnumHasAnyFlags(Caps, ECkDebug3dViewportCapability::FollowSelection))
    {
        Ids.Add(TEXT("Viewport3d.Follow"));
    }
    if (EnumHasAnyFlags(Caps, ECkDebug3dViewportCapability::IsolateSelection))
    {
        Ids.Add(TEXT("Viewport3d.Isolate"));
    }
    return Ids;
}
auto
    SCkDebug_3dPreviewViewport::
    Invoke_CommonControl(FName InControlId)
    -> bool
{
    const auto Id = InControlId.ToString();
    const auto Adapter = _Adapter.Pin();
    if (Id == TEXT("Viewport3d.Camera") || Id == TEXT("Viewport3d.Camera.Perspective"))
    {
        Apply_CameraPreset(ECkDebug3dCameraPreset::Perspective);
        return true;
    }
    const TPair<const TCHAR*, ECkDebug3dCameraPreset> Presets[] = {
        {TEXT("Top"), ECkDebug3dCameraPreset::Top},     {TEXT("Bottom"), ECkDebug3dCameraPreset::Bottom},
        {TEXT("Left"), ECkDebug3dCameraPreset::Left},   {TEXT("Right"), ECkDebug3dCameraPreset::Right},
        {TEXT("Front"), ECkDebug3dCameraPreset::Front}, {TEXT("Back"), ECkDebug3dCameraPreset::Back}};
    for (const auto& [Name, Preset] : Presets)
    {
        if (Id == FString::Printf(TEXT("Viewport3d.Camera.%s"), Name))
        {
            Apply_CameraPreset(Preset);
            return true;
        }
    }
    if (Id == TEXT("Viewport3d.FrameAll"))
    {
        Apply_CameraPreset(ECkDebug3dCameraPreset::FrameAll);
        return true;
    }
    if (Id == TEXT("Viewport3d.FrameSelection") &&
        EnumHasAnyFlags(Get_Capabilities(), ECkDebug3dViewportCapability::FrameSelection))
    {
        Apply_CameraPreset(ECkDebug3dCameraPreset::FrameSelection);
        return true;
    }
    if (Id == TEXT("Viewport3d.Grid") && Adapter.IsValid())
    {
        Adapter->Set_ShowGrid(NOT Adapter->Get_ShowGrid());
        return true;
    }
    if (Id == TEXT("Viewport3d.RenderMode") && Adapter.IsValid())
    {
        const auto Current = Adapter->Get_RenderMode();
        Adapter->Set_RenderMode(Current == ECkDebug3dRenderMode::None ? ECkDebug3dRenderMode::TransparentOnly
                                : Current == ECkDebug3dRenderMode::TransparentOnly ? ECkDebug3dRenderMode::All
                                                                                   : ECkDebug3dRenderMode::None);
        return true;
    }
    if (Id == TEXT("Viewport3d.RenderMode.None") && Adapter.IsValid())
    {
        Adapter->Set_RenderMode(ECkDebug3dRenderMode::None);
        return true;
    }
    if (Id == TEXT("Viewport3d.RenderMode.TransparentOnly") && Adapter.IsValid())
    {
        Adapter->Set_RenderMode(ECkDebug3dRenderMode::TransparentOnly);
        return true;
    }
    if (Id == TEXT("Viewport3d.RenderMode.All") && Adapter.IsValid())
    {
        Adapter->Set_RenderMode(ECkDebug3dRenderMode::All);
        return true;
    }
    if (Id == TEXT("Viewport3d.OrientationCube"))
    {
        _ShowOrientationCube = NOT _ShowOrientationCube;
        return true;
    }
    if (Id == TEXT("Viewport3d.Labels") && Adapter.IsValid() &&
        EnumHasAnyFlags(Get_Capabilities(), ECkDebug3dViewportCapability::Labels))
    {
        Adapter->Set_ShowLabels(NOT Adapter->Get_ShowLabels());
        return true;
    }
    if (Id == TEXT("Viewport3d.DirectionGlyphScale") && Adapter.IsValid() &&
        EnumHasAnyFlags(Get_Capabilities(), ECkDebug3dViewportCapability::DirectionGlyphScale))
    {
        Adapter->Set_DirectionGlyphScale(FMath::Clamp(Adapter->Get_DirectionGlyphScale() + 0.25f, 0.25f, 4.0f));
        return true;
    }
    if (Id == TEXT("Viewport3d.Follow") &&
        EnumHasAnyFlags(Get_Capabilities(), ECkDebug3dViewportCapability::FollowSelection))
    {
        Set_FollowSelection(NOT _FollowSelection);
        return true;
    }
    if (Id == TEXT("Viewport3d.Isolate") &&
        EnumHasAnyFlags(Get_Capabilities(), ECkDebug3dViewportCapability::IsolateSelection))
    {
        Set_IsolateSelection(NOT _IsolateSelection);
        return true;
    }
    if (Id.StartsWith(TEXT("Viewport3d.Bookmark.")))
    {
        const auto IsStore = Id.StartsWith(TEXT("Viewport3d.Bookmark.Store."));
        const auto PrefixLength =
            IsStore ? FString{TEXT("Viewport3d.Bookmark.Store.")}.Len() : FString{TEXT("Viewport3d.Bookmark.")}.Len();
        const auto Suffix = Id.RightChop(PrefixLength);
        int32 Slot = INDEX_NONE;
        if (Suffix.IsEmpty() || NOT LexTryParseString(Slot, *Suffix) || FString::FromInt(Slot) != Suffix || Slot < 0 ||
            Slot >= ck_debug_3d_preview_viewport::BookmarkCount)
        {
            return false;
        }
        if (IsStore)
        {
            Store_CameraBookmark(Slot);
        }
        else
        {
            Recall_CameraBookmark(Slot);
        }
        return true;
    }
    return false;
}
auto
    SCkDebug_3dPreviewViewport::
    Get_RenderModeOptions() const
    -> TArray<ECkDebug3dRenderMode>
{
    return {ECkDebug3dRenderMode::None, ECkDebug3dRenderMode::TransparentOnly, ECkDebug3dRenderMode::All};
}
auto
    SCkDebug_3dPreviewViewport::
    Get_CommonControlsWidget() const
    -> TSharedPtr<SWidget>
{
    return _CommonControls;
}
auto
    SCkDebug_3dPreviewViewport::
    Tick(const FGeometry& InGeometry, double InTime, float InDelta)
    -> void
{
    SViewport::Tick(InGeometry, InTime, InDelta);
    if (_SceneViewport.IsValid())
    {
        _SceneViewport->Invalidate();
    }
    if (_ViewportClient.IsValid())
    {
        _ViewportClient->TickNavigation(InDelta);
        Tick_FollowSelection();
        _ViewportClient->Tick(InDelta);
    }
}
