#if WITH_DEV_AUTOMATION_TESTS

#include "CkSmDebugger/CkSmDebuggerStyle.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkSmDebuggerStyle_GraphEventMappings,
    "Ck.SmDebugger.Style.GraphEventMappings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkSmDebuggerStyle_GraphEventMappings::RunTest(const FString&) -> bool
{
    const auto Quick = FCkSmDebuggerStyle::Get_GraphMotionTiming(
        ECkDebugAxis_GraphMotion::Quick);
    const auto Measured = FCkSmDebuggerStyle::Get_GraphMotionTiming(
        ECkDebugAxis_GraphMotion::Measured);
    const auto Deliberate = FCkSmDebuggerStyle::Get_GraphMotionTiming(
        ECkDebugAxis_GraphMotion::Deliberate);
    TestEqual(TEXT("Quick state event pulse is gradual"), Quick.EntryPulseSeconds, 0.60f);
    TestEqual(TEXT("Quick edge flash is gradual"), Quick.TransitionFlashSeconds, 0.65f);
    TestEqual(TEXT("Measured state event pulse is gradual"), Measured.EntryPulseSeconds, 1.10f);
    TestEqual(TEXT("Measured edge flash is gradual"), Measured.TransitionFlashSeconds, 1.20f);
    TestEqual(TEXT("Deliberate state event pulse is gradual"), Deliberate.EntryPulseSeconds, 1.80f);
    TestEqual(TEXT("Deliberate edge flash is gradual"), Deliberate.TransitionFlashSeconds, 2.00f);

    const auto Subtle = FCkSmDebuggerStyle::Get_GraphEventEmphasis(
        ECkDebugAxis_GraphEventEmphasis::Subtle);
    const auto Clear = FCkSmDebuggerStyle::Get_GraphEventEmphasis(
        ECkDebugAxis_GraphEventEmphasis::Clear);
    const auto Bold = FCkSmDebuggerStyle::Get_GraphEventEmphasis(
        ECkDebugAxis_GraphEventEmphasis::Bold);
    TestTrue(TEXT("Event outline thickness increases with emphasis"),
             Subtle.StateEventOutlinePeakThickness < Clear.StateEventOutlinePeakThickness
             && Clear.StateEventOutlinePeakThickness < Bold.StateEventOutlinePeakThickness);
    TestTrue(TEXT("Event edge flash thickness increases with emphasis"),
             Subtle.EdgeFlashPeakThickness < Clear.EdgeFlashPeakThickness
             && Clear.EdgeFlashPeakThickness < Bold.EdgeFlashPeakThickness);
    TestEqual(TEXT("Clear produces a visibly thick state-event outline"),
              Clear.StateEventOutlinePeakThickness,
              5.0f);
    TestEqual(TEXT("Clear produces a distinct edge flash"),
              Clear.EdgeFlashPeakThickness,
              4.5f);
    return true;
}

#endif
