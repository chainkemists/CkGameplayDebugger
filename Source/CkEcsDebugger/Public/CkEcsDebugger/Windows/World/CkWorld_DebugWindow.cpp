#include "CkWorld_DebugWindow.h"

#include "Cog/Public/CogSubsystem.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

#include <CogImguiInputHelper.h>
#include <Cog/Public/CogWidgets.h>

//--------------------------------------------------------------------------------------------------------------------------

FString ToggleInputCommand   = TEXT("Cog.ToggleInput");

auto
    FCk_World_DebugWindow::
    Initialize()
    -> void
{
    Super::Initialize();
    bHasMenu = true;
    bHasWidget = true;

    const auto SelectCorrectWorld = [](const UWorld* InWorld, bool InPrevious = false)
    {
        auto EcsDebuggerSubsystem = InWorld->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();

        const auto SelectedWorld = EcsDebuggerSubsystem->Get_SelectedWorld();

        auto Worlds = TArray<UWorld*>();
        auto WorldContexts = GEngine->GetWorldContexts();

        for (auto Index = 0; Index < WorldContexts.Num(); ++Index)
        {
            const auto& ContextWorld = WorldContexts[Index].World();

            if (ck::Is_NOT_Valid(ContextWorld))
            { continue; }

            auto GameInstance = ContextWorld->GetGameInstance();

            if (ck::Is_NOT_Valid(GameInstance))
            { continue; }

            Worlds.Emplace(ContextWorld);
        }

        for (auto Index = 0; Index < Worlds.Num(); ++Index)
        {
            const auto& ContextWorld = Worlds[Index];

            if (ContextWorld == SelectedWorld)
            {
                const auto NewIndex = ((InPrevious ? Index - 1 : Index + 1) + Worlds.Num()) % Worlds.Num();;
                EcsDebuggerSubsystem->Set_SelectedWorld(Worlds[NewIndex]);
                break;
            }
        }
    };

    // We store the console command name instead of the IConsoleCommand pointer is so multiple instances don't crash when unregistering the same command multiple times
    // This window exists once per world, but IConsoleManager is a singleton that is shared. There's also no way to check if the IConsoleCommand pointer is still valid.
    IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("Cog.CycleWorlds_Previous"),
        TEXT("Cycle to the previous available world"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([this, SelectCorrectWorld](const TArray<FString>& Args, const UWorld* InWorld)
        {
            SelectCorrectWorld(InWorld, true);
        }),
        ECVF_Cheat);
    _ConsoleCommandNames.Add("Cog.CycleWorlds_Previous");

    IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("Cog.CycleWorlds_Next"),
        TEXT("Cycle to the next available world"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([this, SelectCorrectWorld](const TArray<FString>& Args, const UWorld* InWorld)
        {
            SelectCorrectWorld(InWorld, false);
        }),
        ECVF_Cheat);
    _ConsoleCommandNames.Add("Cog.CycleWorlds_Next");

    {
        const auto PlayerInput = FCogImguiInputHelper::GetPlayerInput(*GetWorld());
        UCogSubsystem::AddCommand(PlayerInput, "Cog.CycleWorlds_Previous", EKeys::PageDown);
    }

    {
        const auto PlayerInput = FCogImguiInputHelper::GetPlayerInput(*GetWorld());
        UCogSubsystem::AddCommand(PlayerInput, "Cog.CycleWorlds_Next", EKeys::PageUp);
    }
}

auto
    FCk_World_DebugWindow::
    Shutdown()
    -> void
{
    for (const auto& ConsoleCommandName : _ConsoleCommandNames)
    {
        IConsoleManager::Get().UnregisterConsoleObject(*ConsoleCommandName);
    }

    FCk_Ecs_DebugWindow::Shutdown();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_World_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text("This window allows selection of all the available worlds");
    ImGui::Text("The currently selected world is highlighted in yellow");
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_World_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    auto EcsDebuggerSubsystem = GetWorld()->GetSubsystem<UCk_EcsDebugger_Subsystem_UE>();

    if (ck::Is_NOT_Valid(EcsDebuggerSubsystem))
    { return; }

    CK_ENSURE_IF_NOT(ck::IsValid(EcsDebuggerSubsystem->Get_SelectedWorld()),
        TEXT("ECS Debugger Subsystem [{}] has NO valid selected world!"), EcsDebuggerSubsystem)
    { return; }

    const auto SelectedWorld = EcsDebuggerSubsystem->Get_SelectedWorld();

    ImGui::Text("Currently Selected: %s", ck::Format_ANSI(TEXT("{}"), SelectedWorld->GetNetMode()).c_str());

    if (ImGui::BeginTable("Worlds", 2, ImGuiTableFlags_SizingFixedFit
                                   | ImGuiTableFlags_Resizable
                                   | ImGuiTableFlags_NoBordersInBodyUntilResize
                                   | ImGuiTableFlags_ScrollY
                                   | ImGuiTableFlags_RowBg
                                   | ImGuiTableFlags_BordersV
                                   | ImGuiTableFlags_Reorderable
                                   | ImGuiTableFlags_Hideable))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("NetMode");
        ImGui::TableSetupColumn("World");
        ImGui::TableHeadersRow();

        int32 Index = 0;
        for (auto& Context : GEngine->GetWorldContexts())
        {
            const auto& ContextWorld = Context.World();

            if (ck::Is_NOT_Valid(ContextWorld))
            { continue; }

            auto GameInstance = ContextWorld->GetGameInstance();

            if (ck::Is_NOT_Valid(GameInstance))
            { continue; }

            ImGui::TableNextRow();
            ImGui::PushID(Index);

            const auto IsSelectedWorld = ContextWorld == SelectedWorld;

            // Highlight selected world
            if (IsSelectedWorld)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 204, 2, 255));
            }

            ImGui::TableNextColumn();
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), ContextWorld->GetNetMode()).c_str());

            ImGui::TableNextColumn();
            if (ImGui::Selectable(ck::Format_ANSI(TEXT("{}"), ContextWorld).c_str(), IsSelectedWorld))
            {
                EcsDebuggerSubsystem->Set_SelectedWorld(ContextWorld);
            }

            if (IsSelectedWorld)
            {
                ImGui::PopStyleColor();
            }

            ImGui::PopID();
            ++Index;
        }

        ImGui::EndTable();
    }
}

// --------------------------------------------------------------------------------------------------------------------