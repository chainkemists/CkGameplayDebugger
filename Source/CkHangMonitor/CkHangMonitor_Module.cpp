#include "CkHangMonitor_Module.h"

#include "CkHangMonitor/CkHangMonitorController.h"
#include "CkHangMonitor/Window/SCkHangMonitor.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "Framework/Docking/TabManager.h"
#include "Misc/CoreDelegates.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SNullWidget.h"

#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkHangMonitorModule"

const FName FCkHangMonitorModule::_DebuggerTabName = TEXT("CkHangMonitor");

static FAutoConsoleCommand CmdHangMonitor(
    TEXT("ck.HangMonitor"),
    TEXT("Opens (1) or closes (0) the CK Hang Monitor. Usage: ck.HangMonitor [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkHangMonitorModule::Get();
        if (InArgs.IsEmpty()) { Module.ToggleDebugger(); return; }

        const auto Value = FCString::Atoi(*InArgs[0]);
        if (Value == 1) { Module.OpenDebugger(); }
        else if (Value == 0) { Module.CloseDebugger(); }
        else { Module.ToggleDebugger(); }
    }));

auto FCkHangMonitorModule::StartupModule() -> void
{
    _Controller = MakeUnique<FCkHangMonitorController>();
    _Controller->Initialize();

    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName, FOnSpawnTab::CreateRaw(this, &FCkHangMonitorModule::OnSpawnDebuggerTab))
        .SetDisplayName(LOCTEXT("HangMonitorDisplayName", "Hang Monitor"))
        .SetTooltipText(LOCTEXT("HangMonitorTooltip", "Arm ProcDump for one unresponsive current-game process."));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkHangMonitor"), _DebuggerTabName,
        LOCTEXT("HangMonitorLauncherName", "[CK] Hang Monitor"),
        LOCTEXT("HangMonitorLauncherTooltip", "Arm ProcDump for an unresponsive current-game process."),
        ECk_Icon::Waiting, ECkDebuggerToolCategory::Tools, 15}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));

    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCkHangMonitorModule::OnEnginePreExit);
}

auto FCkHangMonitorModule::ShutdownModule() -> void
{
    if (_EnginePreExitHandle.IsValid()) { FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle); _EnginePreExitHandle.Reset(); }
    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;
    if (_DebuggerTab.IsValid())
    {
        _DebuggerTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback{});
        _DebuggerTab->SetContent(SNullWidget::NullWidget);
    }
    CloseDebugger();
    if (_Controller) { _Controller->Shutdown(); _Controller.Reset(); }
    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    { FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName); }
}

auto FCkHangMonitorModule::Get() -> FCkHangMonitorModule&
{ return FModuleManager::GetModuleChecked<FCkHangMonitorModule>(TEXT("CkHangMonitor")); }

auto FCkHangMonitorModule::OpenDebugger() -> void
{ ck::debugger_tabs::Invoke_DebuggerTab(_DebuggerTabName); }

auto FCkHangMonitorModule::CloseDebugger() -> void
{
    if (_DebuggerTab.IsValid() && NOT IsEngineExitRequested()) { _DebuggerTab->RequestCloseTab(); }
    _DebuggerTab.Reset();
}

auto FCkHangMonitorModule::ToggleDebugger() -> void
{ if (IsDebuggerOpen()) { CloseDebugger(); } else { OpenDebugger(); } }

auto FCkHangMonitorModule::IsDebuggerOpen() const -> bool
{ return _DebuggerTab.IsValid(); }

auto FCkHangMonitorModule::Get_Controller() -> FCkHangMonitorController&
{ check(_Controller); return *_Controller; }

auto FCkHangMonitorModule::OnSpawnDebuggerTab(const FSpawnTabArgs&) -> TSharedRef<SDockTab>
{
    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("HangMonitorTabLabel", "Hang Monitor"))
        .ToolTipText(LOCTEXT("HangMonitorTabTooltip", "ProcDump hang capture monitor"))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>) { _DebuggerTab.Reset(); })
        [ SNew(SCkHangMonitor).Controller(&Get_Controller()) ];
    return _DebuggerTab.ToSharedRef();
}

auto FCkHangMonitorModule::OnEnginePreExit() -> void
{
    // Tab content binds directly to the module-owned controller. During engine exit,
    // Slate may retain the tab past this callback, so detach it before controller teardown.
    if (_DebuggerTab.IsValid())
    {
        _DebuggerTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback{});
        _DebuggerTab->SetContent(SNullWidget::NullWidget);
    }
    _DebuggerTab.Reset();
    if (_Controller) { _Controller->Shutdown(); }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkHangMonitorModule, CkHangMonitor)
