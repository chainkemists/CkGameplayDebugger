#include "CkTimer_DebugWindow.h"

#include "CogImguiHelper.h"
#include "CogWindowWidgets.h"
#include "CkTimer/CkTimer_Utils.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_Timer_DebugWindow::
    Initialize()
    -> void
{
    Super::Initialize();

    bHasMenu = true;
    bNoPadding = true;

    _Config = GetConfig<UCk_Timer_DebugWindowConfig>();
}

auto
    FCk_Timer_DebugWindow::
    ResetConfig() -> void
{
    Super::ResetConfig();

    _Config->Reset();
}

auto
    FCk_Timer_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text("This window displays the timers of the selected actor");
}

auto
    FCk_Timer_DebugWindow::
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

    if (const auto& HasTimers = UCk_Utils_Timer_UE::Has_Any(SelectionEntity);
        NOT HasTimers)
    {
        ImGui::Text("Selection Actor has no Timers");
        return;
    }

    RenderTable(SelectionEntity);
}

auto
    FCk_Timer_DebugWindow::
    RenderMenu()
    -> void
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Checkbox("Sort by Name", &_Config->SortByName);

            ImGui::Separator();
            ImGui::ColorEdit4("Running Color", reinterpret_cast<float*>(&_Config->RunningColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Paused Color", reinterpret_cast<float*>(&_Config->PausedColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);

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
    FCk_Timer_DebugWindow::
    RenderTable(
        FCk_Handle& InSelectionEntity)
    -> void
{
    if (ImGui::BeginTable("Timers", 2, ImGuiTableFlags_SizingFixedFit
                                     | ImGuiTableFlags_Resizable
                                     | ImGuiTableFlags_NoBordersInBodyUntilResize
                                     | ImGuiTableFlags_ScrollY
                                     | ImGuiTableFlags_RowBg
                                     | ImGuiTableFlags_BordersV
                                     | ImGuiTableFlags_Reorderable
                                     | ImGuiTableFlags_Hideable))
    {

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Timer");
        ImGui::TableSetupColumn("Elapsed Time");
        ImGui::TableHeadersRow();

        static int32 Selected = -1;
        int32 Index = 0;

        const auto& RenderRow = [&](const FCk_Handle_Timer& InTimer)
        {
            if (ck::Is_NOT_Valid(InTimer))
            { return; }

            const auto& TimerLabel = UCk_Utils_GameplayLabel_UE::Get_Label(InTimer);
            const auto& TimerName = StringCast<ANSICHAR>(*TimerLabel.ToString()).Get();

            if (NOT _Filter.PassFilter(TimerName))
            { return; }

            if (_Config->ShowOnlyRunningTimers && UCk_Utils_Timer_UE::Get_CurrentState(InTimer) != ECk_Timer_State::Running)
            { return; }

            ImGui::TableNextRow();

            const ImVec4 Color = FCogImguiHelper::ToImVec4(_Config->Get_TimerColor(InTimer));
            ImGui::PushStyleColor(ImGuiCol_Text, Color);

            //------------------------
            // Name
            //------------------------
            ImGui::TableNextColumn();
            ImGui::Text("");
            ImGui::SameLine();
            if (ImGui::Selectable(TimerName, Selected == Index, ImGuiSelectableFlags_SpanAllColumns))
            {
                Selected = Index;
            }
            ImGui::PopStyleColor(1);

            ImGui::PushStyleColor(ImGuiCol_Text, Color);

            //------------------------
            // Elapsed Timer
            //------------------------
            const auto& TimerValue = UCk_Utils_Timer_UE::Get_CurrentTimerValue(InTimer);
            const auto& ElapsedTime = TimerValue.Get_TimeElapsed().Get_Seconds();
            const auto& Duration = TimerValue.Get_GoalValue().Get_Seconds();

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, IM_COL32(100, 100, 100, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 100));

            // Begin layout for progress bar
            ImGui::BeginGroup();
            {
                // Progress bar
                ImGui::ProgressBar(ElapsedTime / Duration, ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 0.8f), "");
                ImGui::SameLine();
                const auto& GroupSize = ImGui::GetItemRectSize();

                // Centered text
                const char* Text = TCHAR_TO_ANSI(*FString::Printf(TEXT("%.2f / %.2f"), ElapsedTime, Duration));
                const auto& TextSize = ImGui::CalcTextSize(Text);
                const auto& ProgressBarPos = ImGui::GetCursorPos();

                const auto TextPosX = ProgressBarPos.x - (GroupSize.x * 0.5) - (0.6 * TextSize.x);
                const auto TextPos = ImVec2(TextPosX, ProgressBarPos.y);
                ImGui::SetCursorPos(TextPos);
                ImGui::Text(Text);
            }
            ImGui::EndGroup();

            ImGui::PopStyleColor(3);

            ++Index;
        };

        UCk_Utils_Timer_UE::ForEach_Timer(InSelectionEntity, RenderRow);

        ImGui::EndTable();
    }
}

// --------------------------------------------------------------------------------------------------------------------

