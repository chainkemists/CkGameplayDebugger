#include "CkInsightsDebugger/Widgets/SCkFrameTimingGraph.h"

#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_frame_timing_graph_tests
{
    auto MakePointerEvent(
        const FKey& InEffectingButton,
        const FVector2D& InPosition,
        const FVector2D& InPreviousPosition,
        float InWheelDelta = 0.0f) -> FPointerEvent
    {
        auto PressedButtons = TSet<FKey>{};
        if (InEffectingButton != EKeys::Invalid)
        {
            PressedButtons.Add(InEffectingButton);
        }
        return FPointerEvent(0, InPosition, InPreviousPosition, PressedButtons, InEffectingButton,
            InWheelDelta, FModifierKeysState{});
    }

    auto MakeView() -> TSharedPtr<const FCkInsightsFrameTimingView, ESPMode::ThreadSafe>
    {
        auto Result = FCk_FrameAnalysisResult{};
        Result.FrameIndex = 42;
        Result.FrameStartTime = 0.0;
        Result.FrameEndTime = 0.016;
        Result.FrameDurationMs = 16.0;
        Result.HasValidTimeRange = true;
        Result.Events = {
            FCk_TimingEvent{7, 0.000, 0.004, 0},
            FCk_TimingEvent{3, 0.004, 0.008, 0},
            FCk_TimingEvent{7, 0.009, 0.013, 0},
        };
        return FCkInsightsFrameTimingView::Build(Result,
            TMap<uint32, FString>{{3, TEXT("Other Timer")}, {7, TEXT("Duplicate Timer")}}, 1, false);
    }

    auto MakeDeepView() -> TSharedPtr<const FCkInsightsFrameTimingView, ESPMode::ThreadSafe>
    {
        auto Result = FCk_FrameAnalysisResult{};
        Result.FrameIndex = 43;
        Result.FrameStartTime = 0.0;
        Result.FrameEndTime = 0.016;
        Result.FrameDurationMs = 16.0;
        Result.HasValidTimeRange = true;
        Result.Events = {
            FCk_TimingEvent{1, 0.000, 0.016, 0},
            FCk_TimingEvent{2, 0.002, 0.014, 1},
            FCk_TimingEvent{3, 0.004, 0.012, 2},
            FCk_TimingEvent{4, 0.006, 0.010, 3},
            FCk_TimingEvent{5, 0.007, 0.009, 4},
        };
        return FCkInsightsFrameTimingView::Build(Result,
            TMap<uint32, FString>{{1, TEXT("Root")}, {2, TEXT("One")}, {3, TEXT("Two")},
                                  {4, TEXT("Three")}, {5, TEXT("Four")}}, 1, false);
    }

    auto MakeFocusView() -> TSharedPtr<const FCkInsightsFrameTimingView, ESPMode::ThreadSafe>
    {
        auto Result = FCk_FrameAnalysisResult{};
        Result.FrameIndex = 44;
        Result.FrameStartTime = 0.0;
        Result.FrameEndTime = 0.016;
        Result.FrameDurationMs = 16.0;
        Result.HasValidTimeRange = true;
        Result.Events = {
            FCk_TimingEvent{1, 0.000, 0.016, 0},
            FCk_TimingEvent{7, 0.008, 0.009, 3},
            FCk_TimingEvent{7, 0.010, 0.011, 4},
        };
        return FCkInsightsFrameTimingView::Build(Result,
            TMap<uint32, FString>{{1, TEXT("Root")}, {7, TEXT("Focus Timer")}}, 1, false);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkFrameTimingGraph_ZoomAnchorsAndClamps,
    "Ck.InsightsDebugger.AnalyzerTab.FrameTimingGraph.ZoomAnchorsAndClamps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkFrameTimingGraph_ZoomAnchorsAndClamps::RunTest(const FString&)
{
    using namespace ck_frame_timing_graph_tests;

    const auto Graph = SNew(SCkFrameTimingGraph);
    const auto Geometry = FGeometry::MakeRoot(FVector2D{400.0f, 150.0f}, FSlateLayoutTransform{});
    Graph->SetView(MakeView());
    TestEqual(TEXT("Custom timing paint is always clipped to its widget bounds"),
        Graph->GetClipping(), EWidgetClipping::ClipToBoundsAlways);

    constexpr float CursorX = 200.0f;
    const double TimeUnderCursor = 0.5 * Graph->GetVisibleDurationMs();
    Graph->OnMouseWheel(Geometry,
        MakePointerEvent(EKeys::Invalid, FVector2D{CursorX, 40.0f}, FVector2D{CursorX, 40.0f}, 1.0f));
    const double ZoomedTimeUnderCursor = Graph->GetViewStartMs() +
        0.5 * Graph->GetVisibleDurationMs();
    TestTrue(TEXT("Wheel zoom keeps the timestamp under the cursor"),
        FMath::IsNearlyEqual(TimeUnderCursor, ZoomedTimeUnderCursor, 0.001));

    for (int32 Index = 0; Index < 32; ++Index)
    {
        Graph->OnMouseWheel(Geometry,
            MakePointerEvent(EKeys::Invalid, FVector2D{CursorX, 40.0f}, FVector2D{CursorX, 40.0f}, -1.0f));
    }
    TestTrue(TEXT("Zoom-out clamps to exactly one full frame"),
        FMath::IsNearlyEqual(Graph->GetVisibleDurationMs(), 16.0, 0.001));

    const auto View = MakeView();
    const auto NarrowGraph = SNew(SCkFrameTimingGraph);
    const auto WideGraph = SNew(SCkFrameTimingGraph);
    const auto NarrowGeometry = FGeometry::MakeRoot(FVector2D{400.0f, 150.0f}, FSlateLayoutTransform{});
    const auto WideGeometry = FGeometry::MakeRoot(FVector2D{800.0f, 150.0f}, FSlateLayoutTransform{});
    NarrowGraph->SetView(View);
    WideGraph->SetView(View);
    TestTrue(TEXT("Initial narrow view fits exactly one frame"),
        FMath::IsNearlyEqual(NarrowGraph->GetVisibleDurationMs(), 16.0, 0.001));
    TestTrue(TEXT("Initial wide view fits exactly one frame"),
        FMath::IsNearlyEqual(WideGraph->GetVisibleDurationMs(), 16.0, 0.001));
    NarrowGraph->OnMouseWheel(NarrowGeometry,
        MakePointerEvent(EKeys::Invalid, FVector2D{200.0f, 40.0f}, FVector2D{200.0f, 40.0f}, 1.0f));
    WideGraph->OnMouseWheel(WideGeometry,
        MakePointerEvent(EKeys::Invalid, FVector2D{400.0f, 40.0f}, FVector2D{400.0f, 40.0f}, 1.0f));
    TestTrue(TEXT("Equal cursor fractions produce width-independent visible durations"),
        FMath::IsNearlyEqual(NarrowGraph->GetVisibleDurationMs(), WideGraph->GetVisibleDurationMs(), 0.001));
    TestTrue(TEXT("Equal cursor fractions produce width-independent viewport starts"),
        FMath::IsNearlyEqual(NarrowGraph->GetViewStartMs(), WideGraph->GetViewStartMs(), 0.001));

    const auto ExtremeGraph = SNew(SCkFrameTimingGraph);
    ExtremeGraph->SetView(View);
    for (int32 Index = 0; Index < 100; ++Index)
    {
        ExtremeGraph->OnMouseWheel(NarrowGeometry,
            MakePointerEvent(EKeys::Invalid, FVector2D{200.0f, 40.0f}, FVector2D{200.0f, 40.0f}, 1.0f));
    }
    TestTrue(TEXT("Horizontal zoom advances well past the former frame-duration / 10,000 cap"),
        ExtremeGraph->GetVisibleDurationMs() < 16.0 / 10000.0);
    TestTrue(TEXT("Extreme horizontal zoom remains finite and positive"),
        FMath::IsFinite(ExtremeGraph->GetVisibleDurationMs()) && ExtremeGraph->GetVisibleDurationMs() > 0.0);

    const double PreservedVisibleDuration = NarrowGraph->GetVisibleDurationMs();
    const double PreservedViewStart = NarrowGraph->GetViewStartMs();
    NarrowGraph->SetView(View);
    TestTrue(TEXT("Republishing the same view preserves visible duration"),
        FMath::IsNearlyEqual(NarrowGraph->GetVisibleDurationMs(), PreservedVisibleDuration, 0.001));
    TestTrue(TEXT("Republishing the same view preserves viewport start"),
        FMath::IsNearlyEqual(NarrowGraph->GetViewStartMs(), PreservedViewStart, 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkFrameTimingGraph_DragPansViewport,
    "Ck.InsightsDebugger.AnalyzerTab.FrameTimingGraph.DragPansViewport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkFrameTimingGraph_DragPansViewport::RunTest(const FString&)
{
    using namespace ck_frame_timing_graph_tests;

    const auto Graph = SNew(SCkFrameTimingGraph);
    const auto Geometry = FGeometry::MakeRoot(FVector2D{400.0f, 80.0f}, FSlateLayoutTransform{});
    Graph->SetView(MakeDeepView());
    for (int32 Index = 0; Index < 6; ++Index)
    {
        Graph->OnMouseWheel(Geometry,
            MakePointerEvent(EKeys::Invalid, FVector2D{4.0f, 40.0f}, FVector2D{4.0f, 40.0f}, 1.0f));
    }

    Graph->OnMouseButtonDown(Geometry,
        MakePointerEvent(EKeys::MiddleMouseButton, FVector2D{200.0f, 40.0f}, FVector2D{200.0f, 40.0f}));
    Graph->OnMouseMove(Geometry,
        MakePointerEvent(EKeys::MiddleMouseButton, FVector2D{100.0f, 40.0f}, FVector2D{200.0f, 40.0f}));
    TestTrue(TEXT("Middle drag advances the horizontal timing viewport"), Graph->GetViewStartMs() > 0.0);
    Graph->OnMouseButtonUp(Geometry,
        MakePointerEvent(EKeys::MiddleMouseButton, FVector2D{100.0f, 40.0f}, FVector2D{100.0f, 40.0f}));

    Graph->OnMouseButtonDown(Geometry,
        MakePointerEvent(EKeys::RightMouseButton, FVector2D{200.0f, 70.0f}, FVector2D{200.0f, 70.0f}));
    Graph->OnMouseMove(Geometry,
        MakePointerEvent(EKeys::RightMouseButton, FVector2D{100.0f, 30.0f}, FVector2D{200.0f, 70.0f}));
    TestTrue(TEXT("Right drag also advances the horizontal timing viewport"), Graph->GetViewStartMs() > 0.0);
    TestTrue(TEXT("Right drag pans the nested-depth viewport"), Graph->GetViewTopRow() > 0.0);
    Graph->OnMouseButtonUp(Geometry,
        MakePointerEvent(EKeys::RightMouseButton, FVector2D{100.0f, 30.0f}, FVector2D{100.0f, 30.0f}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkFrameTimingGraph_FixedHeightZoomAndFocus,
    "Ck.InsightsDebugger.AnalyzerTab.FrameTimingGraph.FixedHeightZoomAndFocus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkFrameTimingGraph_FixedHeightZoomAndFocus::RunTest(const FString&)
{
    using namespace ck_frame_timing_graph_tests;

    const auto Geometry = FGeometry::MakeRoot(FVector2D{400.0f, 150.0f}, FSlateLayoutTransform{});
    const auto DeepGraph = SNew(SCkFrameTimingGraph);
    DeepGraph->SetView(MakeDeepView());
    constexpr float CursorY = 80.0f;
    const double InitialVisibleRows = DeepGraph->GetVisibleRowCount();
    const double InitialTopRow = DeepGraph->GetViewTopRow();
    DeepGraph->OnMouseWheel(Geometry,
        MakePointerEvent(EKeys::Invalid, FVector2D{200.0f, CursorY}, FVector2D{200.0f, CursorY}, 1.0f));
    TestTrue(TEXT("Wheel zoom reduces the visible time range"), DeepGraph->GetVisibleDurationMs() < 20.0);
    TestTrue(TEXT("Wheel zoom preserves fixed nested-row capacity"),
        FMath::IsNearlyEqual(InitialVisibleRows, DeepGraph->GetVisibleRowCount(), 0.001));
    TestTrue(TEXT("Wheel zoom does not move the nested-depth viewport"),
        FMath::IsNearlyEqual(InitialTopRow, DeepGraph->GetViewTopRow(), 0.001));

    const auto FocusGraph = SNew(SCkFrameTimingGraph);
    FocusGraph->SetView(MakeFocusView());
    for (int32 Index = 0; Index < 5; ++Index)
    {
        FocusGraph->OnMouseWheel(Geometry,
            MakePointerEvent(EKeys::Invalid, FVector2D{4.0f, 80.0f}, FVector2D{4.0f, 80.0f}, 1.0f));
    }
    FocusGraph->FocusTimer(7);
    const double FocusEndMs = FocusGraph->GetViewStartMs() + FocusGraph->GetVisibleDurationMs();
    TestTrue(TEXT("Focus makes the first selected occurrence visible"), FocusGraph->GetViewStartMs() <= 8.0);
    TestTrue(TEXT("Focus makes the selected occurrence union visible when it fits"), FocusEndMs >= 11.0);
    TestTrue(TEXT("Focus keeps the linked timer identity selected"),
        FocusGraph->GetSelectedTimerIndex().IsSet() && FocusGraph->GetSelectedTimerIndex().GetValue() == 7);

    TestEqual(TEXT("Sub-millisecond duration formatting preserves precision"),
        SCkFrameTimingGraph::FormatDuration(0.5), FString(TEXT("0.500ms")));
    TestEqual(TEXT("Long duration formatting uses seconds"),
        SCkFrameTimingGraph::FormatDuration(1500.0), FString(TEXT("1.50s")));
    TestEqual(TEXT("Sub-microsecond duration formatting remains meaningful when deeply zoomed"),
        SCkFrameTimingGraph::FormatDuration(0.0005), FString(TEXT("0.500us")));
    TestEqual(TEXT("Event labels parenthesize their duration"),
        SCkFrameTimingGraph::FormatEventLabel(TEXT("Timer"), 0.5), FString(TEXT("Timer (0.500ms)")));
    TestFalse(TEXT("Bars without label interior do not attempt text"), SCkFrameTimingGraph::ShouldAttemptLabel(8.0f));
    TestTrue(TEXT("Compact bars with label interior attempt clipped text"), SCkFrameTimingGraph::ShouldAttemptLabel(9.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkFrameTimingGraph_SelectsDuplicateTimerOccurrences,
    "Ck.InsightsDebugger.AnalyzerTab.FrameTimingGraph.SelectsDuplicateTimerOccurrences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkFrameTimingGraph_SelectsDuplicateTimerOccurrences::RunTest(const FString&)
{
    using namespace ck_frame_timing_graph_tests;

    TOptional<uint32> DelegateSelection;
    const auto Graph = SNew(SCkFrameTimingGraph)
        .OnTimerSelectionChanged(FOnFrameTimingTimerSelectionChanged::CreateLambda(
            [&DelegateSelection](TOptional<uint32> InTimerIndex) { DelegateSelection = InTimerIndex; }));
    const auto Geometry = FGeometry::MakeRoot(FVector2D{400.0f, 150.0f}, FSlateLayoutTransform{});
    Graph->SetView(MakeView());

    const FVector2D HitPosition{50.0f, 30.0f};
    Graph->OnMouseButtonDown(Geometry, MakePointerEvent(EKeys::LeftMouseButton, HitPosition, HitPosition));
    Graph->OnMouseButtonUp(Geometry, MakePointerEvent(EKeys::LeftMouseButton, HitPosition, HitPosition));

    TestTrue(TEXT("Click selects the trace timer identity"),
        Graph->GetSelectedTimerIndex().IsSet() && Graph->GetSelectedTimerIndex().GetValue() == 7);
    TestEqual(TEXT("All duplicate timer occurrences share the selected identity"),
        Graph->GetSelectedOccurrenceCount(), 2);
    TestTrue(TEXT("Selection delegate carries the selected timer identity"),
        DelegateSelection.IsSet() && DelegateSelection.GetValue() == 7);
    Graph->SetView(Graph->GetView());
    TestTrue(TEXT("Republishing the identical timing view preserves selection"),
        Graph->GetSelectedTimerIndex().IsSet() && Graph->GetSelectedTimerIndex().GetValue() == 7);

    Graph->SetTimerSelection(99);
    TestTrue(TEXT("A linked timer absent from this exact frame remains the selected identity"),
        Graph->GetSelectedTimerIndex().IsSet() && Graph->GetSelectedTimerIndex().GetValue() == 99);
    TestEqual(TEXT("An absent linked timer has no visible occurrences to dim against"),
        Graph->GetSelectedOccurrenceCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkFrameTimingGraph_EmptyClickClearsSelection,
    "Ck.InsightsDebugger.AnalyzerTab.FrameTimingGraph.EmptyClickClearsSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkFrameTimingGraph_EmptyClickClearsSelection::RunTest(const FString&)
{
    using namespace ck_frame_timing_graph_tests;

    TOptional<uint32> DelegateSelection = 7;
    const auto Graph = SNew(SCkFrameTimingGraph)
        .OnTimerSelectionChanged(FOnFrameTimingTimerSelectionChanged::CreateLambda(
            [&DelegateSelection](TOptional<uint32> InTimerIndex) { DelegateSelection = InTimerIndex; }));
    const auto Geometry = FGeometry::MakeRoot(FVector2D{400.0f, 150.0f}, FSlateLayoutTransform{});
    Graph->SetView(MakeView());

    const FVector2D HitPosition{50.0f, 30.0f};
    Graph->OnMouseButtonDown(Geometry, MakePointerEvent(EKeys::LeftMouseButton, HitPosition, HitPosition));
    Graph->OnMouseButtonUp(Geometry, MakePointerEvent(EKeys::LeftMouseButton, HitPosition, HitPosition));

    const FVector2D EmptyPosition{370.0f, 100.0f};
    Graph->OnMouseButtonDown(Geometry, MakePointerEvent(EKeys::LeftMouseButton, EmptyPosition, EmptyPosition));
    Graph->OnMouseButtonUp(Geometry, MakePointerEvent(EKeys::LeftMouseButton, EmptyPosition, EmptyPosition));

    TestFalse(TEXT("Empty left click clears the graph selection"), Graph->GetSelectedTimerIndex().IsSet());
    TestFalse(TEXT("Clear delegate carries an unset identity"), DelegateSelection.IsSet());

    DelegateSelection = 7;
    Graph->OnMouseButtonDown(Geometry, MakePointerEvent(EKeys::LeftMouseButton, EmptyPosition, EmptyPosition));
    Graph->OnMouseButtonUp(Geometry, MakePointerEvent(EKeys::LeftMouseButton, EmptyPosition, EmptyPosition));
    TestFalse(TEXT("A repeated empty click still broadcasts the linked-selection clear"), DelegateSelection.IsSet());

    DelegateSelection = 7;
    const FVector2D AdjacentEventGap{101.5f, 30.0f};
    Graph->OnMouseButtonDown(Geometry,
        MakePointerEvent(EKeys::LeftMouseButton, AdjacentEventGap, AdjacentEventGap));
    Graph->OnMouseButtonUp(Geometry,
        MakePointerEvent(EKeys::LeftMouseButton, AdjacentEventGap, AdjacentEventGap));
    TestFalse(TEXT("The one-pixel separator between adjacent events is also empty for hit testing"),
        DelegateSelection.IsSet());

    DelegateSelection = 7;
    const FVector2D BelowWidget{50.0f, 151.0f};
    Graph->OnMouseButtonDown(Geometry, MakePointerEvent(EKeys::LeftMouseButton, BelowWidget, BelowWidget));
    Graph->OnMouseButtonUp(Geometry, MakePointerEvent(EKeys::LeftMouseButton, BelowWidget, BelowWidget));
    TestFalse(TEXT("A pointer below the graph cannot hit an overflowing zoomed row"), DelegateSelection.IsSet());
    return true;
}

#endif
