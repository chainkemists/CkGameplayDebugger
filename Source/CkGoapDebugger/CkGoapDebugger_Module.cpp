#include "CkGoapDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"
#include "CkGoapDebugger/Data/CkGoapDebugger_Targeting.h"
#if WITH_EDITOR
    #include "CkGoapDebugger/Graph/CkGoapDebugGraphFactory.h"
#endif
#include "CkGoapDebugger/Inspector/CkGoapInspector_Gateway.h"
#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"


#if WITH_EDITOR
    #include "EdGraphUtilities.h"
#endif
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

// ====================================================================================================================

#define LOCTEXT_NAMESPACE "FCkGoapDebuggerModule"

const FName FCkGoapDebuggerModule::_DebuggerTabName = FName("CkGoapDebugger");

// Inspector ID used to register / unregister the CkEcsDebugger gateway. Kept
// in a single named constant so Startup/Shutdown can't drift.
static const FName GGoapInspectorID = FName(TEXT("FCkGoapInspector_Gateway"));

// Roster predicate + lineage resolver moved to CkGoapDebugger_Targeting.h —
// shared with the window's specialized viewport picker.

// ====================================================================================================================

static FAutoConsoleCommand CmdGoapDebugger(
    TEXT("ck.GoapDebugger"),
    TEXT("Opens (1) or closes (0) the CK GOAP Debugger. Usage: ck.GoapDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkGoapDebuggerModule::Get();

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

// ====================================================================================================================

auto
    FCkGoapDebuggerModule::
    StartupModule()
    -> void
{
    FCkGoapDebuggerStyle::Initialize();
    FCkGoapDebugger_DataCollector::Initialize();

#if WITH_EDITOR
    _NodeFactory = MakeShared<FCkGoapDebugGraphFactory>();
    FEdGraphUtilities::RegisterVisualNodeFactory(_NodeFactory);
#endif

    // Explicit inspector registration. The CK_REGISTER_DEBUGGER_INSPECTOR macro
    // performs the same registration at file-scope static init, but that
    // pattern is fragile when the registry singleton lives in a sibling DLL
    // (CkEcsDebugger) — cross-DLL static-init ordering has tripped DebugGame
    // launches in this project's history. Doing it explicitly here also makes
    // the Unregister call in ShutdownModule symmetric.
    FCkDebuggerInspectorRegistry::Get().Register(
        GGoapInspectorID,
        []() -> TSharedPtr<ICkDebuggerComponentInspector_Base>
        {
            return MakeShared<FCkGoapInspector_Gateway>();
        });

    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkGoapDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK GOAP Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK GOAP Debugger window")));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkGoapDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("CK GOAP Debugger")),
        FText::FromString(TEXT("Inspect GOAP planning, world state, actions, and plan history")),
        TEXT("Goap"),
        ECkDebuggerToolCategory::Ai,
        20}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));

    _EntityTargetRouteRegistrationId = FCkDebug_EntityTargetRegistry::Get().Register(FCkDebug_EntityTargetRoute{
        TEXT("CkGoapDebugger"), _DebuggerTabName,
        [](const FCk_Handle& InEntity) { return ck::IsValid(ck_goap_debugger::ResolveGoapTarget(InEntity)); },
        [](const FCk_Handle& InEntity) { SCkGoapDebuggerWindow::OpenForEntity(ck_goap_debugger::ResolveGoapTarget(InEntity)); }});
}

auto
    FCkGoapDebuggerModule::
    ShutdownModule()
    -> void
{
    FCkDebug_EntityTargetRegistry::Get().Unregister(_DebuggerTabName, _EntityTargetRouteRegistrationId);
    _EntityTargetRouteRegistrationId = 0;

    FCkDebuggerToolRegistry::Get().Unregister(_DebuggerTabName, _DebuggerToolRegistrationId);
    _DebuggerToolRegistrationId = 0;

    FCkDebuggerInspectorRegistry::Get().Unregister(GGoapInspectorID);

    if (FGlobalTabmanager::Get()->HasTabSpawner(_DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(_DebuggerTabName);
    }

    _DebuggerWindow.Reset();
    _DebuggerTab.Reset();

#if WITH_EDITOR
    if (_NodeFactory.IsValid())
    {
        FEdGraphUtilities::UnregisterVisualNodeFactory(_NodeFactory);
        _NodeFactory.Reset();
    }
#endif

    FCkGoapDebugger_DataCollector::Shutdown();
    FCkGoapDebuggerStyle::Shutdown();
}

// ====================================================================================================================

auto
    FCkGoapDebuggerModule::
    OpenDebugger()
    -> void
{
    ck::debugger_tabs::Invoke_DebuggerTab(_DebuggerTabName);
}

auto
    FCkGoapDebuggerModule::
    CloseDebugger()
    -> void
{
    if (_DebuggerTab.IsValid())
    {
        // Engine shutdown destroys Slate windows BEFORE module unload — by then
        // the tab's TSharedFromThis backing is gone and RequestCloseTab →
        // SharedThis(this) trips the AsShared check. Just drop the ref on exit.
        if (NOT IsEngineExitRequested())
        { _DebuggerTab->RequestCloseTab(); }
        _DebuggerTab.Reset();
    }
    _DebuggerWindow.Reset();
}

auto
    FCkGoapDebuggerModule::
    ToggleDebugger()
    -> void
{
    if (IsDebuggerOpen())
    { CloseDebugger(); }
    else
    { OpenDebugger(); }
}

auto
    FCkGoapDebuggerModule::
    IsDebuggerOpen() const
    -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto
    FCkGoapDebuggerModule::
    OnSpawnDebuggerTab(
        const FSpawnTabArgs& InArgs)
    -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkGoapDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK GOAP Debugger")))
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

// ====================================================================================================================

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCkGoapDebuggerModule, CkGoapDebugger)
