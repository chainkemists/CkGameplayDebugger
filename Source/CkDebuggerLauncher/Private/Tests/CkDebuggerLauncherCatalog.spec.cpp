#include "../../CkDebuggerLauncher_Module.h"

#include "Styles/CkDebuggerLauncherStyle.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "CkCore/Format/CkFormat.h"

#include "Framework/Docking/TabManager.h"
#include "Misc/AutomationTest.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerLauncherCatalog_AllDebuggersHaveLaunchableDescriptors,
    "Ck.DebuggerLauncher.Catalog.AllDebuggersHaveLaunchableDescriptors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

bool FCkDebuggerLauncherCatalog_AllDebuggersHaveLaunchableDescriptors::RunTest(const FString& Parameters)
{
    auto ExpectedTabIds = TSet<FName>{
        TEXT("CkEcsDebugger"),
        TEXT("CkSmDebugger"),
        TEXT("CkUIDebugger"),
        TEXT("CkSchedulerDebugger"),
        TEXT("CkAStarDebugger"),
        TEXT("CkGoapDebugger"),
        TEXT("CkCrowdDebugger"),
        TEXT("CkEqsDebugger"),
        TEXT("CkInputDebugger"),
        TEXT("CkObjectPoolingDebugger"),
        TEXT("CkInsightsAnalyzerTab"),
        TEXT("CkJoltDebugger"),
        TEXT("CkMapDebugger"),
        TEXT("CkDialogDebugger"),
    };

    const auto Tools = FCkDebuggerToolRegistry::Get().Get_Tools();
    auto SeenOrderSlots = TSet<FString>{};

    TestEqual(TEXT("Catalog contains every standalone debugger tab"), Tools.Num(), ExpectedTabIds.Num());
    TestTrue(TEXT("Launcher tab spawner is registered"),
        FGlobalTabmanager::Get()->HasTabSpawner(FCkDebuggerLauncherModule::LauncherTabName));

    for (const auto& Tool : Tools)
    {
        const auto TabId = Tool.Get_TabId();
        const auto ExpectedMessage = ck::Format_UE(TEXT("Expected tab id: {}"), TabId);
        const auto DisplayNameMessage = ck::Format_UE(TEXT("Display name present: {}"), TabId);
        const auto TooltipMessage = ck::Format_UE(TEXT("Tooltip present: {}"), TabId);
        const auto IconMessage = ck::Format_UE(TEXT("Icon id present: {}"), TabId);
        const auto CategoryMessage = ck::Format_UE(TEXT("Category present: {}"), TabId);
        const auto SpawnerMessage = ck::Format_UE(TEXT("Tab spawner present: {}"), TabId);
        const auto BrushMessage = ck::Format_UE(TEXT("Icon brush present: {}"), TabId);

        TestTrue(*ExpectedMessage, ExpectedTabIds.Remove(TabId) == 1);
        TestFalse(*DisplayNameMessage, Tool.Get_DisplayName().IsEmpty());
        TestFalse(*TooltipMessage, Tool.Get_Tooltip().IsEmpty());
        TestFalse(*IconMessage, Tool.Get_IconId().IsNone());
        TestTrue(*CategoryMessage,
            Tool.Get_Category() != ECkDebuggerToolCategory::Invalid);
        TestTrue(*SpawnerMessage,
            FGlobalTabmanager::Get()->HasTabSpawner(TabId));
        TestNotNull(*BrushMessage,
            FCkDebuggerLauncherStyle::Get_IconBrush(Tool.Get_IconId()));

        const auto OrderSlot = ck::Format_UE(
            TEXT("{}:{}"),
            static_cast<uint8>(Tool.Get_Category()),
            Tool.Get_SortOrder());
        const auto OrderMessage = ck::Format_UE(TEXT("Unique category/order slot: {}"), TabId);
        TestTrue(*OrderMessage,
            NOT SeenOrderSlots.Contains(OrderSlot));
        SeenOrderSlots.Add(OrderSlot);
    }

    TestEqual(TEXT("Every expected tab id was registered"), ExpectedTabIds.Num(), 0);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
