#include "../../CkDebuggerLauncher_Module.h"
#include "CkEditorTools/Style/CkIconStyle.h"

#include "Styles/CkDebuggerLauncherStyle.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "ModuleDescriptor.h"
#include "Widgets/Docking/SDockTab.h"

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
        TEXT("CkAiDebugger"),
        TEXT("CkCrowdDebugger"),
        TEXT("CkEqsDebugger"),
        TEXT("CkInputDebugger"),
        TEXT("CkIntentDebugger"),
        TEXT("CkObjectPoolingDebugger"),
        TEXT("CkInsightsAnalyzerTab"),
#if PLATFORM_WINDOWS
        TEXT("CkHangMonitor"),
#endif
        TEXT("CkJoltDebugger"),
        TEXT("CkJoltBakeInspector"),
        TEXT("CkMapDebugger"),
        TEXT("CkDialogDebugger"),
        TEXT("CkAggroDebugger"),
        TEXT("CkAudioDebugger"),
        TEXT("CkStyleLabDebugger"),
        TEXT("CkSaveDebugger"),
        TEXT("CkOptimizationDebugger"),
        TEXT("CkTextureDebugger"),
        TEXT("CkVisualLodDebugger"),
    };

    const auto Tools = FCkDebuggerToolRegistry::Get().Get_Tools();
    auto SeenOrderSlots = TSet<FString>{};

    TestEqual(TEXT("Catalog contains every standalone debugger tab"), Tools.Num(), ExpectedTabIds.Num());
    TestTrue(TEXT("Launcher tab spawner is registered"),
        FGlobalTabmanager::Get()->HasTabSpawner(FCkDebuggerLauncherModule::LauncherTabName));
    TestEqual(TEXT("Launcher module uses the Common launcher tab identity"),
        FCkDebuggerLauncherModule::LauncherTabName, ck::debugger_tabs::LauncherTabId);

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
        TestFalse(*IconMessage, Tool.Get_IconId() == ECk_Icon::None);
        TestTrue(*CategoryMessage,
            Tool.Get_Category() != ECkDebuggerToolCategory::Invalid);
        TestTrue(*SpawnerMessage,
            FGlobalTabmanager::Get()->HasTabSpawner(TabId));
        TestNotNull(*BrushMessage,
            FCkIconStyle::Get_Brush(Tool.Get_IconId(), ECk_Icon_BrushSize::Size_24x24));

        if (TabId == TEXT("CkInsightsAnalyzerTab"))
        {
            TestEqual(TEXT("Insights Analyzer descriptor is owned by its debugger module"),
                Tool.Get_OwnerModule(), FName{TEXT("CkInsightsDebugger")});
            TestEqual(TEXT("Insights Analyzer descriptor keeps its display name"),
                Tool.Get_DisplayName().ToString(), FString{TEXT("[CK] Insights Analyzer")});
            TestEqual(TEXT("Insights Analyzer descriptor keeps its tooltip"),
                Tool.Get_Tooltip().ToString(), FString{TEXT("Open .utrace files and analyze frame performance")});
            TestTrue(TEXT("Insights Analyzer descriptor keeps its icon"),
                Tool.Get_IconId() == ECk_Icon::Waiting);
            TestEqual(TEXT("Insights Analyzer descriptor stays in the Tools category"),
                Tool.Get_Category(), ECkDebuggerToolCategory::Tools);
            TestEqual(TEXT("Insights Analyzer descriptor keeps its launcher order"),
                Tool.Get_SortOrder(), 10);
        }

#if PLATFORM_WINDOWS
        if (TabId == TEXT("CkHangMonitor"))
        {
            TestEqual(TEXT("Hang Monitor descriptor is owned by its debugger module"),
                Tool.Get_OwnerModule(), FName{TEXT("CkHangMonitor")});
            TestEqual(TEXT("Hang Monitor descriptor keeps its display name"),
                Tool.Get_DisplayName().ToString(), FString{TEXT("[CK] Hang Monitor")});
            TestEqual(TEXT("Hang Monitor descriptor stays in the Tools category"),
                Tool.Get_Category(), ECkDebuggerToolCategory::Tools);
            TestEqual(TEXT("Hang Monitor descriptor keeps its launcher order"),
                Tool.Get_SortOrder(), 15);
        }
#endif

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerLauncherCatalog_AllCatalogTabsCloseAndReopen,
    "Ck.DebuggerLauncher.Catalog.AllCatalogTabsCloseAndReopen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

bool FCkDebuggerLauncherCatalog_AllCatalogTabsCloseAndReopen::RunTest(const FString& Parameters)
{
    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Production-tab lifecycle census requires Slate."));
        return false;
    }

    const TSharedRef<FGlobalTabmanager> TabManager = FGlobalTabmanager::Get();
    const FName HostTabId{TEXT("LevelEditor")};
    if (NOT TestTrue(TEXT("Lifecycle census has a stable Level Editor docking host"),
        TabManager->FindExistingLiveTab(FTabId{HostTabId}).IsValid()))
    { return false; }

    const auto Tools = FCkDebuggerToolRegistry::Get().Get_Tools();
    auto OpenedByTest = TSet<FName>{};
    const auto DrainSlate = []()
    {
        FSlateApplication::Get().PumpMessages();
        FSlateApplication::Get().Tick();
    };
    ON_SCOPE_EXIT
    {
        for (const auto TabId : OpenedByTest)
        {
            if (const auto LiveTab = TabManager->FindExistingLiveTab(FTabId{TabId}); LiveTab.IsValid())
            { LiveTab->RequestCloseTab(); }
        }
        DrainSlate();
    };

    TestEqual(TEXT("Lifecycle census covers every registered standalone debugger tab"), Tools.Num(), 25);
    for (const auto& Tool : Tools)
    {
        const auto TabId = Tool.Get_TabId();
        const auto FixtureTabId = FName{*ck::Format_UE(TEXT("CkLifecycleFixture_{}"), TabId)};
        const auto StartsClosedMessage = ck::Format_UE(
            TEXT("Isolated lifecycle fixture starts with no live tab: {}"),
            TabId);
        if (NOT TestFalse(*StartsClosedMessage, TabManager->FindExistingLiveTab(FTabId{FixtureTabId}).IsValid()))
        { continue; }

        const auto FactoryMessage = ck::Format_UE(TEXT("Catalog exposes a production tab factory: {}"), TabId);
        if (NOT TestTrue(*FactoryMessage, Tool.Get_TabFactory().IsBound()))
        { continue; }

        const TSharedRef<SDockTab> FirstTab = Tool.Get_TabFactory().Execute();
        TabManager->InsertNewDocumentTab(
            HostTabId, FixtureTabId, FTabManager::FLiveTabSearch{HostTabId}, FirstTab);
        const auto OpensMessage = ck::Format_UE(TEXT("Production factory opens tab in a live docking host: {}"), TabId);
        if (NOT TestTrue(*OpensMessage, TabManager->FindExistingLiveTab(FTabId{FixtureTabId}).IsValid()))
        { continue; }
        OpenedByTest.Add(FixtureTabId);

        const auto FirstParentMessage = ck::Format_UE(TEXT("First tab is attached to a production host: {}"), TabId);
        TestTrue(*FirstParentMessage, FirstTab->GetParent().IsValid());
        FirstTab->RequestCloseTab();
        DrainSlate();

        const auto ClosesMessage = ck::Format_UE(TEXT("Normal close removes live tab: {}"), TabId);
        if (NOT TestFalse(*ClosesMessage, TabManager->FindExistingLiveTab(FTabId{FixtureTabId}).IsValid()))
        { continue; }

        const TSharedRef<SDockTab> ReopenedTab = Tool.Get_TabFactory().Execute();
        TabManager->InsertNewDocumentTab(
            HostTabId, FixtureTabId, FTabManager::FLiveTabSearch{HostTabId}, ReopenedTab);
        const auto ReopensMessage = ck::Format_UE(TEXT("Production factory reopens a fresh tab: {}"), TabId);
        TestTrue(*ReopensMessage, TabManager->FindExistingLiveTab(FTabId{FixtureTabId}).IsValid());

        const auto ReopenedParentMessage = ck::Format_UE(TEXT("Reopened tab is attached to a production host: {}"), TabId);
        TestTrue(*ReopenedParentMessage, ReopenedTab->GetParent().IsValid());
        ReopenedTab->RequestCloseTab();
        DrainSlate();

        const auto FinalCloseMessage = ck::Format_UE(TEXT("Reopened tab closes without a stale live entry: {}"), TabId);
        if (TestFalse(*FinalCloseMessage, TabManager->FindExistingLiveTab(FTabId{FixtureTabId}).IsValid()))
        { OpenedByTest.Remove(FixtureTabId); }
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerLauncherPackaging_DevToolDescriptorsIncludeDevelopmentCookedWin64,
    "Ck.DebuggerLauncher.Packaging.DevToolDescriptorsIncludeDevelopmentCookedWin64",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

bool FCkDebuggerLauncherPackaging_DevToolDescriptorsIncludeDevelopmentCookedWin64::RunTest(const FString& Parameters)
{
    const auto TestModule = [this](const TCHAR* InPluginName, const TCHAR* InModuleName)
    {
        const auto Plugin = IPluginManager::Get().FindPlugin(InPluginName);
        const auto PluginIsValid = ck::IsValid(Plugin);
        const auto PluginMessage = ck::Format_UE(TEXT("Plugin is discoverable: {}"), InPluginName);
        TestTrue(*PluginMessage, PluginIsValid);

        if (NOT PluginIsValid)
        {
            return static_cast<const FModuleDescriptor*>(nullptr);
        }

        const auto ModuleName = FName{InModuleName};
        const auto& Modules = Plugin->GetDescriptor().Modules;
        const auto* Module = Modules.FindByPredicate([ModuleName](const FModuleDescriptor& Candidate)
        {
            return Candidate.Name == ModuleName;
        });
        const auto ModuleMessage = ck::Format_UE(
            TEXT("Module descriptor is present: {}:{}"),
            InPluginName,
            ModuleName);
        TestNotNull(*ModuleMessage, Module);
        return Module;
    };

    const auto TestExpectedModule = [&TestModule, this](const TCHAR* InPluginName, const TCHAR* InModuleName)
    {
        const auto* Module = TestModule(InPluginName, InModuleName);
        const auto ModuleIsValid = ck::IsValid(Module, ck::IsValid_Policy_NullptrOnly{});
        if (NOT ModuleIsValid)
        {
            return;
        }

        const auto Prefix = ck::Format_UE(TEXT("{}:{}"), InPluginName, InModuleName);
        TestEqual(*ck::Format_UE(TEXT("{} is a DeveloperTool module"), Prefix),
            Module->Type, EHostType::DeveloperTool);
        TestTrue(*ck::Format_UE(TEXT("{} descriptor includes Win64 Game Development with developer tools and cooked data"), Prefix),
            Module->IsCompiledInConfiguration(
                TEXT("Win64"),
                EBuildConfiguration::Development,
                TEXT("BusterBlock"),
                EBuildTargetType::Game,
                true,
                true));
        TestTrue(*ck::Format_UE(TEXT("{} descriptor includes Win64 Game DebugGame with developer tools and cooked data"), Prefix),
            Module->IsCompiledInConfiguration(
                TEXT("Win64"),
                EBuildConfiguration::DebugGame,
                TEXT("BusterBlock"),
                EBuildTargetType::Game,
                true,
                true));
        TestFalse(*ck::Format_UE(TEXT("{} is excluded from Win64 Game Shipping"), Prefix),
            Module->IsCompiledInConfiguration(
                TEXT("Win64"),
                EBuildConfiguration::Shipping,
                TEXT("BusterBlock"),
                EBuildTargetType::Game,
                false,
                true));
        TestFalse(*ck::Format_UE(TEXT("{} is excluded from Win64 Game Test"), Prefix),
            Module->IsCompiledInConfiguration(
                TEXT("Win64"),
                EBuildConfiguration::Test,
                TEXT("BusterBlock"),
                EBuildTargetType::Game,
                false,
                true));
    };

    TestExpectedModule(TEXT("CkFoundation"), TEXT("CkInsightsAnalyzer"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkEcsDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkSmDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkDialogDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkAudioDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkAggroDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkUIDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkSchedulerDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkAStarDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkGoapDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkAiDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkCrowdDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkEqsDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkInputDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkIntentDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkObjectPoolingDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkJoltDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkMapDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkStyleLabDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkSaveDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkOptimizationDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkTextureDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkVisualLodDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkInsightsDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkHangMonitor"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkDebuggerLauncher"));

    const auto* HangMonitor = TestModule(TEXT("CkDebugger"), TEXT("CkHangMonitor"));
    if (ck::IsValid(HangMonitor, ck::IsValid_Policy_NullptrOnly{}))
    {
        TestFalse(TEXT("CkHangMonitor remains excluded from Shipping when developer tools are forced on"),
            HangMonitor->IsCompiledInConfiguration(TEXT("Win64"), EBuildConfiguration::Shipping, TEXT("BusterBlock"), EBuildTargetType::Game, true, true));
        TestFalse(TEXT("CkHangMonitor remains excluded from Test when developer tools are forced on"),
            HangMonitor->IsCompiledInConfiguration(TEXT("Win64"), EBuildConfiguration::Test, TEXT("BusterBlock"), EBuildTargetType::Game, true, true));
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
