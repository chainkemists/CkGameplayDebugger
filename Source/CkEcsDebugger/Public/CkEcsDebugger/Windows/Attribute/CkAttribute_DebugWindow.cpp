#include "CkAttribute_DebugWindow.h"

#include "CkAttribute/FloatAttribute/CkFloatAttribute_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CogImguiHelper.h"

#include "CogWindowWidgets.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_FloatAttribute_DebugWindow::
    Initialize()
    -> void
{
    Super::Initialize();

    bHasMenu = true;
    bNoPadding = true;

    _Config = GetConfig<UCk_Attribute_DebugWindowConfig>();
}

auto
    FCk_FloatAttribute_DebugWindow::
    ResetConfig()
    -> void
{
    Super::ResetConfig();

    _Config->Reset();
}

auto
    FCk_FloatAttribute_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text
    (
        "This window displays the attributes of the selected actor. "
        "Attributes can be sorted by name "
        "Attributes with the Current value greater than the Base value are displayed in green. "
        "Attributes with the Current value lower than the Base value are displayed in red. "
        "Use the options 'Show Only Modified' to only show the attributes that have modifiers. "
    );
}

auto
    FCk_FloatAttribute_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    RenderMenu();

    auto SelectionEntity = Get_SelectionEntity();

    if (ck::Is_NOT_Valid(SelectionEntity))
    {
        ImGui::Text("Selection Actor is NOT Ecs Ready");
        return;
    }

    if (const auto& HasFloatAttributes = UCk_Utils_FloatAttribute_UE::Has_Any(SelectionEntity);
        NOT HasFloatAttributes)
    {
        ImGui::Text("Selection Actor has no Float Attributes");
        return;
    }

    RenderTable(SelectionEntity);
}

auto
    FCk_FloatAttribute_DebugWindow::
    RenderMenu()
    -> void
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Checkbox("Sort by Name", &_Config->SortByName);
            ImGui::Checkbox("Show Only Modified", &_Config->ShowOnlyModified);
            ImGui::Separator();
            ImGui::ColorEdit4("Positive Color", reinterpret_cast<float*>(&_Config->PositiveColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Negative Color", reinterpret_cast<float*>(&_Config->NegativeColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Neutral Color", reinterpret_cast<float*>(&_Config->NeutralColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset"))
            {
                ResetConfig();
            }
            ImGui::EndMenu();
        }

        FCogWindowWidgets::SearchBar(_Filter);

        ImGui::EndMenuBar();
    }
}

auto
    FCk_FloatAttribute_DebugWindow::
    RenderTable(
        FCk_Handle& InSelectionEntity)
    -> void
{
    if (ImGui::BeginTable("Attributes", 3, ImGuiTableFlags_SizingFixedFit
                                         | ImGuiTableFlags_Resizable
                                         | ImGuiTableFlags_NoBordersInBodyUntilResize
                                         | ImGuiTableFlags_ScrollY
                                         | ImGuiTableFlags_RowBg
                                         | ImGuiTableFlags_BordersV
                                         | ImGuiTableFlags_Reorderable
                                         | ImGuiTableFlags_Hideable))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Attribute");
        ImGui::TableSetupColumn("Base");
        ImGui::TableSetupColumn("Current");
        ImGui::TableHeadersRow();

        static int32 Selected = -1;
        int32 Index = 0;

        const auto& RenderRow = [&](const FCk_Handle_FloatAttribute& InAttribute)
        {
            if (ck::Is_NOT_Valid(InAttribute))
            { return; }

            const auto& AttributeLabel = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
            const auto& AttributeName = StringCast<ANSICHAR>(*AttributeLabel.ToString()).Get();

            if (NOT _Filter.PassFilter(AttributeName))
            { return; }

            const auto& BaseValue = UCk_Utils_FloatAttribute_UE::Get_BaseValue(InAttribute);
            const auto& CurrentValue = UCk_Utils_FloatAttribute_UE::Get_FinalValue(InAttribute);

            if (_Config->ShowOnlyModified && FMath::IsNearlyEqual(CurrentValue, BaseValue))
            { return; }

            ImGui::TableNextRow();

            const ImVec4 Color = FCogImguiHelper::ToImVec4(_Config->Get_AttributeColor(InAttribute, ECk_MinMaxCurrent::Current));
            ImGui::PushStyleColor(ImGuiCol_Text, Color);

            //------------------------
            // Name
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text("");
            ImGui::SameLine();
            if (ImGui::Selectable(AttributeName, Selected == Index, ImGuiSelectableFlags_SpanAllColumns))
            {
                Selected = Index;
            }
            ImGui::PopStyleColor(1);

            //------------------------
            // Popup
            //------------------------
            if (ImGui::IsItemHovered())
            {
                FCogWindowWidgets::BeginTableTooltip();
                DrawAttributeInfo(InAttribute);
                FCogWindowWidgets::EndTableTooltip();
            }

            ImGui::PushStyleColor(ImGuiCol_Text, Color);

            //------------------------
            // Base Value
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", BaseValue);

            //------------------------
            // Current Value
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", CurrentValue);

            ImGui::PopStyleColor(1);

            ++Index;
        };

        UCk_Utils_FloatAttribute_UE::ForEach(InSelectionEntity, RenderRow);

        ImGui::EndTable();
    }
}

auto
    FCk_FloatAttribute_DebugWindow::
    DrawAttributeInfo(
        const FCk_Handle_FloatAttribute& InAttribute) const
    -> void
{
    if (ImGui::BeginTable("Attribute", 4, ImGuiTableFlags_Borders))
    {
        constexpr ImVec4 TextColor{1.0f, 1.0f, 1.0f, 0.5f};

        ImGui::TableSetupColumn("Property");
        ImGui::TableSetupColumn("Value_Min", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value_Current", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value_Final", ImGuiTableColumnFlags_WidthStretch);

        const auto& HasMinValue    = UCk_Utils_FloatAttribute_UE::Has_Component(InAttribute, ECk_MinMaxCurrent::Min);
        const auto& HasMaxValue    = UCk_Utils_FloatAttribute_UE::Has_Component(InAttribute, ECk_MinMaxCurrent::Max);
        const auto& AttributeLabel = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
        const auto& AttributeName  = StringCast<ANSICHAR>(*AttributeLabel.ToString()).Get();

        //------------------------
        // Name
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Name");
        ImGui::TableNextColumn();
        ImGui::Text("%s (Min)", AttributeName);
        ImGui::TableNextColumn();
        ImGui::Text("%s", AttributeName);
        ImGui::TableNextColumn();
        ImGui::Text("%s (Max)", AttributeName);

        //------------------------
        // Base Value (Min)
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Base Value");
        ImGui::TableNextColumn();

        if (HasMinValue)
        {
            ImGui::Text("%0.2f", UCk_Utils_FloatAttribute_UE::Get_BaseValue(InAttribute, ECk_MinMaxCurrent::Min));
        }

        //------------------------
        // Base Value
        //------------------------
        ImGui::TableNextColumn();
        ImGui::Text("%0.2f", UCk_Utils_FloatAttribute_UE::Get_BaseValue(InAttribute));

        //------------------------
        // Base Value (Max)
        //------------------------
        ImGui::TableNextColumn();

        if (HasMaxValue)
        {
            ImGui::Text("%0.2f", UCk_Utils_FloatAttribute_UE::Get_BaseValue(InAttribute, ECk_MinMaxCurrent::Max));
        }

        //------------------------
        // Current Value (Min)
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Current Value");
        ImGui::TableNextColumn();

        if (HasMinValue)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, FCogImguiHelper::ToImVec4(_Config->Get_AttributeColor(InAttribute, ECk_MinMaxCurrent::Min)));
            ImGui::Text("%0.2f", UCk_Utils_FloatAttribute_UE::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Min));
            ImGui::PopStyleColor(1);
        }

        //------------------------
        // Current Value
        //------------------------
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, FCogImguiHelper::ToImVec4(_Config->Get_AttributeColor(InAttribute, ECk_MinMaxCurrent::Current)));
        ImGui::Text("%0.2f", UCk_Utils_FloatAttribute_UE::Get_FinalValue(InAttribute));
        ImGui::PopStyleColor(1);

        //------------------------
        // Current Value (Max)
        //------------------------
        ImGui::TableNextColumn();

        if (HasMaxValue)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, FCogImguiHelper::ToImVec4(_Config->Get_AttributeColor(InAttribute, ECk_MinMaxCurrent::Max)));
            ImGui::Text("%0.2f", UCk_Utils_FloatAttribute_UE::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Max));
            ImGui::PopStyleColor(1);
        }

        ImGui::EndTable();
    }
}

// --------------------------------------------------------------------------------------------------------------------
