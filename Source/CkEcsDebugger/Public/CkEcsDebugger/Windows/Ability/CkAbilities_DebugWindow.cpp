#include "CkAbilities_DebugWindow.h"

#include "CogImguiHelper.h"
#include "CogWindowWidgets.h"

#include "CkAbility/Ability/CkAbility_Script.h"
#include "CkAbility/Ability/CkAbility_Utils.h"
#include "CkAbility/AbilityOwner/CkAbilityOwner_Utils.h"

#include "CkCore/String/CkString_Utils.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

#if WITH_EDITOR
#include <Editor.h>
#include <Subsystems/AssetEditorSubsystem.h>
#endif
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

        FCogWindowWidgets::SearchBar(_Filter);

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
            ImGui::Begin(TCHAR_TO_ANSI(*AbilityName.ToString()), &Open))
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
        FCk_Handle_AbilityOwner& InSelectionEntity)
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

        // TODO: Display Conditions/Cost/Cooldown

        static int32 SelectedIndex = -1;

        const auto AddAbilityToTable = [&](const FCk_Handle_Ability& Ability, int32 InLevel)
        {
            QUICK_SCOPE_CYCLE_COUNTER(AddAbilityToTable)
            ImGui::TableNextRow();

            const auto Index = GetTypeHash(Ability);

            ImGui::PushID(GetTypeHash(Ability));

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

            if (const auto& AbilityName = DoGet_AbilityName(Ability);
                ImGui::Selectable(ck::Format_ANSI(TEXT("{} {}"), UCk_Utils_String_UE::Get_SymbolNTimes(TEXT("  "), InLevel), AbilityName).c_str(),
                    SelectedIndex == Index, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick))
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
                CanActivateResult != ECk_Ability_ActivationRequirementsResult::RequirementsMet && CanActivateResult != ECk_Ability_ActivationRequirementsResult::RequirementsMet_ButAlreadyActive)
            {
                ImGui::Text("Blocked");
            }

            ImGui::PopID();
        };

        const auto AddAllAbilities_StopRecursing = [&](const FCk_Handle_AbilityOwner& InAbilityOwner, int32 InLevel, auto& Recurse) -> void { };
        const auto AddAllAbilities = [&](const FCk_Handle_AbilityOwner& InAbilityOwner, int32 InLevel, auto& Recurse) -> void
        {
            QUICK_SCOPE_CYCLE_COUNTER(AddAllAbilities)
            const auto* Found = _FilteredAbilities.Find(InAbilityOwner);
            if (ck::Is_NOT_Valid(Found, ck::IsValid_Policy_NullptrOnly{}))
            { return; }

            for (auto& Abilities = *Found; auto Ability : Abilities)
            {
                if (UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Ability) != InAbilityOwner)
                { continue; }

                AddAbilityToTable(Ability, InLevel);

                if (_FilteredAbilities.Find(InAbilityOwner))
                {
                    Recurse(UCk_Utils_AbilityOwner_UE::Cast(Ability), ++InLevel, Recurse);
                    --InLevel;
                }
            }
        };

        if (_Config->ShowSubAbilities)
        { AddAllAbilities(InSelectionEntity, 0, AddAllAbilities); }
        else
        { AddAllAbilities(InSelectionEntity, 0, AddAllAbilities_StopRecursing); }

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
        ImGui::Text("%s", TCHAR_TO_ANSI(*AbilityName.ToString()));
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
        ImGui::Text("%s", TCHAR_TO_ANSI(*UCk_Utils_Handle_UE::Conv_HandleToString(InAbility)));

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
        const auto MaybeOwner = UCk_Utils_Ability_UE::TryGet_Owner(InAbility);

        if (ck::Is_NOT_Valid(MaybeOwner))
        { ImGui::Text("There is no Owner"); }
        else
        {
            const auto& OwnerTags = UCk_Utils_AbilityOwner_UE::Get_ActiveTags(MaybeOwner);

            for (const auto& Tag : OwnerTags)
            {
                ImGui::Text(CK_ANSI_TEXT("{}", *Tag.ToString()));
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
        ImGui::Text("%s", TCHAR_TO_ANSI(*ck::Format_UE(TEXT("{}"), NetworkSettings.Get_ReplicationType())));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Execution");
        ImGui::TableNextColumn();
        ImGui::Text("%s", TCHAR_TO_ANSI(*ck::Format_UE(TEXT("{}"), NetworkSettings.Get_ExecutionPolicy())));

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
    if (ImGui::BeginPopupContextItem())
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
            NOT _Filter.PassFilter(TCHAR_TO_ANSI(*AbilityName.ToString())))
        { return; }

        auto& Abilities = _FilteredAbilities.FindOrAdd(InAbilityOwner);
        Abilities.AddUnique(InAbility);
    });
}

// --------------------------------------------------------------------------------------------------------------------
