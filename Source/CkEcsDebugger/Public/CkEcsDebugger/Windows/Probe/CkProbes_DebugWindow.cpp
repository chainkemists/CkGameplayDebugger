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

    auto SelectionEntities = Get_SelectionEntities();

    if (SelectionEntities.Num() == 0)
    {
        ImGui::Text("No entities selected");
        return;
    }

    // Handle previous entities cleanup
    for (const auto& PreviousEntity : Get_PreviousEntities())
    {
        if (auto MaybePreviousProbe = UCk_Utils_Probe_UE::Cast(PreviousEntity);
            ck::IsValid(MaybePreviousProbe))
        {
            UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(MaybePreviousProbe, ECk_EnableDisable::Disable);
        }
    }

    // Check if any selected entity has probes
    bool HasAnyProbes = false;
    TArray<FCk_Handle> EntitiesWithProbes;

    for (const auto& Entity : SelectionEntities)
    {
        if (ck::Is_NOT_Valid(Entity))
        { continue; }

        if (auto EntityAsProbe = UCk_Utils_Probe_UE::Cast(Entity);
            ck::IsValid(EntityAsProbe))
        {
            EntitiesWithProbes.Add(Entity);
            HasAnyProbes = true;
        }
    }

    if (!HasAnyProbes)
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

    // Always show sections for consistency
    if (EntitiesWithProbes.Num() >= 1)
    {
        if (EntitiesWithProbes.Num() > 1)
        {
            ImGui::Text("Multiple entities with probes selected (%d)", EntitiesWithProbes.Num());
            ImGui::Separator();
        }

        for (int32 EntityIndex = 0; EntityIndex < EntitiesWithProbes.Num(); ++EntityIndex)
        {
            const auto& Entity = EntitiesWithProbes[EntityIndex];

            // Entity section header with proper formatting
            const auto& SectionTitle = ck::Format_UE(TEXT("Entity {} Probes"), Entity);

            if (ImGui::CollapsingHeader(ck::Format_ANSI(TEXT("{}"), SectionTitle).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
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
    }
}

auto
    FCk_Probes_DebugWindow::
    RenderTick(
        float InDeltaT)
    -> void
{
    Super::RenderTick(InDeltaT);
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

    // Check if the entity itself is a probe
    if (auto Probe = UCk_Utils_Probe_UE::Cast(InEntity);
        ck::IsValid(Probe))
    {
        UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(Probe, ECk_EnableDisable::Enable);

        const auto& IsEnabled = UCk_Utils_Probe_UE::Get_IsEnabledDisabled(Probe) == ECk_EnableDisable::Enable;
        const auto& IsOverlapping = UCk_Utils_Probe_UE::Get_IsOverlapping(Probe);
        const auto& ResponsePolicy = UCk_Utils_Probe_UE::Get_ResponsePolicy(Probe);

        // Apply filters - if filtered out, show message
        bool IsFilteredOut = false;

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
        if (const auto& ProbeName = DoGet_ProbeName(Probe);
            NOT ck_probes_debug_window::Filter.PassFilter(TCHAR_TO_ANSI(*ProbeName.ToString())))
        {
            IsFilteredOut = true;
        }

        if (IsFilteredOut)
        {
            ImGui::Text("Entity probe is filtered out");
            return;
        }

        // Render probe information in EntityBasics style
        if (ImGui::BeginTable("ProbeInfo", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            // Helper functions for consistent styling
            const auto RenderTableRow_ProbeState = [&](const char* Label, bool InState)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(Label);
                ImGui::TableSetColumnIndex(1);
                FCogWidgets::PushStyleCompact();
                bool State = InState;
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
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 177, 255, 255)); // Blue like EntityBasics
                ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
                ImGui::PopStyleColor();
            };

            const auto RenderTableRow_ProbeStatus = [&](const char* Label, const FString& Value, bool IsPositive)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(Label);
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, IsPositive ? IM_COL32(195, 232, 141, 255) : IM_COL32(255, 87, 34, 255)); // Green/Red
                ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
                ImGui::PopStyleColor();
            };

            const auto RenderTableRow_ProbeConfig = [&](const char* Label, const FString& Value)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(Label);
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 204, 2, 255)); // Yellow like EntityBasics enums
                ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
                ImGui::PopStyleColor();
            };

            const auto RenderTableRow_ProbeDefault = [&](const char* Label, const FString& Value)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(Label);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Value).c_str());
            };

            // === BASIC INFORMATION ===
            // Name
            const auto& ProbeName = DoGet_ProbeName(Probe);
            RenderTableRow_ProbeName("Name:", ProbeName.ToString());

            // Enabled (State)
            RenderTableRow_ProbeState("Enabled:", IsEnabled);

            // === STATUS INFORMATION ===
            // Status (Overlapping/Not Overlapping) - Green for overlapping, Red for not overlapping
            const auto& StatusText = IsOverlapping ? TEXT("Overlapping") : TEXT("Not Overlapping");
            RenderTableRow_ProbeStatus("Status:", StatusText, IsOverlapping);

            // === CONFIGURATION ===
            // Response Policy
            const auto& PolicyText = ResponsePolicy == ECk_ProbeResponse_Policy::Notify ? TEXT("Notify") : TEXT("Silent");
            RenderTableRow_ProbeConfig("Policy:", PolicyText);

            // Motion Type
            const auto& MotionType = DoGet_ProbeMotionType(Probe);
            RenderTableRow_ProbeConfig("Motion Type:", MotionType);

            // Motion Quality
            const auto& MotionQuality = DoGet_ProbeMotionQuality(Probe);
            RenderTableRow_ProbeConfig("Motion Quality:", MotionQuality);

            // === FILTER INFORMATION ===
            // Filter
            const auto& Filter = UCk_Utils_Probe_UE::Get_Filter(Probe);
            FString FilterText;
            if (Filter.IsEmpty())
            {
                FilterText = TEXT("(Empty)");
                RenderTableRow_ProbeDefault("Filter:", FilterText);
            }
            else
            {
                TArray<FString> TagNames;
                for (const auto& Tag : Filter)
                {
                    TagNames.Add(Tag.GetTagName().ToString());
                }
                FilterText = FString::Join(TagNames, TEXT(", "));
                RenderTableRow_ProbeDefault("Filter:", FilterText);
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
    auto SelectionEntities = Get_SelectionEntities();

    if (SelectionEntities.Num() == 0)
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
    DoGet_ProbeName(
        const FCk_Handle_Probe& InProbe) const
        -> FName
{
    return UCk_Utils_Probe_UE::Get_Name(InProbe).GetTagName();
}

auto
    FCk_Probes_DebugWindow::
    DoGet_ProbeMotionType(
        const FCk_Handle_Probe& InProbe) const
        -> FString
{
    // Since we can't directly access the motion type from the probe, we'll check for tags
    if (InProbe.Has<ck::FTag_Probe_MotionType_Static>())
    {
        return TEXT("Static");
    }
    // Default assumption for non-static probes
    return TEXT("Kinematic");
}

auto
    FCk_Probes_DebugWindow::
    DoGet_ProbeMotionQuality(
        const FCk_Handle_Probe& InProbe) const
        -> FString
{
    if (InProbe.Has<ck::FTag_Probe_LinearCast>())
    {
        return TEXT("LinearCast");
    }
    return TEXT("Discrete");
}

// --------------------------------------------------------------------------------------------------------------------