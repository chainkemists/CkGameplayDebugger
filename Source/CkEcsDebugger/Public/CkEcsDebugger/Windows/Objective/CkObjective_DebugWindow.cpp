#include "CkObjective_DebugWindow.h"

#include "CogImguiHelper.h"
#include <Cog/Public/CogWidgets.h>

#include "CkObjective/Objective/CkObjective_Utils.h"
#include "CkObjective/Objective/CkObjective_Fragment.h"
#include "CkObjective/ObjectiveOwner/CkObjectiveOwner_Utils.h"
#include "CkObjective/ObjectiveOwner/CkObjectiveOwner_Fragment.h"

#include "CkEcs/EntityScript/CkEntityScript_Fragment.h"
#include "CkEcs/EntityScript/CkEntityScript_Utils.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

#if WITH_EDITOR
#include <Editor.h>
#include <Subsystems/AssetEditorSubsystem.h>
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_objectives_debug_window
{
    static ImGuiTextFilter Filter;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_Objective_DebugWindow::
    Initialize() -> void
{
    Super::Initialize();

    bHasMenu = true;

    _Config = GetConfig<UCk_Objective_DebugWindowConfig>();
}

auto
    FCk_Objective_DebugWindow::
    ResetConfig()
    -> void
{
    Super::ResetConfig();

    _Config->Reset();
}

auto
    FCk_Objective_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text
    (
        "This window displays the objectives of the selected entities in a tree structure. "
        "Click the status icon or use the action buttons to change objective states. "
        "Right click an objective to open it in a separate window. "
        "When multiple entities are selected, objectives from all entities are shown."
    );
}

auto
    FCk_Objective_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    RenderMenu();

    auto SelectionEntities = Get_SelectionEntities();

    if (SelectionEntities.IsEmpty())
    {
        ImGui::Text("No entities selected");
        return;
    }

    // Check if any selected entity has objectives
    auto HasAnyObjectives = false;
    TArray<FCk_Handle_ObjectiveOwner> ObjectiveOwners;

    for (const auto& Entity : SelectionEntities)
    {
        if (ck::Is_NOT_Valid(Entity))
        { continue; }

        // Check if entity is an ObjectiveOwner
        if (auto EntityAsObjectiveOwner = UCk_Utils_ObjectiveOwner_UE::Cast(Entity);
            ck::IsValid(EntityAsObjectiveOwner))
        {
            ObjectiveOwners.Add(EntityAsObjectiveOwner);
            HasAnyObjectives = true;
        }

        // Also check if entity is an Objective itself (it might be selected directly)
        if (auto EntityAsObjective = UCk_Utils_Objective_UE::Cast(Entity);
            ck::IsValid(EntityAsObjective))
        {
            // Get the owner of this objective
            auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(EntityAsObjective);
            if (auto OwnerAsObjectiveOwner = UCk_Utils_ObjectiveOwner_UE::Cast(LifetimeOwner);
                ck::IsValid(OwnerAsObjectiveOwner))
            {
                ObjectiveOwners.AddUnique(OwnerAsObjectiveOwner);
                HasAnyObjectives = true;
            }
        }
    }

    if (NOT HasAnyObjectives)
    {
        if (SelectionEntities.Num() == 1)
        {
            ImGui::Text("Selection entity has no objectives");
        }
        else
        {
            ImGui::Text("Selected entities have no objectives");
        }
        return;
    }

    RenderObjectiveTree(ObjectiveOwners);
}

auto
    FCk_Objective_DebugWindow::
    RenderTick(
        float InDeltaT)
    -> void
{
    Super::RenderTick(InDeltaT);

    RenderOpenedObjectives();
}

auto
    FCk_Objective_DebugWindow::
    GameTick(
        float InDeltaT)
    -> void
{
    Super::GameTick(InDeltaT);

    if (ck::IsValid(_ObjectiveHandleToProcess))
    {
        ProcessObjectiveAction(_ObjectiveHandleToProcess);
        _ObjectiveHandleToProcess = {};
    }
}

auto
    FCk_Objective_DebugWindow::
    RenderMenu()
    -> void
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Separator();

            ImGui::Checkbox("Sort by Name", &_Config->SortByName);
            ImGui::Checkbox("Show Not Started", &_Config->ShowNotStarted);
            ImGui::Checkbox("Show Active", &_Config->ShowActive);
            ImGui::Checkbox("Show Completed", &_Config->ShowCompleted);
            ImGui::Checkbox("Show Failed", &_Config->ShowFailed);
            ImGui::Checkbox("Show Sub-Objectives", &_Config->ShowSubObjectives);

            ImGui::Separator();

            ImGui::ColorEdit4("Not Started Color", reinterpret_cast<float*>(&_Config->NotStartedColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Active Color", reinterpret_cast<float*>(&_Config->ActiveColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Completed Color", reinterpret_cast<float*>(&_Config->CompletedColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Failed Color", reinterpret_cast<float*>(&_Config->FailedColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);

            ImGui::Separator();

            if (ImGui::MenuItem("Reset"))
            {
                ResetConfig();
            }

            ImGui::EndMenu();
        }

        FCogWidgets::SearchBar("##Filter", ck_objectives_debug_window::Filter);

        ImGui::EndMenuBar();
    }
}

auto
    FCk_Objective_DebugWindow::
    RenderOpenedObjectives()
    -> void
{
    if (const auto& SelectionEntities = Get_SelectionEntities();
        SelectionEntities.IsEmpty())
    { return; }

    for (int32 Index = _OpenedObjectives.Num() - 1; Index >= 0; --Index)
    {
        const auto& ObjectiveOpened = _OpenedObjectives[Index];

        auto Open = true;

        if (const auto& ObjectiveName = DoGet_ObjectiveName(ObjectiveOpened);
            ImGui::Begin(ck::Format_ANSI(TEXT("{}"), ObjectiveName).c_str(), &Open))
        {
            RenderObjectiveInfo(ObjectiveOpened);
            ImGui::End();
        }

        if (NOT Open)
        {
            _OpenedObjectives.RemoveAt(Index);
        }
    }
}

auto
    FCk_Objective_DebugWindow::
    RenderObjectiveTree(
        const TArray<FCk_Handle_ObjectiveOwner>& InObjectiveOwners)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_Objective_DebugWindow_RenderObjectiveTree)
    _FilteredObjectives.Reset();

    // Collect objectives from all objective owners
    for (const auto& ObjectiveOwner : InObjectiveOwners)
    {
        AddToFilteredObjectives(ObjectiveOwner);
    }

    // Sort objectives if needed
    for (auto& KeyVal : _FilteredObjectives)
    {
        auto& Objectives = KeyVal.Value;

        if (_Config->SortByName)
        {
            Objectives.Sort([&](const FCk_Handle_Objective& InObjective1, const FCk_Handle_Objective& InObjective2)
            {
                const auto& ObjectiveName1 = DoGet_ObjectiveName(InObjective1);
                const auto& ObjectiveName2 = DoGet_ObjectiveName(InObjective2);

                return ObjectiveName1.CompareTo(ObjectiveName2) < 0;
            });
        }
    }

    // Begin scrollable region for the tree
    ImGui::BeginChild("ObjectivesTree", ImVec2(-1, -ImGui::GetTextLineHeightWithSpacing() * 2), false);

    // Render objectives for each objective owner
    for (const auto& ObjectiveOwner : InObjectiveOwners)
    {
        RenderObjectiveOwnerTree(ObjectiveOwner);
    }

    ImGui::EndChild();

    // Show summary
    int32 TotalObjectives = 0;
    for (const auto& KeyVal : _FilteredObjectives)
    {
        TotalObjectives += KeyVal.Value.Num();
    }

    ImGui::Separator();

    if (TotalObjectives == 0)
    {
        if (InObjectiveOwners.Num() == 1)
        {
            ImGui::Text("Selected entity has no objectives matching filters");
        }
        else
        {
            ImGui::Text("Selected entities have no objectives matching filters");
        }
    }
    else
    {
        ImGui::Text("Found %d objectives across %d entities", TotalObjectives, InObjectiveOwners.Num());
    }
}

auto
    FCk_Objective_DebugWindow::
    RenderObjectiveOwnerTree(
        const FCk_Handle_ObjectiveOwner& InObjectiveOwner)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(RenderObjectiveOwnerTree)

    const auto* Found = _FilteredObjectives.Find(InObjectiveOwner);
    if (NOT Found)
    { return; }

    for (const auto& Objective : *Found)
    {
        // Only render top-level objectives (those owned directly by the ObjectiveOwner)
        if (UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Objective) == InObjectiveOwner)
        {
            RenderObjectiveNode(Objective);
        }
    }
}

auto
    FCk_Objective_DebugWindow::
    RenderObjectiveNode(
        const FCk_Handle_Objective& InObjective)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(RenderObjectiveNode)

    const auto Index = GetTypeHash(InObjective);
    ImGui::PushID(Index);

    const auto& Color = FCogImguiHelper::ToImVec4(_Config->Get_ObjectiveColor(InObjective));
    const auto Status = UCk_Utils_Objective_UE::Get_Status(InObjective);

    // Check if this objective has sub-objectives that pass the filter
    auto HasVisibleSubObjectives = false;
    if (_Config->ShowSubObjectives && UCk_Utils_ObjectiveOwner_UE::Has(InObjective))
    {
        if (auto ObjectiveAsOwner = UCk_Utils_ObjectiveOwner_UE::Cast(InObjective);
            ck::IsValid(ObjectiveAsOwner))
        {
            if (const auto* SubObjectives = _FilteredObjectives.Find(ObjectiveAsOwner))
            {
                HasVisibleSubObjectives = SubObjectives->Num() > 0;
            }
        }
    }

    // Setup tree node flags
    ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowOverlap;

    // Check if this objective matches the current filter
    const auto& ObjectiveName = DoGet_ObjectiveName(InObjective);
    const auto MatchesFilter = ck_objectives_debug_window::Filter.PassFilter(TCHAR_TO_ANSI(*ObjectiveName.ToString()));

    // Auto-expand nodes that have matching children but don't match themselves
    if (HasVisibleSubObjectives && NOT MatchesFilter)
    {
        NodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    if (NOT HasVisibleSubObjectives)
    {
        NodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // Rest of the rendering code remains the same...
    // Status icon
    const char* StatusIcon = nullptr;
    switch (Status)
    {
        case ECk_ObjectiveStatus::NotStarted:
        {
            StatusIcon = "[ ]";
            break;
        }
        case ECk_ObjectiveStatus::Active:
        {
            StatusIcon = "[>]";
            break;
        }
        case ECk_ObjectiveStatus::Completed:
        {
            StatusIcon = "[O]";
            break;
        }
        case ECk_ObjectiveStatus::Failed:
        {
            StatusIcon = "[X]";
            break;
        }
        default:
        {
            CK_INVALID_ENUM(Status);
            break;
        }
    }

    // Highlight matching text
    ImGui::PushStyleColor(ImGuiCol_Text, Color);

    const auto& DisplayName = UCk_Utils_Objective_UE::Get_DisplayName(InObjective);
    const auto NodeLabel = ck::Format_ANSI(TEXT("{} {} - {}"),
        FString(StatusIcon),
        UCk_Utils_Objective_UE::Get_Name(InObjective),
        DisplayName);

    auto NodeOpen = false;
    if (HasVisibleSubObjectives)
    {
        NodeOpen = ImGui::TreeNodeEx(NodeLabel.c_str(), NodeFlags);
    }
    else
    {
        ImGui::TreeNodeEx(NodeLabel.c_str(), NodeFlags);
    }

    ImGui::PopStyleColor();

    // Handle selection and context menu (unchanged)
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        _SelectedObjective = InObjective;
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup(ck::Format_ANSI(TEXT("ObjectiveContext_{}"), Index).c_str());
    }

    // Tooltip on hover (unchanged)
    if (ImGui::IsItemHovered())
    {
        FCogWidgets::BeginTableTooltip();
        RenderObjectiveInfo(InObjective);
        FCogWidgets::EndTableTooltip();
    }

    // Action buttons (unchanged)
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);

    FCogWidgets::PushStyleCompact();

    switch (Status)
    {
        case ECk_ObjectiveStatus::NotStarted:
        {
            if (ImGui::SmallButton(ck::Format_ANSI(TEXT("Start##{}"), Index).c_str()))
            {
                _ObjectiveHandleToProcess = InObjective;
            }
            break;
        }
        case ECk_ObjectiveStatus::Active:
        {
            if (ImGui::SmallButton(ck::Format_ANSI(TEXT("Complete##{}"), Index).c_str()))
            {
                _ObjectiveHandleToProcess = InObjective;
                _PendingAction = ObjectiveAction::Complete;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(ck::Format_ANSI(TEXT("Fail##{}"), Index).c_str()))
            {
                _ObjectiveHandleToProcess = InObjective;
                _PendingAction = ObjectiveAction::Fail;
            }
            break;
        }
        case ECk_ObjectiveStatus::Completed:
        case ECk_ObjectiveStatus::Failed:
        {
            ImGui::Text("-");
            break;
        }
    }

    FCogWidgets::PopStyleCompact();

    // Context menu (unchanged)
    if (ImGui::BeginPopup(ck::Format_ANSI(TEXT("ObjectiveContext_{}"), Index).c_str()))
    {
        if (ImGui::MenuItem("Open in Window"))
        {
            OpenObjective(InObjective);
        }

        ImGui::EndPopup();
    }

    // Render sub-objectives if node is open
    if (NodeOpen && HasVisibleSubObjectives)
    {
        if (auto ObjectiveAsOwner = UCk_Utils_ObjectiveOwner_UE::Cast(InObjective);
            ck::IsValid(ObjectiveAsOwner))
        {
            if (const auto* SubObjectives = _FilteredObjectives.Find(ObjectiveAsOwner))
            {
                for (const auto& SubObjective : *SubObjectives)
                {
                    if (UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(SubObjective) == ObjectiveAsOwner)
                    {
                        RenderObjectiveNode(SubObjective);
                    }
                }
            }
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}


auto
    FCk_Objective_DebugWindow::
    RenderObjectiveInfo(
        const FCk_Handle_Objective& InObjective)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(RenderObjectiveInfo)
    if (ImGui::BeginTable("Objective", 2, ImGuiTableFlags_Borders))
    {
        const auto& TextColor = ImVec4{1.0f, 1.0f, 1.0f, 0.5f};

        ImGui::TableSetupColumn("Property");
        ImGui::TableSetupColumn("Value");

        const auto& ObjectiveColor = FCogImguiHelper::ToImVec4(_Config->Get_ObjectiveColor(InObjective));

        //------------------------
        // Name
        //------------------------
        const auto& ObjectiveName = UCk_Utils_Objective_UE::Get_Name(InObjective);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Name");
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, ObjectiveColor);
        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), ObjectiveName).c_str());
        ImGui::PopStyleColor(1);

        //------------------------
        // Display Name
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Display Name");
        ImGui::TableNextColumn();
        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), UCk_Utils_Objective_UE::Get_DisplayName(InObjective)).c_str());

        //------------------------
        // Description
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Description");
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", ck::Format_ANSI(TEXT("{}"), UCk_Utils_Objective_UE::Get_Description(InObjective)).c_str());

        //------------------------
        // Status
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Status");
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, ObjectiveColor);
        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), UCk_Utils_Objective_UE::Get_Status(InObjective)).c_str());
        ImGui::PopStyleColor(1);

        //------------------------
        // Handle
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Handle");
        ImGui::TableNextColumn();
        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), UCk_Utils_Handle_UE::Conv_HandleToString(InObjective)).c_str());

        //------------------------
        // Owner Entity
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Owner Entity");
        ImGui::TableNextColumn();
        if (const auto& OwnerEntity = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InObjective);
            ck::IsValid(OwnerEntity))
        {
            const auto& OwnerName = UCk_Utils_Handle_UE::Get_DebugName(OwnerEntity);
            ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), OwnerName).c_str());
        }
        else
        {
            ImGui::Text("No Owner");
        }

        //------------------------
        // Completion Tag (if completed)
        //------------------------
        const auto& Current = InObjective.Get<ck::FFragment_Objective_Current>();
        if (UCk_Utils_Objective_UE::Get_Status(InObjective) == ECk_ObjectiveStatus::Completed &&
            Current.Get_CompletionTag().IsValid())
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(TextColor, "Completion Tag");
            ImGui::TableNextColumn();
            ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Current.Get_CompletionTag().GetTagName()).c_str());
        }

        //------------------------
        // Failure Tag (if failed)
        //------------------------
        if (UCk_Utils_Objective_UE::Get_Status(InObjective) == ECk_ObjectiveStatus::Failed &&
            Current.Get_FailureTag().IsValid())
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(TextColor, "Failure Tag");
            ImGui::TableNextColumn();
            ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Current.Get_FailureTag().GetTagName()).c_str());
        }

#if WITH_EDITOR
        //------------------------
        // Script Asset
        //------------------------
        if (UCk_Utils_EntityScript_UE::Has(InObjective))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(TextColor, "Script Asset");
            ImGui::TableNextColumn();
            if (ImGui::Button("Open Asset"))
            {
                const auto& EntityScriptFragment = InObjective.Get<ck::FFragment_EntityScript_Current>();
                if (const auto& Script = EntityScriptFragment.Get_Script().Get();
                    ck::IsValid(Script))
                {
                    GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Script);
                }
            }
        }
#endif

        ImGui::EndTable();
    }
}

auto
    FCk_Objective_DebugWindow::
    OpenObjective(
        const FCk_Handle_Objective& InObjective)
    -> void
{
    _OpenedObjectives.AddUnique(InObjective);
}

auto
    FCk_Objective_DebugWindow::
    CloseObjective(
        const FCk_Handle_Objective& InObjective)
    -> void
{
    _OpenedObjectives.Remove(InObjective);
}

auto
    FCk_Objective_DebugWindow::
    ProcessObjectiveAction(
        FCk_Handle_Objective& InObjective)
    -> void
{
    if (_PendingAction == ObjectiveAction::Fail)
    {
        UCk_Utils_Objective_UE::Request_Fail(InObjective, FCk_Request_Objective_Fail{});
        _PendingAction = ObjectiveAction::None;
        return;
    }

    switch (const auto Status = UCk_Utils_Objective_UE::Get_Status(InObjective))
    {
        case ECk_ObjectiveStatus::NotStarted:
        {
            UCk_Utils_Objective_UE::Request_Start(InObjective, FCk_Request_Objective_Start{});
            break;
        }
        case ECk_ObjectiveStatus::Active:
        {
            if (_PendingAction == ObjectiveAction::Complete)
            {
                UCk_Utils_Objective_UE::Request_Complete(InObjective, FCk_Request_Objective_Complete{});
            }
            break;
        }
        default:
        {
            CK_INVALID_ENUM(Status);
            break;
        }
    }
    _PendingAction = ObjectiveAction::None;
}

auto
    FCk_Objective_DebugWindow::
    DoGet_ObjectiveName(
        const FCk_Handle_Objective& InObjective) const
    -> FText
{
    const auto& DisplayName = UCk_Utils_Objective_UE::Get_DisplayName(InObjective);
    return FText::FromString(DisplayName.ToString());
}

auto
    FCk_Objective_DebugWindow::
    AddToFilteredObjectives(
        FCk_Handle_ObjectiveOwner InObjectiveOwner)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(AddToFilteredObjectives)

    UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(InObjectiveOwner,
    [this, InObjectiveOwner](const FCk_Handle_Objective& InObjective)
    {
        const auto& ObjectiveLifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InObjective);

        // Only care about direct children of the objective owner
        if (ObjectiveLifetimeOwner != InObjectiveOwner)
        { return; }

        // Apply status filters first
        if (NOT PassesStatusFilter(InObjective))
        { return; }

        // Check if this objective or any of its children should be shown
        if (ShouldShowObjective(InObjective, InObjectiveOwner))
        {
            auto& Objectives = _FilteredObjectives.FindOrAdd(InObjectiveOwner);
            Objectives.AddUnique(InObjective);

            // If this objective is also an ObjectiveOwner, recursively process its children
            if (UCk_Utils_ObjectiveOwner_UE::Has(InObjective))
            {
                AddToFilteredObjectives(UCk_Utils_ObjectiveOwner_UE::CastChecked(InObjective));
            }
        }
    });
}

auto
    FCk_Objective_DebugWindow::
    PassesStatusFilter(
        const FCk_Handle_Objective& InObjective) const
    -> bool
{
    switch (const auto& Status = UCk_Utils_Objective_UE::Get_Status(InObjective))
    {
        case ECk_ObjectiveStatus::NotStarted:
        {
            return _Config->ShowNotStarted;
        }
        case ECk_ObjectiveStatus::Active:
        {
            return _Config->ShowActive;
        }
        case ECk_ObjectiveStatus::Completed:
        {
            return _Config->ShowCompleted;
        }
        case ECk_ObjectiveStatus::Failed:
        {
            return _Config->ShowFailed;
        }
        default:
        {
            CK_INVALID_ENUM(Status);
            return false;
        }
    }
}

auto
    FCk_Objective_DebugWindow::
    ShouldShowObjective(
        const FCk_Handle_Objective& InObjective,
        const FCk_Handle_ObjectiveOwner& InRootOwner) const
    -> bool
{
    // Check if this objective matches the name filter
    if (const auto& ObjectiveName = DoGet_ObjectiveName(InObjective);
        ck_objectives_debug_window::Filter.PassFilter(TCHAR_TO_ANSI(*ObjectiveName.ToString())))
    {  return true; }

    // Check if any child objectives match (recursive)
    if (NOT UCk_Utils_ObjectiveOwner_UE::Has(InObjective))
    { return false; }

    if (auto ObjectiveAsOwner = UCk_Utils_ObjectiveOwner_UE::Cast(InObjective);
        ck::IsValid(ObjectiveAsOwner))
    {
        bool HasMatchingChild = false;
        UCk_Utils_ObjectiveOwner_UE::ForEach_Objective(ObjectiveAsOwner,
        [&](const FCk_Handle_Objective& ChildObjective)
        {
            if (UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(ChildObjective) == ObjectiveAsOwner)
            {
                if (ShouldShowObjective(ChildObjective, InRootOwner))
                {
                    HasMatchingChild = true;
                }
            }
        });

        if (HasMatchingChild)
        {  return true; }
    }

    return false;
}

// --------------------------------------------------------------------------------------------------------------------