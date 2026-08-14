#include "CoreMinimal.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_CommandBar.h"

#include "Misc/AutomationTest.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

// ====================================================================================================================

namespace ck_debug_command_bar_tests
{
    auto MakeGroup(
        FName InId,
        ECkDebug_CommandBarLane InLane = ECkDebug_CommandBarLane::Primary)
        -> FCkDebug_CommandGroup
    {
        return FCkDebug_CommandGroup{
            InId,
            FText::FromName(InId),
            InLane,
            SNew(SBox)};
    }

    auto CountWidgetType(const TSharedRef<SWidget>& InWidget, const FString& InType) -> int32
    {
        auto Count = InWidget->GetTypeAsString() == InType ? 1 : 0;
        auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index)
        { Count += CountWidgetType(Children->GetChildAt(Index), InType); }
        return Count;
    }
}

// ====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugCommandBar_PreservesLaneOrder,
    "Ck.DebuggerCommon.CommandBar.PreservesLaneOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugCommandBar_PreservesLaneOrder::RunTest(const FString& Parameters)
{
    using namespace ck_debug_command_bar_tests;

    const auto Groups = TArray<FCkDebug_CommandGroup>{
        MakeGroup(TEXT("View")),
        MakeGroup(TEXT("Target"), ECkDebug_CommandBarLane::Context),
        MakeGroup(TEXT("Capture")),
        MakeGroup(TEXT("Playback"), ECkDebug_CommandBarLane::Context),
    };

    auto Layout = FCkDebug_CommandBarLayout{};
    TestTrue(TEXT("Valid groups build"), FCkDebug_CommandBarLayout::TryBuild(Groups, Layout));
    TestEqual(TEXT("Primary lane count"), Layout.PrimaryGroupIndices.Num(), 2);
    TestEqual(TEXT("Context lane count"), Layout.ContextGroupIndices.Num(), 2);
    TestEqual(TEXT("First primary retains declared index"), Layout.PrimaryGroupIndices[0], 0);
    TestEqual(TEXT("Second primary retains declared index"), Layout.PrimaryGroupIndices[1], 2);
    TestEqual(TEXT("First context retains declared index"), Layout.ContextGroupIndices[0], 1);
    TestEqual(TEXT("Second context retains declared index"), Layout.ContextGroupIndices[1], 3);
    TestEqual(TEXT("Each two-group lane has one separator"), Layout.Get_SeparatorCount(), 2);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugCommandBar_LanesNeverWrap,
    "Ck.DebuggerCommon.CommandBar.LanesNeverWrap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugCommandBar_LanesNeverWrap::RunTest(const FString& Parameters)
{
    using namespace ck_debug_command_bar_tests;

    const auto Bar = SNew(SCkDebug_CommandBar)
        .Groups({
            MakeGroup(TEXT("View")),
            MakeGroup(TEXT("Capture")),
            MakeGroup(TEXT("Target"), ECkDebug_CommandBarLane::Context),
            MakeGroup(TEXT("Playback"), ECkDebug_CommandBarLane::Context)})
        .UtilityContent()
        [
            SNew(SBox)
        ];

    TestEqual(
        TEXT("Primary and context lanes each own a horizontal scroll viewport"),
        CountWidgetType(Bar, TEXT("SScrollBox")),
        2);
    TestEqual(
        TEXT("Command bar hierarchy contains no wrapping lane"),
        CountWidgetType(Bar, TEXT("SWrapBox")),
        0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugCommandBar_RejectsInvalidDescriptorsAtomically,
    "Ck.DebuggerCommon.CommandBar.RejectsInvalidDescriptorsAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugCommandBar_RejectsInvalidDescriptorsAtomically::RunTest(const FString& Parameters)
{
    using namespace ck_debug_command_bar_tests;

    auto Layout = FCkDebug_CommandBarLayout{};
    Layout.PrimaryGroupIndices = {99};
    TestFalse(
        TEXT("Missing group id rejects the whole bar"),
        FCkDebug_CommandBarLayout::TryBuild({MakeGroup(NAME_None)}, Layout));
    TestTrue(TEXT("Rejected primary lane is empty"), Layout.PrimaryGroupIndices.IsEmpty());
    TestTrue(TEXT("Rejected context lane is empty"), Layout.ContextGroupIndices.IsEmpty());

    auto MissingLabel = MakeGroup(TEXT("MissingLabel"));
    MissingLabel.AccessibleLabel = FText::GetEmpty();
    Layout.PrimaryGroupIndices = {99};
    TestFalse(
        TEXT("Missing accessible meaning rejects the whole bar"),
        FCkDebug_CommandBarLayout::TryBuild({MissingLabel}, Layout));
    TestTrue(TEXT("Missing meaning leaves no partial layout"), Layout.PrimaryGroupIndices.IsEmpty());

    auto MissingContent = MakeGroup(TEXT("MissingContent"));
    MissingContent.Content.Reset();
    TestFalse(
        TEXT("Missing content rejects the whole bar"),
        FCkDebug_CommandBarLayout::TryBuild({MissingContent}, Layout));

    auto InvalidLane = MakeGroup(TEXT("InvalidLane"));
    InvalidLane.Lane = ECkDebug_CommandBarLane::Invalid;
    TestFalse(
        TEXT("Invalid lane rejects the whole bar"),
        FCkDebug_CommandBarLayout::TryBuild({InvalidLane}, Layout));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugCommandBar_RejectsDuplicateIdsAtomically,
    "Ck.DebuggerCommon.CommandBar.RejectsDuplicateIdsAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugCommandBar_RejectsDuplicateIdsAtomically::RunTest(const FString& Parameters)
{
    using namespace ck_debug_command_bar_tests;

    auto Layout = FCkDebug_CommandBarLayout{};
    TestFalse(
        TEXT("Duplicate ids reject even across lanes"),
        FCkDebug_CommandBarLayout::TryBuild({
            MakeGroup(TEXT("Shared")),
            MakeGroup(TEXT("Shared"), ECkDebug_CommandBarLane::Context)}, Layout));
    TestTrue(TEXT("Duplicate rejection leaves no primary groups"), Layout.PrimaryGroupIndices.IsEmpty());
    TestTrue(TEXT("Duplicate rejection leaves no context groups"), Layout.ContextGroupIndices.IsEmpty());
    TestEqual(TEXT("Duplicate rejection leaves no separators"), Layout.Get_SeparatorCount(), 0);

    return true;
}

// ====================================================================================================================
