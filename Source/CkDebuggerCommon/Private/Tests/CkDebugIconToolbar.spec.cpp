#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"

#include "Misc/AutomationTest.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_icon_toolbar_tests
{
auto MakeAction(FName InId) -> FCkDebug_IconToggleAction
{
    return FCkDebug_IconToggleAction{
        InId,
        TEXT("Grid"),
        FText::FromName(InId),
        FText::FromString(TEXT("Test toggle")),
        TAttribute<bool>{false},
        FOnCkDebug_IconToggleChanged::CreateLambda([](bool) {})};
}
} // namespace ck_debug_icon_toolbar_tests

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugIconToolbar_PartitionsAtomically,
    "Ck.DebuggerCommon.IconToolbar.PartitionsAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugIconToolbar_PartitionsAtomically::RunTest(const FString& Parameters)
{
    using namespace ck_debug_icon_toolbar_tests;

    auto Actions = TArray<FCkDebug_IconToggleAction>{};
    for (auto Index = 0; Index < 8; ++Index)
    { Actions.Add(MakeAction(FName{*FString::Printf(TEXT("Action%d"), Index)})); }

    auto Wide = FCkDebug_IconToolbarPartition{};
    TestTrue(TEXT("Wide partition accepts valid actions"),
        FCkDebug_IconToolbarPartition::TryBuild(Actions, 6, Wide));
    TestEqual(TEXT("Wide row keeps six direct actions"), Wide.DirectCount, 6);
    TestEqual(TEXT("Wide row sends two actions to overflow"), Wide.OverflowCount, 2);

    auto Compact = FCkDebug_IconToolbarPartition{};
    TestTrue(TEXT("Compact partition accepts valid actions"),
        FCkDebug_IconToolbarPartition::TryBuild(Actions, 3, Compact));
    TestEqual(TEXT("Compact row keeps three direct actions"), Compact.DirectCount, 3);
    TestEqual(TEXT("Compact row sends five actions to overflow"), Compact.OverflowCount, 5);

    Actions.Add(MakeAction(TEXT("Action0")));
    auto Duplicate = FCkDebug_IconToolbarPartition{99, 99};
    TestFalse(TEXT("Duplicate stable ids reject the whole toolbar"),
        FCkDebug_IconToolbarPartition::TryBuild(Actions, 6, Duplicate));
    TestEqual(TEXT("Rejected toolbar publishes no direct actions"), Duplicate.DirectCount, 0);
    TestEqual(TEXT("Rejected toolbar publishes no overflow actions"), Duplicate.OverflowCount, 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugIconToolbar_RejectsInvalidDescriptors,
    "Ck.DebuggerCommon.IconToolbar.RejectsInvalidDescriptors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugIconToolbar_RejectsInvalidDescriptors::RunTest(const FString& Parameters)
{
    using namespace ck_debug_icon_toolbar_tests;

    auto InvalidAction = MakeAction(NAME_None);
    auto Partition = FCkDebug_IconToolbarPartition{99, 99};
    TestFalse(TEXT("Missing action id is rejected"),
        FCkDebug_IconToolbarPartition::TryBuild({InvalidAction}, 6, Partition));
    TestEqual(TEXT("Invalid descriptor leaves no partial direct layout"), Partition.DirectCount, 0);
    TestEqual(TEXT("Invalid descriptor leaves no partial overflow layout"), Partition.OverflowCount, 0);

    auto MissingIconAction = MakeAction(TEXT("MissingIcon"));
    MissingIconAction.IconId = TEXT("DefinitelyMissingCkDebuggerIcon");
    Partition = {99, 99};
    TestFalse(TEXT("Missing icon rejects the whole toolbar"),
        FCkDebug_IconToolbarPartition::TryBuild({MissingIconAction}, 6, Partition));
    TestEqual(TEXT("Missing icon leaves no partial direct layout"), Partition.DirectCount, 0);
    TestEqual(TEXT("Missing icon leaves no partial overflow layout"), Partition.OverflowCount, 0);

    auto ValidActions = TArray<FCkDebug_IconToggleAction>{MakeAction(TEXT("Valid"))};
    TestFalse(TEXT("Non-positive direct limit is rejected"),
        FCkDebug_IconToolbarPartition::TryBuild(ValidActions, 0, Partition));
    TestEqual(TEXT("Invalid limit leaves no partial layout"), Partition.DirectCount, 0);

    TestNotNull(TEXT("Common icon registry resolves toolbar glyphs"),
        FCkDebuggerCommonStyle::Get_IconBrush(TEXT("Grid")));
    TestNull(TEXT("Common icon registry does not invent missing glyphs"),
        FCkDebuggerCommonStyle::Get_IconBrush(TEXT("DefinitelyMissingCkDebuggerIcon")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
