#include "CkEqsDebugger_Module.h"

#include "CkEqsDebugger/Window/SCkEqsDebuggerWindow.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FCkEqsDebuggerModule"

const FName FCkEqsDebuggerModule::_DebuggerTabName = FName("CkEqsDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdEqsDebugger(
    TEXT("ck.EqsDebugger"),
    TEXT("Opens (1) or closes (0) the CK EQS Debugger. Usage: ck.EqsDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkEqsDebuggerModule::Get();

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
    FCkEqsDebuggerModule::
    StartupModule()
    -> void
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        _DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkEqsDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK EQS Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK Environmental Query System Debugger window")))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
}

auto
    FCkEqsDebuggerModule::
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
    FCkEqsDebuggerModule::
    Get()
    -> FCkEqsDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkEqsDebuggerModule>("CkEqsDebugger");
}

auto
    FCkEqsDebuggerModule::
    OpenDebugger()
    -> void
{
    FGlobalTabmanager::Get()->TryInvokeTab(_DebuggerTabName);
}

auto
    FCkEqsDebuggerModule::
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
    FCkEqsDebuggerModule::
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
    FCkEqsDebuggerModule::
    IsDebuggerOpen() const
    -> bool
{
    return _DebuggerWindow.IsValid() && _DebuggerTab.IsValid();
}

auto
    FCkEqsDebuggerModule::
    OnSpawnDebuggerTab(
        const FSpawnTabArgs& InArgs)
    -> TSharedRef<SDockTab>
{
    _DebuggerWindow = SNew(SCkEqsDebuggerWindow);

    _DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK EQS Debugger")))
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

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkEqsDebuggerModule, CkEqsDebugger)
