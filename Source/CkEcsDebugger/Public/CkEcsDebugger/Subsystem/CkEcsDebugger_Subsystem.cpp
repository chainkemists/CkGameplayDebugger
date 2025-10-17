#include "CkEcsDebugger_Subsystem.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcsDebugger/Window/CkDebuggerWindow_Main.h"

auto UCk_EcsDebugger_Subsystem_UE::Initialize(FSubsystemCollectionBase& InCollection) -> void
{
    Super::Initialize(InCollection);
}

auto UCk_EcsDebugger_Subsystem_UE::Deinitialize() -> void
{
    CloseDebugger();

    Super::Deinitialize();
}

auto UCk_EcsDebugger_Subsystem_UE::OnWorldBeginPlay(UWorld& InWorld) -> void
{
    Super::OnWorldBeginPlay(InWorld);

    // Create the debugger window instance but don't open it yet
    DebuggerWindow = MakeShared<FCkDebuggerWindow_Main>(&InWorld);
}

auto UCk_EcsDebugger_Subsystem_UE::OpenDebugger() -> void
{
    if (NOT DebuggerWindow.IsValid())
    { return; }

    DebuggerWindow->EnableWidget();
}

auto UCk_EcsDebugger_Subsystem_UE::CloseDebugger() -> void
{
    if (NOT DebuggerWindow.IsValid())
    { return; }

    DebuggerWindow->DisableWidget();
}

auto UCk_EcsDebugger_Subsystem_UE::Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection>
{
    if (NOT DebuggerWindow.IsValid())
    { return nullptr; }

    return DebuggerWindow->Get_SelectionModel();
}

auto UCk_EcsDebugger_Subsystem_UE::Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldContext>
{
    if (NOT DebuggerWindow.IsValid())
    { return nullptr; }

    return DebuggerWindow->Get_WorldModel();
}