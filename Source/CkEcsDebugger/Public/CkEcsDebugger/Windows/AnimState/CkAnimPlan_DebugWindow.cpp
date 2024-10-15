#include "CkAnimPlan_DebugWindow.h"

#include "CkAnimation/AnimPlan/CkAnimPlan_Utils.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_AnimPlan_DebugWindow::
    Initialize()
    -> void
{
    Super::Initialize();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_AnimPlan_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text("This window displays AnimPlan basic info of the selected actor");
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_AnimPlan_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    if (ck::Is_NOT_Valid(Get_SelectionEntity()))
    { return; }

    if (const auto& HasAnimPlans = UCk_Utils_AnimPlan_UE::Has_Any(Get_SelectionEntity());
        NOT HasAnimPlans)
    {
        ImGui::Text("Selection Actor has no AnimPlans");
        return;
    }

    RenderTable();
}

auto
    FCk_AnimPlan_DebugWindow::
    RenderTable()
    -> void
{
    auto SelectionEntity = Get_SelectionEntity();

    if (ImGui::BeginTable("AnimPlans", 3, ImGuiTableFlags_SizingFixedFit
                                     | ImGuiTableFlags_Resizable
                                     | ImGuiTableFlags_NoBordersInBodyUntilResize
                                     | ImGuiTableFlags_ScrollY
                                     | ImGuiTableFlags_RowBg
                                     | ImGuiTableFlags_BordersV
                                     | ImGuiTableFlags_Reorderable
                                     | ImGuiTableFlags_Hideable))
    {

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Anim Goal");
        ImGui::TableSetupColumn("Anim Cluster");
        ImGui::TableSetupColumn("Anim State");
        ImGui::TableHeadersRow();

        const auto& RenderRow = [&](const FCk_Handle_AnimPlan& InAnimPlan)
        {
            if (ck::Is_NOT_Valid(InAnimPlan))
            { return; }

            const auto& AnimState = UCk_Utils_AnimPlan_UE::Get_AnimState(InAnimPlan);

            ImGui::TableNextRow();

            //------------------------
            // Anim Goal
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text(CK_ANSI_TEXT("{}", AnimState.Get_AnimGoal()));
            ImGui::SameLine();

            //------------------------
            // Anim Cluster
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text(CK_ANSI_TEXT("{}", AnimState.Get_AnimCluster()));
            ImGui::SameLine();

            //------------------------
            // Anim State
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text(CK_ANSI_TEXT("{}", AnimState.Get_AnimState()));
            ImGui::SameLine();
        };

        UCk_Utils_AnimPlan_UE::ForEach_AnimPlan(SelectionEntity, RenderRow);

        ImGui::EndTable();
    }
}

// --------------------------------------------------------------------------------------------------------------------

