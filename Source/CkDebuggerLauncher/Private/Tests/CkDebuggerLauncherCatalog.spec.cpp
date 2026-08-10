#include "../../CkDebuggerLauncher_Module.h"

#include "Styles/CkDebuggerLauncherStyle.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "Framework/Docking/TabManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "ModuleDescriptor.h"

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
        TEXT("CkIntentDebugger"),
        TEXT("CkObjectPoolingDebugger"),
        TEXT("CkInsightsAnalyzerTab"),
        TEXT("CkJoltDebugger"),
        TEXT("CkMapDebugger"),
        TEXT("CkDialogDebugger"),
        TEXT("CkAggroDebugger"),
        TEXT("CkStyleLabDebugger"),
        TEXT("CkSaveDebugger"),
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

        if (TabId == TEXT("CkInsightsAnalyzerTab"))
        {
            TestEqual(TEXT("Insights Analyzer descriptor is owned by its debugger module"),
                Tool.Get_OwnerModule(), FName{TEXT("CkInsightsDebugger")});
            TestEqual(TEXT("Insights Analyzer descriptor keeps its display name"),
                Tool.Get_DisplayName().ToString(), FString{TEXT("Insights Analyzer")});
            TestEqual(TEXT("Insights Analyzer descriptor keeps its tooltip"),
                Tool.Get_Tooltip().ToString(), FString{TEXT("Open .utrace files and analyze frame performance")});
            TestEqual(TEXT("Insights Analyzer descriptor keeps its icon"),
                Tool.Get_IconId(), FName{TEXT("Hourglass")});
            TestEqual(TEXT("Insights Analyzer descriptor stays in the Tools category"),
                Tool.Get_Category(), ECkDebuggerToolCategory::Tools);
            TestEqual(TEXT("Insights Analyzer descriptor keeps its launcher order"),
                Tool.Get_SortOrder(), 10);
        }

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
    FCkDebuggerLauncherPackaging_DevToolsCompileIntoDevelopmentCookedWin64,
    "Ck.DebuggerLauncher.Packaging.DevToolsCompileIntoDevelopmentCookedWin64",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

bool FCkDebuggerLauncherPackaging_DevToolsCompileIntoDevelopmentCookedWin64::RunTest(const FString& Parameters)
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
        TestTrue(*ck::Format_UE(TEXT("{} compiles for Win64 Game Development with developer tools and cooked data"), Prefix),
            Module->IsCompiledInConfiguration(
                TEXT("Win64"),
                EBuildConfiguration::Development,
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

    const auto TestExpectedEditorOnlyModule = [&TestModule, this](const TCHAR* InModuleName)
    {
        const auto* Module = TestModule(TEXT("CkDebugger"), InModuleName);
        const auto ModuleIsValid = ck::IsValid(Module, ck::IsValid_Policy_NullptrOnly{});
        if (NOT ModuleIsValid)
        { return; }

        const auto Prefix = ck::Format_UE(TEXT("CkDebugger:{}"), InModuleName);
        TestEqual(*ck::Format_UE(TEXT("{} remains an UncookedOnly graph tool"), Prefix),
            Module->Type, EHostType::UncookedOnly);
        TestFalse(*ck::Format_UE(TEXT("{} is excluded from Win64 Game Development cooked builds"), Prefix),
            Module->IsCompiledInConfiguration(
                TEXT("Win64"),
                EBuildConfiguration::Development,
                TEXT("BusterBlock"),
                EBuildTargetType::Game,
                true,
                true));
    };

    TestExpectedModule(TEXT("CkFoundation"), TEXT("CkInsightsAnalyzer"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkDialogDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkAggroDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkUIDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkAStarDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkCrowdDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkEqsDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkInputDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkObjectPoolingDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkJoltDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkMapDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkStyleLabDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkInsightsDebugger"));
    TestExpectedModule(TEXT("CkDebugger"), TEXT("CkDebuggerLauncher"));
    TestExpectedEditorOnlyModule(TEXT("CkEcsDebugger"));
    TestExpectedEditorOnlyModule(TEXT("CkSmDebugger"));
    TestExpectedEditorOnlyModule(TEXT("CkSchedulerDebugger"));
    TestExpectedEditorOnlyModule(TEXT("CkGoapDebugger"));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
