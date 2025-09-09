#include "CkInteractTarget_DebugWindow.h"

#include "CogImguiHelper.h"
#include "CogWidgets.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkInteraction/InteractTarget/CkInteractTarget_Utils.h"
#include "CkInteraction/Interaction/CkInteraction_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_InteractTarget_DebugWindow::
    Initialize() -> void
{
    Super::Initialize();

    bHasMenu = true;

    _Config = GetConfig<UCk_InteractTarget_DebugWindowConfig>();
}

auto
    FCk_InteractTarget_DebugWindow::
    ResetConfig()
    -> void
{
    Super::ResetConfig();

    _Config->Reset();
}

auto
    FCk_InteractTarget_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text(
        "This window displays InteractTarget information for the selected entities. "
        "Shows target parameters, current interactions, and interaction details. "
        "Use the controls to start/cancel interactions and enable/disable targets. "
        "When multiple entities are selected, targets from all entities are shown."
    );
}

auto
    FCk_InteractTarget_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    RenderMenu();

    const auto SelectionEntities = Get_SelectionEntities();

    if (SelectionEntities.IsEmpty())
    {
        ImGui::Text("No entities selected");
        return;
    }

    // Find all entities that have InteractTargets
    TArray<FCk_Handle> EntitiesWithTargets;
    for (const auto& Entity : SelectionEntities)
    {
        UCk_Utils_InteractTarget_UE::ForEach_InteractTarget(Entity, [&](FCk_Handle_InteractTarget Target)
        {
            if (ck::IsValid(Target))
            {
                EntitiesWithTargets.AddUnique(Entity);
            }
        });
    }

    if (EntitiesWithTargets.IsEmpty())
    {
        if (SelectionEntities.Num() == 1)
        {
            ImGui::Text("Selected entity has no InteractTargets");
        }
        else
        {
            ImGui::Text("Selected entities have no InteractTargets");
        }
        return;
    }

    // Show summary for multiple entities
    if (EntitiesWithTargets.Num() > 1)
    {
        ImGui::Text("Multiple entities with InteractTargets selected (%d)", EntitiesWithTargets.Num());
        ImGui::Separator();
    }

    for (int32 EntityIndex = 0; EntityIndex < EntitiesWithTargets.Num(); ++EntityIndex)
    {
        const auto& Entity = EntitiesWithTargets[EntityIndex];

        const auto& SectionTitle = ck::Format_UE(TEXT("Entity {}"), Entity);

        if (ImGui::CollapsingHeader(ck::Format_ANSI(TEXT("{}"), SectionTitle).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            RenderEntityInteractTargetSection(Entity);
            ImGui::Unindent();
        }

        if (EntityIndex < EntitiesWithTargets.Num() - 1)
        {
            ImGui::Separator();
            ImGui::Spacing();
        }
    }
}

auto
    FCk_InteractTarget_DebugWindow::
    RenderMenu()
    -> void
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Separator();

            ImGui::Checkbox("Sort by Name", &_Config->SortByName);

            ImGui::Separator();

            ImGui::Checkbox("Show Enabled Targets", &_Config->ShowEnabledTargets);
            ImGui::Checkbox("Show Disabled Targets", &_Config->ShowDisabledTargets);
            ImGui::Checkbox("Show Active Interactions", &_Config->ShowActiveInteractions);
            ImGui::Checkbox("Show Interaction Details", &_Config->ShowInteractionDetails);
            ImGui::Checkbox("Show Parameters", &_Config->ShowParameters);

            ImGui::Separator();

            ImGui::ColorEdit4("Enabled Target Color", reinterpret_cast<float*>(&_Config->EnabledTargetColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Disabled Target Color", reinterpret_cast<float*>(&_Config->DisabledTargetColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Active Interaction Color", reinterpret_cast<float*>(&_Config->ActiveInteractionColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Inactive Interaction Color", reinterpret_cast<float*>(&_Config->InactiveInteractionColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);

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
    FCk_InteractTarget_DebugWindow::
    RenderEntityInteractTargetSection(
        const FCk_Handle& InEntity)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_InteractTarget_DebugWindow_RenderEntityInteractTargetSection)

    // Get all InteractTargets for this entity
    TArray<FCk_Handle_InteractTarget> InteractTargets;
    UCk_Utils_InteractTarget_UE::ForEach_InteractTarget(InEntity, [&](FCk_Handle_InteractTarget Target)
    {
        if (ck::IsValid(Target))
        {
            InteractTargets.Add(Target);
        }
    });

    if (InteractTargets.IsEmpty())
    {
        ImGui::Text("Entity has no valid InteractTargets");
        return;
    }

    // Sort targets if requested
    if (_Config->SortByName)
    {
        InteractTargets.Sort([](const FCk_Handle_InteractTarget& A, const FCk_Handle_InteractTarget& B)
        {
            const auto ChannelA = UCk_Utils_InteractTarget_UE::Get_InteractionChannel(A);
            const auto ChannelB = UCk_Utils_InteractTarget_UE::Get_InteractionChannel(B);
            return ck::Format_UE(TEXT("{}"), ChannelA) < ck::Format_UE(TEXT("{}"), ChannelB);
        });
    }

    // Render each InteractTarget
    for (int32 TargetIndex = 0; TargetIndex < InteractTargets.Num(); ++TargetIndex)
    {
        const auto& Target = InteractTargets[TargetIndex];
        const auto& Channel = UCk_Utils_InteractTarget_UE::Get_InteractionChannel(Target);
        const auto IsEnabled = UCk_Utils_InteractTarget_UE::Get_Enabled(Target) == ECk_EnableDisable::Enable;

        // Apply filters
        if ((NOT _Config->ShowEnabledTargets && IsEnabled) ||
            (NOT _Config->ShowDisabledTargets && NOT IsEnabled))
        {
            continue;
        }

        if (NOT _Filter.PassFilter(ck::Format_ANSI(TEXT("{}"), Channel).c_str()))
        {
            continue;
        }

        const auto& TargetTitle = ck::Format_UE(TEXT("InteractTarget - {}"), Channel);

        if (ImGui::CollapsingHeader(ck::Format_ANSI(TEXT("{}"), TargetTitle).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            RenderInteractTargetInfo(Target);

            if (_Config->ShowParameters)
            {
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Indent();
                    RenderParametersSection(Target);
                    ImGui::Unindent();
                }
            }

            if (_Config->ShowActiveInteractions)
            {
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Current Interactions", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Indent();
                    RenderCurrentInteractionsSection(Target);
                    ImGui::Unindent();
                }
            }

            ImGui::Separator();
            if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent();
                RenderInteractTargetControls(Target);
                ImGui::Unindent();
            }

            ImGui::Unindent();
        }

        if (TargetIndex < InteractTargets.Num() - 1)
        {
            ImGui::Spacing();
        }
    }
}

auto
    FCk_InteractTarget_DebugWindow::
    RenderInteractTargetInfo(
        const FCk_Handle_InteractTarget& InTarget)
    -> void
{
    if (NOT ImGui::BeginTable("TargetInfo", 2, ImGuiTableFlags_SizingFixedFit))
    {
        return;
    }

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    // Basic target information
    Request_RenderTableRow_Default("Handle:", InTarget.ToString());

    const auto IsEnabled = UCk_Utils_InteractTarget_UE::Get_Enabled(InTarget) == ECk_EnableDisable::Enable;
    Request_RenderTableRow_Target("Enabled:", ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_Enabled(InTarget)), IsEnabled);

    ImGui::EndTable();
}

auto
    FCk_InteractTarget_DebugWindow::
    RenderParametersSection(
        const FCk_Handle_InteractTarget& InTarget)
    -> void
{
    if (NOT ImGui::BeginTable("Parameters", 2, ImGuiTableFlags_SizingFixedFit))
    {
        return;
    }

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    Request_RenderTableRow_Enum("Channel:", ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_InteractionChannel(InTarget)));
    Request_RenderTableRow_Enum("Completion Policy:", ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(InTarget)));
    Request_RenderTableRow_Enum("Concurrent Policy:", ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_ConcurrentInteractionsPolicy(InTarget)));

    const auto Duration = UCk_Utils_InteractTarget_UE::Get_InteractionDuration(InTarget);
    Request_RenderTableRow_Number("Duration:", ck::Format_UE(TEXT("{} seconds"), Duration.Get_Seconds()));

    ImGui::EndTable();
}

auto
    FCk_InteractTarget_DebugWindow::
    RenderCurrentInteractionsSection(
        const FCk_Handle_InteractTarget& InTarget)
    -> void
{
    auto CurrentInteractions = UCk_Utils_InteractTarget_UE::Get_CurrentInteractions(const_cast<FCk_Handle_InteractTarget&>(InTarget));

    if (CurrentInteractions.IsEmpty())
    {
        ImGui::Text("No active interactions");
        return;
    }

    if (ImGui::BeginTable("CurrentInteractions", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders))
    {
        ImGui::TableSetupColumn("Interaction", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& Interaction : CurrentInteractions)
        {
            ImGui::TableNextRow();

            // Interaction handle
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Interaction(true));
            ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Interaction).c_str());
            ImGui::PopStyleColor();

            // Source information (if available through interaction utils)
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("Source Info"); // Would need interaction utils to get source

            // Details
            ImGui::TableSetColumnIndex(2);
            if (_Config->ShowInteractionDetails)
            {
                if (ImGui::TreeNode(ck::Format_ANSI(TEXT("Details_{}"), Interaction).c_str(), "Details"))
                {
                    RenderInteractionDetails(Interaction);
                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::Text("Expand for details");
            }
        }

        ImGui::EndTable();
    }
}

auto
    FCk_InteractTarget_DebugWindow::
    RenderInteractionDetails(
        const FCk_Handle_Interaction& InInteraction)
    -> void
{
    if (NOT ck::IsValid(InInteraction))
    {
        ImGui::Text("Invalid interaction");
        return;
    }

    if (ImGui::BeginTable("InteractionDetails", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        Request_RenderTableRow_Default("Handle:", InInteraction.ToString());
        // Add more interaction details here when interaction utils are available

        ImGui::EndTable();
    }
}

auto
    FCk_InteractTarget_DebugWindow::
    RenderInteractTargetControls(
        const FCk_Handle_InteractTarget& InTarget)
    -> void
{
    ImGui::Text("Target Controls:");
    ImGui::Separator();

    // Enable/Disable toggle
    const auto IsEnabled = UCk_Utils_InteractTarget_UE::Get_Enabled(InTarget) == ECk_EnableDisable::Enable;
    const auto ButtonText = FString{IsEnabled ? TEXT("Disable") : TEXT("Enable")};

    if (ImGui::Button(ck::Format_ANSI(TEXT("{}"), ButtonText).c_str()))
    {
        auto MutableTarget = const_cast<FCk_Handle_InteractTarget&>(InTarget);
        const auto NewState = IsEnabled ? ECk_EnableDisable::Disable : ECk_EnableDisable::Enable;
        UCk_Utils_InteractTarget_UE::Set_Enabled(MutableTarget, NewState);
    }

    ImGui::SameLine();
    ImGui::Text("Target State");

    ImGui::Spacing();

    // Cancel all interactions button
    auto CurrentInteractions = UCk_Utils_InteractTarget_UE::Get_CurrentInteractions(const_cast<FCk_Handle_InteractTarget&>(InTarget));
    if (NOT CurrentInteractions.IsEmpty())
    {
        if (ImGui::Button("Cancel All Interactions"))
        {
            auto MutableTarget = const_cast<FCk_Handle_InteractTarget&>(InTarget);
            UCk_Utils_InteractTarget_UE::Request_CancelAllInteractions(MutableTarget);
        }

        ImGui::SameLine();
        ImGui::Text("(%d active)", CurrentInteractions.Num());
    }
    else
    {
        ImGui::TextDisabled("No interactions to cancel");
    }

    ImGui::Spacing();
    ImGui::Text("Note: Use InteractionResolver to start new interactions with this target");
}

// Helper functions for rendering table rows with colors
auto
    FCk_InteractTarget_DebugWindow::
    Request_RenderTableRow_Default(const char* Label, const FString& Value)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
}

auto
    FCk_InteractTarget_DebugWindow::
    Request_RenderTableRow_Target(const char* Label, const FString& Value, bool InIsEnabled)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Target(InIsEnabled));
    ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

auto
    FCk_InteractTarget_DebugWindow::
    Request_RenderTableRow_Interaction(const char* Label, const FString& Value, bool InIsActive)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Interaction(InIsActive));
    ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

auto
    FCk_InteractTarget_DebugWindow::
    Request_RenderTableRow_Number(const char* Label, const FString& Value)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Number());
    ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

auto
    FCk_InteractTarget_DebugWindow::
    Request_RenderTableRow_Enum(const char* Label, const FString& Value)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Enum());
    ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

auto
    FCk_InteractTarget_DebugWindow::
    Request_RenderTableRow_None(const char* Label, const FString& Value)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_None());
    ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

// Color helpers
auto
    FCk_InteractTarget_DebugWindow::
    Get_ValueColor_Target(bool InIsEnabled)
    -> ImU32
{
    return InIsEnabled ? IM_COL32(0, 255, 128, 255) : IM_COL32(153, 153, 153, 255);
}

auto
    FCk_InteractTarget_DebugWindow::
    Get_ValueColor_Interaction(bool InIsActive)
    -> ImU32
{
    return InIsActive ? IM_COL32(255, 242, 0, 255) : IM_COL32(0, 204, 255, 255);
}

auto
    FCk_InteractTarget_DebugWindow::
    Get_ValueColor_Number()
    -> ImU32
{
    return IM_COL32(248, 187, 217, 255); // Pink
}

auto
    FCk_InteractTarget_DebugWindow::
    Get_ValueColor_Enum()
    -> ImU32
{
    return IM_COL32(255, 204, 2, 255); // Yellow
}

auto
    FCk_InteractTarget_DebugWindow::
    Get_ValueColor_None()
    -> ImU32
{
    return IM_COL32(102, 102, 102, 255); // Gray
}

// --------------------------------------------------------------------------------------------------------------------
