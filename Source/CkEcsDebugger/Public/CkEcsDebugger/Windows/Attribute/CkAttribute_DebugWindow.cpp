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
    if (ImGui::BeginTable("Attributes", 5, ImGuiTableFlags_SizingFixedFit
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
        ImGui::TableSetupColumn("Current");
        ImGui::TableSetupColumn("Base");
        ImGui::TableSetupColumn("Min (Final)");
        ImGui::TableSetupColumn("Max (Final)");
        ImGui::TableHeadersRow();

        static int32 Selected = -1;
        int32 Index = 0;

        const auto& RenderRow = [&](const T_HandleType& InAttribute)
        {
            if (ck::Is_NOT_Valid(InAttribute))
            { return; }

            const auto TagAsString = [](FGameplayTag InTag, TArray<FGameplayTag> InToFilter)
            {
                auto String = ck::Format_UE(TEXT("{}"), InTag);

                for (const auto& Filter : InToFilter)
                {
                    const auto& ToFilter = ck::Format_UE(TEXT("{}."), Filter);
                    String.RemoveFromStart(ToFilter);
                }

                return String;
            };

            const auto& AttributeLabel = UCk_Utils_GameplayLabel_UE::Get_Label(InAttribute);
            const auto& AttributeName = ck::Format_UE(TEXT("{}"), TagAsString(AttributeLabel,
                TArray{TAG_Label_FloatAttribute.GetTag(), TAG_Label_ByteAttribute.GetTag(), TAG_Label_VectorAttribute.GetTag()}));

            if (AttributeName == TAG_Label_FloatAttribute.GetTag().ToString() ||
                AttributeName == TAG_Label_ByteAttribute.GetTag().ToString() ||
                AttributeName == TAG_Label_VectorAttribute.GetTag().ToString())
            { return; }

            if (NOT _Filter.PassFilter(CK_ANSI_TEXT("{}", AttributeName)))
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
            if (ImGui::Selectable(CK_ANSI_TEXT("{}", AttributeName), Selected == Index, ImGuiSelectableFlags_SpanAllColumns))
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
            // Current Value
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), CurrentValue).c_str());

            //------------------------
            // Min Value
            //------------------------
            ImGui::TableNextColumn();
            if (T_UtilsType::Has_Component(InAttribute, ECk_MinMaxCurrent::Min))
            {
                const auto& Value = T_UtilsType::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Min);
                ImGui::Text(ck::Format_ANSI(TEXT("{}"), Value).c_str());
            }
            else
            {
                ImGui::Text(ck::Format_ANSI(TEXT("-")).c_str());
            }

            //------------------------
            // Base Value
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text(ck::Format_ANSI(TEXT("{}"), BaseValue).c_str());

            //------------------------
            // Max Value
            //------------------------
            ImGui::TableNextColumn();
            if (T_UtilsType::Has_Component(InAttribute, ECk_MinMaxCurrent::Max))
            {
                const auto& Value = T_UtilsType::Get_FinalValue(InAttribute, ECk_MinMaxCurrent::Max);
                ImGui::Text(ck::Format_ANSI(TEXT("{}"), Value).c_str());
            }
            else
            {
                ImGui::Text(ck::Format_ANSI(TEXT("-")).c_str());
            }

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
