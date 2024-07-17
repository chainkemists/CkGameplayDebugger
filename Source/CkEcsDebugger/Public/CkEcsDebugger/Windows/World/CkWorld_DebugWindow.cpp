#include "CkWorld_DebugWindow.h"

#include "CogWindowWidgets.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_World_DebugWindow::
    Initialize()
    -> void
{
    Super::Initialize();
    bHasMenu = true;
    bHasWidget = true;
}

auto
    FCk_World_DebugWindow::
    GetMainMenuWidgetWidth(
        int32 SubWidgetIndex,
        float MaxWidth)
    -> float
{
    switch (SubWidgetIndex)
    {
        case 0: return FCogWindowWidgets::GetFontWidth() * 3;
    }

    return -1.0f;
}

auto
    FCk_World_DebugWindow::
    RenderMainMenuWidget(
        int32 SubWidgetIndex,
        float Width)
    -> void
{
    if (SubWidgetIndex == 0)
    {
        ImGui::Button("X", ImVec2(Width, 0));
    }
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_World_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text("This window allows selection of all the available worlds");
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

    ImGui::Text(ck::Format_ANSI(TEXT("{}"),
        EcsDebuggerSubsystem->Get_SelectedWorld()->GetNetMode() == NM_DedicatedServer ? TEXT("Server") : TEXT("Client")).c_str());

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
        ImGui::TableSetupColumn("Server/Client");
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

            ImGui::TableNextColumn();
            ImGui::Text(ck::Format_ANSI(TEXT("{}"),
                ContextWorld->GetNetMode() == NM_DedicatedServer ? TEXT("Server") : TEXT("Client")).c_str());

            ImGui::TableNextColumn();
            if (ImGui::Selectable(ck::Format_ANSI(TEXT("{}"), ContextWorld).c_str()))
            {
                EcsDebuggerSubsystem->Set_SelectedWorld(ContextWorld);
            }

            ImGui::PopID();
            ++Index;
        }

        ImGui::EndTable();
    }
}

// --------------------------------------------------------------------------------------------------------------------

