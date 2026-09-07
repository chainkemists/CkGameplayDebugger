#include "CkInsightsDebugger/Widgets/SCkFrameBarChart.h"

#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_frame_bar_chart_tests
{
    auto MakePointerEvent(
        const FKey& InEffectingButton,
        const FVector2D& InPosition,
        const FVector2D& InPreviousPosition,
        const float InWheelDelta = 0.0f) -> FPointerEvent
    {
        auto PressedButtons = TSet<FKey>{};
        if (InEffectingButton != EKeys::Invalid)
        {
            PressedButtons.Add(InEffectingButton);
        }

        return FPointerEvent(
            0,
            InPosition,
            InPreviousPosition,
            PressedButtons,
            InEffectingButton,
            InWheelDelta,
            FModifierKeysState{});
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkFrameBarChart_AppendPreservesUserViewport,
    "Ck.InsightsDebugger.FrameBarChart.AppendPreservesUserViewport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkFrameBarChart_AppendPreservesUserViewport::RunTest(const FString&)
{
    using namespace ck_frame_bar_chart_tests;

    const auto Chart = SNew(SCkFrameBarChart);
    const auto Geometry = FGeometry::MakeRoot(FVector2D{1000.0f, 200.0f}, FSlateLayoutTransform{});

    Chart->SetFrameData(TArray<double>{16.0, 17.0, 18.0, 19.0});
    Chart->SetSelection(2);
    TestTrue(TEXT("Fresh frame data starts in auto-fit mode"), Chart->IsAutoFitting());

    Chart->AppendFrameData(TArray<double>{20.0, 21.0, 22.0, 23.0});
    TestTrue(TEXT("Incremental data continues auto-fitting before interaction"), Chart->IsAutoFitting());
    const auto IsOnlyFrameTwoSelected = [&Chart]()
    {
        const auto& Runs = Chart->Get_Selection();
        return Runs.Num() == 1 && Runs[0].FirstFrame == 2 && Runs[0].LastFrame == 2;
    };
    TestTrue(TEXT("Append preserves the selected frame"), IsOnlyFrameTwoSelected());

    Chart->OnMouseWheel(
        Geometry,
        MakePointerEvent(EKeys::Invalid, FVector2D{500.0f, 100.0f}, FVector2D{500.0f, 100.0f}, 1.0f));
    const double ZoomedFramesPerPixel = Chart->GetFramesPerPixel();
    TestFalse(TEXT("Wheel zoom exits auto-fit mode"), Chart->IsAutoFitting());

    Chart->AppendFrameData(TArray<double>{24.0, 25.0, 26.0, 27.0});
    TestEqual(TEXT("Append retains the user-selected zoom"), Chart->GetFramesPerPixel(), ZoomedFramesPerPixel);
    TestTrue(TEXT("Append retains selection after zoom"), IsOnlyFrameTwoSelected());

    Chart->SetFrameData(TArray<double>{16.0, 17.0});
    TestTrue(TEXT("Replacing frame data restores auto-fit mode"), Chart->IsAutoFitting());
    Chart->ClearFrameData();
    TestTrue(TEXT("Clearing frame data restores auto-fit mode"), Chart->IsAutoFitting());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkFrameBarChart_AppendPreservesPannedViewport,
    "Ck.InsightsDebugger.FrameBarChart.AppendPreservesPannedViewport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkFrameBarChart_AppendPreservesPannedViewport::RunTest(const FString&)
{
    using namespace ck_frame_bar_chart_tests;

    const auto Chart = SNew(SCkFrameBarChart);
    const auto Geometry = FGeometry::MakeRoot(FVector2D{100.0f, 200.0f}, FSlateLayoutTransform{});
    auto Frames = TArray<double>{};
    Frames.Init(16.0, 500);
    Chart->SetFrameData(MoveTemp(Frames));

    Chart->OnMouseButtonDown(
        Geometry,
        MakePointerEvent(EKeys::MiddleMouseButton, FVector2D{50.0f, 100.0f}, FVector2D{50.0f, 100.0f}));
    Chart->OnMouseMove(
        Geometry,
        MakePointerEvent(EKeys::MiddleMouseButton, FVector2D{10.0f, 100.0f}, FVector2D{50.0f, 100.0f}));
    const double PannedViewOffset = Chart->GetViewOffset();
    TestFalse(TEXT("A pan exits auto-fit mode"), Chart->IsAutoFitting());
    TestTrue(TEXT("The pan moves the visible frame range"), PannedViewOffset > 0.0);

    Chart->AppendFrameData(TArray<double>{16.0, 16.0, 16.0});
    TestEqual(TEXT("Append retains the panned frame range"), Chart->GetViewOffset(), PannedViewOffset);
    Chart->OnMouseButtonUp(
        Geometry,
        MakePointerEvent(EKeys::MiddleMouseButton, FVector2D{10.0f, 100.0f}, FVector2D{10.0f, 100.0f}));
    return true;
}

#endif
