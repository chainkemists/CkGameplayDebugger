#include "CkEcsDebugger_Module.h"

#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Window/CkDebuggerWindow_Main.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FCkEcsDebuggerModule"

const FName FCkEcsDebuggerModule::DebuggerTabName = FName("CkEcsDebugger");

// --------------------------------------------------------------------------------------------------------------------

static FAutoConsoleCommand CmdEcsDebugger(
    TEXT("ck.EcsDebugger"),
    TEXT("Opens (1) or closes (0) the CK ECS Debugger window. Usage: ck.EcsDebugger [0/1]"),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& InArgs)
    {
        auto& Module = FCkEcsDebuggerModule::Get();

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

auto FCkEcsDebuggerModule::StartupModule() -> void
{
    FCkDebuggerStyle::Initialize();

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        DebuggerTabName,
        FOnSpawnTab::CreateRaw(this, &FCkEcsDebuggerModule::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK ECS Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK ECS Debugger window")))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
}

auto FCkEcsDebuggerModule::ShutdownModule() -> void
{
    if (FGlobalTabmanager::Get()->HasTabSpawner(DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DebuggerTabName);
    }

    DebuggerWindow.Reset();
    DebuggerTab.Reset();

    FCkDebuggerStyle::Shutdown();
}

auto FCkEcsDebuggerModule::Get() -> FCkEcsDebuggerModule&
{
    return FModuleManager::GetModuleChecked<FCkEcsDebuggerModule>("CkEcsDebugger");
}

auto FCkEcsDebuggerModule::OpenDebugger() -> void
{
    FGlobalTabmanager::Get()->TryInvokeTab(DebuggerTabName);
}

auto FCkEcsDebuggerModule::CloseDebugger() -> void
{
    if (DebuggerTab.IsValid())
    {
        DebuggerTab->RequestCloseTab();
        DebuggerTab.Reset();
    }

    DebuggerWindow.Reset();
}

auto FCkEcsDebuggerModule::ToggleDebugger() -> void
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

auto FCkEcsDebuggerModule::IsDebuggerOpen() const -> bool
{
    return DebuggerWindow.IsValid() && DebuggerTab.IsValid();
}

auto FCkEcsDebuggerModule::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    DebuggerWindow = SNew(SCkDebuggerWindow_Main);

    DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK ECS Debugger")))
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab>)
        {
            DebuggerWindow.Reset();
            DebuggerTab.Reset();
        })
        [
            DebuggerWindow.ToSharedRef()
        ];

    return DebuggerTab.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkEcsDebuggerModule, CkEcsDebugger)
