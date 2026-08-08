#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"

#include "Misc/AutomationTest.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Geometry.h"

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

    auto Layout = FCkDebug_IconToolbarLayout{};
    TestTrue(TEXT("Direct layout accepts valid actions"),
        FCkDebug_IconToolbarLayout::TryBuild(Actions, Layout));
    TestEqual(TEXT("Direct row publishes every action"), Layout.ActionCount, 8);

    Actions.Add(MakeAction(TEXT("Action0")));
    auto Duplicate = FCkDebug_IconToolbarLayout{99};
    TestFalse(TEXT("Duplicate stable ids reject the whole toolbar"),
        FCkDebug_IconToolbarLayout::TryBuild(Actions, Duplicate));
    TestEqual(TEXT("Rejected toolbar publishes no actions"), Duplicate.ActionCount, 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugIconToggle_DoesNotFillSurplusWidth,
    "Ck.DebuggerCommon.IconToolbar.ToggleDoesNotFillSurplusWidth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugIconToggle_DoesNotFillSurplusWidth::RunTest(const FString& Parameters)
{
    auto Toggle = SNew(SCkDebug_IconToggle)
        .IconId(TEXT("Grid"))
        .Label(FText::FromString(TEXT("Test toggle")))
        .IsOn(false)
        .OnStateChanged(FOnCkDebug_IconToggleChanged::CreateLambda([](bool) {}));

    Toggle->SlatePrepass();
    const auto DesiredWidth = Toggle->GetDesiredSize().X;
    auto Arranged = FArrangedChildren{EVisibility::Visible};
    Toggle->ArrangeChildren(
        FGeometry::MakeRoot(FVector2D{320.0f, 32.0f}, FSlateLayoutTransform{}),
        Arranged);

    TestEqual(TEXT("Toggle arranges one checkbox"), Arranged.Num(), 1);
    if (Arranged.Num() == 1)
    {
        TestTrue(
            TEXT("Checkbox keeps its desired width in a wide action slot"),
            Arranged[0].Geometry.GetLocalSize().X <= DesiredWidth + KINDA_SMALL_NUMBER);
    }

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
    auto Layout = FCkDebug_IconToolbarLayout{99};
    TestFalse(TEXT("Missing action id is rejected"),
        FCkDebug_IconToolbarLayout::TryBuild({InvalidAction}, Layout));
    TestEqual(TEXT("Invalid descriptor leaves no partial layout"), Layout.ActionCount, 0);

    auto MissingIconAction = MakeAction(TEXT("MissingIcon"));
    MissingIconAction.IconId = TEXT("DefinitelyMissingCkDebuggerIcon");
    Layout = {99};
    TestFalse(TEXT("Missing icon rejects the whole toolbar"),
        FCkDebug_IconToolbarLayout::TryBuild({MissingIconAction}, Layout));
    TestEqual(TEXT("Missing icon leaves no partial layout"), Layout.ActionCount, 0);

    TestNotNull(TEXT("Common icon registry resolves toolbar glyphs"),
        FCkDebuggerCommonStyle::Get_IconBrush(TEXT("Grid")));
    TestNull(TEXT("Common icon registry does not invent missing glyphs"),
        FCkDebuggerCommonStyle::Get_IconBrush(TEXT("DefinitelyMissingCkDebuggerIcon")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
