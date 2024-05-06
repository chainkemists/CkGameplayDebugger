#include "CkAttribute_DebugWindow.h"

#include "CkAttribute/FloatAttribute/CkFloatAttribute_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CogImguiHelper.h"

#include "CogWindowWidgets.h"

#include "CkCore/Math/Arithmetic/CkArithmetic_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

template <typename T_HandleType, typename T_UtilsType, typename T_Type>
auto
    FCk_Attribute_DebugWindow<T_HandleType, T_UtilsType, T_Type>::
    Initialize()
    -> void
{
    Super::Initialize();

    bHasMenu = true;
    bNoPadding = true;

    _Config = GetConfig<UCk_Attribute_DebugWindowConfig>();
}

template <typename T_HandleType, typename T_UtilsType, typename T_Type>
auto
    FCk_Attribute_DebugWindow<T_HandleType, T_UtilsType, T_Type>::
    ResetConfig()
    -> void
{
    Super::ResetConfig();

    _Config->Reset();
}

template <typename T_HandleType, typename T_UtilsType, typename T_Type>
auto
    FCk_Attribute_DebugWindow<T_HandleType, T_UtilsType, T_Type>::
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

template <typename T_HandleType, typename T_UtilsType, typename T_Type>
auto
    FCk_Attribute_DebugWindow<T_HandleType, T_UtilsType, T_Type>::
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

    if (const auto& HasAttribute = T_UtilsType::Has_Any(SelectionEntity);
        NOT HasAttribute)
    {
        ImGui::Text(ck::Format_ANSI(TEXT("Selection Actor has no {} Attributes"), ck::Get_RuntimeTypeToString<T_Type>()).c_str());
        return;
    }

    RenderTable(SelectionEntity);
}

template <typename T_HandleType, typename T_UtilsType, typename T_Type>
auto
    FCk_Attribute_DebugWindow<T_HandleType, T_UtilsType, T_Type>::
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

template <typename T_HandleType, typename T_UtilsType, typename T_Type>
auto
    FCk_Attribute_DebugWindow<T_HandleType, T_UtilsType, T_Type>::
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

        const auto& RenderRow = [&](const T_HandleType& InAttribute)
        {
            if (ck::Is_NOT_Valid(InAttribute))
            { return; }

            const auto& AttributeLabel = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
            const auto& AttributeName = StringCast<ANSICHAR>(*AttributeLabel.ToString()).Get();

            if (NOT _Filter.PassFilter(AttributeName))
            { return; }

            const auto& BaseValue = T_UtilsType::Get_BaseValue(InAttribute);
            const auto& CurrentValue = T_UtilsType::Get_FinalValue(InAttribute);

            if (_Config->ShowOnlyModified && UCk_Utils_Arithmetic_UE::Get_IsNearlyEqual(CurrentValue, BaseValue))
            { return; }

            ImGui::TableNextRow();

            const ImVec4 Color = FCogImguiHelper::ToImVec4(_Config->Get_AttributeColor<T_UtilsType>(InAttribute, ECk_MinMaxCurrent::Current));
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
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), BaseValue).c_str());

            //------------------------
            // Current Value
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), CurrentValue).c_str());

            ImGui::PopStyleColor(1);

            ++Index;
        };

        T_UtilsType::ForEach(InSelectionEntity, RenderRow);

        ImGui::EndTable();
    }
}

template <typename T_HandleType, typename T_UtilsType, typename T_Type>
auto
    FCk_Attribute_DebugWindow<T_HandleType, T_UtilsType, T_Type>::
    DrawAttributeInfo(
        const T_HandleType& InAttribute) const
    -> void
{
    if (ImGui::BeginTable("Attribute", 4, ImGuiTableFlags_Borders))
    {
        constexpr ImVec4 TextColor{1.0f, 1.0f, 1.0f, 0.5f};

        ImGui::TableSetupColumn("Property");
        ImGui::TableSetupColumn("Value_Min", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value_Current", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value_Final", ImGuiTableColumnFlags_WidthStretch);

        const auto& HasMinValue    = T_UtilsType::Has_Component(InAttribute, ECk_MinMaxCurrent::Min);
        const auto& HasMaxValue    = T_UtilsType::Has_Component(InAttribute, ECk_MinMaxCurrent::Max);
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
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), T_UtilsType::Get_BaseValue(InAttribute, ECk_MinMaxCurrent::Min)).c_str());
        }

        //------------------------
        // Base Value
        //------------------------
        ImGui::TableNextColumn();
        ImGui::Text(ck::Format_ANSI(TEXT("{}"), T_UtilsType::Get_BaseValue(InAttribute)).c_str());

        //------------------------
        // Base Value (Max)
        //------------------------
        ImGui::TableNextColumn();

        if (HasMaxValue)
        {
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), T_UtilsType::Get_BaseValue(InAttribute, ECk_MinMaxCurrent::Max)).c_str());
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
            ImGui::PushStyleColor(ImGuiCol_Text, FCogImguiHelper::ToImVec4(_Config->Get_AttributeColor<T_UtilsType>(InAttribute, ECk_MinMaxCurrent::Min)));
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), T_UtilsType::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Min)).c_str());
            ImGui::PopStyleColor(1);
        }

        //------------------------
        // Current Value
        //------------------------
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, FCogImguiHelper::ToImVec4(_Config->Get_AttributeColor<T_UtilsType>(InAttribute, ECk_MinMaxCurrent::Current)));
        ImGui::Text(ck::Format_ANSI(TEXT("{}"), T_UtilsType::Get_FinalValue(InAttribute)).c_str());
        ImGui::PopStyleColor(1);

        //------------------------
        // Current Value (Max)
        //------------------------
        ImGui::TableNextColumn();

        if (HasMaxValue)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, FCogImguiHelper::ToImVec4(_Config->Get_AttributeColor<T_UtilsType>(InAttribute, ECk_MinMaxCurrent::Max)));
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), T_UtilsType::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Max)).c_str());
            ImGui::PopStyleColor(1);
        }

        ImGui::EndTable();
    }
}

// --------------------------------------------------------------------------------------------------------------------
