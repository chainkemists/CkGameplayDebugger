#include "CkInteractionResolver_DebugWindow.h"

#include "CogImguiHelper.h"
#include "CogWidgets.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkInteraction/InteractionResolver/CkInteractionResolver_Utils.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_InteractionResolver_DebugWindow::
    Initialize() -> void
{
    Super::Initialize();

    bHasMenu = true;

    _Config = GetConfig<UCk_InteractionResolver_DebugWindowConfig>();
}

auto
    FCk_InteractionResolver_DebugWindow::
    ResetConfig()
    -> void
{
    Super::ResetConfig();

    _Config->Reset();
}

auto
    FCk_InteractionResolver_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text(
        "This window displays InteractionResolver information for the selected entities. "
        "Shows active intents, available targets, and cached best targets. "
        "Use the controls to start/stop intents and add/remove targets. "
        "When multiple entities are selected, resolvers from all entities are shown."
    );
}

auto
    FCk_InteractionResolver_DebugWindow::
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

    auto EntitiesWithResolvers = ck::algo::Filter(SelectionEntities, [](const FCk_Handle& InEntity)
    {
        return UCk_Utils_InteractionResolver_UE::Has(InEntity);
    });

    if (EntitiesWithResolvers.IsEmpty())
    {
        if (SelectionEntities.Num() == 1)
        {
            ImGui::Text("Selected entity has no InteractionResolver");
        }
        else
        {
            ImGui::Text("Selected entities have no InteractionResolvers");
        }
        return;
    }

    // Show summary for multiple entities
    if (EntitiesWithResolvers.Num() > 1)
    {
        ImGui::Text("Multiple entities with InteractionResolvers selected (%d)", EntitiesWithResolvers.Num());
        ImGui::Separator();
    }

    for (int32 EntityIndex = 0; EntityIndex < EntitiesWithResolvers.Num(); ++EntityIndex)
    {
        const auto& Entity = EntitiesWithResolvers[EntityIndex];

        const auto& SectionTitle = ck::Format_UE(TEXT("Entity {}"), Entity);

        if (ImGui::CollapsingHeader(ck::Format_ANSI(TEXT("{}"), SectionTitle).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            RenderEntityInteractionResolverSection(Entity);
            ImGui::Unindent();
        }

        if (EntityIndex < EntitiesWithResolvers.Num() - 1)
        {
            ImGui::Separator();
            ImGui::Spacing();
        }
    }
}

auto
    FCk_InteractionResolver_DebugWindow::
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

            ImGui::Checkbox("Show Active Intents", &_Config->ShowActiveIntents);
            ImGui::Checkbox("Show Inactive Intents", &_Config->ShowInactiveIntents);
            ImGui::Checkbox("Show Best Targets", &_Config->ShowBestTargets);
            ImGui::Checkbox("Show Available Targets", &_Config->ShowAvailableTargets);
            ImGui::Checkbox("Show Target Details", &_Config->ShowTargetDetails);
            ImGui::Checkbox("Show Parameters", &_Config->ShowParameters);

            ImGui::Separator();

            ImGui::ColorEdit4("Active Intent Color", reinterpret_cast<float*>(&_Config->ActiveIntentColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Inactive Intent Color", reinterpret_cast<float*>(&_Config->InactiveIntentColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Best Target Color", reinterpret_cast<float*>(&_Config->BestTargetColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Available Target Color", reinterpret_cast<float*>(&_Config->AvailableTargetColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);

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
    FCk_InteractionResolver_DebugWindow::
    RenderEntityInteractionResolverSection(
        const FCk_Handle& InEntity)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_InteractionResolver_DebugWindow_RenderEntityInteractionResolverSection)

    if (auto Resolver = UCk_Utils_InteractionResolver_UE::Cast(InEntity);
        ck::IsValid(Resolver))
    {
        RenderInteractionResolverInfo(Resolver);

        if (_Config->ShowParameters)
        {
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent();
                RenderParametersSection(Resolver);
                ImGui::Unindent();
            }
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Active Intents", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            RenderActiveIntentsSection(Resolver);
            ImGui::Unindent();
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Targets", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            RenderTargetsSection(Resolver);
            ImGui::Unindent();
        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            RenderInteractionResolverControls(Resolver);
            ImGui::Unindent();
        }
    }
    else
    {
        ImGui::Text("Entity is not an InteractionResolver");
    }
}

auto
    FCk_InteractionResolver_DebugWindow::
    RenderInteractionResolverInfo(
        const FCk_Handle_InteractionResolver& InResolver)
    -> void
{
    if (NOT ImGui::BeginTable("ResolverInfo", 2, ImGuiTableFlags_SizingFixedFit))
    {
        return;
    }

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    // Basic resolver information
    Request_RenderTableRow_Default("Resolver:", InResolver.ToString());

    ImGui::EndTable();
}

auto
    FCk_InteractionResolver_DebugWindow::
    RenderParametersSection(
        const FCk_Handle_InteractionResolver& InResolver)
    -> void
{
    if (NOT ImGui::BeginTable("Parameters", 2, ImGuiTableFlags_SizingFixedFit))
    {
        return;
    }

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

    // Access parameters through the fragment
    if (const auto& Entity = static_cast<FCk_Handle>(InResolver);
        Entity.Has<ck::FFragment_InteractionResolver_Params>())
    {
        const auto& Params = Entity.Get<ck::FFragment_InteractionResolver_Params>();

        Request_RenderTableRow_Enum("Distance Sorting:", ck::Format_UE(TEXT("{}"), Params.Get_DistanceSorting()));
        Request_RenderTableRow_Number("Max Concurrent:", ck::Format_UE(TEXT("{}"), Params.Get_MaxConcurrentInteractions()));

        // Intent-Channel mappings
        const auto& Mappings = Params.Get_IntentChannelMappings();
        if (Mappings.IsEmpty())
        {
            Request_RenderTableRow_None("Intent Mappings:", TEXT("None"));
        }
        else
        {
            Request_RenderTableRow_Number("Intent Mappings:", ck::Format_UE(TEXT("{} mappings"), Mappings.Num()));

            // Show detailed mappings in a sub-table with buttons
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(1);

            if (ImGui::TreeNode("IntentMappings", "Mapping Details"))
            {
                // Check which intents are currently active for color coding and button states
                TSet<FGameplayTag> ActiveIntents;
                if (const auto& ResolverEntity = static_cast<FCk_Handle>(InResolver);
                    ResolverEntity.Has<ck::FFragment_InteractionResolver_Current>())
                {
                    ActiveIntents = ResolverEntity.Get<ck::FFragment_InteractionResolver_Current>().Get_ActiveIntents();
                }

                if (ImGui::BeginTable("IntentMappingsDetails", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn("Intent", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                    ImGui::TableSetupColumn("Channels", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableHeadersRow();

                    for (const auto& Mapping : Mappings)
                    {
                        const auto& Intent = Mapping.Get_Intent();
                        const auto IsActive = ActiveIntents.Contains(Intent);

                        ImGui::TableNextRow();

                        // Intent column with color coding
                        ImGui::TableSetColumnIndex(0);
                        ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Intent(IsActive));
                        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Intent.GetTagName().ToString()).c_str());
                        ImGui::PopStyleColor();

                        // Channels column
                        ImGui::TableSetColumnIndex(1);
                        FString ChannelsText = TEXT("");
                        for (int32 i = 0; i < Mapping.Get_Channels().Num(); ++i)
                        {
                            ChannelsText += Mapping.Get_Channels()[i].GetTagName().ToString();
                            if (i < Mapping.Get_Channels().Num() - 1)
                            {
                                ChannelsText += TEXT(", ");
                            }
                        }
                        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), ChannelsText).c_str());

                        // Actions column with Start/Stop buttons
                        ImGui::TableSetColumnIndex(2);
                        const auto ButtonText = IsActive ? "Stop" : "Start";
                        const auto ButtonId = ck::Format_ANSI(TEXT("{}##{}"), FString{ButtonText}, Intent);

                        if (ImGui::Button(ButtonId.c_str()))
                        {
                            auto MutableResolver = const_cast<FCk_Handle_InteractionResolver&>(InResolver);
                            if (IsActive)
                            {
                                UCk_Utils_InteractionResolver_UE::Request_StopIntent(MutableResolver, FCk_Request_InteractionResolver_StopIntent{Intent});
                            }
                            else
                            {
                                UCk_Utils_InteractionResolver_UE::Request_StartIntent(MutableResolver, FCk_Request_InteractionResolver_StartIntent{Intent});
                            }
                        }
                    }

                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }
    }
    else
    {
        Request_RenderTableRow_None("Parameters:", TEXT("No parameters fragment"));
    }

    ImGui::EndTable();
}

auto
    FCk_InteractionResolver_DebugWindow::
    RenderActiveIntentsSection(
        const FCk_Handle_InteractionResolver& InResolver)
    -> void
{
    if (NOT ImGui::BeginTable("ActiveIntents", 2, ImGuiTableFlags_SizingFixedFit))
    {
        return;
    }

    ImGui::TableSetupColumn("Intent", ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn("Best Targets", ImGuiTableColumnFlags_WidthStretch);

    // Access current fragment to get active intents and cached targets
    if (const auto& Entity = static_cast<FCk_Handle>(InResolver);
        Entity.Has<ck::FFragment_InteractionResolver_Current>())
    {
        const auto& Current = Entity.Get<ck::FFragment_InteractionResolver_Current>();
        const auto& ActiveIntents = Current.Get_ActiveIntents();
        const auto& CachedBestTargets = Current.Get_CachedBestTargets();

        if (ActiveIntents.IsEmpty())
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            Request_RenderTableRow_None("Active Intents:", TEXT("None"));
        }
        else
        {
            auto IntentArray = ActiveIntents.Array();
            if (_Config->SortByName)
            {
                IntentArray.Sort([](const FGameplayTag& A, const FGameplayTag& B)
                {
                    return A.GetTagName().ToString() < B.GetTagName().ToString();
                });
            }

            for (const auto& Intent : IntentArray)
            {
                if (NOT _Filter.PassFilter(ck::Format_ANSI(TEXT("{}"), Intent.GetTagName().ToString()).c_str()))
                {
                    continue;
                }

                if (NOT _Config->ShowActiveIntents)
                {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                // Color the intent as active
                ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Intent(true));
                ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Intent.GetTagName().ToString()).c_str());
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);

                // Show best targets for this intent
                if (const auto* BestTargets = CachedBestTargets.Find(Intent))
                {
                    if (BestTargets->IsEmpty())
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_None());
                        ImGui::Text("No targets");
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Target(true));
                        ImGui::Text("(%d targets)", BestTargets->Num());
                        ImGui::PopStyleColor();

                        if (_Config->ShowTargetDetails && ImGui::TreeNode(ck::Format_ANSI(TEXT("Targets_{}"), Intent.GetTagName().ToString()).c_str(), "Target Details"))
                        {
                            for (const auto& Target : *BestTargets)
                            {
                                RenderTargetDetails(Target);
                            }
                            ImGui::TreePop();
                        }
                    }
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_None());
                    ImGui::Text("No cached targets");
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    ImGui::EndTable();
}

auto
    FCk_InteractionResolver_DebugWindow::
    RenderTargetsSection(
        const FCk_Handle_InteractionResolver& InResolver)
    -> void
{
    if (NOT ImGui::BeginTable("Targets", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders))
    {
        return;
    }

    ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Access current fragment to get available targets and cached best targets
    if (const auto& Entity = static_cast<FCk_Handle>(InResolver);
        Entity.Has<ck::FFragment_InteractionResolver_Current>())
    {
        const auto& Current = Entity.Get<ck::FFragment_InteractionResolver_Current>();
        const auto& AvailableTargets = Current.Get_AvailableTargets();
        const auto& CachedBestTargets = Current.Get_CachedBestTargets();

        // Collect all best targets for quick lookup
        TSet<FCk_Handle_InteractTarget> AllBestTargets;
        for (const auto& [Intent, Targets] : CachedBestTargets)
        {
            AllBestTargets.Append(Targets);
        }

        if (AvailableTargets.IsEmpty())
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_None());
            ImGui::Text("No available targets");
            ImGui::PopStyleColor();
        }
        else
        {
            auto TargetArray = AvailableTargets.Array();
            if (_Config->SortByName)
            {
                TargetArray.Sort([](const FCk_Handle_InteractTarget& A, const FCk_Handle_InteractTarget& B)
                {
                    return A.ToString() < B.ToString();
                });
            }

            for (const auto& Target : TargetArray)
            {
                const auto IsBestTarget = AllBestTargets.Contains(Target);

                // Apply filters
                if ((NOT _Config->ShowBestTargets && IsBestTarget) ||
                    (NOT _Config->ShowAvailableTargets && NOT IsBestTarget))
                {
                    continue;
                }

                if (NOT _Filter.PassFilter(ck::Format_ANSI(TEXT("{}"), Target.ToString()).c_str()))
                {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                // Color based on whether it's a best target
                ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Target(IsBestTarget));
                ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Target.ToString()).c_str());
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Enum());
                ImGui::Text(IsBestTarget ? "Best" : "Available");
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(2);
                if (_Config->ShowTargetDetails)
                {
                    if (ImGui::TreeNode(ck::Format_ANSI(TEXT("Details_{}"), Target.ToString()).c_str(), "Details"))
                    {
                        RenderTargetDetails(Target);
                        ImGui::TreePop();
                    }
                }
                else
                {
                    // Show basic info inline
                    if (ck::IsValid(Target))
                    {
                        const auto Channel = UCk_Utils_InteractTarget_UE::Get_InteractionChannel(Target);
                        const auto Enabled = UCk_Utils_InteractTarget_UE::Get_Enabled(Target);
                        ImGui::Text("Channel: %s, Enabled: %s",
                            ck::Format_ANSI(TEXT("{}"), Channel.GetTagName().ToString()).c_str(),
                            ck::Format_ANSI(TEXT("{}"), Enabled).c_str());
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_None());
                        ImGui::Text("Invalid target");
                        ImGui::PopStyleColor();
                    }
                }
            }
        }
    }

    ImGui::EndTable();
}

auto
    FCk_InteractionResolver_DebugWindow::
    RenderTargetDetails(
        const FCk_Handle_InteractTarget& InTarget)
    -> void
{
    if (NOT ck::IsValid(InTarget))
    {
        ImGui::Text("Invalid target");
        return;
    }

    if (ImGui::BeginTable("TargetDetails", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        Request_RenderTableRow_Default("Handle:", InTarget.ToString());
        Request_RenderTableRow_Enum("Channel:", UCk_Utils_InteractTarget_UE::Get_InteractionChannel(InTarget).GetTagName().ToString());
        Request_RenderTableRow_Enum("Enabled:", ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_Enabled(InTarget)));
        Request_RenderTableRow_Enum("Completion Policy:", ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(InTarget)));
        Request_RenderTableRow_Enum("Concurrent Policy:", ck::Format_UE(TEXT("{}"), UCk_Utils_InteractTarget_UE::Get_ConcurrentInteractionsPolicy(InTarget)));

        const auto CurrentInteractions = UCk_Utils_InteractTarget_UE::Get_CurrentInteractions(const_cast<FCk_Handle_InteractTarget&>(InTarget));
        Request_RenderTableRow_Number("Active Interactions:", ck::Format_UE(TEXT("{}"), CurrentInteractions.Num()));

        ImGui::EndTable();
    }
}

auto
    FCk_InteractionResolver_DebugWindow::
    RenderInteractionResolverControls(
        const FCk_Handle_InteractionResolver& InResolver)
    -> void
{
    ImGui::Text("Target Controls:");
    ImGui::Separator();

    ImGui::Text("Note: Use the InteractTarget system to add/remove targets");
    ImGui::Text("Intent controls are available in the Parameters -> Mapping Details section above");
}

// Helper functions for rendering table rows with colors
auto
    FCk_InteractionResolver_DebugWindow::
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
    FCk_InteractionResolver_DebugWindow::
    Request_RenderTableRow_Intent(const char* Label, const FString& Value, bool InIsActive)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Intent(InIsActive));
    ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

auto
    FCk_InteractionResolver_DebugWindow::
    Request_RenderTableRow_Target(const char* Label, const FString& Value, bool InIsInBestTargets)
    -> void
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", Label);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushStyleColor(ImGuiCol_Text, Get_ValueColor_Target(InIsInBestTargets));
    ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
    ImGui::PopStyleColor();
}

auto
    FCk_InteractionResolver_DebugWindow::
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
    FCk_InteractionResolver_DebugWindow::
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
    FCk_InteractionResolver_DebugWindow::
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
    FCk_InteractionResolver_DebugWindow::
    Get_ValueColor_Intent(bool InIsActive)
    -> ImU32
{
    return InIsActive ? IM_COL32(0, 255, 128, 255) : IM_COL32(153, 153, 153, 255);
}

auto
    FCk_InteractionResolver_DebugWindow::
    Get_ValueColor_Target(bool InIsInBestTargets)
    -> ImU32
{
    return InIsInBestTargets ? IM_COL32(255, 242, 0, 255) : IM_COL32(0, 204, 255, 255);
}

auto
    FCk_InteractionResolver_DebugWindow::
    Get_ValueColor_Number()
    -> ImU32
{
    return IM_COL32(248, 187, 217, 255); // Pink
}

auto
    FCk_InteractionResolver_DebugWindow::
    Get_ValueColor_Enum()
    -> ImU32
{
    return IM_COL32(255, 204, 2, 255); // Yellow
}

auto
    FCk_InteractionResolver_DebugWindow::
    Get_ValueColor_None()
    -> ImU32
{
    return IM_COL32(102, 102, 102, 255); // Gray
}

// --------------------------------------------------------------------------------------------------------------------
