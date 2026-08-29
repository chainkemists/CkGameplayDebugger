#include "CkVisualLodDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkVisualLodDebugger/Window/SCkVisualLodDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkVisualLodDebuggerModule"

const FName FCkVisualLodDebuggerModule::_DebuggerTabName = FName("CkVisualLodDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdVisualLodDebugger(
    TEXT("ck.VisualLodDebugger"),
    TEXT("Opens (1) or closes (0) the CK Visual LOD Debugger. Usage: ck.VisualLodDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkVisualLodDebuggerModule::Get();

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

auto FCkVisualLodDebuggerModule::StartupModule() -> void
{
    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkVisualLodDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK Visual LOD Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Visual LOD Debugger window")));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkVisualLodDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("Visual LOD")),
        FText::FromString(TEXT("Inspect visual LOD arbitration — budgets, crowd pools, promotions, and per-member representation")),
        // The arbiter's whole job is choosing between a per-entity SKMC proxy and an instanced batched
        // crowd, so the instanced-renderer glyph is the one that already means that in this suite. It is
        // not claimed by any other tool (Crowd is CkCrowdDebugger's).
        ECk_Icon::IsmRenderer,
        // Systems/50 — Systems already uses 10 (Scheduler), 20 (ObjectPooling), 30 (Jolt), 40 (Audio). The
        // launcher census spec asserts every category/order slot is unique, so this must stay distinct.
        ECkDebuggerToolCategory::Systems,
        50}
        .Set_TabFactory(FCkDebuggerToolTabFactory::CreateLambda([this]
        { return OnSpawnDebuggerTab(FSpawnTabArgs{TSharedPtr<SWindow>{}, FTabId{_DebuggerTabName}}); })));

    _EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCkVisualLodDebuggerModule::HandleEnginePreExit);

    // Registered AFTER the tab spawner, so the route can never resolve to a tab that cannot be opened. The predicate
    // is the SAME public static the viewport picker filters with, and the open callback drives the SAME roster
    // selection a click does — a route whose two halves disagree about the target is invalid.
    _EntityTargetRouteRegistrationId = FCkDebug_EntityTargetRegistry::Get().Register(FCkDebug_EntityTargetRoute{
        TEXT("CkVisualLodDebugger"), _DebuggerTabName,
        [](const FCk_Handle& InEntity)
        { return SCkVisualLodDebuggerWindow::Is_VisualLodPickCandidate(InEntity); },
        [](const FCk_Handle& InEntity)
        {
            if (ck::Is_NOT_Valid(InEntity))
            { return; }

            auto& Module = FCkVisualLodDebuggerModule::Get();
            Module.OpenDebugger();

            if (Module._DebuggerWindow.IsValid())
            { Module._DebuggerWindow->TargetEntity(InEntity); }
        }});
}

auto FCkVisualLodDebuggerModule::ShutdownModule() -> void
{
    if (_EnginePreExitHandle.IsValid())
    {
        FCoreDelegates::OnEnginePreExit.Remove(_EnginePreExitHandle);
        _EnginePreExitHandle.Reset();
    }

    // Before the tab spawner goes, so no route ever points at a tab that can no longer be opened.
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

auto FCkVisualLodDebuggerModule::Get() -> FCkVisualLodDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkVisualLodDebuggerModule>("CkVisualLodDebugger");
}

auto FCkVisualLodDebuggerModule::Get_TabName() -> FName
{
    return _DebuggerTabName;
}

auto FCkVisualLodDebuggerModule::OpenDebugger() -> void
{
    ck::debugger_tabs::Invoke_DebuggerTab(_DebuggerTabName);
}

auto FCkVisualLodDebuggerModule::CloseDebugger() -> void
{
    if (_DebuggerTab.IsValid())
    {
        // Engine shutdown destroys Slate windows BEFORE module unload — by then the tab's TSharedFromThis
        // backing is gone and RequestCloseTab → SharedThis(this) trips the AsShared check. Just drop the
        // ref on exit.
        if (NOT IsEngineExitRequested())
        { _DebuggerTab->RequestCloseTab(); }
        _DebuggerTab.Reset();
    }

    _DebuggerWindow.Reset();
}

auto FCkVisualLodDebuggerModule::ToggleDebugger() -> void
{
    if (IsDebuggerOpen())
    {
        CloseDebugger();
    }
    else
    {
        OpenDebugger();
    }
}

auto FCkVisualLodDebuggerModule::IsDebuggerOpen() const -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto FCkVisualLodDebuggerModule::HandleEnginePreExit() -> void
{
    _DebuggerWindow.Reset();
    _DebuggerTab.Reset();
}

auto FCkVisualLodDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkVisualLodDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("Visual LOD")))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            _DebuggerWindow.Reset();
            _DebuggerTab.Reset();
        })
        [
            _DebuggerWindow.ToSharedRef()
        ];

    // Hand the window a weak ref to its tab so the refresh gate can query visibility.
    _DebuggerWindow->Set_OwningTab(_DebuggerTab);

    return _DebuggerTab.ToSharedRef();
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkVisualLodDebuggerModule, CkVisualLodDebugger)

#undef LOCTEXT_NAMESPACE
