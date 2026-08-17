#include "CkIntentDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkIntentDebugger/Window/SCkIntentDebuggerWindow.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"


#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

#define LOCTEXT_NAMESPACE "FCkIntentDebuggerModule"

const FName FCkIntentDebuggerModule::_DebuggerTabName = FName("CkIntentDebugger");

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_module
{
    // The roster predicate lives on the window (SCkIntentDebuggerWindow::Is_IntentDebuggerEntity)
    // so the viewport picker and this route resolve the same real target.
    auto
        Resolve_IntentTarget(
            const FCk_Handle& InSelected)
        -> FCk_Handle
    {
        return ck::DebugSelectionSync::Resolve_ClosestLineageMatch(InSelected,
            [](const FCk_Handle& InCandidate)
            { return SCkIntentDebuggerWindow::Is_IntentDebuggerEntity(InCandidate); });
    }
}

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdIntentDebugger(
    TEXT("ck.IntentDebugger"),
    TEXT("Opens (1) or closes (0) the CK Intent Debugger. Usage: ck.IntentDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkIntentDebuggerModule::Get();

        if (InArgs.IsEmpty())
        {
            Module.ToggleDebugger();
            return;
        }

        const auto Value = FCString::Atoi(*InArgs[0]);

        if (Value == 1) { Module.OpenDebugger(); }
        else if (Value == 0) { Module.CloseDebugger(); }
        else { Module.ToggleDebugger(); }
    })
);

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebuggerModule::
    StartupModule()
    -> void
{
    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkIntentDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK Intent Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Intent Debugger window")));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkIntentDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("[CK] Intent Debugger")),
        FText::FromString(TEXT("Inspect the frame record, layer stack, resolution tables, and near-missed intents")),
        TEXT("Crosshair"),
        ECkDebuggerToolCategory::Interface,
        30}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));

    _EntityTargetRouteRegistrationId = FCkDebug_EntityTargetRegistry::Get().Register(FCkDebug_EntityTargetRoute{
        TEXT("CkIntentDebugger"), _DebuggerTabName,
        [](const FCk_Handle& InEntity)
        { return ck::IsValid(ck_intent_debugger_module::Resolve_IntentTarget(InEntity)); },
        [](const FCk_Handle& InEntity)
        { SCkIntentDebuggerWindow::OpenForEntity(ck_intent_debugger_module::Resolve_IntentTarget(InEntity)); }});

    // The window and its ViewModel hold FCk_Handle values. By ShutdownModule the registry's shared state is already
    // freed and ~FCk_Handle would release a dangling reference — so the tree goes down here, while it is still alive.
    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(
        this, &FCkIntentDebuggerModule::HandleEnginePreExit);
}

auto
    FCkIntentDebuggerModule::
    ShutdownModule()
    -> void
{
    if (_EnginePreExitHandle.IsValid())
    {
        FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle);
        _EnginePreExitHandle.Reset();
    }

    FCkDebug_EntityTargetRegistry::Get().Unregister(_DebuggerTabName, _EntityTargetRouteRegistrationId);
    _EntityTargetRouteRegistrationId = 0;

    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;

    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName);
    }

    _DebuggerWindow.Reset();
    _DebuggerTab.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebuggerModule::
    Get()
    -> FCkIntentDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkIntentDebuggerModule>("CkIntentDebugger");
}

auto
    FCkIntentDebuggerModule::
    OpenDebugger()
    -> void
{
    ck::debugger_tabs::Invoke_DebuggerTab(_DebuggerTabName);
}

auto
    FCkIntentDebuggerModule::
    CloseDebugger()
    -> void
{
    if (_DebuggerTab.IsValid())
    {
        // Engine shutdown destroys Slate windows BEFORE module unload — by then the tab's TSharedFromThis backing
        // is gone and RequestCloseTab -> SharedThis(this) trips the AsShared check. Just drop the ref on exit.
        if (NOT IsEngineExitRequested())
        { _DebuggerTab->RequestCloseTab(); }

        _DebuggerTab.Reset();
    }

    _DebuggerWindow.Reset();
}

auto
    FCkIntentDebuggerModule::
    ToggleDebugger()
    -> void
{
    if (IsDebuggerOpen())
    { CloseDebugger(); }
    else
    { OpenDebugger(); }
}

auto
    FCkIntentDebuggerModule::
    IsDebuggerOpen() const
    -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebuggerModule::
    HandleEnginePreExit()
    -> void
{
    // Deliberately no RequestCloseTab: the editor's window teardown runs before UEngine::PreExit, so the tab's
    // weak-self can already be cleared. Dropping the refs is what releases the handle-bearing tree in time.
    _DebuggerTab.Reset();
    _DebuggerWindow.Reset();
}

auto
    FCkIntentDebuggerModule::
    OnSpawnDebuggerTab(
        const FSpawnTabArgs& InArgs)
    -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkIntentDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK Intent")))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            _DebuggerWindow.Reset();
            _DebuggerTab.Reset();
        })
        [
            _DebuggerWindow.ToSharedRef()
        ];

    _DebuggerWindow->Set_OwningTab(_DebuggerTab);

    return _DebuggerTab.ToSharedRef();
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkIntentDebuggerModule, CkIntentDebugger)
