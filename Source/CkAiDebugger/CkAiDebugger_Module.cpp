#include "CkAiDebugger_Module.h"

#include "CkAiDebugger/Window/SCkAiDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkAiDebuggerModule"

const FName FCkAiDebuggerModule::_DebuggerTabName{TEXT("CkAiDebugger")};

static FAutoConsoleCommand CmdAiDebugger(
    TEXT("ck.AiDebugger"),
    TEXT("Opens (1) or closes (0) the CK AI Overview. Usage: ck.AiDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkAiDebuggerModule::Get();
        if (InArgs.IsEmpty()) { Module.ToggleDebugger(); return; }
        FCString::Atoi(*InArgs[0]) == 0 ? Module.CloseDebugger() : Module.OpenDebugger();
    }));

auto FCkAiDebuggerModule::StartupModule() -> void
{
    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName, FOnSpawnTab::CreateRaw(this, &FCkAiDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(LOCTEXT("AiOverviewTab", "CK AI Overview"))
        .SetTooltipText(LOCTEXT("AiOverviewTooltip", "Inspect the selected NPC's decision, state, crowd, and motion evidence."));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkAiDebugger"), _DebuggerTabName,
        LOCTEXT("AiOverviewLauncher", "[CK] AI Overview"),
        LOCTEXT("AiOverviewLauncherTip", "A concise selected-agent view linking GOAP, State Machine, and Crowd evidence."),
        ECk_Icon::Diagnostics, ECkDebuggerToolCategory::Ai, 5}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));

    _EntityTargetRouteRegistrationId = FCkDebug_EntityTargetRegistry::Get().Register(FCkDebug_EntityTargetRoute{
        TEXT("CkAiDebugger"), _DebuggerTabName,
        [](const FCk_Handle& InEntity) { return SCkAiDebuggerWindow::Is_AiEntity(InEntity); },
        [](const FCk_Handle& InEntity) { SCkAiDebuggerWindow::OpenForEntity(InEntity); }});

    _SelectionSyncHandle = ck::DebugSelectionSync::Get_OnSelection().AddRaw(
        this, &FCkAiDebuggerModule::HandleGlobalSelection);
    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCkAiDebuggerModule::HandleEnginePreExit);
}

auto FCkAiDebuggerModule::ShutdownModule() -> void
{
    // Close and detach the handle-bearing Slate tree while this module and the debug registries are still live.
    ck::debugger_tabs::Release_DebuggerTab(_DebuggerTab, true);
    _DebuggerWindow.Reset();
    if (_EnginePreExitHandle.IsValid()) { FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle); }
    if (_SelectionSyncHandle.IsValid()) { ck::DebugSelectionSync::Get_OnSelection().Remove(_SelectionSyncHandle); }
    FCkDebug_EntityTargetRegistry::Get().Unregister(_DebuggerTabName, _EntityTargetRouteRegistrationId);
    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    { FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName); }
}

auto FCkAiDebuggerModule::Get() -> FCkAiDebuggerModule&
{ return FModuleManager::GetModuleChecked<FCkAiDebuggerModule>(TEXT("CkAiDebugger")); }

auto FCkAiDebuggerModule::OpenDebugger() -> void
{ ck::debugger_tabs::Invoke_DebuggerTab(_DebuggerTabName); }

auto FCkAiDebuggerModule::CloseDebugger() -> void
{
    ck::debugger_tabs::Release_DebuggerTab(_DebuggerTab, true);
    _DebuggerWindow.Reset();
}

auto FCkAiDebuggerModule::ToggleDebugger() -> void
{ IsDebuggerOpen() ? CloseDebugger() : OpenDebugger(); }

auto FCkAiDebuggerModule::IsDebuggerOpen() const -> bool
{ return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid(); }

auto FCkAiDebuggerModule::HandleEnginePreExit() -> void
{
    // Window teardown has started, so detach without RequestCloseTab/SharedThis.
    ck::debugger_tabs::Release_DebuggerTab(_DebuggerTab, false);
    _DebuggerWindow.Reset();
}

auto FCkAiDebuggerModule::HandleGlobalSelection(const FCk_Handle& InEntity, FName InSource) -> void
{
    if (InSource != _DebuggerTabName && _DebuggerWindow.IsValid() && SCkAiDebuggerWindow::Is_AiEntity(InEntity))
    { _DebuggerWindow->Select_Entity(InEntity, false); }
}

auto FCkAiDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs&) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkAiDebuggerWindow);
    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("AiOverviewLabel", "CK AI Overview"))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>) { _DebuggerWindow.Reset(); _DebuggerTab.Reset(); })
        [_DebuggerWindow.ToSharedRef()];
    _DebuggerWindow->Set_OwningTab(_DebuggerTab);
    return _DebuggerTab.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkAiDebuggerModule, CkAiDebugger)
