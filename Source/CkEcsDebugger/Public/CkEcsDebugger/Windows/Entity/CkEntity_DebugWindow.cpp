#include "CkEntity_DebugWindow.h"

#include "CkRelationship/Team/CkTeam_Utils.h"

#include "CkEcs/Net/CkNet_Utils.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Initialize()
    -> void
{
    Super::Initialize();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text("This window displays Entity basic info of the selected actor");
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    if (const auto& SelectionEntity = Get_SelectionEntity(); ck::IsValid(SelectionEntity))
    {
        ImGui::Text(ck::Format_ANSI(TEXT("Entity: {}"), SelectionEntity).c_str());
        ImGui::Text(ck::Format_ANSI(TEXT("NetMode: {}"), UCk_Utils_Net_UE::Get_EntityNetMode(SelectionEntity)).c_str());
        ImGui::Text(ck::Format_ANSI(TEXT("NetRole: {}"), UCk_Utils_Net_UE::Get_EntityNetRole(SelectionEntity)).c_str());

        if (const auto TeamEntity = UCk_Utils_Team_UE::Cast(SelectionEntity);
            ck::IsValid(TeamEntity))
        {
            ImGui::Text(ck::Format_ANSI(TEXT("Team: {} (Starts from ZERO)"), UCk_Utils_Team_UE::Get_ID(TeamEntity)).c_str());
        }
        else
        {
            ImGui::Text(ck::Format_ANSI(TEXT("Team: Unknown")).c_str());
        }

        if (UCk_Utils_OwningActor_UE::Has(SelectionEntity))
        {
            ImGui::Text(ck::Format_ANSI(TEXT("Actor: {}"), UCk_Utils_OwningActor_UE::Get_EntityOwningActor(SelectionEntity)).c_str());
        }
        else
        {
            ImGui::Text(ck::Format_ANSI(TEXT("Actor: None")).c_str());
        }
    }
    else
    {
        ImGui::Text("Selection Actor is NOT Ecs Ready");
    }
}

// --------------------------------------------------------------------------------------------------------------------

