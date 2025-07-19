#include "CkAbilityOwnerTags_DebugWindow.h"

#include <Cog/Public/CogWidgets.h>
#include "CkAbility/AbilityOwner/CkAbilityOwner_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_AbilityOwnerTags_DebugWindow::
    Initialize()
    -> void
{
    Super::Initialize();

    bHasMenu = true;

    _Config = GetConfig<UCk_AbilityOwnerTags_DebugWindowConfig>();
}

auto
    FCk_AbilityOwnerTags_DebugWindow::
    ResetConfig()
    -> void
{
    Super::ResetConfig();

    _Config->Reset();
}

auto
    FCk_AbilityOwnerTags_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text("This window displays the Ability Owner Active Tags of the selected actor");
}

auto
    FCk_AbilityOwnerTags_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    RenderMenu();

    auto SelectionEntity = Get_PrimarySelectionEntity();

    if (ck::Is_NOT_Valid(SelectionEntity))
    {
        ImGui::Text("No entities selected");
        return;
    }

    auto SelectionEntityAsAbilityOwner = UCk_Utils_AbilityOwner_UE::Cast(SelectionEntity);

    if (ck::Is_NOT_Valid(SelectionEntityAsAbilityOwner))
    {
        ImGui::Text("Primary selected entity is not an Ability Owner");
        return;
    }

    RenderTable(SelectionEntityAsAbilityOwner);
}

auto
    FCk_AbilityOwnerTags_DebugWindow::
    RenderMenu()
    -> void
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Checkbox("Sort by Name", &_Config->SortByName);

            ImGui::Separator();

            if (ImGui::MenuItem("Reset"))
            {
                ResetConfig();
            }

            ImGui::EndMenu();
        }

        FCogWidgets::SearchBar("##Filter", _Filter);

        ImGui::EndMenuBar();
    }
}

auto
    FCk_AbilityOwnerTags_DebugWindow::
    RenderTable(
        FCk_Handle_AbilityOwner& InSelectionEntity)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_AbilityOwnerTags_DebugWindow_RenderTable)
    const auto& ActiveTagsWithCount = UCk_Utils_AbilityOwner_UE::Get_ActiveTagsWithCount(InSelectionEntity);

    if (ImGui::BeginTable("Tags", 2, ImGuiTableFlags_SizingFixedFit
                                   | ImGuiTableFlags_Resizable
                                   | ImGuiTableFlags_NoBordersInBodyUntilResize
                                   | ImGuiTableFlags_ScrollY
                                   | ImGuiTableFlags_RowBg
                                   | ImGuiTableFlags_BordersV
                                   | ImGuiTableFlags_Reorderable
                                   | ImGuiTableFlags_Hideable))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Count");
        ImGui::TableSetupColumn(TCHAR_TO_ANSI(*GetName()));
        ImGui::TableHeadersRow();

        const auto& FilteredActiveTagsWithCount = ActiveTagsWithCount.FilterByPredicate([&](const TPair<FGameplayTag, int32>& InKvp)
        {
            return _Filter.PassFilter(TCHAR_TO_ANSI(*InKvp.Key.ToString()));
        });

        auto FilteredAndSortedTags = TArray<FGameplayTag>{};
        FilteredActiveTagsWithCount.GenerateKeyArray(FilteredAndSortedTags);

        if (_Config->SortByName)
        {
            FilteredAndSortedTags.Sort([](const FGameplayTag& Tag1, const FGameplayTag& Tag2)
            {
                return Tag1.GetTagName().Compare(Tag2.GetTagName()) < 0;
            });
        }

        static int32 Selected = -1;
        int32 Index = 0;

        for (const auto& Tag : FilteredAndSortedTags)
        {
            ImGui::TableNextRow();

            ImGui::PushID(Index);

            //------------------------
            // Count
            //------------------------
            ImGui::TableNextColumn();
            const auto& TagCount = *FilteredActiveTagsWithCount.Find(Tag);
            ImGui::TextColored(TagCount > 1 ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%d", TagCount);

            //------------------------
            // Name
            //------------------------
            ImGui::TableNextColumn();
            if (ImGui::Selectable(TCHAR_TO_ANSI(*Tag.ToString()), Selected == Index, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick))
            {
                Selected = Index;
            }

            //------------------------
            // Tooltip
            //------------------------
            if (ImGui::IsItemHovered())
            {
                FCogWidgets::BeginTableTooltip();
                //RenderTag(AbilitySystemComponent, Tag);
                FCogWidgets::EndTableTooltip();
            }

            ImGui::PopID();
            ++Index;
        }

        ImGui::EndTable();
    }
}

// --------------------------------------------------------------------------------------------------------------------
