#include "CkAStarDebugger_Module.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkAStarDebugger/Window/SCkAStarDebuggerWindow.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"

#include "CkAStar/CkAStar_Fragment.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#if WITH_EDITOR
    #include "WorkspaceMenuStructure.h"
    #include "WorkspaceMenuStructureModule.h"
#endif

#define LOCTEXT_NAMESPACE "FCkAStarDebuggerModule"

const FName FCkAStarDebuggerModule::_DebuggerTabName = FName("CkAStarDebugger");

namespace
{
    auto ResolveAStarTarget(const FCk_Handle& InSelected) -> FCk_Handle
    {
        return ck::DebugSelectionSync::Resolve_ClosestLineageMatch(InSelected,
            [](const FCk_Handle& InCandidate)
            {
                return ck::IsValid(InCandidate)
                    && InCandidate.Has<ck::FFragment_AStar_Debug>();
            });
    }
}

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdAStarDebugger(
    TEXT("ck.AStarDebugger"),
    TEXT("Opens (1) or closes (0) the CK A* Debugger. Usage: ck.AStarDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkAStarDebuggerModule::Get();

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

auto
    FCkAStarDebuggerModule::
    StartupModule()
    -> void
{
    auto& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkAStarDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK A* Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK A* Search Debugger window")));
#if WITH_EDITOR
    TabSpawner.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
#endif

    _DebuggerToolRegistrationId = FCkDebuggerToolRegistry::Get().Register(FCkDebuggerToolDescriptor{
        TEXT("CkAStarDebugger"),
        _DebuggerTabName,
        FText::FromString(TEXT("CK A* Debugger")),
        FText::FromString(TEXT("Inspect A* search graphs, candidates, and path costs")),
        TEXT("Grid"),
        ECkDebuggerToolCategory::Ai,
        10});

    _EntityTargetRouteRegistrationId = FCkDebug_EntityTargetRegistry::Get().Register(FCkDebug_EntityTargetRoute{
        TEXT("CkAStarDebugger"), _DebuggerTabName,
        [](const FCk_Handle& InEntity) { return ck::IsValid(ResolveAStarTarget(InEntity)); },
        [this](const FCk_Handle& InEntity)
        {
            const auto Target = ResolveAStarTarget(InEntity);
            if (NOT ck::IsValid(Target)) { return; }
            OpenDebugger();
            if (_DebuggerWindow.IsValid()) { _DebuggerWindow->TargetEntity(Target); }
        }});

    // Selection sync only adopts into a live window. Unlike the entity-target
    // route above, it must not create a tab or take foreground focus.
    _SelectionSyncHandle = ck::DebugSelectionSync::Get_OnSelection().AddLambda(
        [this](const FCk_Handle& InEntity, FName InSource)
        {
            if (InSource == TEXT("AStarDebugger")) { return; }
            if (ck::Is_NOT_Valid(InEntity))          { return; }
            if (NOT _DebuggerWindow.IsValid())       { return; }

            const auto Target = ResolveAStarTarget(InEntity);
            if (NOT ck::IsValid(Target)) { return; }

            const auto Guard = ck::DebugSelectionSync::FApplyGuard{};
            _DebuggerWindow->TargetEntity(Target);
        });
}

auto
    FCkAStarDebuggerModule::
    ShutdownModule()
    -> void
{
    if (_SelectionSyncHandle.IsValid())
    {
        ck::DebugSelectionSync::Get_OnSelection().Remove(_SelectionSyncHandle);
        _SelectionSyncHandle.Reset();
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

auto
    FCkAStarDebuggerModule::
    Get()
    -> FCkAStarDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkAStarDebuggerModule>("CkAStarDebugger");
}

auto
    FCkAStarDebuggerModule::
    OpenDebugger()
    -> void
{
    FGlobalTabmanager::Get()->TryInvokeTab(_DebuggerTabName);
}

auto
    FCkAStarDebuggerModule::
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
    FCkAStarDebuggerModule::
    ToggleDebugger()
    -> void
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

auto
    FCkAStarDebuggerModule::
    IsDebuggerOpen() const
    -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto
    FCkAStarDebuggerModule::
    OnSpawnDebuggerTab(
        const FSpawnTabArgs& InArgs)
    -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkAStarDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK A* Debugger")))
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

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkAStarDebuggerModule, CkAStarDebugger)
