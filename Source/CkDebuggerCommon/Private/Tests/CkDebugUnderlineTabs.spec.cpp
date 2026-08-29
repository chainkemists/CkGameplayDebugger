#include "CoreMinimal.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_UnderlineTabs.h"

#include "Misc/AutomationTest.h"

#include <limits>

// ====================================================================================================================
// The underline tab strip is one physical line, so past the allotted width some tabs must leave it.
// These pin the only property that makes that acceptable: a tab that left the line is still
// REACHABLE through the overflow control, and the tab the reader is currently on never leaves.
//
// Nothing here may fatal. No check()/checkf()/verify()/FindChecked()/[]-on-missing-key, including
// in the helpers -- a fixture that asserts takes the whole lane down with it.
// ====================================================================================================================

namespace ck_debug_underline_tabs_tests
{
    auto MakeWidths(int32 InCount, float InEach) -> TArray<float>
    {
        auto Widths = TArray<float>{};

        if (InCount <= 0)
        { return Widths; }

        Widths.Init(InEach, InCount);
        return Widths;
    }

    auto Verify_EveryTabReachable(
        FAutomationTestBase& InTest,
        const FString& InWhat,
        const FCkDebug_UnderlineTabLayout& InLayout,
        int32 InTabCount)
        -> void
    {
        auto Seen = TArray<bool>{};

        if (InTabCount > 0)
        { Seen.Init(false, InTabCount); }

        auto OutOfRangeCount = 0;
        auto DuplicateCount  = 0;

        const auto Mark = [&Seen, &OutOfRangeCount, &DuplicateCount](const TArray<int32>& InIndices)
        {
            for (const auto Index : InIndices)
            {
                if (NOT Seen.IsValidIndex(Index))
                {
                    ++OutOfRangeCount;
                    continue;
                }

                if (Seen[Index])
                {
                    ++DuplicateCount;
                    continue;
                }

                Seen[Index] = true;
            }
        };

        Mark(InLayout.VisibleIndices);
        Mark(InLayout.OverflowIndices);

        auto MissingCount = 0;
        for (const auto WasSeen : Seen)
        {
            if (NOT WasSeen)
            { ++MissingCount; }
        }

        InTest.TestEqual(InWhat + TEXT(" - no index out of range"), OutOfRangeCount, 0);
        InTest.TestEqual(InWhat + TEXT(" - no index in both lists"), DuplicateCount, 0);
        InTest.TestEqual(InWhat + TEXT(" - every tab reachable"), MissingCount, 0);
    }

    auto Verify_OrderPreserved(
        FAutomationTestBase& InTest,
        const FString& InWhat,
        const TArray<int32>& InIndices)
        -> void
    {
        auto Ascending = true;

        for (auto Index = 1; Index < InIndices.Num(); ++Index)
        {
            if (InIndices[Index] <= InIndices[Index - 1])
            { Ascending = false; }
        }

        InTest.TestTrue(InWhat + TEXT(" - declared order preserved"), Ascending);
    }
}

// ====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugUnderlineTabs_EveryTabStaysReachable,
    "Ck.DebuggerCommon.UnderlineTabs.EveryTabStaysReachable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugUnderlineTabs_EveryTabStaysReachable::RunTest(const FString& Parameters)
{
    using namespace ck_debug_underline_tabs_tests;

    constexpr auto TabCount = 6;
    constexpr auto TabWidth = 100.0f;
    constexpr auto OverflowWidth = 46.0f;

    const auto Widths = MakeWidths(TabCount, TabWidth);
    TestEqual(TEXT("Fixture built the expected tab count"), Widths.Num(), TabCount);

    // A width sweep from "everything fits" down through "nothing but the anchor fits", plus the
    // degenerate values a real geometry produces before the first prepass or while collapsed.
    const auto AvailableWidths = TArray<float>{
        4000.0f, 1000.0f, 620.0f, 500.0f, 337.0f, 190.0f, 120.0f, 47.0f, 1.0f,
        0.0f, -250.0f, std::numeric_limits<float>::quiet_NaN()};

    for (const auto AvailableWidth : AvailableWidths)
    {
        for (auto ActiveIndex = 0; ActiveIndex < TabCount; ++ActiveIndex)
        {
            const auto Layout = FCkDebug_UnderlineTabLayout::Compute(
                AvailableWidth, Widths, OverflowWidth, ActiveIndex);

            const auto What = ck::Format_UE(TEXT("width {} active {}"), AvailableWidth, ActiveIndex);

            Verify_EveryTabReachable(*this, What, Layout, TabCount);
            Verify_OrderPreserved(*this, What + TEXT(" [visible]"), Layout.VisibleIndices);
            Verify_OrderPreserved(*this, What + TEXT(" [overflow]"), Layout.OverflowIndices);
        }
    }

    // Degenerate WIDTHS, not just a degenerate available width: a zero/negative/NaN measurement
    // must fail closed to one anchor plus overflow rather than crash or drop a tab.
    const auto DegenerateWidthSets = TArray<TArray<float>>{
        TArray<float>{100.0f, 0.0f, 100.0f},
        TArray<float>{100.0f, -50.0f, 100.0f},
        TArray<float>{100.0f, std::numeric_limits<float>::quiet_NaN(), 100.0f}};

    for (const auto& DegenerateWidths : DegenerateWidthSets)
    {
        const auto Layout = FCkDebug_UnderlineTabLayout::Compute(
            600.0f, DegenerateWidths, OverflowWidth, 2);

        Verify_EveryTabReachable(*this, TEXT("degenerate widths"), Layout, DegenerateWidths.Num());
        TestEqual(TEXT("Degenerate input keeps exactly one tab on the line"), Layout.VisibleIndices.Num(), 1);
    }

    // An empty strip is a legal strip.
    const auto EmptyLayout = FCkDebug_UnderlineTabLayout::Compute(600.0f, TArray<float>{}, OverflowWidth, 0);
    TestTrue(TEXT("No tabs means no visible entries"), EmptyLayout.VisibleIndices.IsEmpty());
    TestTrue(TEXT("No tabs means no overflow entries"), EmptyLayout.OverflowIndices.IsEmpty());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugUnderlineTabs_ActiveTabIsAlwaysVisible,
    "Ck.DebuggerCommon.UnderlineTabs.ActiveTabIsAlwaysVisible",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugUnderlineTabs_ActiveTabIsAlwaysVisible::RunTest(const FString& Parameters)
{
    using namespace ck_debug_underline_tabs_tests;

    constexpr auto TabCount = 8;
    constexpr auto OverflowWidth = 46.0f;

    const auto Widths = MakeWidths(TabCount, 90.0f);
    TestEqual(TEXT("Fixture built the expected tab count"), Widths.Num(), TabCount);

    const auto AvailableWidths = TArray<float>{
        2000.0f, 700.0f, 400.0f, 220.0f, 100.0f, 30.0f, 1.0f,
        0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN()};

    for (const auto AvailableWidth : AvailableWidths)
    {
        for (auto ActiveIndex = 0; ActiveIndex < TabCount; ++ActiveIndex)
        {
            const auto Layout = FCkDebug_UnderlineTabLayout::Compute(
                AvailableWidth, Widths, OverflowWidth, ActiveIndex);

            const auto What = ck::Format_UE(
                TEXT("Active tab {} stays on the line at width {}"), ActiveIndex, AvailableWidth);

            TestTrue(What, Layout.VisibleIndices.Contains(ActiveIndex));
            TestFalse(What + TEXT(" (and never in overflow)"), Layout.OverflowIndices.Contains(ActiveIndex));
        }
    }

    // A last tab that only fits by evicting every trailing sibling still fits.
    const auto TightLayout = FCkDebug_UnderlineTabLayout::Compute(
        150.0f, Widths, OverflowWidth, TabCount - 1);

    TestTrue(TEXT("Trailing active tab survives eviction"), TightLayout.VisibleIndices.Contains(TabCount - 1));
    Verify_EveryTabReachable(*this, TEXT("trailing active"), TightLayout, TabCount);

    // An ActiveIndex the caller could not resolve must not corrupt the partition.
    const auto NoActiveLayout = FCkDebug_UnderlineTabLayout::Compute(
        400.0f, Widths, OverflowWidth, INDEX_NONE);

    Verify_EveryTabReachable(*this, TEXT("no active tab"), NoActiveLayout, TabCount);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugUnderlineTabs_NoOverflowWhenEverythingFits,
    "Ck.DebuggerCommon.UnderlineTabs.NoOverflowWhenEverythingFits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugUnderlineTabs_NoOverflowWhenEverythingFits::RunTest(const FString& Parameters)
{
    using namespace ck_debug_underline_tabs_tests;

    constexpr auto TabCount = 5;
    constexpr auto TabWidth = 80.0f;
    constexpr auto TotalWidth = TabCount * TabWidth;

    const auto Widths = MakeWidths(TabCount, TabWidth);
    TestEqual(TEXT("Fixture built the expected tab count"), Widths.Num(), TabCount);

    // Generously wide, and exactly wide enough. The overflow control must not be reserved for
    // when there is nothing to put in it, or the last tab would be pushed out by its own remedy.
    const auto FittingWidths = TArray<float>{5000.0f, TotalWidth + 1.0f, TotalWidth};

    for (const auto AvailableWidth : FittingWidths)
    {
        const auto Layout = FCkDebug_UnderlineTabLayout::Compute(AvailableWidth, Widths, 46.0f, 0);

        const auto What = ck::Format_UE(TEXT("width {}"), AvailableWidth);

        TestTrue(What + TEXT(" - no overflow"), Layout.OverflowIndices.IsEmpty());
        TestEqual(What + TEXT(" - every tab on the line"), Layout.VisibleIndices.Num(), TabCount);
        Verify_EveryTabReachable(*this, What, Layout, TabCount);
    }

    // One pixel short: overflow appears, and nothing is lost.
    const auto TightLayout = FCkDebug_UnderlineTabLayout::Compute(TotalWidth - 1.0f, Widths, 46.0f, 0);

    TestFalse(TEXT("One pixel short produces overflow"), TightLayout.OverflowIndices.IsEmpty());
    Verify_EveryTabReachable(*this, TEXT("one pixel short"), TightLayout, TabCount);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugUnderlineTabs_PreservesDeclaredOrder,
    "Ck.DebuggerCommon.UnderlineTabs.PreservesDeclaredOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugUnderlineTabs_PreservesDeclaredOrder::RunTest(const FString& Parameters)
{
    using namespace ck_debug_underline_tabs_tests;

    // Uneven widths so a greedy pass has something to get wrong.
    const auto Widths = TArray<float>{60.0f, 140.0f, 30.0f, 200.0f, 75.0f, 110.0f};
    const auto TabCount = Widths.Num();

    const auto AvailableWidths = TArray<float>{2000.0f, 500.0f, 300.0f, 180.0f, 60.0f};

    for (const auto AvailableWidth : AvailableWidths)
    {
        for (auto ActiveIndex = 0; ActiveIndex < TabCount; ++ActiveIndex)
        {
            const auto Layout = FCkDebug_UnderlineTabLayout::Compute(
                AvailableWidth, Widths, 46.0f, ActiveIndex);

            const auto What = ck::Format_UE(TEXT("width {} active {}"), AvailableWidth, ActiveIndex);

            Verify_OrderPreserved(*this, What + TEXT(" [visible]"), Layout.VisibleIndices);
            Verify_OrderPreserved(*this, What + TEXT(" [overflow]"), Layout.OverflowIndices);
            Verify_EveryTabReachable(*this, What, Layout, TabCount);
        }
    }

    return true;
}

// ====================================================================================================================
