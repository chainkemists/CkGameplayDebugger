#include "CkJoltDebugger/Viewport/SCkJoltDebugger_3dViewport.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_OrientationCube.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkJoltDebugger/CkJoltDebugger_Log.h"
#include "CkJoltDebugger/Settings/CkJoltDebuggerSettings.h"

#include "CkJolt/Subsystem/CkJolt_DebugDrawTarget.h"

#include "Components/Viewport.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "PreviewScene.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SOverlay.h"

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

    // How long a hover pick may go unrepeated. A pick is O(live instances), and the mouse produces a move event
    // per pixel — without this the hover would be the most expensive thing this window does (P8-D58).
    auto Get_HoverThrottleSeconds() -> double
    { return 0.06; }

    // How much of the pivot distance one wheel notch travels. The step scales with the pivot so a notch means
    // the same thing on a rope link and on a streamed cell (P7-D71/F6).
    auto Get_WheelDollyFraction() -> double
    { return 0.1; }

    // The world-axis gizmo's footprint, in local Slate units. Small enough to stay out of the way of the pick
    // surface it overlays, big enough that three axis edges are readable at a glance.
    auto Get_GizmoSize() -> float
    { return 56.0f; }

    // Camera bookmarks (P8-D59): ten slots, keyed by the DIGIT itself, so the slot IS the array index and a
    // bookmark can never drift onto another key's number.
    constexpr int32 NumBookmarkSlots = 10;

    /*
     * The top-row digit keys only. A numpad digit is deliberately not a bookmark key: the pad is where a value
     * gets typed, and these keys only ever reach this client while the VIEWPORT holds keyboard focus anyway.
     */
    auto TryGet_BookmarkSlot(
        const FKey& InKey) -> TOptional<int32>
    {
        static const TArray<FKey> DigitKeys =
        {
            EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
            EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine
        };

        const auto Index = DigitKeys.IndexOfByKey(InKey);

        return Index == INDEX_NONE ? TOptional<int32>{} : TOptional<int32>{Index};
    }

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

auto
    ck_jolt_debugger_viewport::
    Make_InverseViewMatrix(
        const FVector& InViewOrigin,
        const FMatrix& InViewRotationMatrix)
    -> FMatrix
{
    // Match FSceneView's inverse-view construction: camera translation is part of the transform expected by
    // DeprojectScreenToWorld's split inverse-view / inverse-projection overload.
    return InViewRotationMatrix.GetTransposed() * FTranslationMatrix(InViewOrigin);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck_jolt_debugger_viewport::
    Select_NearestLabels(
        const TArray<FCk_Jolt_DebugDrawLabel>& InLabels,
        const FVector&                         InViewLocation,
        int32                                  InCap,
        TArray<int32>&                         OutIndices)
    -> void
{
    // Reset, not Empty: the paint hands the same member array back every pass, and the capacity it reached is
    // the whole point of handing it back.
    OutIndices.Reset();

    if (InCap <= 0)
    { return; }

    const auto NumLabels = InLabels.Num();

    // Under the cap the order is the CAPTURE's, untouched: sorting a set that is going to be drawn in full
    // would reshuffle the draw order every time the camera moved, for no visible gain.
    if (NumLabels <= InCap)
    {
        OutIndices.Reserve(NumLabels);

        for (auto Index = 0; Index < NumLabels; ++Index)
        { OutIndices.Emplace(Index); }

        return;
    }

    const auto Get_DistanceSquared = [&InLabels, &InViewLocation](int32 InIndex)
    { return FVector::DistSquared(InLabels[InIndex].Get_WorldPosition(), InViewLocation); };

    // FARTHEST at the top, which is what makes the heap bounded: the only element a candidate has to beat is
    // the worst one kept so far, and beating it is one pop and one push rather than a re-sort.
    const auto IsFartherFirst = [&Get_DistanceSquared](int32 InLeft, int32 InRight)
    { return Get_DistanceSquared(InLeft) > Get_DistanceSquared(InRight); };

    OutIndices.Reserve(InCap);

    for (auto Index = 0; Index < InCap; ++Index)
    { OutIndices.Emplace(Index); }

    OutIndices.Heapify(IsFartherFirst);

    for (auto Index = InCap; Index < NumLabels; ++Index)
    {
        if (Get_DistanceSquared(Index) >= Get_DistanceSquared(OutIndices.HeapTop()))
        { continue; }

        OutIndices.HeapPopDiscard(IsFartherFirst, EAllowShrinking::No);
        OutIndices.HeapPush(Index, IsFartherFirst);
    }

    // The kept set is heap-ordered, which is farthest-first — the paint wants nearest-first, so this last pass
    // orders `InCap` elements rather than all of them.
    OutIndices.Sort([&Get_DistanceSquared](int32 InLeft, int32 InRight)
    { return Get_DistanceSquared(InLeft) < Get_DistanceSquared(InRight); });
}

auto
    ck_jolt_debugger_viewport::
    Select_NearestLabels(
        const TArray<FCk_Jolt_DebugDrawLabel>& InLabels,
        const FVector&                         InViewLocation,
        int32                                  InCap)
    -> TArray<int32>
{
    auto Indices = TArray<int32>{};
    Select_NearestLabels(InLabels, InViewLocation, InCap, Indices);

    return Indices;
}

// --------------------------------------------------------------------------------------------------------------------

class FCkJoltDebugger_3dViewportClient final : public FUMGViewportClient
{
public:
    explicit FCkJoltDebugger_3dViewportClient(
        FPreviewScene& InPreviewScene)
        : FUMGViewportClient(&InPreviewScene)
    {
        EngineShowFlags.EnableAdvancedFeatures();
        EngineShowFlags.SetLighting(true);
        EngineShowFlags.SetPostProcessing(true);
        EngineShowFlags.SetAntiAliasing(true);
        EngineShowFlags.SetTemporalAA(true);
        EngineShowFlags.SetDynamicShadows(false);
        EngineShowFlags.SetMotionBlur(false);
        EngineShowFlags.SetDepthOfField(false);
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

    auto Set_OnToggleIsolate(FSimpleDelegate InDelegate) -> void
    { _OnToggleIsolate = MoveTemp(InDelegate); }

    auto Set_OnDragArm(FOnCkJoltDebugger_DragArm InDelegate) -> void
    { _OnDragArm = MoveTemp(InDelegate); }

    auto Set_OnDragRay(FOnCkJoltDebugger_DragRay InDelegate) -> void
    { _OnDragRay = MoveTemp(InDelegate); }

    auto Set_OnDragPlaneShift(FOnCkJoltDebugger_DragPlaneShift InDelegate) -> void
    { _OnDragPlaneShift = MoveTemp(InDelegate); }

    auto Set_OnDragRelease(FSimpleDelegate InDelegate) -> void
    { _OnDragRelease = MoveTemp(InDelegate); }

    auto Set_OnBodyHovered(FOnCkJoltDebugger_BodyHovered InDelegate) -> void
    { _OnBodyHovered = MoveTemp(InDelegate); }

    auto Set_DragEnabled(bool InIsEnabled) -> void
    { _IsDragEnabled = InIsEnabled; }

    auto Set_FollowSelection(bool InIsEnabled) -> void
    {
        if (_FollowSelection == InIsEnabled)
        { return; }

        _FollowSelection = InIsEnabled;

        // The follow anchor is a DELTA source, not a target: re-arming it on the switch means turning Follow
        // on never teleports the camera, it only starts tracking from wherever the selection is right now.
        _LastFollowCenter.Reset();
    }

    auto Get_ProjectionMode() const -> ECameraProjectionMode::Type
    { return ViewInfo.ProjectionMode; }

    auto Get_RenderFeatures() const -> FCkJoltDebugger_ViewportRenderFeatures
    {
        return FCkJoltDebugger_ViewportRenderFeatures{
            static_cast<bool>(EngineShowFlags.Lighting),
            static_cast<bool>(EngineShowFlags.PostProcessing),
            static_cast<bool>(EngineShowFlags.AntiAliasing),
            static_cast<bool>(EngineShowFlags.TemporalAA),
            ck_jolt_debugger_viewport::AntiAliasingMethod,
            static_cast<bool>(EngineShowFlags.DynamicShadows),
            static_cast<bool>(EngineShowFlags.MotionBlur),
            static_cast<bool>(EngineShowFlags.DepthOfField),
            static_cast<bool>(EngineShowFlags.EyeAdaptation)};
    }

    auto CalcSceneView(FSceneViewFamily* InViewFamily) -> FSceneView* override
    {
        auto* View = FUMGViewportClient::CalcSceneView(InViewFamily);
        if (View != nullptr)
        {
            // CkPlugins disables AA project-wide. This inspection viewport owns a persistent view state, so it can
            // opt into temporal AA locally without changing how any game or test viewport renders.
            View->AntiAliasingMethod = ck_jolt_debugger_viewport::AntiAliasingMethod;
        }

        return View;
    }

    auto Get_LookAt() const -> FVector
    { return _LookAt; }

    /*
     * The forward half of GetCursorWorldRay's math, for the label paint: the same FSceneViewInitOptions the
     * deprojection builds, composed into one world -> clip matrix. Built the same way on purpose — a projection
     * derived differently from the deprojection would put labels where clicks do not land.
     */
    auto TryGet_ViewProjection(
        FViewport* InViewport,
        FMatrix&   OutViewProjection,
        FIntRect&  OutViewRect) const -> bool
    {
        if (InViewport == nullptr)
        { return false; }

        const auto Size = InViewport->GetSizeXY();

        if (Size.X <= 0 || Size.Y <= 0)
        { return false; }

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

        OutViewProjection = FTranslationMatrix(-ViewInitOptions.ViewOrigin) *
            ViewInitOptions.ViewRotationMatrix * ViewInitOptions.ProjectionMatrix;
        OutViewRect = ViewInitOptions.ViewRect;

        return true;
    }

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

                // Alt+LMB is the ORBIT gesture, not a click: arming a pending pick on it means every orbit
                // that happens to start and end over the same body re-selects it (P7-D71/F7).
                const auto IsPlainPress = NOT _IsRightMouseDown && NOT _IsMiddleMouseDown && NOT _IsAltDown;

                if (IsPlainPress && InViewport != nullptr)
                {
                    auto PressPosition = FIntPoint{};
                    InViewport->GetMousePos(PressPosition);
                    _PendingPickPress = PressPosition;
                }

                /*
                 * Ctrl+LMB is ONE gesture with two jobs (P7-D53 + P7-D54): it adds the body to the selection
                 * and it opens the drag. The pick therefore resolves on the PRESS rather than on the release,
                 * unlike the plain click — and it resolves through TryPick_BodyHit, so the SAME pick that made
                 * the selection also hands the drag its grab point, on the surface the user clicked (P7-D70/i).
                 */
                if (IsPlainPress && _IsControlDown)
                {
                    _IsCtrlGesture = true;
                    const auto Hit = DoPick(InViewport, true);

#if !UE_BUILD_SHIPPING
                    // A Ctrl+press on empty space opens NO drag: a gesture that ate the mouse and moved the
                    // previous selection is what the pick-keyed arm exists to make impossible.
                    if (_IsDragEnabled && Hit._Key.IsSet())
                    {
                        _IsDragGesture = true;
                        _OnDragArm.ExecuteIfBound(Hit._Key, Hit._PointWorld);
                    }
#endif
                }

                return true;
            }

            if (InEvent == IE_Released)
            {
                const auto WasCtrlGesture = _IsCtrlGesture;
                _IsCtrlGesture = false;

#if !UE_BUILD_SHIPPING
                if (_IsDragGesture)
                {
                    _IsDragGesture = false;
                    _OnDragRelease.ExecuteIfBound();
                }
#endif

                const auto PressPosition = _PendingPickPress;
                _PendingPickPress.Reset();

                // The Ctrl gesture already picked on the press; picking again here would replace the
                // multi-selection it just built with the one body under the cursor.
                if (WasCtrlGesture)
                { return true; }

                if (NOT PressPosition.IsSet() || NOT _OnBodyPicked.IsBound() || InViewport == nullptr)
                { return true; }

                auto ReleasePosition = FIntPoint{};
                InViewport->GetMousePos(ReleasePosition);

                const auto DragOffset = FVector2D{ReleasePosition - *PressPosition};

                if (DragOffset.SizeSquared() >
                    ck_jolt_debugger_3d_viewport::Get_PickDragThresholdSquared())
                { return true; }

                DoPick(InViewport, false);

                return true;
            }
        }

        // A camera button arriving while the left one is down turns the gesture into a drag after the fact.
        const auto IsCameraButtonPress = InEvent == IE_Pressed &&
            (InKey == EKeys::RightMouseButton || InKey == EKeys::MiddleMouseButton);
        if (IsCameraButtonPress)
        { _PendingPickPress.Reset(); }

        const auto IsUnmodifiedPress = InEvent == IE_Pressed && NOT Get_IsAnyModifierDown();

        /*
         * Camera bookmarks (P8-D59). Ctrl+digit stores the pose in that digit's slot, a bare digit recalls it.
         * They are safe as bare keys for the same reason Space, Enter, I and F are: nothing reaches this client
         * unless the VIEWPORT owns keyboard focus, so a digit typed into a search box stays in the search box.
         */
        if (InEvent == IE_Pressed)
        {
            const auto BookmarkSlot = ck_jolt_debugger_3d_viewport::TryGet_BookmarkSlot(InKey);

            if (BookmarkSlot.IsSet())
            {
                const auto IsControlOnly = _IsControlDown &&
                    NOT _IsAltDown && NOT _IsShiftDown && NOT _IsCommandDown;

                if (IsControlOnly)
                {
                    DoStore_Bookmark(*BookmarkSlot);
                    return true;
                }

                if (IsUnmodifiedPress)
                {
                    DoRecall_Bookmark(*BookmarkSlot);
                    return true;
                }
            }
        }

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

        if (IsUnmodifiedPress && InKey == EKeys::I)
        {
            _OnToggleIsolate.ExecuteIfBound();
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

#if !UE_BUILD_SHIPPING
        // Ctrl+wheel during a drag moves the drag PLANE along the view, not the camera — "further away" is
        // the one thing a camera-parallel plane cannot otherwise express.
        if (_IsDragGesture && IsMouseWheel && InEvent == IE_Pressed)
        {
            _OnDragPlaneShift.ExecuteIfBound(InKey == EKeys::MouseScrollUp ? 1.0f : -1.0f);
            return true;
        }
#endif

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
            DoWheelDolly(InKey == EKeys::MouseScrollUp ? 1.0 : -1.0);
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

#if !UE_BUILD_SHIPPING
        // A live drag OWNS the mouse: the camera must not move under a body the user is placing, or the
        // grab point and the drag plane built from it stop describing the same world.
        if (_IsDragGesture)
        {
            auto RayOrigin = FVector::ZeroVector;
            auto RayDirection = FVector::ZeroVector;

            if (GetCursorWorldRay(InViewport, RayOrigin, RayDirection))
            { _OnDragRay.ExecuteIfBound(RayOrigin, RayDirection); }

            Invalidate();
            return true;
        }
#endif

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

    /*
     * Hover (P8-D58). MouseMove is the NON-captured move: FSceneViewport routes a move with no button held here
     * and a move with one held to CapturedMouseMove, which is exactly the split the hover wants — a hover must
     * never fire while a gesture owns the cursor.
     */
    virtual auto MouseMove(
        FViewport* InViewport,
        int32      InX,
        int32      InY) -> void override
    {
        FUMGViewportClient::MouseMove(InViewport, InX, InY);
        DoTick_Hover(InViewport);
    }

    /*
     * The cursor LEAVING is not a move, so nothing else here ever hears about it (P8-D74/F6). Without this the
     * last hovered body keeps its half-alpha overlay and its tooltip while the mouse is somewhere else
     * entirely — and `LostFocus` does not cover it, because moving the cursor out of a viewport that still
     * holds focus is the common case.
     */
    virtual auto MouseLeave(
        FViewport* InViewport) -> void override
    {
        DoClear_Hover();
        FUMGViewportClient::MouseLeave(InViewport);
    }

    // Losing focus eats the release: FSceneViewport empties its key state on the way out, so a left button that
    // went down in here never reports IE_Released. The press left standing would then pair with whatever
    // release arrives next — a click somewhere else entirely, resolving as a pick. The tracked gesture state
    // goes with it, or the next drag would open with a button this client still believes is held.
    virtual auto LostFocus(FViewport* InViewport) -> void override
    {
        _PendingPickPress.Reset();
        _IsCtrlGesture = false;

#if !UE_BUILD_SHIPPING
        // A drag whose release never arrives would leave the facility's spring attached to a body forever.
        if (_IsDragGesture)
        {
            _IsDragGesture = false;
            _OnDragRelease.ExecuteIfBound();
        }
#endif

        DoClear_Hover();

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

    /*
     * Follow-selection (P7-D53): the camera keeps its OFFSET to the selection rather than re-framing it.
     * Rotation, orbit distance and projection are untouched — a follow that re-framed would fight every
     * gesture the user made while it was on, and a body moving at speed would make the view unusable.
     *
     * Driven off the same selection bounds the window pushes down every tick, so a multi-selection is
     * followed by its union's centre, which is what the highlight is showing.
     */
    auto Tick_FollowSelection() -> void
    {
        if (NOT _FollowSelection || NOT _SelectionBounds.IsSet() || _SelectionBounds->IsValid == 0)
        {
            _LastFollowCenter.Reset();
            return;
        }

        const auto Center = _SelectionBounds->GetCenter();

        if (_LastFollowCenter.IsSet())
        {
            const auto Delta = Center - *_LastFollowCenter;

            if (NOT Delta.IsNearlyZero())
            {
                SetViewLocation(GetViewLocation() + Delta);
                DoSet_LookAt(_LookAt + Delta);
                Invalidate();
            }
        }

        _LastFollowCenter = Center;
    }

private:
    /*
     * A bookmark is the camera's STATE, not a framing: eye, rotation, projection and — for an ortho pose —
     * the ortho width, which no look-at could reproduce. The slots are dense and indexed by the digit itself,
     * so slot 3 is `CameraBookmarks[3]` whether or not 0..2 were ever stored.
     */
    auto DoStore_Bookmark(int32 InSlot) -> void
    {
        auto* Settings = GetMutableDefault<UCkJoltDebuggerSettings>();

        if (ck::Is_NOT_Valid(Settings))
        { return; }

        if (Settings->CameraBookmarks.Num() < ck_jolt_debugger_3d_viewport::NumBookmarkSlots)
        { Settings->CameraBookmarks.SetNum(ck_jolt_debugger_3d_viewport::NumBookmarkSlots); }

        auto& Bookmark = Settings->CameraBookmarks[InSlot];
        Bookmark.Location       = GetViewLocation();
        Bookmark.Rotation       = GetViewRotation();
        Bookmark.OrthoWidth     = ViewInfo.OrthoWidth;
        Bookmark.IsOrthographic = ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic;

        // Without this an untouched slot recalls the struct's own defaults, which reads as the camera snapping
        // to the origin — a bookmark that was never taken must be inert, not a pose.
        Bookmark.IsSet = true;

        Settings->SaveConfig();
    }

    auto DoRecall_Bookmark(int32 InSlot) -> void
    {
        const auto* Settings = GetDefault<UCkJoltDebuggerSettings>();

        if (ck::Is_NOT_Valid(Settings) || NOT Settings->CameraBookmarks.IsValidIndex(InSlot))
        { return; }

        const auto& Bookmark = Settings->CameraBookmarks[InSlot];

        if (NOT Bookmark.IsSet)
        { return; }

        ViewInfo.ProjectionMode = Bookmark.IsOrthographic
            ? ECameraProjectionMode::Orthographic
            : ECameraProjectionMode::Perspective;

        if (Bookmark.IsOrthographic)
        { ViewInfo.OrthoWidth = Bookmark.OrthoWidth; }

        SetViewLocation(Bookmark.Location);
        SetViewRotation(Bookmark.Rotation);

        // The pose carries no pivot, so the look-at is rebuilt from the restored rotation at the current orbit
        // distance — the same thing a preset does, and the invariant every other path maintains.
        DoResync_LookAtFromRotation();

        Invalidate();
    }

    auto DoClear_Hover() -> void
    {
        if (NOT _HoveredBodyKey.IsSet())
        { return; }

        _HoveredBodyKey.Reset();
        _OnBodyHovered.ExecuteIfBound(TOptional<uint64>{});
    }

    auto DoTick_Hover(
        FViewport* InViewport) -> void
    {
        if (NOT _OnBodyHovered.IsBound() || InViewport == nullptr)
        { return; }

        // A gesture in flight OWNS the cursor. Hovering through a click would move the subdued overlay under the
        // body the user is picking, and through a drag it would fight the body they are placing.
        const auto IsGestureInFlight = _PendingPickPress.IsSet() || _IsCtrlGesture ||
            _IsLeftMouseDown || _IsRightMouseDown || _IsMiddleMouseDown;

        if (IsGestureInFlight)
        { return; }

        const auto Now = FPlatformTime::Seconds();

        if (Now - _LastHoverSeconds < ck_jolt_debugger_3d_viewport::Get_HoverThrottleSeconds())
        { return; }

        _LastHoverSeconds = Now;

        auto RayOrigin = FVector::ZeroVector;
        auto RayDirection = FVector::ZeroVector;

        if (NOT GetCursorWorldRay(InViewport, RayOrigin, RayDirection))
        { return; }

        const auto Target = _Target.Pin();

        const auto Hovered = Target.IsValid()
            ? Target->TryPick_Body(RayOrigin, RayDirection)
            : TOptional<uint64>{};

        // Reported on CHANGE only: the consumer re-stamps the facility and a tooltip from this, and re-stamping
        // the same key sixteen times a second would invalidate an overlay that never moved.
        if (Hovered == _HoveredBodyKey)
        { return; }

        _HoveredBodyKey = Hovered;
        _OnBodyHovered.Execute(_HoveredBodyKey);
    }

    /// What one pick resolved to. An unset key means the ray left the scene without touching a drawn body.
    struct FPickHit
    {
        TOptional<uint64> _Key;
        FVector           _PointWorld = FVector::ZeroVector;
    };

    /*
     * The one pick path: deproject, ask the facility for the HIT, report the key and whether the click was
     * additive. The hit is RETURNED as well as broadcast, because the Ctrl gesture needs the grab point from
     * the very same pick — a second TryPick_BodyHit would be a second O(live instances) walk for an answer
     * this one already produced.
     */
    auto DoPick(
        FViewport* InViewport,
        const bool InIsAdditive) -> FPickHit
    {
        auto Hit = FPickHit{};

        if (InViewport == nullptr)
        { return Hit; }

        auto RayOrigin = FVector::ZeroVector;
        auto RayDirection = FVector::ZeroVector;

        if (NOT GetCursorWorldRay(InViewport, RayOrigin, RayDirection))
        { return Hit; }

        if (const auto Target = _Target.Pin())
        {
            auto Key = uint64{0};
            auto HitPoint = FVector::ZeroVector;
            auto Distance = 0.0f;

            if (Target->TryPick_BodyHit(RayOrigin, RayDirection, Key, HitPoint, Distance))
            {
                Hit._Key = Key;
                Hit._PointWorld = HitPoint;
            }
        }

        _OnBodyPicked.ExecuteIfBound(Hit._Key, InIsAdditive);

        return Hit;
    }

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

    /*
     * Pan and dolly move by a fraction of how much world the view is showing, so a camera framed on a rope link
     * and one framed on a whole streamed cell both feel the same.
     *
     * In ORTHO that quantity is the ortho width, not the pivot distance (P7-D71/F5): an orthographic camera's
     * distance to its pivot says nothing about how much world a pixel covers, so a pan scaled off it drags by
     * the same world amount however far the user has zoomed in — unusable at both ends of the zoom range.
     */
    auto Get_GestureScale() const -> double
    {
        const auto SpeedScale = FMath::Clamp(_CameraSpeed / 1000.0f, 0.1f, 100.0f);

        const auto Extent = ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic
            ? FMath::Max(static_cast<double>(ViewInfo.OrthoWidth), 1.0)
            : FMath::Max(_OrbitDistance, 1.0);

        return Extent * 0.001 * SpeedScale;
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
            ck_jolt_debugger_viewport::Make_InverseViewMatrix(
                ViewInitOptions.ViewOrigin, ViewInitOptions.ViewRotationMatrix),
            ViewInitOptions.ProjectionMatrix.InverseFast(),
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

    /*
     * The wheel (P7-D71/F6). In perspective this is a DOLLY in the Unreal sense: the eye and the look-at both
     * travel along the view, by a step scaled to how far the pivot is. Shrinking the orbit distance instead —
     * which is what this did — walks the eye towards a pivot it can never pass, so the wheel stalls in front
     * of whatever the camera last framed and the user cannot fly through it. Moving both leaves the invariant
     * (eye + forward * distance == look-at) intact by construction, so _OrbitDistance stays exactly as sane as
     * it was; orbit and framing are unaffected.
     *
     * In ortho there is nothing to dolly towards — the wheel widens or narrows the frustum instead.
     */
    auto DoWheelDolly(double InDirection) -> void
    {
        if (ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic)
        {
            const auto Factor = InDirection > 0.0 ? 1.1f : 1.0f / 1.1f;
            ViewInfo.OrthoWidth = FMath::Clamp(ViewInfo.OrthoWidth / Factor, 10.0f, 1000000.0f);
            Invalidate();
            return;
        }

        const auto Step = GetViewRotation().Vector() *
            (InDirection * FMath::Max(_OrbitDistance, 1.0) * ck_jolt_debugger_3d_viewport::Get_WheelDollyFraction());

        SetViewLocation(GetViewLocation() + Step);
        DoSet_LookAt(_LookAt + Step);

        Invalidate();
    }

private:
    TWeakPtr<FCk_Jolt_DebugDrawTarget> _Target;
    TWeakPtr<FSceneViewport> _Viewport;

    TOptional<FBox> _SelectionBounds;
    FOnCkJoltDebugger_BodyPicked _OnBodyPicked;
    FSimpleDelegate _OnTogglePause;
    FSimpleDelegate _OnStepOnce;
    FSimpleDelegate _OnToggleIsolate;
    FOnCkJoltDebugger_DragArm _OnDragArm;
    FOnCkJoltDebugger_DragRay _OnDragRay;
    FOnCkJoltDebugger_DragPlaneShift _OnDragPlaneShift;
    FSimpleDelegate _OnDragRelease;
    FOnCkJoltDebugger_BodyHovered _OnBodyHovered;

    // The last hover answer, so the delegate fires on change alone, plus when it was taken.
    TOptional<uint64> _HoveredBodyKey;
    double _LastHoverSeconds = 0.0;

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

    // A Ctrl+LMB gesture in flight. Tracked separately from the drag: on a client the drag never opens, but
    // the Ctrl-click still added to the selection on the press, and the release must not pick a second time.
    bool _IsCtrlGesture = false;

    bool _IsDragEnabled = false;
    bool _IsDragGesture = false;

    bool _FollowSelection = false;
    TOptional<FVector> _LastFollowCenter;

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
            .SetCreateDefaultLighting(true)
            .SetSkyBrightness(1.0f)
            .SetLightBrightness(UE_PI)
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
    _ViewportClient->Set_OnToggleIsolate(InArgs._OnToggleIsolate);
    _ViewportClient->Set_OnDragArm(InArgs._OnDragArm);
    _ViewportClient->Set_OnDragRay(InArgs._OnDragRay);
    _ViewportClient->Set_OnDragPlaneShift(InArgs._OnDragPlaneShift);
    _ViewportClient->Set_OnDragRelease(InArgs._OnDragRelease);
    _ViewportClient->Set_OnBodyHovered(InArgs._OnBodyHovered);
    _SceneViewport = MakeShared<FSceneViewport>(_ViewportClient.Get(), SharedThis(this));
    _ViewportClient->Set_Viewport(_SceneViewport.ToSharedRef());
    SetViewportInterface(_SceneViewport.ToSharedRef());

    /*
     * The world-axis gizmo (P8-D59, ruled to fork (b) by P5-D61/S7): the suite's own orientation cube in the
     * viewport's own child slot, which SViewport draws OVER the rendered scene. No hand-drawn OnPaint gizmo.
     *
     * It is fed the INVERSE of the view rotation, because the cube shows an object's orientation under a fixed
     * three-quarter camera and what this needs to show is the WORLD's orientation under a moving one — turning
     * the camera left has to swing the axes right. The cube's own fixed camera offset stays, so the gizmo reads
     * as a three-quarter view of the world axes rather than as a screen-aligned triad.
     *
     * HitTestInvisible: it sits over the bottom-left corner of the pick surface, and a gizmo that ate clicks
     * there would be a dead zone in the viewport.
     */
    ChildSlot
    [
        SNew(SOverlay)

        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Bottom)
        .Padding(CkStyle::SpaceM)
        [
            SNew(SCkDebug_OrientationCube)
            .Visibility(EVisibility::HitTestInvisible)
            .Size(ck_jolt_debugger_3d_viewport::Get_GizmoSize())
            .Rotation_Lambda([this]() -> FQuat
            {
                return _ViewportClient.IsValid()
                    ? _ViewportClient->GetViewRotation().Quaternion().Inverse()
                    : FQuat::Identity;
            })
        ]
    ];

    // Built once, never in OnPaint (CkDebuggerCommon/CLAUDE.md § OnPaint). It therefore does NOT follow a live
    // Style Lab text-scale flip; the label size is fixed for the life of the window.
    _LabelFont = ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall());

    // The hover name. Empty text shows no tooltip at all, which is what an empty-space hover reports.
    SetToolTipText(TAttribute<FText>::CreateSP(this, &SCkJoltDebugger_3dViewport::Get_HoverTooltip));
}

auto
    SCkJoltDebugger_3dViewport::
    Set_PrimaryLabel(
        TOptional<FCkJoltDebugger_ViewportLabel> InLabel)
    -> void
{
    _PrimaryLabel = MoveTemp(InLabel);
}

auto
    SCkJoltDebugger_3dViewport::
    Set_HoverLabel(
        FText InText)
    -> void
{
    _HoverText = MoveTemp(InText);
}

auto
    SCkJoltDebugger_3dViewport::
    Get_HoverTooltip() const
    -> FText
{
    return _HoverText;
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
    Set_DragEnabled(
        bool InIsEnabled)
    -> void
{
    if (_ViewportClient.IsValid())
    { _ViewportClient->Set_DragEnabled(InIsEnabled); }
}

auto
    SCkJoltDebugger_3dViewport::
    Set_FollowSelection(
        bool InIsEnabled)
    -> void
{
    if (_ViewportClient.IsValid())
    { _ViewportClient->Set_FollowSelection(InIsEnabled); }
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
    Get_RenderFeatures() const
    -> FCkJoltDebugger_ViewportRenderFeatures
{
    return _ViewportClient.IsValid()
        ? _ViewportClient->Get_RenderFeatures()
        : FCkJoltDebugger_ViewportRenderFeatures{};
}

auto
    SCkJoltDebugger_3dViewport::
    Set_Target(
        TSharedPtr<FCk_Jolt_DebugDrawTarget> InTarget)
    -> void
{
    _Target = InTarget;

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
        _ViewportClient->Tick_FollowSelection();
        _ViewportClient->Tick(InDeltaTime);
    }
}

// --------------------------------------------------------------------------------------------------------------------

/*
 * The 2D layer over the 3D one (P8-D58): the facility hands out labels as WORLD positions and does not render
 * text itself, so somebody has to project them — and Slate is the only text renderer this preview scene has.
 *
 * Nothing here allocates PER PAINT that it can avoid, which is not the same as allocating nothing (P8-D74/F3).
 * The font is a member built at construct, the colours come off the label and the palette, and the label
 * selection writes into a member scratch array that keeps its capacity across paints — so the steady state is
 * allocation-free. What still allocates is Slate's own: `FSlateDrawElement::MakeText` copies the text into the
 * element list, exactly as it does for every other text-drawing widget in the suite.
 */
auto
    SCkJoltDebugger_3dViewport::
    OnPaint(
        const FPaintArgs&        InArgs,
        const FGeometry&         InAllottedGeometry,
        const FSlateRect&        InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32                    InLayerId,
        const FWidgetStyle&      InWidgetStyle,
        bool                     InParentEnabled) const
    -> int32
{
    const auto SceneLayer = SViewport::OnPaint(InArgs, InAllottedGeometry, InCullingRect,
        OutDrawElements, InLayerId, InWidgetStyle, InParentEnabled);

    const auto Target = _Target.Pin();

    const auto HasLabelWork = _PrimaryLabel.IsSet() ||
        (Target.IsValid() && NOT Target->Get_Labels().IsEmpty());

    if (NOT HasLabelWork || NOT _ViewportClient.IsValid() || NOT _SceneViewport.IsValid())
    { return SceneLayer; }

    auto ViewProjection = FMatrix::Identity;
    auto ViewRect = FIntRect{};

    if (NOT _ViewportClient->TryGet_ViewProjection(_SceneViewport.Get(), ViewProjection, ViewRect))
    { return SceneLayer; }

    const auto RectSize = ViewRect.Size();

    if (RectSize.X <= 0 || RectSize.Y <= 0)
    { return SceneLayer; }

    // The viewport's pixels and the widget's local space are the same rectangle at different DPI scales, so a
    // projected pixel becomes a local point by ratio rather than by another projection.
    const auto LocalSize = InAllottedGeometry.GetLocalSize();
    const auto PixelToLocal = FVector2D{
        LocalSize.X / static_cast<double>(RectSize.X),
        LocalSize.Y / static_cast<double>(RectSize.Y)};

    const auto LabelLayer = SceneLayer + 1;

    const auto PaintLabel = [&](const FVector& InWorldPosition, const FString& InText, const FLinearColor& InColor)
    {
        if (InText.IsEmpty())
        { return; }

        auto ScreenPosition = FVector2D::ZeroVector;

        // False for anything behind the eye — a label the camera turned away from must not wrap around to the
        // other side of the screen.
        if (NOT FSceneView::ProjectWorldToScreen(InWorldPosition, ViewRect, ViewProjection, ScreenPosition))
        { return; }

        const auto LocalPosition = FVector2D{
            ScreenPosition.X * PixelToLocal.X,
            ScreenPosition.Y * PixelToLocal.Y};

        FSlateDrawElement::MakeText(
            OutDrawElements,
            LabelLayer,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f{LocalSize.X, LocalSize.Y},
                FSlateLayoutTransform{FVector2f{
                    static_cast<float>(LocalPosition.X), static_cast<float>(LocalPosition.Y)}}),
            InText,
            _LabelFont,
            ESlateDrawEffect::None,
            InColor);
    };

    if (_PrimaryLabel.IsSet())
    { PaintLabel(_PrimaryLabel->WorldPosition, _PrimaryLabel->Text, _PrimaryLabel->Color); }

    if (NOT Target.IsValid())
    { return LabelLayer; }

    const auto& Labels = Target->Get_Labels();

    ck_jolt_debugger_viewport::Select_NearestLabels(
        Labels, _ViewportClient->GetViewLocation(), ck_jolt_debugger_viewport::MaxPaintedLabels, _LabelScratch);

    if (Labels.Num() > _LabelScratch.Num() && NOT _LabelCapLogged)
    {
        _LabelCapLogged = true;
        ck::jolt_debugger::Display(TEXT("Jolt debugger viewport is painting the {} nearest of {} labels; the rest are dropped by the hard cap"), _LabelScratch.Num(), Labels.Num());
    }

    for (const auto Index : _LabelScratch)
    {
        const auto& Label = Labels[Index];
        PaintLabel(Label.Get_WorldPosition(), Label.Get_Text(), Label.Get_Color());
    }

    return LabelLayer;
}

// --------------------------------------------------------------------------------------------------------------------
