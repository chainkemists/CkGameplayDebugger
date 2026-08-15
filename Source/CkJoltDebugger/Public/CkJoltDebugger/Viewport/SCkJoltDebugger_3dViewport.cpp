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

    // How far the cursor may travel between press and release and still count as a click rather than a drag.
    auto Get_PickDragThresholdSquared() -> double
    {
        constexpr auto ThresholdPixels = 4.0;
        return ThresholdPixels * ThresholdPixels;
    }

    // Where the look-at pivot sits when the camera has never framed anything. The default eye and rotation are
    // authored, not derived from a look-at, so the pivot needs a length of its own rather than the origin.
    auto Get_DefaultOrbitDistance() -> double
    { return 2500.0; }

    auto Get_LookSensitivity() -> double
    { return 0.25; }

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
        _OrbitDistance = ck_jolt_debugger_3d_viewport::Get_DefaultOrbitDistance();
        DoResync_LookAtFromRotation();
    }

    auto Set_Viewport(const TSharedRef<FSceneViewport>& InViewport) -> void
    { _Viewport = InViewport; }

    auto Set_Target(TSharedPtr<FCk_Jolt_DebugDrawTarget> InTarget) -> void
    { _Target = InTarget; }

    auto Set_SelectionBounds(TOptional<FBox> InBounds) -> void
    { _SelectionBounds = MoveTemp(InBounds); }

    auto Set_OnBodyPicked(FOnCkJoltDebugger_BodyPicked InDelegate) -> void
    { _OnBodyPicked = MoveTemp(InDelegate); }

    auto Set_OnTogglePause(FSimpleDelegate InDelegate) -> void
    { _OnTogglePause = MoveTemp(InDelegate); }

    auto Set_OnStepOnce(FSimpleDelegate InDelegate) -> void
    { _OnStepOnce = MoveTemp(InDelegate); }

    auto Get_ProjectionMode() const -> ECameraProjectionMode::Type
    { return ViewInfo.ProjectionMode; }

    auto Get_LookAt() const -> FVector
    { return _LookAt; }

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

        // The preset turns the camera where it stands, so the pivot has to follow the new forward before any
        // framing runs — an orbit taken right after a preset would otherwise swing around the OLD pivot.
        DoResync_LookAtFromRotation();
        FrameBounds(Get_TargetContentBounds());
    }

    virtual auto InputKey(const FInputKeyEventArgs& InEventArgs) -> bool override
    {
        return Handle_Key(InEventArgs.Viewport, InEventArgs.Key, InEventArgs.Event);
    }

    virtual auto InputAxis(const FInputKeyEventArgs& InEventArgs) -> bool override
    {
        return Handle_MouseAxis(InEventArgs.Viewport, InEventArgs.Key, InEventArgs.AmountDepressed);
    }

    /*
     * The Unreal editor camera scheme (P7-D49). Gesture state is TRACKED here rather than polled off the
     * viewport's key map, because a gesture is a sequence — press, drag, release — and the drag half arrives on
     * InputAxis, which carries no modifier state of its own. Tracking it also means the scheme can be driven
     * without a viewport at all, which is the only way a spec can pin it.
     */
    auto Handle_Key(
        FViewport*         InViewport,
        const FKey&        InKey,
        const EInputEvent  InEvent) -> bool
    {
        DoRefresh_InputStateFromViewport(InViewport);
        DoApply_KeyEventToInputState(InKey, InEvent);

        // A camera drag opens with a button press too, so a press is never enough to know a click happened.
        // The pick resolves on RELEASE, and only if the cursor barely moved since the press — releasing at the
        // end of an orbit or a pan must not re-select whatever ended up under the cursor.
        if (InKey == EKeys::LeftMouseButton)
        {
            if (InEvent == IE_Pressed)
            {
                _PendingPickPress.Reset();

                const auto IsPlainPress = NOT _IsRightMouseDown && NOT _IsMiddleMouseDown;

                if (IsPlainPress && InViewport != nullptr)
                {
                    auto PressPosition = FIntPoint{};
                    InViewport->GetMousePos(PressPosition);
                    _PendingPickPress = PressPosition;
                }

                return true;
            }

            if (InEvent == IE_Released)
            {
                const auto PressPosition = _PendingPickPress;
                _PendingPickPress.Reset();

                if (NOT PressPosition.IsSet() || NOT _OnBodyPicked.IsBound() || InViewport == nullptr)
                { return true; }

                auto ReleasePosition = FIntPoint{};
                InViewport->GetMousePos(ReleasePosition);

                const auto DragOffset = FVector2D{ReleasePosition - *PressPosition};

                if (DragOffset.SizeSquared() >
                    ck_jolt_debugger_3d_viewport::Get_PickDragThresholdSquared())
                { return true; }

                auto RayOrigin = FVector::ZeroVector;
                auto RayDirection = FVector::ZeroVector;

                if (GetCursorWorldRay(InViewport, RayOrigin, RayDirection))
                {
                    const auto Target = _Target.Pin();
                    _OnBodyPicked.Execute(Target.IsValid()
                        ? Target->TryPick_Body(RayOrigin, RayDirection)
                        : TOptional<uint64>{});
                }

                return true;
            }
        }

        // A camera button arriving while the left one is down turns the gesture into a drag after the fact.
        const auto IsCameraButtonPress = InEvent == IE_Pressed &&
            (InKey == EKeys::RightMouseButton || InKey == EKeys::MiddleMouseButton);
        if (IsCameraButtonPress)
        { _PendingPickPress.Reset(); }

        const auto IsUnmodifiedPress = InEvent == IE_Pressed && NOT Get_IsAnyModifierDown();

        if (IsUnmodifiedPress && InKey == EKeys::SpaceBar)
        {
            _OnTogglePause.ExecuteIfBound();
            return true;
        }

        if (IsUnmodifiedPress && InKey == EKeys::Enter)
        {
            _OnStepOnce.ExecuteIfBound();
            return true;
        }

        // Ctrl+F, Alt+F and friends belong to whatever else the editor binds them to; only a bare F frames.
        const auto IsFrameSelectionKey = (InEvent == IE_Pressed || InEvent == IE_Repeat) &&
            InKey == EKeys::F &&
            NOT Get_IsAnyModifierDown();
        if (IsFrameSelectionKey)
        {
            ApplyPreset(ECkJoltDebugger_CameraPreset::FrameSelection);
            return true;
        }

        const auto IsFrameAllKey = (InEvent == IE_Pressed || InEvent == IE_Repeat) &&
            InKey == EKeys::Home;
        if (IsFrameAllKey)
        {
            ApplyPreset(ECkJoltDebugger_CameraPreset::FrameAll);
            return true;
        }

        const auto IsMouseWheel = InKey == EKeys::MouseScrollUp || InKey == EKeys::MouseScrollDown;
        const auto IsSpeedChange = ViewInfo.ProjectionMode == ECameraProjectionMode::Perspective &&
            InEvent == IE_Pressed &&
            IsMouseWheel &&
            _IsRightMouseDown;
        if (IsSpeedChange)
        {
            const auto Direction = InKey == EKeys::MouseScrollDown ? -1.0f : 1.0f;
            _CameraSpeed = FMath::Clamp(_CameraSpeed + _CameraSpeed * 0.1f * Direction, 0.00001f, 10000.0f);
            return true;
        }

        if (IsMouseWheel && InEvent == IE_Pressed)
        {
            Zoom(InKey == EKeys::MouseScrollUp ? 1.1f : 1.0f / 1.1f);
            return true;
        }

        return InKey == EKeys::RightMouseButton || InKey == EKeys::MiddleMouseButton;
    }

    auto Handle_MouseAxis(
        FViewport*   InViewport,
        const FKey&  InAxisKey,
        const float  InDelta) -> bool
    {
        if (InAxisKey != EKeys::MouseX && InAxisKey != EKeys::MouseY)
        { return false; }

        DoRefresh_InputStateFromViewport(InViewport);

        const auto IsHorizontal = InAxisKey == EKeys::MouseX;

        // An orthographic preset is an AXIS-LOCKED view: rotating it turns it into an arbitrary perspective-less
        // camera with no way back, so every drag pans and rotation is refused outright.
        if (ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
        {
            if (NOT _IsRightMouseDown && NOT _IsMiddleMouseDown)
            { return false; }

            DoPan(IsHorizontal, InDelta);
            Invalidate();
            return true;
        }

        if (_IsAltDown && _IsLeftMouseDown)
        { DoOrbit(IsHorizontal, InDelta); }
        else if (_IsAltDown && _IsRightMouseDown)
        { DoDolly(IsHorizontal ? InDelta : -InDelta); }
        else if (_IsRightMouseDown)
        { DoLookInPlace(IsHorizontal, InDelta); }
        else if (_IsMiddleMouseDown)
        { DoPan(IsHorizontal, InDelta); }
        else if (_IsLeftMouseDown)
        { DoTrackAndYaw(IsHorizontal, InDelta); }
        else
        { return false; }

        Invalidate();
        return true;
    }

    // Losing focus eats the release: FSceneViewport empties its key state on the way out, so a left button that
    // went down in here never reports IE_Released. The press left standing would then pair with whatever
    // release arrives next — a click somewhere else entirely, resolving as a pick. The tracked gesture state
    // goes with it, or the next drag would open with a button this client still believes is held.
    virtual auto LostFocus(FViewport* InViewport) -> void override
    {
        _PendingPickPress.Reset();
        DoReset_InputState();
        FUMGViewportClient::LostFocus(InViewport);
    }

    // Flight is a RIGHT-MOUSE gesture in the editor scheme: WASD with no button held belongs to whatever else
    // is listening, and a viewport that flew on a bare keypress would steal every typed character.
    auto Tick_Navigation(float InDeltaTime) -> void
    {
        const auto SceneViewport = _Viewport.Pin();
        if (SceneViewport == nullptr || ViewInfo.ProjectionMode != ECameraProjectionMode::Perspective)
        { return; }

        if (NOT _IsRightMouseDown)
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
        DoSet_LookAt(_LookAt + Delta);
        Invalidate();
    }

private:
    auto Get_IsAnyModifierDown() const -> bool
    { return _IsAltDown || _IsControlDown || _IsShiftDown || _IsCommandDown; }

    auto DoRefresh_InputStateFromViewport(
        const FViewport* InViewport) -> void
    {
        if (InViewport == nullptr)
        { return; }

        _IsAltDown     = InViewport->KeyState(EKeys::LeftAlt)     || InViewport->KeyState(EKeys::RightAlt);
        _IsControlDown = InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
        _IsShiftDown   = InViewport->KeyState(EKeys::LeftShift)   || InViewport->KeyState(EKeys::RightShift);
        _IsCommandDown = InViewport->KeyState(EKeys::LeftCommand) || InViewport->KeyState(EKeys::RightCommand);

        _IsLeftMouseDown   = InViewport->KeyState(EKeys::LeftMouseButton);
        _IsRightMouseDown  = InViewport->KeyState(EKeys::RightMouseButton);
        _IsMiddleMouseDown = InViewport->KeyState(EKeys::MiddleMouseButton);
    }

    auto DoApply_KeyEventToInputState(
        const FKey&       InKey,
        const EInputEvent InEvent) -> void
    {
        if (InEvent != IE_Pressed && InEvent != IE_Released)
        { return; }

        const auto IsDown = InEvent == IE_Pressed;

        if (InKey == EKeys::LeftAlt || InKey == EKeys::RightAlt)             { _IsAltDown = IsDown; }
        else if (InKey == EKeys::LeftControl || InKey == EKeys::RightControl){ _IsControlDown = IsDown; }
        else if (InKey == EKeys::LeftShift || InKey == EKeys::RightShift)    { _IsShiftDown = IsDown; }
        else if (InKey == EKeys::LeftCommand || InKey == EKeys::RightCommand){ _IsCommandDown = IsDown; }
        else if (InKey == EKeys::LeftMouseButton)                            { _IsLeftMouseDown = IsDown; }
        else if (InKey == EKeys::RightMouseButton)                           { _IsRightMouseDown = IsDown; }
        else if (InKey == EKeys::MiddleMouseButton)                          { _IsMiddleMouseDown = IsDown; }
    }

    auto DoReset_InputState() -> void
    {
        _IsAltDown = false;
        _IsControlDown = false;
        _IsShiftDown = false;
        _IsCommandDown = false;
        _IsLeftMouseDown = false;
        _IsRightMouseDown = false;
        _IsMiddleMouseDown = false;
    }

    // The pivot follows the eye. This is the look-in-place half of the invariant — the eye stays, the pivot moves.
    auto DoResync_LookAtFromRotation() -> void
    { DoSet_LookAt(GetViewLocation() + GetViewRotation().Vector() * FMath::Max(_OrbitDistance, 1.0)); }

    auto DoSet_LookAt(
        const FVector& InLookAt) -> void
    {
        _LookAt = InLookAt;
        SetLookAtLocation(_LookAt);
    }

    // Pan and dolly move by a fraction of how far away the pivot is, so a camera framed on a rope link and one
    // framed on a whole streamed cell both feel the same.
    auto Get_GestureScale() const -> double
    {
        const auto SpeedScale = FMath::Clamp(_CameraSpeed / 1000.0f, 0.1f, 100.0f);
        return FMath::Max(_OrbitDistance, 1.0) * 0.001 * SpeedScale;
    }

    auto DoLookInPlace(
        const bool   InIsHorizontal,
        const float  InDelta) -> void
    {
        auto Rotation = GetViewRotation();

        if (InIsHorizontal)
        { Rotation.Yaw += InDelta * ck_jolt_debugger_3d_viewport::Get_LookSensitivity(); }
        else
        { Rotation.Pitch = FMath::Clamp(Rotation.Pitch + InDelta * ck_jolt_debugger_3d_viewport::Get_LookSensitivity(), -89.0, 89.0); }

        SetViewRotation(Rotation);
        DoResync_LookAtFromRotation();
    }

    auto DoOrbit(
        const bool   InIsHorizontal,
        const float  InDelta) -> void
    {
        auto Rotation = GetViewRotation();

        if (InIsHorizontal)
        { Rotation.Yaw += InDelta * ck_jolt_debugger_3d_viewport::Get_LookSensitivity(); }
        else
        { Rotation.Pitch = FMath::Clamp(Rotation.Pitch + InDelta * ck_jolt_debugger_3d_viewport::Get_LookSensitivity(), -89.0, 89.0); }

        SetViewRotation(Rotation);
        SetViewLocation(_LookAt - Rotation.Vector() * FMath::Max(_OrbitDistance, 1.0));
    }

    auto DoPan(
        const bool   InIsHorizontal,
        const float  InDelta) -> void
    {
        const auto Axis = InIsHorizontal
            ? GetViewRotation().RotateVector(FVector::RightVector) * -1.0
            : GetViewRotation().RotateVector(FVector::UpVector);

        const auto Delta = Axis * (InDelta * Get_GestureScale());

        SetViewLocation(GetViewLocation() + Delta);
        DoSet_LookAt(_LookAt + Delta);
    }

    auto DoDolly(
        const float InDelta) -> void
    {
        _OrbitDistance = FMath::Max(_OrbitDistance - InDelta * Get_GestureScale(), 1.0);
        SetViewLocation(_LookAt - GetViewRotation().Vector() * _OrbitDistance);
    }

    // The editor's plain left-drag: vertical tracks the eye along the view, horizontal yaws it where it stands.
    auto DoTrackAndYaw(
        const bool   InIsHorizontal,
        const float  InDelta) -> void
    {
        if (InIsHorizontal)
        {
            auto Rotation = GetViewRotation();
            Rotation.Yaw += InDelta * ck_jolt_debugger_3d_viewport::Get_LookSensitivity();
            SetViewRotation(Rotation);
            DoResync_LookAtFromRotation();
            return;
        }

        const auto Delta = GetViewRotation().Vector() * (-InDelta * Get_GestureScale());
        SetViewLocation(GetViewLocation() + Delta);
        DoSet_LookAt(_LookAt + Delta);
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
            DoSet_LookAt(Center);

            // Sitting the eye ON the centre puts half the content behind the near plane, so solid bodies get
            // sliced away. Back off past the bounding sphere before looking in.
            constexpr auto EyeClearanceScale = 2.0f;
            _OrbitDistance = FMath::Max(Radius * EyeClearanceScale, 1.0);
            SetViewLocation(Center - GetViewRotation().Vector() * _OrbitDistance);
        }
        else
        {
            if (AspectRatio > 1.0f)
            { Radius *= AspectRatio; }

            _OrbitDistance = FMath::Max(Radius / FMath::Tan(FMath::DegreesToRadians(ViewInfo.FOV * 0.5f)), 1.0);
            DoSet_LookAt(Center);
            SetViewLocation(Center - GetViewRotation().Vector() * _OrbitDistance);
        }

        Invalidate();
    }

    auto Zoom(float InFactor) -> void
    {
        if (ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
        { ViewInfo.OrthoWidth = FMath::Clamp(ViewInfo.OrthoWidth / InFactor, 10.0f, 1000000.0f); }
        else
        {
            _OrbitDistance = FMath::Clamp(_OrbitDistance / InFactor, 1.0, 100000000.0);
            SetViewLocation(_LookAt - GetViewRotation().Vector() * _OrbitDistance);
        }

        Invalidate();
    }

private:
    TWeakPtr<FCk_Jolt_DebugDrawTarget> _Target;
    TWeakPtr<FSceneViewport> _Viewport;

    TOptional<FBox> _SelectionBounds;
    FOnCkJoltDebugger_BodyPicked _OnBodyPicked;
    FSimpleDelegate _OnTogglePause;
    FSimpleDelegate _OnStepOnce;

    // Where a plain left press landed, until it releases. Unset means no click is in flight — either none
    // started, or a camera button turned the one that did into a drag.
    TOptional<FIntPoint> _PendingPickPress;

    // The camera's real state, beside the eye and the rotation: eye + forward * distance == look-at, on every
    // path. Orbit and framing read the pivot; look-in-place, pan and flight move it.
    FVector _LookAt = FVector::ZeroVector;
    double _OrbitDistance = ck_jolt_debugger_3d_viewport::Get_DefaultOrbitDistance();

    bool _IsAltDown = false;
    bool _IsControlDown = false;
    bool _IsShiftDown = false;
    bool _IsCommandDown = false;
    bool _IsLeftMouseDown = false;
    bool _IsRightMouseDown = false;
    bool _IsMiddleMouseDown = false;

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
    _ViewportClient->Set_OnTogglePause(InArgs._OnTogglePause);
    _ViewportClient->Set_OnStepOnce(InArgs._OnStepOnce);
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
    Get_LookAtLocation() const
    -> FVector
{
    return _ViewportClient.IsValid() ? _ViewportClient->Get_LookAt() : FVector::ZeroVector;
}

auto
    SCkJoltDebugger_3dViewport::
    Input_Key(
        const FKey& InKey,
        EInputEvent InEvent)
    -> bool
{
    return _ViewportClient.IsValid() && _ViewportClient->Handle_Key(nullptr, InKey, InEvent);
}

auto
    SCkJoltDebugger_3dViewport::
    Input_MouseAxis(
        const FKey& InAxisKey,
        float InDelta)
    -> bool
{
    return _ViewportClient.IsValid() && _ViewportClient->Handle_MouseAxis(nullptr, InAxisKey, InDelta);
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
