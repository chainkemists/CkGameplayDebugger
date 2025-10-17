#include "CkEcsDebugger_Subsystem.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcsDebugger/Window/CkDebuggerWindow_Main.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"


const FName UCkEcsDebugger_Subsystem::DebuggerTabName = FName("CkEcsDebugger");

auto UCkEcsDebugger_Subsystem::Initialize(FSubsystemCollectionBase& InCollection) -> void
{
    Super::Initialize(InCollection);

    RegisterTabSpawner();

    static FAutoConsoleCommandWithWorldAndArgs CmdEcsDebugger(
        TEXT("ck.EcsDebugger"),
        TEXT("Opens (1) or closes (0) the CK ECS Debugger window. Usage: ck.EcsDebugger 0/1"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&UCkEcsDebugger_Subsystem::ConsoleCommand_EcsDebugger)
    );
}

auto UCkEcsDebugger_Subsystem::Deinitialize() -> void
{
    CloseDebugger();
    UnregisterTabSpawner();

    Super::Deinitialize();
}

auto UCkEcsDebugger_Subsystem::ConsoleCommand_EcsDebugger(const TArray<FString>& InArgs, UWorld* InWorld) -> void
{
    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    auto* Subsystem = InWorld->GetSubsystem<UCkEcsDebugger_Subsystem>();
    if (ck::Is_NOT_Valid(Subsystem))
    { return; }

    if (InArgs.IsEmpty())
    {
        Subsystem->ToggleDebugger();
        return;
    }

    const auto& Arg = InArgs[0];
    const auto Value = FCString::Atoi(*Arg);

    if (Value == 1)
    {
        Subsystem->OpenDebugger();
    }
    else if (Value == 0)
    {
        Subsystem->CloseDebugger();
    }
    else
    {
        Subsystem->ToggleDebugger();
    }
}

auto UCkEcsDebugger_Subsystem::OnWorldBeginPlay(UWorld& InWorld) -> void
{
    Super::OnWorldBeginPlay(InWorld);
}

auto UCkEcsDebugger_Subsystem::OpenDebugger() -> void
{
    if (IsDebuggerOpen())
    { return; }

    if (NOT FGlobalTabmanager::Get()->HasTabSpawner(DebuggerTabName))
    {
        RegisterTabSpawner();
    }

    FGlobalTabmanager::Get()->TryInvokeTab(DebuggerTabName);
}

auto UCkEcsDebugger_Subsystem::CloseDebugger() -> void
{
    if (NOT IsDebuggerOpen())
    { return; }

    if (DebuggerTab.IsValid())
    {
        DebuggerTab->RequestCloseTab();
        DebuggerTab.Reset();
    }

    DebuggerWindow.Reset();
}

auto UCkEcsDebugger_Subsystem::ToggleDebugger() -> void
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

auto UCkEcsDebugger_Subsystem::IsDebuggerOpen() const -> bool
{
    return DebuggerWindow.IsValid() && DebuggerTab.IsValid();
}

auto UCkEcsDebugger_Subsystem::Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection>
{
    if (NOT DebuggerWindow.IsValid())
    { return nullptr; }

    return DebuggerWindow->Get_SelectionModel();
}

auto UCkEcsDebugger_Subsystem::Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldContext>
{
    if (NOT DebuggerWindow.IsValid())
    { return nullptr; }

    return DebuggerWindow->Get_WorldModel();
}

auto UCkEcsDebugger_Subsystem::OnSpawnDebuggerTab(const FSpawnTabArgs& InArgs) -> TSharedRef<SDockTab>
{
    DebuggerWindow = SNew(SCkDebuggerWindow_Main);

    DebuggerTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(FText::FromString(TEXT("CK ECS Debugger")))
        [
            DebuggerWindow.ToSharedRef()
        ];

    return DebuggerTab.ToSharedRef();
}

auto UCkEcsDebugger_Subsystem::RegisterTabSpawner() -> void
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        DebuggerTabName,
        FOnSpawnTab::CreateUObject(this, &UCkEcsDebugger_Subsystem::OnSpawnDebuggerTab))
        .SetDisplayName(FText::FromString(TEXT("CK ECS Debugger")))
        .SetTooltipText(FText::FromString(TEXT("Opens the CK ECS Debugger window")))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
}

auto UCkEcsDebugger_Subsystem::UnregisterTabSpawner() -> void
{
    if (FGlobalTabmanager::Get()->HasTabSpawner(DebuggerTabName))
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DebuggerTabName);
    }
}