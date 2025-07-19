#include "CkEntity_DebugWindow.h"

#include "CkCore/Debug/CkDebugDraw_Utils.h"

#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkRelationship/Team/CkTeam_Utils.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Get_SectionHeaderColor()
    -> ImU32
{
    return IM_COL32(100, 149, 237, 255); // Cornflower Blue #6495ED
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Request_RenderTableRow_Vector(const char* Label, const FString& Value)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text(Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Vector());
    ImGui::Text(ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Request_RenderTableRow_Number(const char* Label, const FString& Value)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text(Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Number());
    ImGui::Text(ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Request_RenderTableRow_Enum(const char* Label, const FString& Value)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text(Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Enum());
    ImGui::Text(ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Request_RenderTableRow_Unknown(const char* Label, const FString& Value)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text(Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Unknown());
    ImGui::Text(ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Request_RenderTableRow_None(const char* Label, const FString& Value)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text(Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_None());
    ImGui::Text(ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Request_RenderTableRow_Default(const char* Label, const FString& Value)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text(Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text(ck::Format_ANSI(TEXT("{}"), Value).c_str());
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Request_RenderEntityInformation(const FCk_Handle& SelectionEntity)
    -> void
{
    if (NOT ImGui::BeginTable("EntityInfo", 2, ImGuiTableFlags_SizingFixedFit))
    {
        return;
    }

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    Request_RenderTableRow_Entity("Entity:", ck::Format_UE(TEXT("{}"), SelectionEntity));
    // Actor Information
    if (UCk_Utils_OwningActor_UE::Has(SelectionEntity))
    {
        const auto ActorName = ck::Format_UE(TEXT("{}"), UCk_Utils_OwningActor_UE::Get_EntityOwningActor(SelectionEntity));
        Request_RenderTableRow_Default("Actor:", ActorName);
    }
    else
    {
        Request_RenderTableRow_None("Actor:", TEXT("None"));
    }

    ImGui::EndTable();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Request_RenderTransformSection(const FCk_Handle& SelectionEntity)
    -> void
{
    if (NOT UCk_Utils_Transform_UE::Has(SelectionEntity))
    { return; }

    if (NOT ImGui::BeginTable("Transform", 2, ImGuiTableFlags_SizingFixedFit))
    { return; }

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    const auto& Transform = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(SelectionEntity);

    Request_RenderTableRow_Vector("Location:", ck::Format_UE(TEXT("{}"), Transform.GetLocation()));
    Request_RenderTableRow_Vector("Rotation:", ck::Format_UE(TEXT("{}"), Transform.GetRotation().Rotator()));
    Request_RenderTableRow_Vector("Scale:", ck::Format_UE(TEXT("{}"), Transform.GetScale3D()));

    UCk_Utils_DebugDraw_UE::DrawDebugTransformGizmo(UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(SelectionEntity), Transform);

    ImGui::EndTable();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Request_RenderNetworkSection(const FCk_Handle& SelectionEntity)
    -> void
{
    if (NOT ImGui::BeginTable("Network", 2, ImGuiTableFlags_SizingFixedFit))
    {  return; }

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    Request_RenderTableRow_Enum("NetMode:", ck::Format_UE(TEXT("{}"), UCk_Utils_Net_UE::Get_EntityNetMode(SelectionEntity)));
    Request_RenderTableRow_Enum("NetRole:", ck::Format_UE(TEXT("{}"), UCk_Utils_Net_UE::Get_EntityNetRole(SelectionEntity)));

    ImGui::EndTable();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Request_RenderRelationshipsSection(const FCk_Handle& SelectionEntity)
    -> void
{
    if (NOT ImGui::BeginTable("Relationships", 2, ImGuiTableFlags_SizingFixedFit))
    { return; }

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    // Team Information
    if (const auto TeamEntity = UCk_Utils_Team_UE::Cast(SelectionEntity);
        ck::IsValid(TeamEntity))
    {
        const auto TeamID = UCk_Utils_Team_UE::Get_ID(TeamEntity);
        Request_RenderTableRow_Number("Team:", ck::Format_UE(TEXT("{} (Starts from ZERO)"), TeamID));
    }
    else
    {
        Request_RenderTableRow_Unknown("Team:", TEXT("Unknown"));
    }

    // Context Owner Information
    if (UCk_Utils_ContextOwner_UE::Has(SelectionEntity))
    {
        const auto ContextOwner = UCk_Utils_ContextOwner_UE::Get_ContextOwner(SelectionEntity);
        Request_RenderTableRow_Entity("Context Owner:", ck::Format_UE(TEXT("{}"), ContextOwner));
    }
    else
    {
        Request_RenderTableRow_None("Context Owner:", TEXT("None"));
    }

    // Lifetime Owner Information
    if (SelectionEntity.Has<ck::FFragment_LifetimeOwner>())
    {
        const auto& LifetimeOwner = SelectionEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        Request_RenderTableRow_Entity("Lifetime Owner:", ck::Format_UE(TEXT("{}"), LifetimeOwner));
    }
    else
    {
        Request_RenderTableRow_None("Lifetime Owner:", TEXT("None"));
    }

    ImGui::EndTable();
}

// --------------------------------------------------------------------------------------------------------------------
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
    ImGui::Text("This window displays Entity basic info of the selected entities");
    ImGui::Text("When multiple entities are selected, information for each entity is shown in separate sections");
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    const auto& SelectionEntities = Get_SelectionEntities();
    if (SelectionEntities.Num() == 0)
    {
        ImGui::Text("No entities selected");
        return;
    }

    // Single entity display
    if (SelectionEntities.Num() == 1)
    {
        const auto& SelectionEntity = SelectionEntities[0];
        if (ck::Is_NOT_Valid(SelectionEntity))
        {
            ImGui::Text("Selection Actor is NOT Ecs Ready");
            return;
        }

        Request_RenderEntityInformation(SelectionEntity);
        ImGui::Separator();
        Request_RenderTransformSection(SelectionEntity);
        ImGui::Separator();
        Request_RenderNetworkSection(SelectionEntity);
        ImGui::Separator();
        Request_RenderRelationshipsSection(SelectionEntity);
    }
    // Multiple entities display
    else
    {
        ImGui::Text("Multiple entities selected (%d)", SelectionEntities.Num());
        ImGui::Separator();

        for (int32 EntityIndex = 0; EntityIndex < SelectionEntities.Num(); ++EntityIndex)
        {
            const auto& SelectionEntity = SelectionEntities[EntityIndex];

            // Entity section header with proper formatting
            const auto& SectionTitle = ck::Format_UE(TEXT("Entity {}"), SelectionEntity);

            if (ImGui::CollapsingHeader(ck::Format_ANSI(TEXT("{}"), SectionTitle).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent();

                Request_RenderEntityInformation(SelectionEntity);
                ImGui::Separator();
                Request_RenderTransformSection(SelectionEntity);
                ImGui::Separator();
                Request_RenderNetworkSection(SelectionEntity);
                ImGui::Separator();
                Request_RenderRelationshipsSection(SelectionEntity);

                ImGui::Unindent();
            }

            if (EntityIndex < SelectionEntities.Num() - 1)
            {
                ImGui::Separator();
                ImGui::Spacing();
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Get_ValueColor_Entity()
    -> ImU32
{
    return IM_COL32(130, 177, 255, 255); // Blue #82b1ff
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Get_ValueColor_Vector()
    -> ImU32
{
    return IM_COL32(195, 232, 141, 255); // Green #c3e88d
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Get_ValueColor_Number()
    -> ImU32
{
    return IM_COL32(248, 187, 217, 255); // Pink #f8bbd9
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Get_ValueColor_Enum()
    -> ImU32
{
    return IM_COL32(255, 204, 2, 255); // Yellow #ffcc02
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Get_ValueColor_Unknown()
    -> ImU32
{
    return IM_COL32(255, 87, 34, 255); // Red #ff5722
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Get_ValueColor_None()
    -> ImU32
{
    return IM_COL32(102, 102, 102, 255); // Gray #666666
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Request_RenderTableRow_Entity(const char* Label, const FString& Value)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text(Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Entity());
    ImGui::Text(ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

//--------------------------------------------------------------------------------------------------------------------------