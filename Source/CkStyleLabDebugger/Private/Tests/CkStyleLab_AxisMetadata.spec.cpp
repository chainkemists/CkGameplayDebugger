#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkStyleLabDebugger/Styles/CkStyleLab_AxisMetadata.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkStyleLab_GraphEventEmphasisMetadata,
    "Ck.StyleLab.GraphEventEmphasisMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkStyleLab_GraphEventEmphasisMetadata::RunTest(const FString&) -> bool
{
    const auto PropertyName = FName{TEXT("GraphEventEmphasis")};
    const auto* Axis = ck::style_lab::Find_AxisMetadata(PropertyName);
    TestNotNull(TEXT("Graph Event Emphasis has Style Lab metadata"), Axis);
    if (Axis == nullptr)
    {
        return false;
    }

    TestEqual(TEXT("Graph Event Emphasis has its authored display name"),
              Axis->DisplayName.ToString(),
              FString{TEXT("Graph Event Emphasis")});
    TestFalse(TEXT("Graph Event Emphasis explains its purpose"), Axis->ToolTip.IsEmpty());

    const auto* Subtle = ck::style_lab::Find_AxisOptionLabel(
        PropertyName, static_cast<int64>(ECkDebugAxis_GraphEventEmphasis::Subtle));
    const auto* Clear = ck::style_lab::Find_AxisOptionLabel(
        PropertyName, static_cast<int64>(ECkDebugAxis_GraphEventEmphasis::Clear));
    const auto* Bold = ck::style_lab::Find_AxisOptionLabel(
        PropertyName, static_cast<int64>(ECkDebugAxis_GraphEventEmphasis::Bold));
    TestEqual(TEXT("Style Lab labels Subtle"), Subtle ? Subtle->ToString() : FString{}, FString{TEXT("Subtle")});
    TestEqual(TEXT("Style Lab labels Clear"), Clear ? Clear->ToString() : FString{}, FString{TEXT("Clear")});
    TestEqual(TEXT("Style Lab labels Bold"), Bold ? Bold->ToString() : FString{}, FString{TEXT("Bold")});
    return true;
}

#endif
