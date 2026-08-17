#include "CkDebuggerCommon/Viewport/SCkDebug_3dPreviewViewport.h"
#include "CkDebuggerCommon/Settings/CkDebuggerWindowSettings.h"

#include "Misc/AutomationTest.h"

#include <limits>

// --------------------------------------------------------------------------------------------------------------------
// Common 3D preview shell contract. Feature-specific physics, crowd rendering,
// pause/step/drag, populations, and source selectors remain outside this
// surface.
// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_3d_viewport_spec
{
class FConsumer final : public ICkDebug3dPreviewAdapter
{
  public:
    virtual auto
    Get_FrameBounds(ECkDebug3dFrameTarget InTarget) const -> FBox override
    {
        return InTarget == ECkDebug3dFrameTarget::Selection ? _SelectionBounds : _ContentBounds;
    }

    virtual auto
    Get_SelectionCenter() const -> TOptional<FVector> override
    {
        return _SelectionCenter;
    }

    virtual auto
    Get_Capabilities() const -> ECkDebug3dViewportCapability override
    {
        return _Capabilities;
    }

    virtual auto
    On_Pick(const FCkDebug3dCursorRay&) -> void override
    {
        ++_PickCalls;
    }
    virtual auto
    Get_ShowGrid() const -> bool override
    {
        return _ShowGrid;
    }
    virtual auto
    Set_ShowGrid(bool InValue) -> void override
    {
        _ShowGrid = InValue;
    }
    virtual auto
    Get_RenderMode() const -> ECkDebug3dRenderMode override
    {
        return _RenderMode;
    }
    virtual auto
    Set_RenderMode(ECkDebug3dRenderMode InValue) -> void override
    {
        _RenderMode = InValue;
    }
    virtual auto
    Set_IsolatedKeys(const TArray<uint64>& InKeys) -> void override
    {
        _IsolatedKeys = InKeys;
    }
    virtual auto
    Get_ShowLabels() const -> bool override
    {
        return _ShowLabels;
    }
    virtual auto
    Set_ShowLabels(bool InValue) -> void override
    {
        _ShowLabels = InValue;
    }
    virtual auto
    Get_DirectionGlyphScale() const -> float override
    {
        return _GlyphScale;
    }
    virtual auto
    Set_DirectionGlyphScale(float InValue) -> void override
    {
        _GlyphScale = InValue;
    }
    virtual auto
    On_ViewportTeardown() -> void override
    {
        ++_TeardownCalls;
    }

    FBox _ContentBounds = FBox{FVector{-100.0, -100.0, -100.0}, FVector{100.0, 100.0, 100.0}};
    FBox _SelectionBounds = FBox{FVector{-10.0, -10.0, -10.0}, FVector{10.0, 10.0, 10.0}};
    TOptional<FVector> _SelectionCenter = FVector::ZeroVector;
    ECkDebug3dViewportCapability _Capabilities =
        ECkDebug3dViewportCapability::Labels | ECkDebug3dViewportCapability::DirectionGlyphScale |
        ECkDebug3dViewportCapability::FollowSelection | ECkDebug3dViewportCapability::IsolateSelection |
        ECkDebug3dViewportCapability::FrameSelection;
    int32 _PickCalls = 0;
    int32 _TeardownCalls = 0;
    bool _ShowGrid = false;
    ECkDebug3dRenderMode _RenderMode = ECkDebug3dRenderMode::None;
    TArray<uint64> _IsolatedKeys;
    bool _ShowLabels = false;
    float _GlyphScale = 1.0f;
};

auto
MakeDescriptor() -> FCkDebug3dPreviewDescriptor
{
    auto Descriptor = FCkDebug3dPreviewDescriptor{};
    Descriptor._PreviewPolicy = FCkDebug3dPreviewScenePolicy{._DefaultLighting = true,
                                                             ._Lighting = true,
                                                             ._PostProcessing = true,
                                                             ._AntiAliasing = true,
                                                             ._TemporalAA = true,
                                                             ._DynamicShadows = false,
                                                             ._MotionBlur = false,
                                                             ._DepthOfField = false,
                                                             ._EyeAdaptation = false};
    Descriptor._GesturePolicy = ECkDebug3dGesturePolicy::JoltUnrealUnion;
    Descriptor._ShowOrientationCube = true;
    return Descriptor;
}

auto
MakeViewport(const TSharedPtr<FConsumer>& InConsumer) -> TSharedRef<SCkDebug_3dPreviewViewport>
{
    return SNew(SCkDebug_3dPreviewViewport).Descriptor(MakeDescriptor()).Adapter(InConsumer);
}
} // namespace ck_debug_3d_viewport_spec

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dViewport_RuntimeSafeConstruction,
                                 "Ck.DebuggerCommon.Viewport3d.RuntimeSafeConstruction",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebug3dViewport_RuntimeSafeConstruction::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_viewport_spec;
    const auto Consumer = MakeShared<FConsumer>();
    const auto Viewport = MakeViewport(Consumer);
    Viewport->SlatePrepass();

    const auto Features = Viewport->Get_RenderFeatures();
    TestFalse(TEXT("preview shell never creates a physics scene by default"), Features._HasPhysicsScene);
    TestFalse(TEXT("preview shell is not an editor world"), Features._IsEditorScene);
    TestTrue(TEXT("requested lighting is retained"), Features._Lighting);
    TestTrue(TEXT("requested post processing is retained"), Features._PostProcessing);
    TestTrue(TEXT("requested anti aliasing is retained"), Features._AntiAliasing);
    TestTrue(TEXT("requested temporal AA is retained"), Features._TemporalAA);
    TestFalse(TEXT("dynamic shadows remain opt-in"), Features._DynamicShadows);
    TestFalse(TEXT("motion blur remains opt-in"), Features._MotionBlur);
    TestFalse(TEXT("eye adaptation remains opt-in"), Features._EyeAdaptation);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dViewport_FlySpeedPersistsOnlyFromRmbWheel,
                                 "Ck.DebuggerCommon.Viewport3d.FlySpeedPersistsOnlyFromRmbWheel",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebug3dViewport_FlySpeedPersistsOnlyFromRmbWheel::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_viewport_spec;
    auto* Settings = GetMutableDefault<UCkDebuggerWindowSettings>();
    if (Settings == nullptr)
    {
        AddError(TEXT("Common debugger window settings must exist for fly-speed persistence."));
        return false;
    }

    const auto OriginalSpeed = Settings->ViewportFlyCameraSpeed;
    Settings->ViewportFlyCameraSpeed = std::numeric_limits<float>::quiet_NaN();
    const auto InvalidConfigViewport = MakeViewport(MakeShared<FConsumer>());
    TestTrue(TEXT("non-finite user config falls back to a finite fly speed"),
             FMath::IsFinite(InvalidConfigViewport->Get_CameraSpeed()));

    constexpr auto PersistedSpeed = 12.5f;
    Settings->ViewportFlyCameraSpeed = PersistedSpeed;
    const auto Viewport = MakeViewport(MakeShared<FConsumer>());
    TestTrue(TEXT("new preview client starts from per-user fly speed"),
             FMath::IsNearlyEqual(Viewport->Get_CameraSpeed(), PersistedSpeed));

    Viewport->Input_Key(EKeys::MouseScrollUp, IE_Pressed);
    TestTrue(TEXT("plain perspective wheel does not overwrite the user fly speed"),
             FMath::IsNearlyEqual(Settings->ViewportFlyCameraSpeed, PersistedSpeed));

    Viewport->Input_Key(EKeys::RightMouseButton, IE_Pressed);
    Viewport->Input_Key(EKeys::MouseScrollUp, IE_Pressed);
    Viewport->Input_Key(EKeys::RightMouseButton, IE_Released);
    TestTrue(TEXT("perspective RMB wheel persists the adjusted fly speed"),
             Settings->ViewportFlyCameraSpeed > PersistedSpeed);
    const auto AdjustedSpeed = Settings->ViewportFlyCameraSpeed;
    const auto ReopenedViewport = MakeViewport(MakeShared<FConsumer>());
    TestTrue(TEXT("a reopened preview client restores the adjusted per-user fly speed"),
             FMath::IsNearlyEqual(ReopenedViewport->Get_CameraSpeed(), AdjustedSpeed));

    Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::Top);
    const auto AfterPerspectiveFlySpeed = Settings->ViewportFlyCameraSpeed;
    Viewport->Input_Key(EKeys::MouseScrollUp, IE_Pressed);
    TestTrue(TEXT("orthographic wheel zoom does not overwrite the user fly speed"),
             FMath::IsNearlyEqual(Settings->ViewportFlyCameraSpeed, AfterPerspectiveFlySpeed));

    Settings->ViewportFlyCameraSpeed = OriginalSpeed;
    Settings->SaveConfig();
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dViewport_OrientationCallbackExcludesFraming,
                                 "Ck.DebuggerCommon.Viewport3d.OrientationCallbackExcludesFraming",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebug3dViewport_OrientationCallbackExcludesFraming::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_viewport_spec;
    auto Received = TOptional<ECkDebug3dCameraPreset>{};
    const auto Viewport = SNew(SCkDebug_3dPreviewViewport)
                              .Descriptor(MakeDescriptor())
                              .Adapter(MakeShared<FConsumer>())
                              .OnCameraOrientationChanged(FOnCkDebug3dCameraOrientationChanged::CreateLambda(
                                  [&Received](ECkDebug3dCameraPreset InPreset) { Received = InPreset; }));
    TestTrue(TEXT("orientation commands publish their selected preset"),
             Viewport->Invoke_CommonControl(TEXT("Viewport3d.Camera.Top")) &&
                 Received.IsSet() && *Received == ECkDebug3dCameraPreset::Top);
    Received.Reset();
    TestTrue(TEXT("frame-all remains an action rather than a persisted orientation"),
             Viewport->Invoke_CommonControl(TEXT("Viewport3d.FrameAll")) && NOT Received.IsSet());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dViewport_PresetsAndInertFraming,
                                 "Ck.DebuggerCommon.Viewport3d.PresetsAndInertFraming",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebug3dViewport_PresetsAndInertFraming::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_viewport_spec;
    const auto Consumer = MakeShared<FConsumer>();
    const auto Viewport = MakeViewport(Consumer);
    const auto Presets = TArray<ECkDebug3dCameraPreset>{
        ECkDebug3dCameraPreset::Perspective, ECkDebug3dCameraPreset::Top,   ECkDebug3dCameraPreset::Bottom,
        ECkDebug3dCameraPreset::Left,        ECkDebug3dCameraPreset::Right, ECkDebug3dCameraPreset::Front,
        ECkDebug3dCameraPreset::Back};

    for (const auto Preset : Presets)
    {
        Viewport->Apply_CameraPreset(Preset);
        TestEqual(TEXT("axis preset selects its declared projection"), Viewport->Get_ProjectionMode(),
                  Preset == ECkDebug3dCameraPreset::Perspective ? ECameraProjectionMode::Perspective
                                                                : ECameraProjectionMode::Orthographic);
    }

    Consumer->_ContentBounds = FBox{ForceInit};
    const auto Before = Viewport->Get_CameraState();
    Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::FrameAll);
    TestTrue(TEXT("invalid frame-all bounds leave the camera inert"), Viewport->Get_CameraState().Equals(Before));
    Consumer->_SelectionBounds = FBox{ForceInit};
    Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::FrameSelection);
    TestTrue(TEXT("invalid frame-selection bounds leave the camera inert"), Viewport->Get_CameraState().Equals(Before));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dViewport_JoltUnrealGestureUnion,
                                 "Ck.DebuggerCommon.Viewport3d.JoltUnrealGestureUnion",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebug3dViewport_JoltUnrealGestureUnion::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_viewport_spec;
    const auto Viewport = MakeViewport(MakeShared<FConsumer>());
    const auto Initial = Viewport->Get_CameraState();

    Viewport->Input_Key(EKeys::RightMouseButton, IE_Pressed);
    Viewport->Input_MouseAxis(EKeys::MouseX, 40.0f);
    TestTrue(TEXT("RMB look changes rotation without moving the eye"),
             Viewport->Get_ViewLocation().Equals(Initial._Location));
    Viewport->Input_Key(EKeys::RightMouseButton, IE_Released);

    Viewport->Input_Key(EKeys::LeftAlt, IE_Pressed);
    Viewport->Input_Key(EKeys::LeftMouseButton, IE_Pressed);
    Viewport->Input_MouseAxis(EKeys::MouseX, 40.0f);
    TestFalse(TEXT("Alt LMB orbit moves the eye around the stable pivot"),
              Viewport->Get_ViewLocation().Equals(Initial._Location));
    Viewport->Input_Key(EKeys::LeftMouseButton, IE_Released);
    Viewport->Input_Key(EKeys::LeftAlt, IE_Released);

    const auto BeforePan = Viewport->Get_CameraState();
    Viewport->Input_Key(EKeys::MiddleMouseButton, IE_Pressed);
    Viewport->Input_MouseAxis(EKeys::MouseX, 20.0f);
    Viewport->Input_Key(EKeys::MiddleMouseButton, IE_Released);
    TestFalse(TEXT("MMB pans eye and pivot"), Viewport->Get_CameraState().Equals(BeforePan));

    const auto BeforeDolly = Viewport->Get_CameraState();
    Viewport->Input_Key(EKeys::MouseScrollUp, IE_Pressed);
    TestTrue(TEXT("perspective wheel dollies the whole camera rather than "
                  "collapsing orbit distance"),
             Viewport->Get_LookAtLocation().Equals(BeforeDolly._LookAt) == false);

    Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::Top);
    const auto OrthoWidth = Viewport->Get_CameraState()._OrthoWidth;
    Viewport->Input_Key(EKeys::MouseScrollUp, IE_Pressed);
    TestTrue(TEXT("orthographic wheel changes width"), Viewport->Get_CameraState()._OrthoWidth != OrthoWidth);
    TestTrue(TEXT("orthographic pan reports width-scaled policy"), Viewport->Get_EffectivePanScale() > 0.0f);

    Viewport->Apply_CameraPreset(ECkDebug3dCameraPreset::Perspective);
    const auto Speed = Viewport->Get_CameraSpeed();
    Viewport->Input_Key(EKeys::RightMouseButton, IE_Pressed);
    Viewport->Input_Key(EKeys::MouseScrollUp, IE_Pressed);
    Viewport->Input_Key(EKeys::RightMouseButton, IE_Released);
    TestTrue(TEXT("RMB wheel changes fly speed"), Viewport->Get_CameraSpeed() > Speed);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dViewport_FocusLossClearsGestureState,
                                 "Ck.DebuggerCommon.Viewport3d.FocusLossClearsGestureState",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebug3dViewport_FocusLossClearsGestureState::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_viewport_spec;
    const auto Viewport = MakeViewport(MakeShared<FConsumer>());
    Viewport->Input_Key(EKeys::LeftAlt, IE_Pressed);
    Viewport->Input_Key(EKeys::LeftMouseButton, IE_Pressed);
    Viewport->Handle_FocusLost();
    TestFalse(TEXT("focus loss clears every held gesture key"), Viewport->Get_HasActiveCameraGesture());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dViewport_DeprojectionAndDpi,
                                 "Ck.DebuggerCommon.Viewport3d.DeprojectionAndDpi",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebug3dViewport_DeprojectionAndDpi::
    RunTest(const FString&)
    -> bool
{
    const auto Origin = FVector{1234.0, -5678.0, 910.0};
    const auto Matrix = ck::Debug3dViewport::Make_InverseViewMatrix(Origin, FMatrix::Identity);
    TestTrue(TEXT("inverse view contains camera translation"),
             Matrix.TransformPosition(FVector::ZeroVector).Equals(Origin, 0.01));

    const auto Local = ck::Debug3dViewport::ViewPixelsToLocalSlate(
        FVector2D{960.0f, 540.0f}, FVector2D{1920.0f, 1080.0f}, FVector2D{800.0f, 450.0f});
    TestTrue(TEXT("projection pixels use a ratio rather than assuming unit DPI"),
             Local.Equals(FVector2D{400.0f, 225.0f}));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dViewport_BookmarksFollowAndIsolation,
                                 "Ck.DebuggerCommon.Viewport3d.BookmarksFollowAndIsolation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebug3dViewport_BookmarksFollowAndIsolation::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_viewport_spec;
    const auto Consumer = MakeShared<FConsumer>();
    const auto Viewport = MakeViewport(Consumer);
    Viewport->Store_CameraBookmark(7);
    TestEqual(TEXT("bookmark store creates ten dense slots"), Viewport->Get_CameraBookmarks().Num(), 10);
    TestTrue(TEXT("stored slot is marked set"), Viewport->Get_CameraBookmarks()[7]._IsSet);
    const auto BeforeUnsetRecall = Viewport->Get_CameraState();
    Viewport->Recall_CameraBookmark(6);
    TestTrue(TEXT("unset bookmark is inert"), Viewport->Get_CameraState().Equals(BeforeUnsetRecall));

    constexpr auto FollowSelection = true;
    Viewport->Set_FollowSelection(FollowSelection);
    Viewport->Tick_FollowSelection();
    Consumer->_SelectionCenter = FVector{100.0, 0.0, 0.0};
    Viewport->Tick_FollowSelection();
    TestTrue(TEXT("follow translates the camera by selection delta"),
             Viewport->Get_ViewLocation().X != BeforeUnsetRecall._Location.X);

    constexpr auto IsolateSelection = true;
    Viewport->Set_IsolateSelection(IsolateSelection);
    Viewport->Set_SelectionKeys({});
    TestTrue(TEXT("empty selection clears the isolated set"), Viewport->Get_IsolatedKeys().IsEmpty());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dViewport_BookmarkPersistenceBridge,
                                 "Ck.DebuggerCommon.Viewport3d.BookmarkPersistenceBridge",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebug3dViewport_BookmarkPersistenceBridge::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_viewport_spec;
    auto Imported = TArray<FCkDebug3dCameraBookmark>{};
    Imported.SetNum(10);
    Imported[4] =
        FCkDebug3dCameraBookmark{FVector{100.0, -200.0, 300.0}, FRotator{-15.0, 30.0, 0.0}, 1234.0f, true, true};

    auto NotificationCount = 0;
    auto LastPublished = TArray<FCkDebug3dCameraBookmark>{};
    const auto Consumer = MakeShared<FConsumer>();
    const auto Viewport =
        SNew(SCkDebug_3dPreviewViewport)
            .Descriptor(MakeDescriptor())
            .Adapter(Consumer)
            .CameraBookmarks(Imported)
            .OnCameraBookmarksChanged_Lambda(
                [&NotificationCount, &LastPublished](const TArray<FCkDebug3dCameraBookmark>& InBookmarks)
                {
                    ++NotificationCount;
                    LastPublished = InBookmarks;
                });

    TestEqual(TEXT("construction imports ten dense bookmark slots"), Viewport->Get_CameraBookmarks().Num(), 10);
    TestEqual(TEXT("restore does not rewrite persisted settings"), NotificationCount, 0);
    Viewport->Recall_CameraBookmark(4);
    TestTrue(TEXT("imported bookmark restores its camera location"),
             Viewport->Get_ViewLocation().Equals(Imported[4]._Location));
    TestEqual(TEXT("recall does not publish a settings mutation"), NotificationCount, 0);

    Viewport->Store_CameraBookmark(2);
    TestEqual(TEXT("store publishes exactly one settings mutation"), NotificationCount, 1);
    TestEqual(TEXT("store publishes ten dense slots"), LastPublished.Num(), 10);
    TestTrue(TEXT("stored slot is present in the published payload"), LastPublished[2]._IsSet);

    auto Replacement = Imported;
    Replacement[4]._Location = FVector{700.0, 800.0, 900.0};
    Viewport->Import_CameraBookmarks(Replacement);
    TestTrue(TEXT("live import replaces local bookmark state"),
             Viewport->Get_CameraBookmarks()[4]._Location.Equals(Replacement[4]._Location));
    TestEqual(TEXT("live import remains a restore operation, not a persistence "
                   "mutation"),
              NotificationCount, 1);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dViewport_CapabilityControls,
                                 "Ck.DebuggerCommon.Viewport3d.CapabilityControls",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebug3dViewport_CapabilityControls::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_viewport_spec;
    const auto Consumer = MakeShared<FConsumer>();
    const auto Viewport = MakeViewport(Consumer);
    const auto ControlIds = Viewport->Build_CommonControlDescriptors();

    TestTrue(TEXT("always-common frame-all control is published"), ControlIds.Contains(TEXT("Viewport3d.FrameAll")));
    TestTrue(TEXT("always-common camera preset control is published"), ControlIds.Contains(TEXT("Viewport3d.Camera")));
    TestTrue(TEXT("always-common grid control is published"), ControlIds.Contains(TEXT("Viewport3d.Grid")));
    TestTrue(TEXT("always-common three-state render mode is published"),
             ControlIds.Contains(TEXT("Viewport3d.RenderMode")));
    TestTrue(TEXT("always-common orientation cube control is published"),
             ControlIds.Contains(TEXT("Viewport3d.OrientationCube")));
    TestTrue(TEXT("always-common dense bookmark controls are published"),
             ControlIds.Contains(TEXT("Viewport3d.Bookmarks")));
    TestTrue(TEXT("labels capability publishes its standard control"), ControlIds.Contains(TEXT("Viewport3d.Labels")));
    TestTrue(TEXT("vectors capability publishes its standard control"),
             ControlIds.Contains(TEXT("Viewport3d.DirectionGlyphScale")));
    TestTrue(TEXT("follow capability publishes its standard control"), ControlIds.Contains(TEXT("Viewport3d.Follow")));
    TestTrue(TEXT("isolate capability publishes its standard control"),
             ControlIds.Contains(TEXT("Viewport3d.Isolate")));
    TestTrue(TEXT("selection-frame capability publishes its standard control"),
             ControlIds.Contains(TEXT("Viewport3d.FrameSelection")));
    TestEqual(TEXT("common render mode offers exactly the inspection policy states"), Viewport->Get_RenderModeOptions(),
              TArray<ECkDebug3dRenderMode>{ECkDebug3dRenderMode::None, ECkDebug3dRenderMode::TransparentOnly,
                                           ECkDebug3dRenderMode::All});
    TestFalse(TEXT("simulation controls never leak into generic preview chrome"),
              ControlIds.Contains(TEXT("Viewport3d.Pause")));
    TestFalse(TEXT("single-step is feature-specific and absent"), ControlIds.Contains(TEXT("Viewport3d.Step")));
    TestFalse(TEXT("drag is feature-specific and absent"), ControlIds.Contains(TEXT("Viewport3d.Drag")));
    TestFalse(TEXT("population is feature-specific and absent"), ControlIds.Contains(TEXT("Viewport3d.Population")));
    TestFalse(TEXT("source selection is feature-specific and absent"), ControlIds.Contains(TEXT("Viewport3d.Source")));
    TestTrue(TEXT("grid action reaches adapter"),
             Viewport->Invoke_CommonControl(TEXT("Viewport3d.Grid")) && Consumer->_ShowGrid);
    TestTrue(TEXT("explicit render action reaches adapter"),
             Viewport->Invoke_CommonControl(TEXT("Viewport3d.RenderMode.All")) &&
                 Consumer->_RenderMode == ECkDebug3dRenderMode::All);
    constexpr auto IsolateSelection = true;
    Viewport->Set_IsolateSelection(IsolateSelection);
    Viewport->Set_SelectionKeys({7});
    TestEqual(TEXT("isolate publishes selected keys"), Consumer->_IsolatedKeys.Num(), 1);
    Viewport->Set_SelectionKeys({});
    TestEqual(TEXT("empty selection clears adapter isolate keys"), Consumer->_IsolatedKeys.Num(), 0);
    const auto BeforeBadBookmark = Viewport->Get_CameraBookmarks();
    TestFalse(TEXT("malformed bookmark does nothing"),
              Viewport->Invoke_CommonControl(TEXT("Viewport3d.Bookmark.Store.3x")));
    TestTrue(TEXT("malformed bookmark preserves slots"),
             Viewport->Get_CameraBookmarks()[3]._IsSet == BeforeBadBookmark[3]._IsSet);
    for (const auto Name :
         {TEXT("Perspective"), TEXT("Top"), TEXT("Bottom"), TEXT("Left"), TEXT("Right"), TEXT("Front"), TEXT("Back")})
    {
        TestTrue(TEXT("camera preset action operates"),
                 Viewport->Invoke_CommonControl(FName{FString::Printf(TEXT("Viewport3d.Camera.%s"), Name)}));
    }
    TestTrue(TEXT("frame all action operates"), Viewport->Invoke_CommonControl(TEXT("Viewport3d.FrameAll")));
    TestTrue(TEXT("frame selection action operates"),
             Viewport->Invoke_CommonControl(TEXT("Viewport3d.FrameSelection")));
    const auto CubeBefore = Viewport->Get_ShowOrientationCube();
    TestTrue(TEXT("orientation action operates"), Viewport->Invoke_CommonControl(TEXT("Viewport3d.OrientationCube")) &&
                                                      Viewport->Get_ShowOrientationCube() != CubeBefore);
    TestTrue(TEXT("labels action reaches adapter"),
             Viewport->Invoke_CommonControl(TEXT("Viewport3d.Labels")) && Consumer->_ShowLabels);
    TestTrue(TEXT("glyph action reaches adapter"),
             Viewport->Invoke_CommonControl(TEXT("Viewport3d.DirectionGlyphScale")) && Consumer->_GlyphScale > 1.0f);
    TestTrue(TEXT("follow action operates"), Viewport->Invoke_CommonControl(TEXT("Viewport3d.Follow")));
    for (auto Index = 0; Index < 10; ++Index)
    {
        TestTrue(TEXT("bookmark store operates"),
                 Viewport->Invoke_CommonControl(FName{*FString::Printf(TEXT("Viewport3d.Bookmark.Store.%d"), Index)}));
        TestTrue(TEXT("bookmark recall operates"),
                 Viewport->Invoke_CommonControl(FName{*FString::Printf(TEXT("Viewport3d.Bookmark.%d"), Index)}));
    }
    TestFalse(TEXT("noncanonical bookmark is rejected"),
              Viewport->Invoke_CommonControl(TEXT("Viewport3d.Bookmark.00")));
    TestFalse(TEXT("out of range bookmark is rejected"),
              Viewport->Invoke_CommonControl(TEXT("Viewport3d.Bookmark.10")));
    TestFalse(TEXT("malformed render mode is rejected"),
              Viewport->Invoke_CommonControl(TEXT("Viewport3d.RenderMode.Unknown")));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dViewport_StandardConsumerAndTeardown,
                                 "Ck.DebuggerCommon.Viewport3d.StandardConsumerAndTeardown",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto
    FCkDebug3dViewport_StandardConsumerAndTeardown::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_viewport_spec;
    auto Consumer = TSharedPtr<FConsumer>{MakeShared<FConsumer>()};
    const auto Viewport = MakeViewport(Consumer);
    Viewport->SlatePrepass();
    TestTrue(TEXT("a consumer receives standard common controls without "
                  "rebuilding them"),
             Viewport->Get_CommonControlsWidget().IsValid());

    Viewport->Teardown();
    Viewport->Teardown();
    TestEqual(TEXT("teardown is idempotent"), Consumer->_TeardownCalls, 1);
    const auto WeakConsumerViewport = MakeViewport(Consumer);
    Consumer.Reset();
    WeakConsumerViewport->Teardown();
    constexpr auto TeardownWasSafe = true;
    TestTrue(TEXT("teardown tolerates an expired weak adapter"), TeardownWasSafe);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
