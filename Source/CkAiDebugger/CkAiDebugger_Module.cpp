#include "CkAiDebugger_Module.h"

#include "CkAiDebugger/Window/SCkAiDebuggerWindow.h"

#include "CkCore/Ensure/CkEnsure.h"
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
    const auto TabIdIsAvailable = NOT FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName);
    CK_ENSURE_IF_NOT(TabIdIsAvailable,
        TEXT("AI Overview cannot register duplicate tab id [{}]"),
        _DebuggerTabName)
    {}
    if (NOT TabIdIsAvailable)
    { return; }

    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName, FOnSpawnTab::CreateRaw(this, &FCkAiDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(LOCTEXT("AiOverviewTab", "CK AI Overview"))
        .SetTooltipText(LOCTEXT("AiOverviewTooltip", "Inspect the selected NPC's decision, state, crowd, and motion evidence."));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif
    _OwnsTabSpawner = true;

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkAiDebugger"), _DebuggerTabName,
        LOCTEXT("AiOverviewLauncher", "[CK] AI Overview"),
        LOCTEXT("AiOverviewLauncherTip", "A concise selected-agent view linking GOAP, State Machine, and Crowd evidence."),
        ECk_Icon::Diagnostics, ECkDebuggerToolCategory::Ai, 5}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));
    const auto ToolRegistrationSucceeded = _DebuggerToolRegistrationId != 0;
    CK_ENSURE_IF_NOT(ToolRegistrationSucceeded,
        TEXT("AI Overview failed to register its debugger-tool descriptor"))
    {}
    if (NOT ToolRegistrationSucceeded)
    {
        RollbackStartupRegistrations();
        return;
    }

    _EntityTargetRouteRegistrationId = FCkDebug_EntityTargetRegistry::Get().Register(FCkDebug_EntityTargetRoute{
        TEXT("CkAiDebugger"), _DebuggerTabName,
        [](const FCk_Handle& InEntity) { return SCkAiDebuggerWindow::Is_AiEntity(InEntity); },
        [](const FCk_Handle& InEntity) { SCkAiDebuggerWindow::OpenForEntity(InEntity); }});
    const auto TargetRouteRegistrationSucceeded = _EntityTargetRouteRegistrationId != 0;
    CK_ENSURE_IF_NOT(TargetRouteRegistrationSucceeded,
        TEXT("AI Overview failed to register its entity-target route"))
    {}
    if (NOT TargetRouteRegistrationSucceeded)
    {
        RollbackStartupRegistrations();
        return;
    }

    _SelectionSyncHandle = ck::DebugSelectionSync::Get_OnSelection().AddRaw(
        this, &FCkAiDebuggerModule::HandleGlobalSelection);
    const auto SelectionRegistrationSucceeded = _SelectionSyncHandle.IsValid();
    CK_ENSURE_IF_NOT(SelectionRegistrationSucceeded,
        TEXT("AI Overview failed to register its selection-sync receiver"))
    {}
    if (NOT SelectionRegistrationSucceeded)
    {
        RollbackStartupRegistrations();
        return;
    }

    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCkAiDebuggerModule::HandleEnginePreExit);
    const auto PreExitRegistrationSucceeded = _EnginePreExitHandle.IsValid();
    CK_ENSURE_IF_NOT(PreExitRegistrationSucceeded,
        TEXT("AI Overview failed to register its engine pre-exit teardown"))
    {}
    if (NOT PreExitRegistrationSucceeded)
    { RollbackStartupRegistrations(); }
}

auto FCkAiDebuggerModule::ShutdownModule() -> void
{
    // Close and detach the handle-bearing Slate tree while this module and the debug registries are still live.
    ck::debugger_tabs::Release_DebuggerTab(_DebuggerTab, true);
    _DebuggerWindow.Reset();
    RollbackStartupRegistrations();
}

auto FCkAiDebuggerModule::RollbackStartupRegistrations() -> void
{
    if (_EnginePreExitHandle.IsValid())
    {
        FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle);
        _EnginePreExitHandle.Reset();
    }
    if (_SelectionSyncHandle.IsValid())
    {
        ck::DebugSelectionSync::Get_OnSelection().Remove(_SelectionSyncHandle);
        _SelectionSyncHandle.Reset();
    }

    FCkDebug_EntityTargetRegistry::Get().Unregister(_DebuggerTabName, _EntityTargetRouteRegistrationId);
    _EntityTargetRouteRegistrationId = 0;
    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;

    if (_OwnsTabSpawner)
    {
        const auto OwnedSpawnerStillExists = FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName);
        CK_ENSURE_IF_NOT(OwnedSpawnerStillExists,
            TEXT("AI Overview lost the tab spawner it registered before rollback"))
        {}
        if (OwnedSpawnerStillExists)
        { FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName); }
        _OwnsTabSpawner = false;
    }
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
