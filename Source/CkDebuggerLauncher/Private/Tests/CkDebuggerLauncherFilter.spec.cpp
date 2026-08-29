#include "Window/CkDebuggerLauncherFilter.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Misc/AutomationTest.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugger_launcher_filter_spec
{
    auto
        Make_Tool(
            const TCHAR* InTabId,
            const TCHAR* InDisplayName,
            ECkDebuggerToolCategory InCategory)
        -> FCkDebuggerToolDescriptor
    {
        return FCkDebuggerToolDescriptor{
            FName{TEXT("CkDebuggerLauncherFilterSpec")},
            FName{InTabId},
            FText::FromString(InDisplayName),
            FText::FromString(TEXT("Tooltip")),
            ECk_Icon::Diagnostics,
            InCategory,
            0};
    }

    auto
        Make_Catalog()
        -> TArray<FCkDebuggerToolDescriptor>
    {
        return TArray<FCkDebuggerToolDescriptor>{
            Make_Tool(TEXT("CkEcsDebugger"), TEXT("[CK] ECS Debugger"), ECkDebuggerToolCategory::Core),
            Make_Tool(TEXT("CkSmDebugger"), TEXT("[CK] State Machine Debugger"), ECkDebuggerToolCategory::Core),
            Make_Tool(TEXT("CkGoapDebugger"), TEXT("[CK] GOAP Debugger"), ECkDebuggerToolCategory::Ai),
            Make_Tool(TEXT("CkCrowdDebugger"), TEXT("[CK] Crowd Debugger"), ECkDebuggerToolCategory::Ai),
            Make_Tool(TEXT("CkAudioDebugger"), TEXT("[CK] Audio Debugger"), ECkDebuggerToolCategory::Tools)};
    }

    auto
        Get_TabIds(
            const TArray<FCkDebuggerToolDescriptor>& InTools)
        -> TArray<FName>
    {
        auto TabIds = TArray<FName>{};
        TabIds.Reserve(InTools.Num());

        for (const auto& Tool : InTools)
        { TabIds.Add(Tool.Get_TabId()); }

        return TabIds;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerLauncherFilter_QueryNarrowsTheRail,
    "Ck.DebuggerLauncher.Filter.QueryNarrowsTheRail",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

bool FCkDebuggerLauncherFilter_QueryNarrowsTheRail::RunTest(const FString& Parameters)
{
    const auto Catalog = ck_debugger_launcher_filter_spec::Make_Catalog();

    {
        const auto Visible = ck::debugger_launcher::Filter_Tools(FString{}, Catalog);
        TestEqual(TEXT("An empty query keeps the whole catalog"), Visible.Num(), Catalog.Num());
        TestEqual(TEXT("An empty query preserves catalog order"),
            ck_debugger_launcher_filter_spec::Get_TabIds(Visible),
            ck_debugger_launcher_filter_spec::Get_TabIds(Catalog));
    }

    {
        const auto Visible = ck::debugger_launcher::Filter_Tools(TEXT("   "), Catalog);
        TestEqual(TEXT("A whitespace-only query keeps the whole catalog"), Visible.Num(), Catalog.Num());
    }

    {
        // Matching is case-insensitive, and it reaches across every category rather than being
        // scoped to the section the user happens to be looking at.
        const auto Visible = ck::debugger_launcher::Filter_Tools(TEXT("dEbUgGeR"), Catalog);
        TestEqual(TEXT("A case-insensitive match on the shared suffix keeps every tool"),
            Visible.Num(), Catalog.Num());
    }

    {
        const auto Visible = ck::debugger_launcher::Filter_Tools(TEXT("crowd"), Catalog);
        TestEqual(TEXT("A single-tool query keeps exactly one entry"), Visible.Num(), 1);

        if (Visible.IsValidIndex(0))
        {
            TestEqual(TEXT("The single match is the expected tool"),
                Visible[0].Get_TabId(), FName{TEXT("CkCrowdDebugger")});
        }
    }

    {
        // Order is load-bearing twice over: category grouping stays contiguous, and element 0 is
        // the entry Enter activates.
        const auto Visible = ck::debugger_launcher::Filter_Tools(TEXT("c"), Catalog);
        const auto TabIds = ck_debugger_launcher_filter_spec::Get_TabIds(Visible);

        auto ExpectedTabIds = TArray<FName>{};
        for (const auto& Tool : Catalog)
        {
            if (Tool.Get_DisplayName().ToString().Contains(TEXT("c"), ESearchCase::IgnoreCase))
            { ExpectedTabIds.Add(Tool.Get_TabId()); }
        }

        TestEqual(TEXT("A partial query preserves catalog order"), TabIds, ExpectedTabIds);
    }

    {
        const auto Visible = ck::debugger_launcher::Filter_Tools(TEXT("nothing matches this"), Catalog);
        TestEqual(TEXT("A query with no match hides every tool"), Visible.Num(), 0);
    }

    {
        // Surrounding whitespace comes from the user's typing, not from the tool names.
        const auto Visible = ck::debugger_launcher::Filter_Tools(TEXT("  goap  "), Catalog);
        TestEqual(TEXT("A padded query is trimmed before matching"), Visible.Num(), 1);
    }

    {
        const auto Empty = TArray<FCkDebuggerToolDescriptor>{};
        const auto Visible = ck::debugger_launcher::Filter_Tools(TEXT("ecs"), Empty);
        TestEqual(TEXT("An empty catalog filters to nothing rather than misbehaving"), Visible.Num(), 0);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
