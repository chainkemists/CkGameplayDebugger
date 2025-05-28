#include "CkAbilities_DebugWindow.h"

#include "CogImguiHelper.h"
#include "CogWindowWidgets.h"

#include "CkAbility/Ability/CkAbility_Script.h"
#include "CkAbility/Ability/CkAbility_Utils.h"
#include "CkAbility/AbilityOwner/CkAbilityOwner_Utils.h"

#include "CkCore/String/CkString_Utils.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkTimer/CkTimer_Utils.h"

#if WITH_EDITOR
#include <Editor.h>
#include <Subsystems/AssetEditorSubsystem.h>
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_abilities_debug_window
{
    static ImGuiTextFilter Filter;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_Abilities_DebugWindow::
    Initialize() -> void
{
    Super::Initialize();

    bHasMenu = true;
    bNoPadding = true;

    _Config = GetConfig<UCk_Abilities_DebugWindowConfig>();
}

auto
    FCk_Abilities_DebugWindow::
    ResetConfig()
    -> void
{
    Super::ResetConfig();

    _Config->Reset();
}

auto
    FCk_Abilities_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text
    (
        "This window displays the abilities of the selected actor. "
        "Click the ability check box to force its activation or deactivation. "
        "Right click an ability to open or close the ability separate window. "
    );
}

auto
    FCk_Abilities_DebugWindow::
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

    auto SelectedEntityAsAbilityOwner = UCk_Utils_AbilityOwner_UE::Cast(SelectionEntity);

    if (ck::Is_NOT_Valid(SelectedEntityAsAbilityOwner))
    {
        ImGui::Text("Selection Actor has no abilities");
        return;
    }

    if (const auto& HasAbilities = UCk_Utils_AbilityOwner_UE::Has_Any(SelectedEntityAsAbilityOwner);
        NOT HasAbilities)
    {
        ImGui::Text("Selection Actor has no abilities");
        return;
    }

    RenderTable(SelectedEntityAsAbilityOwner);
}

auto
    FCk_Abilities_DebugWindow::
    RenderTick(
        float InDeltaT)
    -> void
{
    Super::RenderTick(InDeltaT);

    RenderOpenedAbilities();
}

auto
    FCk_Abilities_DebugWindow::
    GameTick(
        float InDeltaT)
    -> void
{
    Super::GameTick(InDeltaT);

    if (ck::IsValid(_AbilityHandleToActivate))
    {
        ProcessAbilityActivation(_AbilityHandleToActivate);
        _AbilityHandleToActivate = {};
    }
}

auto
    FCk_Abilities_DebugWindow::
    RenderMenu()
    -> void
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Separator();

            ImGui::Checkbox("Sort by Name", &_Config->SortByName);
            ImGui::Checkbox("Blueprint Name Only", &_Config->BlueprintNameOnly);
            ImGui::Checkbox("Show Active", &_Config->ShowActive);
            ImGui::Checkbox("Show Inactive", &_Config->ShowInactive);
            ImGui::Checkbox("Show Blocked", &_Config->ShowBlocked);
            ImGui::Checkbox("Show SubAbilities", &_Config->ShowSubAbilities);

            ImGui::Separator();

            ImGui::ColorEdit4("Active Color", reinterpret_cast<float*>(&_Config->ActiveColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Inactive Color", reinterpret_cast<float*>(&_Config->InactiveColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Blocked Color", reinterpret_cast<float*>(&_Config->BlockedColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);

            ImGui::Separator();

            if (ImGui::MenuItem("Reset"))
            {
                ResetConfig();
            }

            ImGui::EndMenu();
        }

        FCogWindowWidgets::SearchBar(ck_abilities_debug_window::Filter);

        ImGui::EndMenuBar();
    }
}

auto
    FCk_Abilities_DebugWindow::
    RenderOpenedAbilities()
    -> void
{
    auto SelectionEntity = Get_SelectionEntity();

    if (ck::Is_NOT_Valid(SelectionEntity))
    { return; }

    if (auto SelectedEntityAsAbilityOwner = UCk_Utils_AbilityOwner_UE::Cast(SelectionEntity);
        ck::Is_NOT_Valid(SelectedEntityAsAbilityOwner))
    { return; }


    for (int32 Index = _OpenedAbilities.Num() - 1; Index >= 0; --Index)
    {
        const auto& AbilityOpened = _OpenedAbilities[Index];

        bool Open = true;

        if (const auto& AbilityName = DoGet_AbilityName(AbilityOpened);
            ImGui::Begin(ck::Format_ANSI(TEXT("{}"), AbilityName).c_str(), &Open))
        {
            RenderAbilityInfo(AbilityOpened);
            ImGui::End();
        }

        if (NOT Open)
        {
            _OpenedAbilities.RemoveAt(Index);
        }
    }
}

auto
    FCk_Abilities_DebugWindow::
    RenderTable(
        const FCk_Handle_AbilityOwner& InSelectionEntity)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_Abilities_DebugWindow_RenderTable)
    _FilteredAbilities.Reset();

    AddToFilteredAbilities(InSelectionEntity);

    for (auto& KeyVal : _FilteredAbilities)
    {
        auto& Abilities = KeyVal.Value;

        if (_Config->SortByName)
        {
            Abilities.Sort([&](const FCk_Handle_Ability& InAbility1, const FCk_Handle_Ability& InAbility2)
            {
                const auto& AbilityName1 = DoGet_AbilityName(InAbility1);
                const auto& AbilityName2 = DoGet_AbilityName(InAbility2);

                return AbilityName1.Compare(AbilityName2) < 0;
            });
        }
    }

    if (ImGui::BeginTable("Abilities", 3, ImGuiTableFlags_SizingFixedFit
                                        | ImGuiTableFlags_Resizable
                                        | ImGuiTableFlags_NoBordersInBodyUntilResize
                                        | ImGuiTableFlags_ScrollY
                                        | ImGuiTableFlags_RowBg
                                        | ImGuiTableFlags_BordersV
                                        | ImGuiTableFlags_Reorderable
                                        | ImGuiTableFlags_Hideable))
    {
        ImGui::TableSetupColumn("Activation", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Ability");
        ImGui::TableSetupColumn("Blocked");
        ImGui::TableHeadersRow();

        static int32 SelectedIndex = -1;

        // TODO: Move to common util AND clean up this mess... (╯°□°)╯︵ ┻━┻
        const auto& HighlightSearchMatch = [](const std::string& InText, const std::string& InQuery, ImVec4 InHighlightColor)
        {
            // Helper lambda to split a string by delimiters
            const auto SplitString = [](const std::string& Input, const std::string& Delimiters) -> std::vector<std::string>
            {
                std::vector<std::string> Tokens;
                size_t Start = 0;
                size_t End = 0;

                while ((End = Input.find_first_of(Delimiters, Start)) != std::string::npos)
                {
                    if (End > Start)
                        Tokens.emplace_back(Input.substr(Start, End - Start));
                    Start = End + 1;
                }

                if (Start < Input.size())
                    Tokens.emplace_back(Input.substr(Start));

                return Tokens;
            };

            // Split the query into individual search terms
            const auto QueryParts = SplitString(InQuery, ", ");

            // Lowercase the input text for case-insensitive comparison
            auto LowerText = InText;
            std::ranges::transform(LowerText, LowerText.begin(), ::tolower);

            // Track the remaining part of the text to render
            auto RemainingText = InText;

            while (NOT RemainingText.empty())
            {
                size_t MatchPos = std::string::npos;
                size_t MatchLength = 0;

                // Find the earliest match across all query parts
                for (const auto& QueryPart : QueryParts)
                {
                    auto LowerQuery = QueryPart;
                    std::ranges::transform(LowerQuery, LowerQuery.begin(), ::tolower);

                    if (const auto Pos = LowerText.find(LowerQuery);
                        Pos != std::string::npos && (MatchPos == std::string::npos || Pos < MatchPos))
                    {
                        MatchPos = Pos;
                        MatchLength = QueryPart.length();
                    }
                }

                if (MatchPos == std::string::npos)
                {
                    // Render the remaining text if no matches are found
                    ImGui::TextUnformatted(RemainingText.c_str());
                    break;
                }

                // Render text before the match
                const auto BeforeMatch = RemainingText.substr(0, MatchPos);
                if (NOT BeforeMatch.empty())
                {
                    ImGui::TextUnformatted(BeforeMatch.c_str());
                    ImGui::SameLine(0.0f, 0.0f);
                }

                // Render the matched text with highlight
                const auto MatchText = RemainingText.substr(MatchPos, MatchLength);
                const auto MatchStartPos = ImGui::GetCursorScreenPos();
                const auto MatchSize = ImGui::CalcTextSize(MatchText.c_str());

                auto* DrawList = ImGui::GetWindowDrawList();
                DrawList->AddRectFilled(
                    MatchStartPos,
                    ImVec2(MatchStartPos.x + MatchSize.x, MatchStartPos.y + MatchSize.y),
                    ImGui::ColorConvertFloat4ToU32(InHighlightColor)
                );

                ImGui::TextUnformatted(MatchText.c_str());
                ImGui::SameLine(0.0f, 0.0f);

                // Adjust remaining text and lowercase text
                RemainingText = RemainingText.substr(MatchPos + MatchLength);
                LowerText = LowerText.substr(MatchPos + MatchLength);
            }
        };

        const auto AddAbilityToTable = [&](const FCk_Handle_Ability& Ability, int32 InLevel)
        {
            QUICK_SCOPE_CYCLE_COUNTER(AddAbilityToTable)
            ImGui::TableNextRow();
            const auto Index = GetTypeHash(Ability);
            ImGui::PushID(Index);

            const auto& Color = FCogImguiHelper::ToImVec4(_Config->Get_AbilityColor(Ability));
            ImGui::PushStyleColor(ImGuiCol_Text, Color);

            //------------------------
            // Activation
            //------------------------
            ImGui::TableNextColumn();
            FCogWindowWidgets::PushStyleCompact();
            auto IsActive = UCk_Utils_Ability_UE::Get_Status(Ability) == ECk_Ability_Status::Active;
            if (ImGui::Checkbox("##Activation", &IsActive))
            {
                _AbilityHandleToActivate = Ability;
            }
            FCogWindowWidgets::PopStyleCompact();

            //------------------------
            // Name
            //------------------------
            ImGui::TableNextColumn();

            const std::string AbilityName = ck::Format_ANSI(TEXT("{}{}"),
                UCk_Utils_String_UE::Get_SymbolNTimes(TEXT("  "), InLevel), DoGet_AbilityName(Ability)).c_str();

            DoGet_AbilityTimer(Ability);

            if (ck_abilities_debug_window::Filter.IsActive() && ck_abilities_debug_window::Filter.PassFilter(AbilityName.c_str()))
            {
                HighlightSearchMatch(AbilityName, ck_abilities_debug_window::Filter.InputBuf, ImVec4(0.31f, 0.31f, 0.33f, 1.f));
            }
            else if (ImGui::Selectable(AbilityName.c_str(),
                                      SelectedIndex == Index,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick))
            {
                SelectedIndex = Index;

                if (ImGui::IsMouseDoubleClicked(0))
                {
                    OpenAbility(Ability);
                }
            }

            ImGui::PopStyleColor(1);

            //------------------------
            // Popup
            //------------------------
            if (ImGui::IsItemHovered())
            {
                FCogWindowWidgets::BeginTableTooltip();
                RenderAbilityInfo(Ability);
                FCogWindowWidgets::EndTableTooltip();
            }

            //------------------------
            // ContextMenu
            //------------------------
            RenderAbilityContextMenu(Ability, Index);

            //------------------------
            // Blocked
            //------------------------
            ImGui::TableNextColumn();
            if (const auto& CanActivateResult = UCk_Utils_Ability_UE::Get_CanActivate(Ability);
                CanActivateResult != ECk_Ability_ActivationRequirementsResult::RequirementsMet &&
                CanActivateResult != ECk_Ability_ActivationRequirementsResult::RequirementsMet_ButAlreadyActive)
            {
                ImGui::Text("Blocked");
            }

            ImGui::PopID();
        };

        const auto AddAllAbilities_StopRecursing = [&](const FCk_Handle_AbilityOwner& InAbilityOwner, int32 InLevel, auto& Recurse) -> void {};
        const auto AddAllAbilities = [&](const FCk_Handle_AbilityOwner& InAbilityOwner, int32 InLevel, auto& Recurse) -> void
        {
            QUICK_SCOPE_CYCLE_COUNTER(AddAllAbilities)
            const auto* Found = _FilteredAbilities.Find(InAbilityOwner);
            if (!Found) { return; }

            for (auto& Abilities = *Found; auto Ability : Abilities)
            {
                if (UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Ability) != InAbilityOwner) { continue; }

                AddAbilityToTable(Ability, InLevel);

                if (_FilteredAbilities.Find(InAbilityOwner))
                {
                    Recurse(UCk_Utils_AbilityOwner_UE::Cast(Ability), ++InLevel, Recurse);
                    --InLevel;
                }
            }
        };

        if (_Config->ShowSubAbilities)
        {
            AddAllAbilities(InSelectionEntity, 0, AddAllAbilities);
        }
        else
        {
            AddAllAbilities(InSelectionEntity, 0, AddAllAbilities_StopRecursing);
        }

        ImGui::EndTable();
    }
}

auto
    FCk_Abilities_DebugWindow::
    RenderAbilityInfo(
        const FCk_Handle_Ability& InAbility)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(RenderAbilityInfo)
    if (ImGui::BeginTable("Ability", 2, ImGuiTableFlags_Borders))
    {
        const auto& TextColor = ImVec4{1.0f, 1.0f, 1.0f, 0.5f};

        ImGui::TableSetupColumn("Property");
        ImGui::TableSetupColumn("Value");

        const auto& AbilityColor = FCogImguiHelper::ToImVec4(_Config->Get_AbilityColor(InAbility));

        //------------------------
        // Name
        //------------------------
        const auto& AbilityName = DoGet_AbilityName(InAbility);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Name");
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, AbilityColor);
        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), AbilityName).c_str());
        ImGui::PopStyleColor(1);

        //------------------------
        // Script Asset
        //------------------------
#if WITH_EDITOR
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Script Asset");
        ImGui::TableNextColumn();
        if (ImGui::Button("Open Asset"))
        {
            const auto& AbilityScriptClass = UCk_Utils_Ability_UE::Get_ScriptClass(InAbility);
            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AbilityScriptClass->ClassGeneratedBy);
        }
#endif

        //------------------------
        // Activation
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Activation");
        ImGui::TableNextColumn();
        FCogWindowWidgets::PushStyleCompact();
        auto IsActive = UCk_Utils_Ability_UE::Get_Status(InAbility) == ECk_Ability_Status::Active;
        if (ImGui::Checkbox("##Activation", &IsActive))
        {
            _AbilityHandleToActivate = InAbility;
        }
        FCogWindowWidgets::PopStyleCompact();

        //------------------------
        // Handle
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Handle");
        ImGui::TableNextColumn();
        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), UCk_Utils_Handle_UE::Conv_HandleToString(InAbility)).c_str());

        //------------------------
        // AbilityTags
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Tag Requirements");
        ImGui::TableNextColumn();
        const auto& CanActivate = UCk_Utils_Ability_UE::Get_CanActivate(InAbility);
        ImGui::Text("%s", TCHAR_TO_ANSI(*ck::Format_UE(TEXT("{}"), CanActivate)));

        //------------------------
        // Tags on Owner
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Tags on Owner");
        ImGui::TableNextColumn();

        if (const auto MaybeOwner = UCk_Utils_Ability_UE::TryGet_Owner(InAbility);
            ck::Is_NOT_Valid(MaybeOwner))
        {
            ImGui::Text("There is no Owner");
        }
        else
        {
            for (const auto& OwnerTags = UCk_Utils_AbilityOwner_UE::Get_ActiveTags(MaybeOwner);
                const auto& Tag : OwnerTags)
            {
                ImGui::Text(ck::Format_ANSI(TEXT("{}"), Tag.GetTagName()).c_str());
            }
        }

        //------------------------
        // NetworkSettings
        //------------------------
        const auto& NetworkSettings = UCk_Utils_Ability_UE::Get_NetworkSettings(InAbility);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Replication");
        ImGui::TableNextColumn();
        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), NetworkSettings.Get_ReplicationType()).c_str());

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Execution");
        ImGui::TableNextColumn();
        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), NetworkSettings.Get_ExecutionPolicy()).c_str());

        ImGui::EndTable();
    }
}

auto
    FCk_Abilities_DebugWindow::
    RenderAbilityContextMenu(
        const FCk_Handle_Ability& InAbility,
        int32 InIndex)
    -> void
{
    if (ImGui::BeginPopupContextItem(ck::Format_ANSI(TEXT("{}"), DoGet_AbilityName(InAbility)).c_str()))
    {
        auto Open = _OpenedAbilities.Contains(InAbility);
        if (ImGui::Checkbox("Open", &Open))
        {
            if (Open)
            {
                OpenAbility(InAbility);
            }
            else
            {
                CloseAbility(InAbility);
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

auto
    FCk_Abilities_DebugWindow::
    OpenAbility(
        const FCk_Handle_Ability& InAbility)
    -> void
{
    _OpenedAbilities.AddUnique(InAbility);
}

auto
    FCk_Abilities_DebugWindow::
    CloseAbility(
        const FCk_Handle_Ability& InAbility)
    -> void
{
    _OpenedAbilities.Remove(InAbility);
}

auto
    FCk_Abilities_DebugWindow::
    ProcessAbilityActivation(
        FCk_Handle_Ability& InAbility)
    -> void
{
    auto SelectionEntity = Get_SelectionEntity();

    if (ck::Is_NOT_Valid(SelectionEntity))
    { return; }

    auto SelectedEntityAsAbilityOwner = UCk_Utils_AbilityOwner_UE::Cast(SelectionEntity);

    if (ck::Is_NOT_Valid(SelectedEntityAsAbilityOwner))
    { return; }

    switch (UCk_Utils_Ability_UE::Get_Status(InAbility))
    {
        case ECk_Ability_Status::NotActive:
        {
            UCk_Utils_AbilityOwner_UE::Request_TryActivateAbility(SelectedEntityAsAbilityOwner, FCk_Request_AbilityOwner_ActivateAbility{InAbility}, {});
            break;
        }
        case ECk_Ability_Status::Active:
        {
            UCk_Utils_AbilityOwner_UE::Request_DeactivateAbility(SelectedEntityAsAbilityOwner, FCk_Request_AbilityOwner_DeactivateAbility{InAbility}, {});
            break;
        }
    }
}

auto
    FCk_Abilities_DebugWindow::
    DoGet_AbilityName(
        const FCk_Handle_Ability& InAbility) const
        -> FName
{
    if (_Config->BlueprintNameOnly)
    {
        return UCk_Utils_Debug_UE::Get_DebugName(UCk_Utils_Ability_UE::Get_ScriptClass(InAbility), ECk_DebugNameVerbosity_Policy::Compact);
    }

    return UCk_Utils_Ability_UE::Get_DisplayName(InAbility);
}

auto
    FCk_Abilities_DebugWindow::
    DoGet_AbilityTimer(
        const FCk_Handle_Ability& InAbility)
        -> FString
{
    auto AllTimers = FString{};
    UCk_Utils_Timer_UE::ForEach_Timer(InAbility, [&](const FCk_Handle_Timer& InTimer)
    {
        if (UCk_Utils_Timer_UE::Get_Name(InTimer) == Tag_Timer_CategoryName)
        { return; }

        const auto& Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(InTimer);
        const auto TimerName = UCk_Utils_Timer_UE::Get_Name(InTimer);
        const auto ElapsedTime = Chrono.Get_TimeElapsed().Get_Seconds();
        const auto Duration = Chrono.Get_GoalValue().Get_Seconds();

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, IM_COL32(100, 100, 100, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 100));

        ImGui::ProgressBar(ElapsedTime / Duration, ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 0.8f), "");
        ImGui::SameLine();
        const auto& GroupSize = ImGui::GetItemRectSize();

        // Centered text
        const auto FormattedString = [&]()
        {
            if (UCk_Utils_GameplayLabel_UE::Get_IsUnnamedLabel(InTimer))
            {
                return ck::Format_ANSI(TEXT("[{:.2f}/{:.2f}]"), ElapsedTime, Duration);
            }
            else
            {
                return ck::Format_ANSI(TEXT("{}[{:.2f}/{:.2f}]"), TimerName, ElapsedTime, Duration);
            }
        }();
        const char* Text = FormattedString.c_str();
        const auto& TextSize = ImGui::CalcTextSize(Text);
        const auto& ProgressBarPos = ImGui::GetCursorPos();

        const auto TextPosX = ProgressBarPos.x - (GroupSize.x * 0.5) - (0.6 * TextSize.x);
        const auto TextPos = ImVec2(TextPosX, ProgressBarPos.y);
        ImGui::SetCursorPos(TextPos);
        ImGui::Text(Text);

        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    });

    return AllTimers;
}

auto
    FCk_Abilities_DebugWindow::
    AddToFilteredAbilities(
        FCk_Handle_AbilityOwner InAbilityOwner)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(AddToFilteredAbilities)

    UCk_Utils_AbilityOwner_UE::ForEach_Ability(InAbilityOwner, [this, InAbilityOwner](const FCk_Handle_Ability& InAbility)
    {
        const auto& AbilityLifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InAbility);

        // ForEach_Ability will recursively go through all extensions even if they aren't directly a child of InAbilityOwner.
        // We only care about direct children of the ability owner, and will recurse to find deeper children,
        // We need to recurse manually instead of relying on extensions since we want to find non-extension abilities as well
        if (AbilityLifetimeOwner != InAbilityOwner)
        { return; }

        if (UCk_Utils_AbilityOwner_UE::Has(InAbility))
        {
            AddToFilteredAbilities(UCk_Utils_AbilityOwner_UE::CastChecked(InAbility));
        }

        const auto& CanActivateResult = UCk_Utils_Ability_UE::Get_CanActivate(InAbility);
        const auto& ActivationStatus = UCk_Utils_Ability_UE::Get_Status(InAbility);

        const auto& IsJustBlocked  = CanActivateResult != ECk_Ability_ActivationRequirementsResult::RequirementsMet && CanActivateResult != ECk_Ability_ActivationRequirementsResult::RequirementsMet_ButAlreadyActive;
        const auto& IsJustActive   = ActivationStatus == ECk_Ability_Status::Active && NOT IsJustBlocked ;
        const auto& IsJustInactive = ActivationStatus == ECk_Ability_Status::NotActive && NOT IsJustBlocked;

        if (NOT _Config->ShowBlocked && IsJustBlocked)
        { return; }

        if (NOT _Config->ShowActive && IsJustActive)
        { return; }

        if (NOT _Config->ShowInactive && IsJustInactive)
        { return; }

        if (const auto& AbilityName = DoGet_AbilityName(InAbility);
            NOT ck_abilities_debug_window::Filter.PassFilter(TCHAR_TO_ANSI(*AbilityName.ToString())))
        {
            auto OwnerMaybeAbility = UCk_Utils_Ability_UE::Cast(UCk_Utils_Ability_UE::TryGet_Owner(InAbility));
            auto Found = false;
            while(ck::IsValid(OwnerMaybeAbility))
            {
                if (const auto& AbilityOwnerName = DoGet_AbilityName(OwnerMaybeAbility);
                    ck_abilities_debug_window::Filter.PassFilter(TCHAR_TO_ANSI(*AbilityOwnerName.ToString())))
                {
                    Found = true;
                    break;
                }
                OwnerMaybeAbility = UCk_Utils_Ability_UE::Cast(UCk_Utils_Ability_UE::TryGet_Owner(OwnerMaybeAbility));
            }

            if (NOT Found)
            { return; }
        }
        else
        {
            auto OwnerMaybeAbility = UCk_Utils_Ability_UE::Cast(UCk_Utils_Ability_UE::TryGet_Owner(InAbility));
            while(ck::IsValid(OwnerMaybeAbility))
            {
                _FilteredAbilities.FindOrAdd(UCk_Utils_Ability_UE::TryGet_Owner(OwnerMaybeAbility)).AddUnique(OwnerMaybeAbility);
                OwnerMaybeAbility = UCk_Utils_Ability_UE::Cast(UCk_Utils_Ability_UE::TryGet_Owner(OwnerMaybeAbility));
            }
        }

        auto& Abilities = _FilteredAbilities.FindOrAdd(InAbilityOwner);
        Abilities.AddUnique(InAbility);
    });
}

// --------------------------------------------------------------------------------------------------------------------
