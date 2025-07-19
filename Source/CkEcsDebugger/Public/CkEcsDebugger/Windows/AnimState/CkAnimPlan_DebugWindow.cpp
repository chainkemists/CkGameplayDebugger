#include "CkAnimPlan_DebugWindow.h"

#include "CkAnimation/AnimPlan/CkAnimPlan_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

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
    ImGui::Text("This window displays AnimPlan basic info of the selected entities");
    ImGui::Text("When multiple entities are selected, information for each entity is shown in separate sections");
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_AnimPlan_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    const auto SelectionEntities = Get_SelectionEntities();

    if (SelectionEntities.IsEmpty())
    {
        ImGui::Text("No entities selected");
        return;
    }

    auto EntitiesWithAnimPlans = ck::algo::Filter(SelectionEntities, [](const FCk_Handle& InEntity)
    {
        return UCk_Utils_AnimPlan_UE::Has_Any(InEntity);
    });

    if (EntitiesWithAnimPlans.IsEmpty())
    {
        if (SelectionEntities.Num() == 1)
        {
            ImGui::Text("Selected entity has no AnimPlans");
        }
        else
        {
            ImGui::Text("Selected entities have no AnimPlans");
        }
        return;
    }

    // Show summary for multiple entities
    if (EntitiesWithAnimPlans.Num() > 1)
    {
        ImGui::Text("Multiple entities with AnimPlans selected (%d)", EntitiesWithAnimPlans.Num());
        ImGui::Separator();
    }

    for (int32 EntityIndex = 0; EntityIndex < EntitiesWithAnimPlans.Num(); ++EntityIndex)
    {
        const auto& Entity = EntitiesWithAnimPlans[EntityIndex];

        const auto& SectionTitle = ck::Format_UE(TEXT("Entity {}"), Entity);

        if (ImGui::CollapsingHeader(ck::Format_ANSI(TEXT("{}"), SectionTitle).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            RenderEntityAnimPlansSection(Entity);
            ImGui::Unindent();
        }

        if (EntityIndex < EntitiesWithAnimPlans.Num() - 1)
        {
            ImGui::Separator();
            ImGui::Spacing();
        }
    }
}

auto
    FCk_AnimPlan_DebugWindow::
    RenderEntityAnimPlansSection(
        const FCk_Handle& InEntity)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_AnimPlan_DebugWindow_RenderEntityAnimPlansSection)

    if (NOT UCk_Utils_AnimPlan_UE::Has_Any(InEntity))
    {
        ImGui::Text("Entity has no AnimPlans");
        return;
    }

    RenderAnimPlansTable(InEntity);
}

auto
    FCk_AnimPlan_DebugWindow::
    RenderAnimPlansTable(
        const FCk_Handle& InEntity)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_AnimPlan_DebugWindow_RenderAnimPlansTable)

    auto AnimPlanIndex = 0;
    const auto& RenderAnimPlan = [&](const FCk_Handle_AnimPlan& InAnimPlan)
    {
        if (ck::Is_NOT_Valid(InAnimPlan))
        { return; }

        if (AnimPlanIndex > 0)
        {
            ImGui::Separator();
        }

        const auto& AnimState = UCk_Utils_AnimPlan_UE::Get_AnimState(InAnimPlan);

        const auto TagAsString = [](FGameplayTag InTag, FGameplayTag InToFilter)
        {
            auto String = ck::Format_UE(TEXT("{}"), InTag);
            const auto& ToFilter = ck::Format_UE(TEXT("{}."), InToFilter);
            String.RemoveFromStart(ToFilter);
            return String;
        };

        if (ImGui::BeginTable("AnimPlanInfo", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            //------------------------
            // Anim Goal
            //------------------------
            const auto& AnimGoalTag = AnimState.Get_AnimGoal();
            const auto& AnimGoalString = TagAsString(AnimGoalTag, TAG_Label_AnimPlan_Goal);
            Request_RenderTableRow_AnimPlanGoal("Anim Goal:", AnimGoalString);

            //------------------------
            // Anim Cluster
            //------------------------
            const auto& AnimClusterTag = AnimState.Get_AnimCluster();
            const auto& AnimClusterString = TagAsString(AnimClusterTag, Tag_Label_AnimPlan_Cluster);
            const auto IsClusterValid = ck::IsValid(AnimClusterTag);
            Request_RenderTableRow_AnimPlanValue("Anim Cluster:", AnimClusterString, IsClusterValid);

            //------------------------
            // Anim State
            //------------------------
            const auto& AnimStateTag = AnimState.Get_AnimState();
            const auto& AnimStateString = TagAsString(AnimStateTag, TAG_Label_AnimPlan_State);
            const auto IsStateValid = ck::IsValid(AnimStateTag);
            Request_RenderTableRow_AnimPlanValue("Anim State:", AnimStateString, IsStateValid);

            ImGui::EndTable();
        }

        ++AnimPlanIndex;
    };

    UCk_Utils_AnimPlan_UE::ForEach_AnimPlan(const_cast<FCk_Handle&>(InEntity), RenderAnimPlan);
}

auto
    FCk_AnimPlan_DebugWindow::
    Request_RenderTableRow_AnimPlanValue(
        const char* InLabel,
        const FString& InValue,
        bool InIsValid)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text(InLabel);
    ImGui::TableSetColumnIndex(1);

    if (InIsValid)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 204, 2, 255)); // Yellow
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(102, 102, 102, 255)); // Gray
    }

    ImGui::Text(ck::Format_ANSI(TEXT("{}"), InValue).c_str());
    ImGui::PopStyleColor();
}

auto
    FCk_AnimPlan_DebugWindow::
    Request_RenderTableRow_AnimPlanGoal(
        const char* InLabel,
        const FString& InValue)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text(InLabel);
    ImGui::TableSetColumnIndex(1);

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(195, 232, 141, 255)); // Green
    ImGui::Text(ck::Format_ANSI(TEXT("{}"), InValue).c_str());
    ImGui::PopStyleColor();
}

// --------------------------------------------------------------------------------------------------------------------