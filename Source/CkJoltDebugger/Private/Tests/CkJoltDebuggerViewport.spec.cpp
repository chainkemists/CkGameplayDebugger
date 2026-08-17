#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkJoltDebugger/Viewport/SCkJoltDebugger_3dViewport.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "UObject/UObjectIterator.h"

namespace ck_jolt_debugger_viewport_spec
{
    struct FPresetExpectation
    {
        ECkJoltDebugger_CameraPreset _Preset;
        ECameraProjectionMode::Type _ProjectionMode;
        FRotator _Rotation;
        const TCHAR* _Name;
    };

    auto Get_PresetExpectations() -> TArray<FPresetExpectation>
    {
        return
        {
            {ECkJoltDebugger_CameraPreset::Perspective, ECameraProjectionMode::Perspective,  FRotator{-25.0, 45.0, 0.0}, TEXT("Perspective")},
            {ECkJoltDebugger_CameraPreset::Top,         ECameraProjectionMode::Orthographic, FRotator{-90.0, 0.0, 0.0},  TEXT("Top")},
            {ECkJoltDebugger_CameraPreset::Bottom,      ECameraProjectionMode::Orthographic, FRotator{90.0, 0.0, 0.0},   TEXT("Bottom")},
            {ECkJoltDebugger_CameraPreset::Left,        ECameraProjectionMode::Orthographic, FRotator{0.0, 180.0, 0.0},  TEXT("Left")},
            {ECkJoltDebugger_CameraPreset::Right,       ECameraProjectionMode::Orthographic, FRotator{0.0, 0.0, 0.0},    TEXT("Right")},
            {ECkJoltDebugger_CameraPreset::Front,       ECameraProjectionMode::Orthographic, FRotator{0.0, -90.0, 0.0},  TEXT("Front")},
            {ECkJoltDebugger_CameraPreset::Back,        ECameraProjectionMode::Orthographic, FRotator{0.0, 90.0, 0.0},   TEXT("Back")}
        };
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerViewport_CursorRayUsesCameraTranslation,
    "Ck.JoltDebugger.Viewport.CursorRayUsesCameraTranslation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerViewport_CursorRayUsesCameraTranslation::RunTest(const FString&) -> bool
{
    const auto ViewOrigin = FVector{1234.0, -5678.0, 910.0};
    const auto ViewRotationMatrix = FMatrix{FInverseRotationMatrix(FRotator{-20.0, 35.0, 0.0})} * FMatrix(
        FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));

    const auto InverseView = ck_jolt_debugger_viewport::Make_InverseViewMatrix(
        ViewOrigin, ViewRotationMatrix);

    TestTrue(
        TEXT("inverse view maps the view-space origin back to the camera's world location"),
        InverseView.TransformPosition(FVector::ZeroVector).Equals(ViewOrigin, 0.01));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerViewport_ConstructsWithoutEnsure,
    "Ck.JoltDebugger.Viewport.ConstructsWithoutEnsure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerViewport_ConstructsWithoutEnsure::RunTest(const FString&) -> bool
{
    const auto Viewport = SNew(SCkJoltDebugger_3dViewport);
    Viewport->SlatePrepass();

    auto* PreviewWorld = Viewport->Get_PreviewWorld();
    TestNotNull(TEXT("Jolt debugger viewport owns a preview world"), PreviewWorld);

    const auto RenderFeatures = Viewport->Get_RenderFeatures();
    TestTrue(TEXT("lighting is enabled"), RenderFeatures.Lighting);
    TestTrue(TEXT("post processing is enabled"), RenderFeatures.PostProcessing);
    TestTrue(TEXT("anti-aliasing is enabled"), RenderFeatures.AntiAliasing);
    TestTrue(TEXT("temporal anti-aliasing is enabled"), RenderFeatures.TemporalAA);
    TestEqual(
        TEXT("the viewport resolves temporal AA even when the project default is none"),
        RenderFeatures.ForcedAntiAliasingMethod,
        EAntiAliasingMethod::AAM_TemporalAA);
    TestFalse(TEXT("dynamic shadows stay disabled"), RenderFeatures.DynamicShadows);
    TestFalse(TEXT("motion blur stays disabled"), RenderFeatures.MotionBlur);
    TestFalse(TEXT("depth of field stays disabled"), RenderFeatures.DepthOfField);
    TestFalse(TEXT("eye adaptation stays disabled"), RenderFeatures.EyeAdaptation);

    auto HasDirectionalLight = false;
    auto HasSkyLight = false;

    for (TObjectIterator<UDirectionalLightComponent> It; It; ++It)
    {
        if (It->GetWorld() == PreviewWorld && It->IsRegistered())
        { HasDirectionalLight = true; }
    }

    for (TObjectIterator<USkyLightComponent> It; It; ++It)
    {
        if (It->GetWorld() == PreviewWorld && It->IsRegistered())
        { HasSkyLight = true; }
    }

    TestTrue(TEXT("the preview has a directional light for form"), HasDirectionalLight);
    TestTrue(TEXT("the preview has a skylight so no side falls to black"), HasSkyLight);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerViewport_CameraPresets,
    "Ck.JoltDebugger.Viewport.CameraPresets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerViewport_CameraPresets::RunTest(const FString&) -> bool
{
    const auto Viewport = SNew(SCkJoltDebugger_3dViewport);
    Viewport->SlatePrepass();

    // Rotators are compared through their forward vector: SetViewRotation is free to normalize (180 vs -180
    // yaw), but where the camera LOOKS is what the preset actually promises.
    constexpr auto MinDirectionDot = 0.999;

    for (const auto& Expectation : ck_jolt_debugger_viewport_spec::Get_PresetExpectations())
    {
        Viewport->ApplyPreset(Expectation._Preset);

        TestEqual(
            *FString::Printf(TEXT("preset %s selects its projection mode"), Expectation._Name),
            static_cast<int32>(Viewport->Get_ProjectionMode()),
            static_cast<int32>(Expectation._ProjectionMode));

        const auto DirectionDot = FVector::DotProduct(
            Viewport->Get_ViewRotation().Vector(),
            Expectation._Rotation.Vector());

        TestTrue(
            *FString::Printf(TEXT("preset %s points the camera down its expected axis"), Expectation._Name),
            DirectionDot > MinDirectionDot);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerViewport_FrameAllWithoutContentIsInert,
    "Ck.JoltDebugger.Viewport.FrameAllWithoutContentIsInert",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerViewport_FrameAllWithoutContentIsInert::RunTest(const FString&) -> bool
{
    const auto Viewport = SNew(SCkJoltDebugger_3dViewport);
    Viewport->SlatePrepass();

    Viewport->ApplyPreset(ECkJoltDebugger_CameraPreset::Perspective);
    const auto LocationBefore = Viewport->Get_ViewLocation();

    // No target, so no content bounds. Framing an invalid box must leave the camera exactly where it was
    // rather than snapping it to the origin.
    Viewport->ApplyPreset(ECkJoltDebugger_CameraPreset::FrameAll);

    TestTrue(
        TEXT("framing with no content leaves the camera untouched"),
        Viewport->Get_ViewLocation().Equals(LocationBefore));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerViewport_CameraSchemeIsUnrealStyle,
    "Ck.JoltDebugger.Viewport.CameraSchemeIsUnrealStyle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerViewport_CameraSchemeIsUnrealStyle::RunTest(const FString&) -> bool
{
    const auto Viewport = SNew(SCkJoltDebugger_3dViewport);
    Viewport->SlatePrepass();

    constexpr auto DragAmount = 40.0f;

    const auto Drag = [&Viewport](const FKey& InButton, float InAmount)
    {
        Viewport->Input_Key(InButton, IE_Pressed);
        Viewport->Input_MouseAxis(EKeys::MouseX, InAmount);
        Viewport->Input_Key(InButton, IE_Released);
    };

    // RIGHT-DRAG LOOKS IN PLACE. This is the whole reversal: the old scheme orbited here, which moved the eye.
    {
        Viewport->ApplyPreset(ECkJoltDebugger_CameraPreset::Perspective);

        const auto LocationBefore = Viewport->Get_ViewLocation();
        const auto RotationBefore = Viewport->Get_ViewRotation();

        Drag(EKeys::RightMouseButton, DragAmount);

        TestTrue(TEXT("right-drag leaves the eye exactly where it was"),
            Viewport->Get_ViewLocation().Equals(LocationBefore));
        TestFalse(TEXT("right-drag turns the camera"),
            Viewport->Get_ViewRotation().Equals(RotationBefore, 0.01f));
    }

    // ALT+LEFT-DRAG ORBITS. The discriminating half is the pivot: an orbit moves the eye AROUND a look-at that
    // does not move, which is exactly what look-in-place above does not do.
    {
        Viewport->ApplyPreset(ECkJoltDebugger_CameraPreset::Perspective);

        const auto LocationBefore = Viewport->Get_ViewLocation();
        const auto LookAtBefore = Viewport->Get_LookAtLocation();

        Viewport->Input_Key(EKeys::LeftAlt, IE_Pressed);
        Drag(EKeys::LeftMouseButton, DragAmount);
        Viewport->Input_Key(EKeys::LeftAlt, IE_Released);

        TestFalse(TEXT("alt+left-drag moves the eye"),
            Viewport->Get_ViewLocation().Equals(LocationBefore, 1.0));
        TestTrue(TEXT("alt+left-drag orbits about a pivot that stays put"),
            Viewport->Get_LookAtLocation().Equals(LookAtBefore, 1.0));
    }

    // MIDDLE-DRAG PANS: the eye translates and the camera never turns.
    {
        Viewport->ApplyPreset(ECkJoltDebugger_CameraPreset::Perspective);

        const auto LocationBefore = Viewport->Get_ViewLocation();
        const auto RotationBefore = Viewport->Get_ViewRotation();

        Drag(EKeys::MiddleMouseButton, DragAmount);

        TestFalse(TEXT("middle-drag moves the eye"),
            Viewport->Get_ViewLocation().Equals(LocationBefore, 0.01));
        TestTrue(TEXT("middle-drag never turns the camera"),
            Viewport->Get_ViewRotation().Equals(RotationBefore, 0.01f));
    }

    // THE WHEEL DOLLIES THE WHOLE CAMERA (P7-D71/F6): the eye AND the pivot travel along the view, so the
    // offset between them is unchanged. Shrinking the orbit distance instead — which is what this used to do —
    // walks the eye towards a pivot it can never pass, and the wheel stalls in front of whatever was framed.
    {
        Viewport->ApplyPreset(ECkJoltDebugger_CameraPreset::Perspective);

        const auto LocationBefore = Viewport->Get_ViewLocation();
        const auto LookAtBefore = Viewport->Get_LookAtLocation();
        const auto OffsetBefore = LookAtBefore - LocationBefore;

        Viewport->Input_Key(EKeys::MouseScrollUp, IE_Pressed);

        TestFalse(TEXT("the wheel moves the eye"),
            Viewport->Get_ViewLocation().Equals(LocationBefore, 1.0));
        TestFalse(TEXT("and the pivot with it"),
            Viewport->Get_LookAtLocation().Equals(LookAtBefore, 1.0));
        TestTrue(TEXT("so the distance between them is untouched — the wheel never stalls at the pivot"),
            (Viewport->Get_LookAtLocation() - Viewport->Get_ViewLocation()).Equals(OffsetBefore, 0.01));
    }

    // AN ORTHO PAN SCALES OFF THE ORTHO WIDTH, NOT THE PIVOT DISTANCE (P7-D71/F5). The same drag has to cover
    // more world when more world is on screen; scaled off the pivot it would drag by the same amount at every
    // zoom level, which is unusable at both ends of the range.
    {
        Viewport->ApplyPreset(ECkJoltDebugger_CameraPreset::Top);

        for (auto Notch = 0; Notch < 10; ++Notch)
        { Viewport->Input_Key(EKeys::MouseScrollUp, IE_Pressed); }

        const auto NarrowBefore = Viewport->Get_ViewLocation();
        Drag(EKeys::MiddleMouseButton, 100.0f);
        const auto NarrowDelta = (Viewport->Get_ViewLocation() - NarrowBefore).Size();

        for (auto Notch = 0; Notch < 20; ++Notch)
        { Viewport->Input_Key(EKeys::MouseScrollDown, IE_Pressed); }

        const auto WideBefore = Viewport->Get_ViewLocation();
        Drag(EKeys::MiddleMouseButton, 100.0f);
        const auto WideDelta = (Viewport->Get_ViewLocation() - WideBefore).Size();

        TestTrue(TEXT("a zoomed-out ortho pan covers more world than a zoomed-in one"),
            WideDelta > NarrowDelta * 2.0);
    }

    // AN ORTHOGRAPHIC PRESET IS AXIS-LOCKED: a rotate gesture pans instead, and the rotation is untouched.
    {
        Viewport->ApplyPreset(ECkJoltDebugger_CameraPreset::Top);

        const auto RotationBefore = Viewport->Get_ViewRotation();

        Drag(EKeys::RightMouseButton, DragAmount);

        TestTrue(TEXT("an ortho preset refuses to rotate"),
            Viewport->Get_ViewRotation().Equals(RotationBefore, 0.01f));

        Viewport->Input_Key(EKeys::LeftAlt, IE_Pressed);
        Drag(EKeys::LeftMouseButton, DragAmount);
        Viewport->Input_Key(EKeys::LeftAlt, IE_Released);

        TestTrue(TEXT("an ortho preset refuses to orbit either"),
            Viewport->Get_ViewRotation().Equals(RotationBefore, 0.01f));
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

/*
 * The half of the label paint a headless spec CAN reach. OnPaint itself needs a live FSceneViewport with a
 * non-zero rect, which an automation run does not have; the projection under it is engine math with no branch
 * of ours in it, so what is worth pinning is the CAP — which labels survive, and in what order (P8-D58).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkJoltDebuggerViewport_LabelCapKeepsTheNearest,
    "Ck.JoltDebugger.Viewport.LabelCapKeepsTheNearest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerViewport_LabelCapKeepsTheNearest::RunTest(const FString&) -> bool
{
    const auto ViewLocation = FVector{0.0, 0.0, 0.0};

    auto Labels = TArray<FCk_Jolt_DebugDrawLabel>{};

    // Authored FARTHEST first, so an implementation that simply took the first N would fail every assertion.
    for (auto Index = 0; Index < 10; ++Index)
    {
        Labels.Emplace(FCk_Jolt_DebugDrawLabel{
            FVector{static_cast<double>((10 - Index) * 100), 0.0, 0.0},
            FString::FromInt(Index),
            FLinearColor::White});
    }

    {
        // Under the cap nothing is reordered: the capture's own order is the draw order.
        const auto All = ck_jolt_debugger_viewport::Select_NearestLabels(Labels, ViewLocation, 100);

        TestEqual(TEXT("under the cap every label survives"), All.Num(), 10);

        if (All.Num() == 10)
        {
            TestEqual(TEXT("and the capture's order is untouched"), All[0], 0);
            TestEqual(TEXT("all the way down"), All[9], 9);
        }
    }

    {
        const auto Nearest = ck_jolt_debugger_viewport::Select_NearestLabels(Labels, ViewLocation, 3);

        TestEqual(TEXT("over the cap exactly the cap survives"), Nearest.Num(), 3);

        if (Nearest.Num() == 3)
        {
            // Index 9 sits at x=100, 8 at x=200, 7 at x=300 — the three nearest, nearest first.
            TestEqual(TEXT("the nearest label leads"), Nearest[0], 9);
            TestEqual(TEXT("then the next nearest"), Nearest[1], 8);
            TestEqual(TEXT("then the third"), Nearest[2], 7);
        }
    }

    {
        const auto None = ck_jolt_debugger_viewport::Select_NearestLabels(Labels, ViewLocation, 0);
        TestEqual(TEXT("a zero cap paints nothing"), None.Num(), 0);
    }

    {
        const auto Empty = ck_jolt_debugger_viewport::Select_NearestLabels(
            TArray<FCk_Jolt_DebugDrawLabel>{}, ViewLocation, ck_jolt_debugger_viewport::MaxPaintedLabels);

        TestEqual(TEXT("no labels select nothing"), Empty.Num(), 0);
    }

    // The shipped cap is the one the ruling names, and a viewport that quietly raised it would be paying for
    // labels the user cannot read anyway.
    TestEqual(TEXT("the shipped cap is 500"), ck_jolt_debugger_viewport::MaxPaintedLabels, 500);

    return true;
}

#endif
