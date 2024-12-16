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
    QUICK_SCOPE_CYCLE_COUNTER(FCk_AnimPlan_DebugWindow_RenderTable)
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

            const auto TagAsString = [](FGameplayTag InTag, FGameplayTag InToFilter)
            {
                auto String = ck::Format_UE(TEXT("{}"), InTag);
                const auto& ToFilter = ck::Format_UE(TEXT("{}."), InToFilter);
                String.RemoveFromStart(ToFilter);
                return String;
            };

            //------------------------
            // Anim Goal
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text(CK_ANSI_TEXT("{}", TagAsString(AnimState.Get_AnimGoal(), TAG_Label_AnimPlan_Goal)));
            ImGui::SameLine();

            //------------------------
            // Anim Cluster
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text(CK_ANSI_TEXT("{}", TagAsString(AnimState.Get_AnimCluster(), Tag_Label_AnimPlan_Cluster)));
            ImGui::SameLine();

            //------------------------
            // Anim State
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text(CK_ANSI_TEXT("{}", TagAsString(AnimState.Get_AnimState(), TAG_Label_AnimPlan_State)));
            ImGui::SameLine();
        };

        UCk_Utils_AnimPlan_UE::ForEach_AnimPlan(SelectionEntity, RenderRow);

        ImGui::EndTable();
    }
}

// --------------------------------------------------------------------------------------------------------------------

