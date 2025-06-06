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
        "This window displays the probes of the selected actor. "
        "Click the enable/disable checkbox to toggle probe state. "
        "Right click a probe to open or close the probe separate window. "
    );
}

auto
    FCk_Probes_DebugWindow::
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

    if (auto MaybePreviousProbe = UCk_Utils_Probe_UE::Cast(Get_PreviousEntity());
        ck::IsValid(MaybePreviousProbe))
    {
        UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(MaybePreviousProbe, ECk_EnableDisable::Disable);
    }

    RenderTable(SelectionEntity);
}

auto
    FCk_Probes_DebugWindow::
    RenderTick(
        float InDeltaT)
    -> void
{
    Super::RenderTick(InDeltaT);

    RenderOpenedProbes();
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
    RenderOpenedProbes()
    -> void
{
    auto SelectionEntity = Get_SelectionEntity();

    if (ck::Is_NOT_Valid(SelectionEntity))
    { return; }

    for (int32 Index = _OpenedProbes.Num() - 1; Index >= 0; --Index)
    {
        const auto& ProbeOpened = _OpenedProbes[Index];

        bool Open = true;

        if (const auto& ProbeName = DoGet_ProbeName(ProbeOpened);
            ImGui::Begin(ck::Format_ANSI(TEXT("{}"), ProbeName).c_str(), &Open))
        {
            RenderProbeInfo(ProbeOpened);
            ImGui::End();
        }

        if (NOT Open)
        {
            _OpenedProbes.RemoveAt(Index);
        }
    }
}

auto
    FCk_Probes_DebugWindow::
    RenderTable(
        const FCk_Handle& InSelectionEntity)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_Probes_DebugWindow_RenderTable)
    _FilteredProbes.Reset();

    AddToFilteredProbes(InSelectionEntity);

    if (_Config->_SortByName)
    {
        _FilteredProbes.Sort([&](const FCk_Handle_Probe& InProbe1, const FCk_Handle_Probe& InProbe2)
        {
            const auto& ProbeName1 = DoGet_ProbeName(InProbe1);
            const auto& ProbeName2 = DoGet_ProbeName(InProbe2);

            return ProbeName1.Compare(ProbeName2) < 0;
        });
    }

    if (ImGui::BeginTable("Probes", 4, ImGuiTableFlags_SizingFixedFit
                                        | ImGuiTableFlags_Resizable
                                        | ImGuiTableFlags_NoBordersInBodyUntilResize
                                        | ImGuiTableFlags_ScrollY
                                        | ImGuiTableFlags_RowBg
                                        | ImGuiTableFlags_BordersV
                                        | ImGuiTableFlags_Reorderable
                                        | ImGuiTableFlags_Hideable))
    {
        ImGui::TableSetupColumn("Enable", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Probe");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Policy");
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

        for (const auto& Probe : _FilteredProbes)
        {
            QUICK_SCOPE_CYCLE_COUNTER(AddProbeToTable)
            ImGui::TableNextRow();
            const auto Index = GetTypeHash(Probe);
            ImGui::PushID(Index);

            const auto& Color = FCogImguiHelper::ToImVec4(_Config->Get_ProbeColor(Probe));
            ImGui::PushStyleColor(ImGuiCol_Text, Color);

            //------------------------
            // Enable/Disable
            //------------------------
            ImGui::TableNextColumn();
            FCogWidgets::PushStyleCompact();
            auto IsEnabled = UCk_Utils_Probe_UE::Get_IsEnabledDisabled(Probe) == ECk_EnableDisable::Enable;
            if (ImGui::Checkbox("##Enable", &IsEnabled))
            {
                _ProbeHandleToToggle = Probe;
            }
            FCogWidgets::PopStyleCompact();

            //------------------------
            // Name
            //------------------------
            ImGui::TableNextColumn();

            const std::string ProbeName = ck::Format_ANSI(TEXT("{}"), DoGet_ProbeName(Probe)).c_str();

            if (ck_probes_debug_window::Filter.IsActive() && ck_probes_debug_window::Filter.PassFilter(ProbeName.c_str()))
            {
                HighlightSearchMatch(ProbeName, ck_probes_debug_window::Filter.InputBuf, ImVec4(0.31f, 0.31f, 0.33f, 1.f));
            }
            else if (ImGui::Selectable(ProbeName.c_str(),
                                      SelectedIndex == Index,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick))
            {
                SelectedIndex = Index;

                if (ImGui::IsMouseDoubleClicked(0))
                {
                    OpenProbe(Probe);
                }
            }

            ImGui::PopStyleColor(1);

            //------------------------
            // Popup
            //------------------------
            if (ImGui::IsItemHovered())
            {
                FCogWidgets::BeginTableTooltip();
                RenderProbeInfo(Probe);
                FCogWidgets::EndTableTooltip();
            }

            //------------------------
            // ContextMenu
            //------------------------
            RenderProbeContextMenu(Probe, Index);

            //------------------------
            // Status
            //------------------------
            ImGui::TableNextColumn();
            const auto& StatusText = UCk_Utils_Probe_UE::Get_IsOverlapping(Probe) ? "Overlapping" : "Not Overlapping";
            ImGui::Text("%s", StatusText);

            //------------------------
            // Policy
            //------------------------
            ImGui::TableNextColumn();
            const auto& PolicyText = UCk_Utils_Probe_UE::Get_ResponsePolicy(Probe) == ECk_ProbeResponse_Policy::Notify ? "Notify" : "Silent";
            ImGui::Text("%s", PolicyText);

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

auto
    FCk_Probes_DebugWindow::
    RenderProbeInfo(
        const FCk_Handle_Probe& InProbe)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(RenderProbeInfo)
    if (ImGui::BeginTable("Probe", 2, ImGuiTableFlags_Borders))
    {
        const auto& TextColor = ImVec4{1.0f, 1.0f, 1.0f, 0.5f};

        ImGui::TableSetupColumn("Property");
        ImGui::TableSetupColumn("Value");

        const auto& ProbeColor = FCogImguiHelper::ToImVec4(_Config->Get_ProbeColor(InProbe));

        //------------------------
        // Name
        //------------------------
        const auto& ProbeName = DoGet_ProbeName(InProbe);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Name");
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, ProbeColor);
        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), ProbeName).c_str());
        ImGui::PopStyleColor(1);

        //------------------------
        // Handle
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Handle");
        ImGui::TableNextColumn();
        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), UCk_Utils_Handle_UE::Conv_HandleToString(InProbe)).c_str());

        //------------------------
        // Enabled/Disabled
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "State");
        ImGui::TableNextColumn();
        FCogWidgets::PushStyleCompact();
        auto IsEnabled = UCk_Utils_Probe_UE::Get_IsEnabledDisabled(InProbe) == ECk_EnableDisable::Enable;
        if (ImGui::Checkbox("##State", &IsEnabled))
        {
            _ProbeHandleToToggle = InProbe;
        }
        FCogWidgets::PopStyleCompact();

        //------------------------
        // Response Policy
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Response Policy");
        ImGui::TableNextColumn();
        const auto& ResponsePolicy = UCk_Utils_Probe_UE::Get_ResponsePolicy(InProbe);
        ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), ResponsePolicy).c_str());

        //------------------------
        // Filter
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Filter");
        ImGui::TableNextColumn();
        const auto& Filter = UCk_Utils_Probe_UE::Get_Filter(InProbe);
        if (Filter.IsEmpty())
        {
            ImGui::Text("(Empty)");
        }
        else
        {
            for (const auto& Tag : Filter)
            {
                ImGui::Text("%s", ck::Format_ANSI(TEXT("{}"), Tag.GetTagName()).c_str());
            }
        }

        //------------------------
        // Motion Type
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Motion Type");
        ImGui::TableNextColumn();
        ImGui::Text("%s", TCHAR_TO_ANSI(*DoGet_ProbeMotionType(InProbe)));

        //------------------------
        // Motion Quality
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Motion Quality");
        ImGui::TableNextColumn();
        ImGui::Text("%s", TCHAR_TO_ANSI(*DoGet_ProbeMotionQuality(InProbe)));

        //------------------------
        // Is Overlapping
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Is Overlapping");
        ImGui::TableNextColumn();
        const auto& IsOverlapping = UCk_Utils_Probe_UE::Get_IsOverlapping(InProbe);
        ImGui::Text("%s", IsOverlapping ? "Yes" : "No");

        //------------------------
        // Surface Info
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Physical Material");
        ImGui::TableNextColumn();
        const auto& SurfaceInfo = UCk_Utils_Probe_UE::Get_SurfaceInfo(InProbe);
        if (const auto& PhysMat = SurfaceInfo.Get_PhysicalMaterial())
        {
            ImGui::Text("%s", TCHAR_TO_ANSI(*PhysMat->GetName()));
        }
        else
        {
            ImGui::Text("(None)");
        }

        //------------------------
        // Current Overlaps
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Current Overlaps");
        ImGui::TableNextColumn();

        const auto& CurrentOverlaps = InProbe.Get<ck::FFragment_Probe_Current>().Get_CurrentOverlaps();
        if (CurrentOverlaps.IsEmpty())
        {
            ImGui::Text("(None)");
        }
        else
        {
            ImGui::Text("Count: %d", CurrentOverlaps.Num());

            // Show overlapping entities
            for (const auto& OverlapInfo : CurrentOverlaps)
            {
                const auto& OtherEntity = OverlapInfo.Get_OtherEntity();

                // Try to get a meaningful name for the overlapping entity
                FString EntityDisplayName = TEXT("Unknown");
                if (const auto OtherProbe = UCk_Utils_Probe_UE::Cast(OtherEntity); ck::IsValid(OtherProbe))
                {
                    EntityDisplayName = UCk_Utils_Probe_UE::Get_Name(OtherProbe).GetTagName().ToString();
                }
                else
                {
                    // Fallback to handle string
                    EntityDisplayName = UCk_Utils_Handle_UE::Conv_HandleToString(OtherEntity);
                }

                // Display the overlapping entity with contact info
                const auto& ContactPoints = OverlapInfo.Get_ContactPoints();
                ImGui::Text("%s (%d contacts)", TCHAR_TO_ANSI(*EntityDisplayName), ContactPoints.Num());

                for (int32 i = 0; i < ContactPoints.Num() && i < 5; ++i) // Limit to first 5 contact points
                {
                    const auto& ContactPoint = ContactPoints[i];
                    DrawDebugPoint(GetWorld(), ContactPoint, 10.0f, FColor::Green);

                    const auto& ContactNormal = OverlapInfo.Get_ContactNormal();
                    DrawDebugDirectionalArrow(GetWorld(), ContactPoint, ContactPoint + ContactNormal * 10.0f, 5.0f, FColor::Red);
                }

                if (ImGui::IsItemHovered() && ContactPoints.Num() > 0)
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Contact Points:");
                    for (int32 i = 0; i < ContactPoints.Num() && i < 5; ++i) // Limit to first 5 contact points
                    {
                        const auto& ContactPoint = ContactPoints[i];
                        ImGui::Text("  [%d] (%.2f, %.2f, %.2f)", i, ContactPoint.X, ContactPoint.Y, ContactPoint.Z);
                    }
                    if (ContactPoints.Num() > 5)
                    {
                        ImGui::Text("  ... and %d more", ContactPoints.Num() - 5);
                    }

                    const auto& ContactNormal = OverlapInfo.Get_ContactNormal();
                    ImGui::Text("Normal: (%.2f, %.2f, %.2f)", ContactNormal.X, ContactNormal.Y, ContactNormal.Z);
                    ImGui::EndTooltip();
                }
            }
        }



        ImGui::EndTable();
    }
}

auto
    FCk_Probes_DebugWindow::
    RenderProbeContextMenu(
        const FCk_Handle_Probe& InProbe,
        int32 InIndex)
    -> void
{
    if (ImGui::BeginPopupContextItem(ck::Format_ANSI(TEXT("{}"), DoGet_ProbeName(InProbe)).c_str()))
    {
        auto Open = _OpenedProbes.Contains(InProbe);
        if (ImGui::Checkbox("Open", &Open))
        {
            if (Open)
            {
                OpenProbe(InProbe);
            }
            else
            {
                CloseProbe(InProbe);
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

auto
    FCk_Probes_DebugWindow::
    OpenProbe(
        const FCk_Handle_Probe& InProbe)
    -> void
{
    _OpenedProbes.AddUnique(InProbe);
}

auto
    FCk_Probes_DebugWindow::
    CloseProbe(
        const FCk_Handle_Probe& InProbe)
    -> void
{
    _OpenedProbes.Remove(InProbe);
}

auto
    FCk_Probes_DebugWindow::
    ProcessProbeEnableDisable(
        FCk_Handle_Probe& InProbe)
    -> void
{
    auto SelectionEntity = Get_SelectionEntity();

    if (ck::Is_NOT_Valid(SelectionEntity))
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

auto
    FCk_Probes_DebugWindow::
    AddToFilteredProbes(
        const FCk_Handle& InEntity)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(AddToFilteredProbes)

    // Check if the entity itself is a probe
    if (auto Probe = UCk_Utils_Probe_UE::Cast(InEntity);
        ck::IsValid(Probe))
    {
        UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(Probe, ECk_EnableDisable::Enable);

        const auto& IsEnabled = UCk_Utils_Probe_UE::Get_IsEnabledDisabled(Probe) == ECk_EnableDisable::Enable;
        const auto& IsOverlapping = UCk_Utils_Probe_UE::Get_IsOverlapping(Probe);
        const auto& ResponsePolicy = UCk_Utils_Probe_UE::Get_ResponsePolicy(Probe);

        // Apply filters
        if (NOT _Config->_ShowEnabled && IsEnabled)
        { return; }

        if (NOT _Config->_ShowDisabled && NOT IsEnabled)
        { return; }

        if (NOT _Config->_ShowOverlapping && IsOverlapping)
        { return; }

        if (NOT _Config->_ShowNotOverlapping && NOT IsOverlapping)
        { return; }

        if (NOT _Config->_ShowNotifyPolicy && ResponsePolicy == ECk_ProbeResponse_Policy::Notify)
        { return; }

        if (NOT _Config->_ShowSilentPolicy && ResponsePolicy == ECk_ProbeResponse_Policy::Silent)
        { return; }

        // Apply search filter
        if (const auto& ProbeName = DoGet_ProbeName(Probe);
            NOT ck_probes_debug_window::Filter.PassFilter(TCHAR_TO_ANSI(*ProbeName.ToString())))
        { return; }

        _FilteredProbes.AddUnique(Probe);
    }

    // TODO: If needed, add logic to search child entities for probes
    // This would be similar to how abilities search through sub-abilities
}

// --------------------------------------------------------------------------------------------------------------------