#include "CkAnimPlan_DebugWindow.h"

#include "CkAnimation/Public/CkAnimation/AnimState/CkAnimState_Fragment_Data.h"
#include "CkAnimation/Public/CkAnimation/AnimState/CkAnimState_Utils.h"

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

    if (const auto& SelectionEntity = UCk_Utils_AnimState_UE::Cast(Get_SelectionEntity()); ck::IsValid(SelectionEntity))
    {
        ImGui::Text(CK_ANSI_TEXT("Goal: {}", UCk_Utils_AnimState_UE::Get_AnimGoal(SelectionEntity)));
        ImGui::Text(CK_ANSI_TEXT("Cluster: {}", UCk_Utils_AnimState_UE::Get_AnimCluster(SelectionEntity)));
        ImGui::Text(CK_ANSI_TEXT("State: {}", UCk_Utils_AnimState_UE::Get_AnimState(SelectionEntity)));
    }
    else
    {
        ImGui::Text("Selection Actor does NOT have AnimPlan Feature");
    }
}

// --------------------------------------------------------------------------------------------------------------------

