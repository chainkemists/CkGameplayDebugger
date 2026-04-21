#include "CkAStarDebugger_Module.h"

#include "CkAStarDebugger/Window/SCkAStarDebuggerWindow.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FCkAStarDebuggerModule"

const FName FCkAStarDebuggerModule::_DebuggerTabName = FName("CkAStarDebugger");

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
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkAStarDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK A* Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK A* Search Debugger window")))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
}

auto
    FCkAStarDebuggerModule::
    ShutdownModule()
    -> void
{
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
        _DebuggerTab->RequestCloseTab();
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
