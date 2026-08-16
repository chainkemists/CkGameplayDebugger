#include "CkInsightsDebugger_Module.h"

#include "CkInsightsDebugger/Window/SCkInsightsAnalyzerTab.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkInsightsDebuggerModule"

const FName FCkInsightsDebuggerModule::_DebuggerTabName = FName{TEXT("CkInsightsAnalyzerTab")};

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdInsightsDebugger(
    TEXT("ck.InsightsAnalyzer"),
    TEXT("Opens (1) or closes (0) the CK Insights Analyzer. Usage: ck.InsightsAnalyzer [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkInsightsDebuggerModule::Get();

        if (InArgs.IsEmpty())
        {
            Module.ToggleDebugger();
            return;
        }

        const auto Value = FCString::Atoi(*InArgs[0]);
        if (Value == 1) { Module.OpenDebugger(); }
        else if (Value == 0) { Module.CloseDebugger(); }
        else { Module.ToggleDebugger(); }
    }));

// --------------------------------------------------------------------------------------------------------------------

auto FCkInsightsDebuggerModule::StartupModule() -> void
{
    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkInsightsDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(LOCTEXT("InsightsAnalyzerDisplayName", "Insights Analyzer"))
        .SetTooltipText(LOCTEXT("InsightsAnalyzerTooltip", "Open .utrace files and analyze frame performance"));

#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkInsightsDebugger"),
        _DebuggerTabName,
        LOCTEXT("InsightsAnalyzerLauncherName", "[CK] Insights Analyzer"),
        LOCTEXT("InsightsAnalyzerLauncherTooltip", "Open .utrace files and analyze frame performance"),
        TEXT("Hourglass"),
        ECkDebuggerToolCategory::Tools,
        10}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));
}

auto FCkInsightsDebuggerModule::ShutdownModule() -> void
{
    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;

    CloseDebugger();

    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName);
    }

    _DebuggerWindow.Reset();
    _DebuggerTab.Reset();
}

auto FCkInsightsDebuggerModule::Get() -> FCkInsightsDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkInsightsDebuggerModule>(TEXT("CkInsightsDebugger"));
}

auto FCkInsightsDebuggerModule::OpenDebugger() -> void
{
    ck::debugger_tabs::Invoke_DebuggerTab(_DebuggerTabName);
}

auto FCkInsightsDebuggerModule::CloseDebugger() -> void
{
    if (_DebuggerTab.IsValid())
    {
        if (NOT IsEngineExitRequested())
        { _DebuggerTab->RequestCloseTab(); }
        _DebuggerTab.Reset();
    }

    _DebuggerWindow.Reset();
}

auto FCkInsightsDebuggerModule::ToggleDebugger() -> void
{
    if (IsDebuggerOpen()) { CloseDebugger(); }
    else { OpenDebugger(); }
}

auto FCkInsightsDebuggerModule::IsDebuggerOpen() const -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto FCkInsightsDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkInsightsAnalyzerTab);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("InsightsAnalyzerTabLabel", "Insights Analyzer"))
        .ToolTipText(LOCTEXT("InsightsAnalyzerTabTooltip", "Analyze Unreal Insights .utrace files"))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            _DebuggerWindow.Reset();
            _DebuggerTab.Reset();
        })
        [
            _DebuggerWindow.ToSharedRef()
        ];

    return _DebuggerTab.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkInsightsDebuggerModule, CkInsightsDebugger)
