#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkJoltDebugger/Viewport/SCkJoltDebugger_3dViewport.h"

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
    FCkJoltDebuggerViewport_ConstructsWithoutEnsure,
    "Ck.JoltDebugger.Viewport.ConstructsWithoutEnsure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkJoltDebuggerViewport_ConstructsWithoutEnsure::RunTest(const FString&) -> bool
{
    const auto Viewport = SNew(SCkJoltDebugger_3dViewport);
    Viewport->SlatePrepass();

    TestNotNull(TEXT("Jolt debugger viewport owns a preview world"), Viewport->Get_PreviewWorld());
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

#endif
