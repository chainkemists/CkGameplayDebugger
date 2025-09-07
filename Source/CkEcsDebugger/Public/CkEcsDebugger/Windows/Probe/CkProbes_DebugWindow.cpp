#include "CkProbes_DebugWindow.h"

#include "CogImguiHelper.h"
#include "CogWidgets.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkSpatialQuery/Public/CkSpatialQuery/Probe/CkProbe_Fragment.h"
#include "CkSpatialQuery/Public/CkSpatialQuery/Probe/CkProbe_Utils.h"

#if WITH_EDITOR
#include <Editor.h>
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_probes_debug_window
{
    static ImGuiTextFilter Filter;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_Probes_DebugWindow::
    Initialize() -> void
{
    Super::Initialize();

    bHasMenu = true;

    _Config = GetConfig<UCk_Probes_DebugWindowConfig>();
}

auto
    FCk_Probes_DebugWindow::
    ResetConfig()
    -> void
{
    Super::ResetConfig();

    _Config->Reset();
}

auto
    FCk_Probes_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text
    (
        "This window displays the probes of the selected entities. "
        "Click the enable/disable checkbox to toggle probe state. "
        "When multiple entities are selected, probes from all entities are shown."
    );
}

auto
    FCk_Probes_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    RenderMenu();

    // Step 1: ALWAYS disable debug drawing for previous entities (cleanup)
    for (const auto& PreviousEntity : Get_PreviousEntities())
    {
        if (auto MaybePreviousProbe = UCk_Utils_Probe_UE::Cast(PreviousEntity);
            ck::IsValid(MaybePreviousProbe))
        {
            UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(MaybePreviousProbe, ECk_EnableDisable::Disable);
        }
    }

    const auto SelectionEntities = Get_SelectionEntities();

    if (SelectionEntities.IsEmpty())
    {
        ImGui::Text("No entities selected");
        return;
    }

    auto EntitiesWithProbes = ck::algo::Filter(SelectionEntities, [](const FCk_Handle& InEntity)
    {
        return UCk_Utils_Probe_UE::Has(InEntity);
    });

    auto EntitiesAsProbes = ck::algo::Transform<TArray<FCk_Handle_Probe>>(SelectionEntities, [](const FCk_Handle& InEntity)
    {
        return UCk_Utils_Probe_UE::Cast(InEntity);
    });

    if (EntitiesWithProbes.IsEmpty())
    {
        if (SelectionEntities.Num() == 1)
        {
            ImGui::Text("Selected entity has no probes");
        }
        else
        {
            ImGui::Text("Selected entities have no probes");
        }
        return;
    }

    // Step 2: Enable debug drawing based on configuration
    if (_Config->_AlwaysDrawDebug)
    {
        // Always draw debug for all selected entities with probes
        for (auto Probe : EntitiesAsProbes)
        {
            UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(Probe, ECk_EnableDisable::Enable);
        }
    }

    // Step 3: Render UI sections
    if (EntitiesWithProbes.Num() > 1)
    {
        ImGui::Text("Multiple entities with probes selected (%d)", EntitiesWithProbes.Num());
        ImGui::Separator();
    }

    // Track which entities have their sections expanded so we can handle debug drawing for non-always mode
    TSet<FCk_Handle> ExpandedEntities;

    for (int32 EntityIndex = 0; EntityIndex < EntitiesWithProbes.Num(); ++EntityIndex)
    {
        const auto& Entity = EntitiesWithProbes[EntityIndex];

        const auto& SectionTitle = ck::Format_UE(TEXT("Entity {}"), Entity);

        if ([[maybe_unused]] const auto& IsExpanded = ImGui::CollapsingHeader(ck::Format_ANSI(TEXT("{}"), SectionTitle).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ExpandedEntities.Add(Entity);

            ImGui::Indent();
            RenderEntityProbesSection(Entity);
            ImGui::Unindent();
        }

        if (EntityIndex < EntitiesWithProbes.Num() - 1)
        {
            ImGui::Separator();
            ImGui::Spacing();
        }
    }

    // Step 4: Handle debug drawing for non-always mode
    if (NOT _Config->_AlwaysDrawDebug)
    {
        // Only enable debug drawing for expanded sections
        for (auto Probe : EntitiesAsProbes)
        {
            const auto& ShouldDrawDebug = ExpandedEntities.Contains(Probe);
            UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(Probe,
                ShouldDrawDebug ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable);
        }
    }
}

auto
    FCk_Probes_DebugWindow::
    GameTick(
        float InDeltaT)
    -> void
{
    Super::GameTick(InDeltaT);

    if (ck::IsValid(_ProbeHandleToToggle))
    {
        ProcessProbeEnableDisable(_ProbeHandleToToggle);
        _ProbeHandleToToggle = {};
    }
}

auto
    FCk_Probes_DebugWindow::
    RenderMenu()
    -> void
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Separator();

            ImGui::Checkbox("Sort by Name", &_Config->_SortByName);
            ImGui::Checkbox("Always Draw Debug", &_Config->_AlwaysDrawDebug);

            ImGui::Separator();

            ImGui::Checkbox("Show Enabled", &_Config->_ShowEnabled);
            ImGui::Checkbox("Show Disabled", &_Config->_ShowDisabled);
            ImGui::Checkbox("Show Overlapping", &_Config->_ShowOverlapping);
            ImGui::Checkbox("Show Not Overlapping", &_Config->_ShowNotOverlapping);
            ImGui::Checkbox("Show Notify Policy", &_Config->_ShowNotifyPolicy);
            ImGui::Checkbox("Show Silent Policy", &_Config->_ShowSilentPolicy);

            ImGui::Separator();

            ImGui::ColorEdit4("Enabled Color", reinterpret_cast<float*>(&_Config->EnabledColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Disabled Color", reinterpret_cast<float*>(&_Config->DisabledColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Overlapping Color", reinterpret_cast<float*>(&_Config->OverlappingColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Notify Color", reinterpret_cast<float*>(&_Config->NotifyColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Silent Color", reinterpret_cast<float*>(&_Config->SilentColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);

            ImGui::Separator();

            if (ImGui::MenuItem("Reset"))
            {
                ResetConfig();
            }

            ImGui::EndMenu();
        }

        FCogWidgets::SearchBar("##Filter", ck_probes_debug_window::Filter);

        ImGui::EndMenuBar();
    }
}

auto
    FCk_Probes_DebugWindow::
    RenderEntityProbesSection(
        const FCk_Handle& InEntity)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_Probes_DebugWindow_RenderEntityProbesSection)

    if (auto Probe = UCk_Utils_Probe_UE::Cast(InEntity);
        ck::IsValid(Probe))
    {
        const auto& IsEnabled = UCk_Utils_Probe_UE::Get_IsEnabledDisabled(Probe) == ECk_EnableDisable::Enable;
        const auto& IsOverlapping = UCk_Utils_Probe_UE::Get_IsOverlapping(Probe);
        const auto& ResponsePolicy = UCk_Utils_Probe_UE::Get_ResponsePolicy(Probe);

        // Apply filters - if filtered out, show message
        auto IsFilteredOut = false;

        if ((NOT _Config->_ShowEnabled && IsEnabled) ||
            (NOT _Config->_ShowDisabled && NOT IsEnabled) ||
            (NOT _Config->_ShowOverlapping && IsOverlapping) ||
            (NOT _Config->_ShowNotOverlapping && NOT IsOverlapping) ||
            (NOT _Config->_ShowNotifyPolicy && ResponsePolicy == ECk_ProbeResponse_Policy::Notify) ||
            (NOT _Config->_ShowSilentPolicy && ResponsePolicy == ECk_ProbeResponse_Policy::Silent))
        {
            IsFilteredOut = true;
        }

        // Apply search filter
        if (const auto& ProbeName = Get_ProbeName(Probe);
            NOT ck_probes_debug_window::Filter.PassFilter(TCHAR_TO_ANSI(*ProbeName)))
        {
            IsFilteredOut = true;
        }

        if (IsFilteredOut)
        {
            ImGui::Text("Entity probe is filtered out");
            return;
        }

        if (ImGui::BeginTable("ProbeInfo", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            const auto RenderTableRow_ProbeState = [&](const char* Label, bool InState)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(Label);
                ImGui::TableSetColumnIndex(1);
                FCogWidgets::PushStyleCompact();
                auto State = InState;

                if (ImGui::Checkbox("##ProbeState", &State))
                {
                    _ProbeHandleToToggle = Probe;
                }

                FCogWidgets::PopStyleCompact();
            };

            const auto RenderTableRow_ProbeName = [&](const char* Label, const FString& Value)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(Label);
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 177, 255, 255));
                ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
                ImGui::PopStyleColor();
            };

            const auto RenderTableRow_ProbeStatus = [&](const char* Label, const FString& Value, bool IsPositive)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(Label);
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, IsPositive ? IM_COL32(195, 232, 141, 255) : IM_COL32(255, 87, 34, 255));
                ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
                ImGui::PopStyleColor();
            };

            const auto RenderTableRow_ProbeConfig = [&](const char* Label, const FString& Value)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(Label);
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 204, 2, 255));
                ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
                ImGui::PopStyleColor();
            };

            const auto RenderTableRow_ProbeFilter = [&](const char* Label, const FString& Value, bool IsEmpty)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(Label);
                ImGui::TableSetColumnIndex(1);

                if (IsEmpty)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(102, 102, 102, 255)); // Gray
                }

                ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());

                if (IsEmpty)
                {
                    ImGui::PopStyleColor();
                }
            };

            const auto RenderTableRow_OverlapDetails = [&](const char* Label, const TSet<FCk_Probe_OverlapInfo>& Overlaps)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(Label);
                ImGui::TableSetColumnIndex(1);

                if (Overlaps.IsEmpty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(102, 102, 102, 255)); // Gray
                    ImGui::Text("None");
                    ImGui::PopStyleColor();
                }
                else
                {
                    // Show overlap count and entities
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(195, 232, 141, 255)); // Green for overlaps
                    ImGui::Text("(%d entities)", Overlaps.Num());
                    ImGui::PopStyleColor();

                    // Create a collapsing header for overlap details
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(1);

                    if (ImGui::TreeNode("OverlapDetails", "Overlap Details"))
                    {
                        ImGui::Indent();

                        int32 OverlapIndex = 0;
                        for (const auto& OverlapInfo : Overlaps)
                        {
                            const auto& OtherEntity = OverlapInfo.Get_OtherEntity();

                            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 177, 255, 255)); // Blue for entity handles
                            ImGui::Text("[%d] Entity: %s", OverlapIndex + 1, ck::Format_ANSI(TEXT("{}"), OtherEntity).c_str());
                            ImGui::PopStyleColor();

                            // Show contact points if available
                            const auto& ContactPoints = OverlapInfo.Get_ContactPoints();
                            if (ContactPoints.Num() > 0)
                            {
                                ImGui::Indent();
                                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 204, 2, 255)); // Yellow for details
                                ImGui::Text("Contact Points: %d", ContactPoints.Num());

                                // Show first contact point as example
                                if (ContactPoints.Num() > 0)
                                {
                                    const FVector& FirstContact = ContactPoints[0];
                                    ImGui::Text("  First: (%.2f, %.2f, %.2f)", FirstContact.X, FirstContact.Y, FirstContact.Z);
                                }

                                // Show contact normal
                                const FVector& Normal = OverlapInfo.Get_ContactNormal();
                                ImGui::Text("Normal: (%.2f, %.2f, %.2f)", Normal.X, Normal.Y, Normal.Z);
                                ImGui::PopStyleColor();
                                ImGui::Unindent();
                            }

                            OverlapIndex++;

                            // Add spacing between overlap entries
                            if (OverlapIndex < Overlaps.Num())
                            {
                                ImGui::Spacing();
                            }
                        }

                        ImGui::Unindent();
                        ImGui::TreePop();
                    }
                }
            };

            // === BASIC INFORMATION ===
            RenderTableRow_ProbeName("Name:", Get_ProbeName(Probe));

            RenderTableRow_ProbeState("Enabled:", IsEnabled);

            // === STATUS INFORMATION ===
            // Status (Overlapping/Not Overlapping) - Green for overlapping, Red for not overlapping
            const auto& StatusText = IsOverlapping ? TEXT("Overlapping") : TEXT("Not Overlapping");
            RenderTableRow_ProbeStatus("Status:", StatusText, IsOverlapping);

            // === OVERLAP DETAILS ===
            if (IsOverlapping)
            {
                const auto& CurrentOverlaps = UCk_Utils_Probe_UE::Get_CurrentOverlaps(Probe);
                RenderTableRow_OverlapDetails("Overlapping With:", CurrentOverlaps);
            }

            // === CONFIGURATION ===
            RenderTableRow_ProbeConfig("Response Policy:", Get_ProbeResponsePolicy(Probe));
            RenderTableRow_ProbeConfig("Motion Type:", Get_ProbeMotionType(Probe));
            RenderTableRow_ProbeConfig("Motion Quality:", Get_ProbeMotionQuality(Probe));

            // === FILTER INFORMATION ===
            if (const auto& Filter = UCk_Utils_Probe_UE::Get_Filter(Probe);
                Filter.IsEmpty())
            {
                RenderTableRow_ProbeFilter("Filter:", TEXT("(Empty)"), true);
            }
            else
            {
                RenderTableRow_ProbeFilter("Filter:", ck::Format_UE(TEXT("{}"), Filter), false);
            }

            ImGui::EndTable();
        }
    }
    else
    {
        ImGui::Text("Entity is not a probe");
    }
}

auto
    FCk_Probes_DebugWindow::
    ProcessProbeEnableDisable(
        FCk_Handle_Probe& InProbe)
    -> void
{
    if (const auto& SelectionEntities = Get_SelectionEntities();
        SelectionEntities.IsEmpty())
    { return; }

    switch (UCk_Utils_Probe_UE::Get_IsEnabledDisabled(InProbe))
    {
        case ECk_EnableDisable::Enable:
        {
            UCk_Utils_Probe_UE::Request_EnableDisable(InProbe, FCk_Request_Probe_EnableDisable{ECk_EnableDisable::Disable});
            break;
        }
        case ECk_EnableDisable::Disable:
        {
            UCk_Utils_Probe_UE::Request_EnableDisable(InProbe, FCk_Request_Probe_EnableDisable{ECk_EnableDisable::Enable});
            break;
        }
    }
}

auto
    FCk_Probes_DebugWindow::
    Get_ProbeName(
        const FCk_Handle_Probe& InProbe)
    -> FString
{
    return UCk_Utils_Probe_UE::Get_Name(InProbe).GetTagName().ToString();
}

auto
    FCk_Probes_DebugWindow::
    Get_ProbeMotionType(
        const FCk_Handle_Probe& InProbe)
    -> FString
{
    return ck::Format_UE(TEXT("{}"), UCk_Utils_Probe_UE::Get_MotionType(InProbe));
}

auto
    FCk_Probes_DebugWindow::
    Get_ProbeMotionQuality(
        const FCk_Handle_Probe& InProbe)
    -> FString
{
    return ck::Format_UE(TEXT("{}"), UCk_Utils_Probe_UE::Get_MotionQuality(InProbe));
}

auto
    FCk_Probes_DebugWindow::
    Get_ProbeResponsePolicy(
        const FCk_Handle_Probe& InProbe)
    -> FString
{
    return ck::Format_UE(TEXT("{}"), UCk_Utils_Probe_UE::Get_ResponsePolicy(InProbe));
}

// --------------------------------------------------------------------------------------------------------------------