#include "CkAudioTrack_DebugWindow.h"

#include "CogImguiHelper.h"
#include <Cog/Public/CogWidgets.h>

#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"
#include "CkAudio/AudioDirector/CkAudioDirector_Utils.h"

#include "CkCore/String/CkString_Utils.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_audiotrack_debug_window
{
    static ImGuiTextFilter Filter;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_AudioTrack_DebugWindow::
    Initialize() -> void
{
    Super::Initialize();

    bHasMenu = true;

    _Config = GetConfig<UCk_AudioTrack_DebugWindowConfig>();
}

auto
    FCk_AudioTrack_DebugWindow::
    ResetConfig()
    -> void
{
    Super::ResetConfig();

    _Config->Reset();
}

auto
    FCk_AudioTrack_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text
    (
        "This window displays the audio tracks of the selected entities. "
        "Click the control buttons to play/pause/stop tracks. "
        "When multiple entities are selected, tracks from all entities are shown."
    );
}

auto
    FCk_AudioTrack_DebugWindow::
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

    RenderTable(SelectionEntities);
}

auto
    FCk_AudioTrack_DebugWindow::
    GameTick(
        float InDeltaT)
    -> void
{
    Super::GameTick(InDeltaT);

    if (ck::IsValid(_PendingTrack) && !_PendingAction.IsEmpty())
    {
        ProcessTrackRequest(_PendingTrack, _PendingAction);
        _PendingTrack = {};
        _PendingAction.Empty();
    }
}

auto
    FCk_AudioTrack_DebugWindow::
    RenderMenu()
    -> void
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Separator();

            ImGui::Checkbox("Sort by Name", &_Config->SortByName);
            ImGui::Checkbox("Sort by Priority", &_Config->SortByPriority);
            ImGui::Checkbox("Show Only Playing", &_Config->ShowOnlyPlaying);
            ImGui::Checkbox("Show Spatial Info", &_Config->ShowSpatialInfo);

            ImGui::Separator();

            ImGui::ColorEdit4("Playing Color", reinterpret_cast<float*>(&_Config->PlayingColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Fading Color", reinterpret_cast<float*>(&_Config->FadingColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Stopped Color", reinterpret_cast<float*>(&_Config->StoppedColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::ColorEdit4("Paused Color", reinterpret_cast<float*>(&_Config->PausedColor), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf);

            ImGui::Separator();

            if (ImGui::MenuItem("Reset"))
            {
                ResetConfig();
            }

            ImGui::EndMenu();
        }

        FCogWidgets::SearchBar("##Filter", ck_audiotrack_debug_window::Filter);

        ImGui::EndMenuBar();
    }
}

auto
    FCk_AudioTrack_DebugWindow::
    RenderTable(
        const TArray<FCk_Handle>& InSelectionEntities)
    -> void
{
    QUICK_SCOPE_CYCLE_COUNTER(FCk_AudioTrack_DebugWindow_RenderTable)

    // Group tracks by their source entity
    TMap<FCk_Handle, TArray<FCk_Handle_AudioTrack>> TracksByEntity;
    TMap<FCk_Handle, FString> EntityTypes;

    for (const auto& Entity : InSelectionEntities)
    {
        if (ck::Is_NOT_Valid(Entity))
        { continue; }

        // Check if this entity is an AudioTrack directly
        if (auto Track = UCk_Utils_AudioTrack_UE::Cast(Entity); ck::IsValid(Track))
        {
            TracksByEntity.FindOrAdd(Entity).Add(Track);
            EntityTypes.Add(Entity, TEXT("AudioTrack"));
        }

        // Check if this entity has an AudioDirector that contains tracks
        if (auto Director = UCk_Utils_AudioDirector_UE::Cast(Entity); ck::IsValid(Director))
        {
            const auto& DirectorTracks = UCk_Utils_AudioDirector_UE::Get_AllTracks(Director);
            if (DirectorTracks.Num() > 0)
            {
                TracksByEntity.FindOrAdd(Entity) = DirectorTracks;
                EntityTypes.Add(Entity, TEXT("AudioDirector"));
            }
        }
    }

    if (TracksByEntity.IsEmpty())
    {
        if (InSelectionEntities.Num() == 1)
        {
            ImGui::Text("Selected entity has no audio tracks");
        }
        else
        {
            ImGui::Text("Selected entities have no audio tracks");
        }
        return;
    }

    // Render sections for each entity
    int32 TotalTracks = 0;
    int32 TotalFilteredTracks = 0;

    for (const auto& EntityTrackPair : TracksByEntity)
    {
        const auto& SourceEntity = EntityTrackPair.Key;
        const auto& AllTracks = EntityTrackPair.Value;
        const auto* EntityType = EntityTypes.Find(SourceEntity);

        TotalTracks += AllTracks.Num();

        // Filter tracks for this entity
        TArray<FCk_Handle_AudioTrack> FilteredTracks;
        for (const auto& Track : AllTracks)
        {
            const auto& TrackName = Get_TrackName(Track);

            if (ck_audiotrack_debug_window::Filter.IsActive() &&
                NOT ck_audiotrack_debug_window::Filter.PassFilter(TCHAR_TO_ANSI(*TrackName)))
            { continue; }

            if (_Config->ShowOnlyPlaying)
            {
                const auto& State = UCk_Utils_AudioTrack_UE::Get_State(Track);
                if (State != ECk_AudioTrack_State::Playing &&
                    State != ECk_AudioTrack_State::FadingIn &&
                    State != ECk_AudioTrack_State::FadingOut)
                { continue; }
            }

            FilteredTracks.Add(Track);
        }

        TotalFilteredTracks += FilteredTracks.Num();

        // Skip empty sections if filtering
        if (FilteredTracks.IsEmpty() && (ck_audiotrack_debug_window::Filter.IsActive() || _Config->ShowOnlyPlaying))
        { continue; }

        // Sort tracks for this entity
        if (_Config->SortByPriority)
        {
            FilteredTracks.Sort([](const FCk_Handle_AudioTrack& A, const FCk_Handle_AudioTrack& B)
            {
                const auto PriorityA = UCk_Utils_AudioTrack_UE::Get_Priority(A);
                const auto PriorityB = UCk_Utils_AudioTrack_UE::Get_Priority(B);
                return PriorityA > PriorityB; // Higher priority first
            });
        }
        else if (_Config->SortByName)
        {
            FilteredTracks.Sort([this](const FCk_Handle_AudioTrack& A, const FCk_Handle_AudioTrack& B)
            {
                const auto NameA = Get_TrackName(A);
                const auto NameB = Get_TrackName(B);
                return NameA < NameB;
            });
        }

        // Create section header
        const auto& EntityName = UCk_Utils_Handle_UE::Get_DebugName(SourceEntity);
        const auto& SectionTitle = ck::Format_UE(TEXT("{} ({}) - {} tracks"),
            EntityName, EntityType ? *EntityType : TEXT("Unknown"), FilteredTracks.Num());

        if (ImGui::CollapsingHeader(TCHAR_TO_ANSI(*SectionTitle), ImGuiTreeNodeFlags_DefaultOpen))
        {
            RenderTracksTable(FilteredTracks, SourceEntity);
        }

        // Add spacing between sections
        if (TracksByEntity.Num() > 1)
        {
            ImGui::Spacing();
        }
    }

    // Summary
    ImGui::Separator();
    if (TotalFilteredTracks != TotalTracks)
    {
        ImGui::Text("Showing %d of %d audio tracks across %d entities",
            TotalFilteredTracks, TotalTracks, TracksByEntity.Num());
    }
    else
    {
        ImGui::Text("Found %d audio tracks across %d entities",
            TotalTracks, TracksByEntity.Num());
    }
}

auto
    FCk_AudioTrack_DebugWindow::
    RenderTracksTable(
        const TArray<FCk_Handle_AudioTrack>& InTracks,
        const FCk_Handle& InSourceEntity)
    -> void
{
    if (InTracks.IsEmpty())
    {
        ImGui::Text("  No tracks to display");
        return;
    }

    // Calculate appropriate table height based on number of rows
    const float RowHeight = ImGui::GetTextLineHeightWithSpacing();
    const float HeaderHeight = RowHeight;
    const float MaxTableHeight = HeaderHeight + (RowHeight * InTracks.Num()) + 10.0f;

    // Use source entity handle hash for stable, unique table ID
    const auto TableID = FString::Printf(TEXT("AudioTracksTable_%u"), GetTypeHash(InSourceEntity));

    if (ImGui::BeginTable(TCHAR_TO_ANSI(*TableID), _Config->ShowSpatialInfo ? 6 : 5,
        ImGuiTableFlags_Resizable
        | ImGuiTableFlags_NoBordersInBodyUntilResize
        | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_BordersV
        | ImGuiTableFlags_Reorderable
        | ImGuiTableFlags_Hideable,
        ImVec2(0.0f, MaxTableHeight)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Track Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        if (_Config->ShowSpatialInfo)
        {
            ImGui::TableSetupColumn("Spatial", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        }
        ImGui::TableHeadersRow();

        for (int32 Index = 0; Index < InTracks.Num(); ++Index)
        {
            const auto& Track = InTracks[Index];

            ImGui::TableNextRow();
            ImGui::PushID(GetTypeHash(Track));

            const auto& Color = FCogImguiHelper::ToImVec4(_Config->Get_TrackColor(Track));

            //------------------------
            // Controls
            //------------------------
            ImGui::TableNextColumn();
            FCogWidgets::PushStyleCompact();

            const auto& State = UCk_Utils_AudioTrack_UE::Get_State(Track);
            const bool IsPlaying = State == ECk_AudioTrack_State::Playing || State == ECk_AudioTrack_State::FadingIn;

            if (ImGui::Button(IsPlaying ? "||" : ">"))
            {
                UE_LOG(LogTemp, Warning, TEXT("Play/Pause button clicked for track!"));
                _PendingTrack = Track;
                _PendingAction = IsPlaying ? TEXT("Pause") : TEXT("Play");
            }
            ImGui::SameLine(0, 2);
            if (ImGui::Button("[]"))
            {
                UE_LOG(LogTemp, Warning, TEXT("Stop button clicked for track!"));
                _PendingTrack = Track;
                _PendingAction = TEXT("Stop");
            }

            FCogWidgets::PopStyleCompact();

            //------------------------
            // Track Name
            //------------------------
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, Color);
            const auto& TrackName = Get_TrackName(Track);
            ImGui::Text(TCHAR_TO_ANSI(*TrackName));

            if (ImGui::IsItemHovered())
            {
                FCogWidgets::BeginTableTooltip();
                RenderTrackInfo(Track);
                FCogWidgets::EndTableTooltip();
            }
            ImGui::PopStyleColor();

            //------------------------
            // State
            //------------------------
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, Color);
            const auto& StateString = Get_TrackStateString(Track);
            ImGui::Text(TCHAR_TO_ANSI(*StateString));
            ImGui::PopStyleColor();

            //------------------------
            // Priority
            //------------------------
            ImGui::TableNextColumn();
            const auto Priority = UCk_Utils_AudioTrack_UE::Get_Priority(Track);
            const auto PriorityColor = Get_PriorityBadgeColor(Priority);
            ImGui::PushStyleColor(ImGuiCol_Text, PriorityColor);
            ImGui::Text("%d", Priority);
            ImGui::PopStyleColor();

            //------------------------
            // Volume
            //------------------------
            ImGui::TableNextColumn();
            const auto Volume = UCk_Utils_AudioTrack_UE::Get_CurrentVolume(Track);
            ImGui::Text("%.2f", Volume);

            //------------------------
            // Spatial Info
            //------------------------
            if (_Config->ShowSpatialInfo)
            {
                ImGui::TableNextColumn();
                const auto& SpatialInfo = Get_SpatialInfoString(Track);
                ImGui::Text(TCHAR_TO_ANSI(*SpatialInfo));
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

auto
    FCk_AudioTrack_DebugWindow::
    RenderTrackInfo(
        const FCk_Handle_AudioTrack& InTrack)
    -> void
{
    if (ImGui::BeginTable("TrackInfo", 2, ImGuiTableFlags_Borders))
    {
        const auto& TextColor = ImVec4{1.0f, 1.0f, 1.0f, 0.5f};

        ImGui::TableSetupColumn("Property");
        ImGui::TableSetupColumn("Value");

        const auto& TrackColor = FCogImguiHelper::ToImVec4(_Config->Get_TrackColor(InTrack));

        //------------------------
        // Track Name
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Track Name");
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, TrackColor);
        const auto& TrackName = Get_TrackName(InTrack);
        ImGui::Text(TCHAR_TO_ANSI(*TrackName));
        ImGui::PopStyleColor();

        //------------------------
        // State
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "State");
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, TrackColor);
        const auto& StateString = Get_TrackStateString(InTrack);
        ImGui::Text(TCHAR_TO_ANSI(*StateString));
        ImGui::PopStyleColor();

        //------------------------
        // Priority
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Priority");
        ImGui::TableNextColumn();
        const auto Priority = UCk_Utils_AudioTrack_UE::Get_Priority(InTrack);
        const auto PriorityColor = Get_PriorityBadgeColor(Priority);
        ImGui::PushStyleColor(ImGuiCol_Text, PriorityColor);
        ImGui::Text("%d", Priority);
        ImGui::PopStyleColor();

        //------------------------
        // Volume
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Volume");
        ImGui::TableNextColumn();
        const auto Volume = UCk_Utils_AudioTrack_UE::Get_CurrentVolume(InTrack);
        ImGui::Text("%.2f", Volume);

        //------------------------
        // Override Behavior
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Override Behavior");
        ImGui::TableNextColumn();
        const auto& OverrideBehavior = UCk_Utils_AudioTrack_UE::Get_OverrideBehavior(InTrack);
        ImGui::Text(TCHAR_TO_ANSI(*ck::Format_UE(TEXT("{}"), OverrideBehavior)));

        //------------------------
        // Spatial
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Spatial");
        ImGui::TableNextColumn();
        const auto IsSpatial = UCk_Utils_AudioTrack_UE::Get_IsSpatial(InTrack);
        ImGui::Text("%s", IsSpatial ? "Yes" : "No");

        //------------------------
        // Handle
        //------------------------
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(TextColor, "Handle");
        ImGui::TableNextColumn();
        ImGui::Text(TCHAR_TO_ANSI(*UCk_Utils_Handle_UE::Conv_HandleToString(InTrack)));

        ImGui::EndTable();
    }
}

auto
    FCk_AudioTrack_DebugWindow::
    ProcessTrackRequest(
        FCk_Handle_AudioTrack& InTrack,
        const FString& InAction)
    -> void
{
    UE_LOG(LogTemp, Warning, TEXT("AudioTrack Debug: Processing action '%s' for track %s"),
        *InAction, *UCk_Utils_Handle_UE::Conv_HandleToString(InTrack));

    if (InAction == TEXT("Play"))
    {
        UCk_Utils_AudioTrack_UE::Request_Play(InTrack, FCk_Time::ZeroSecond());
    }
    else if (InAction == TEXT("Stop"))
    {
        UCk_Utils_AudioTrack_UE::Request_Stop(InTrack, FCk_Time::ZeroSecond());
    }
    else if (InAction == TEXT("Pause"))
    {
        // If there's no pause functionality, use stop instead
        UCk_Utils_AudioTrack_UE::Request_Stop(InTrack, FCk_Time::ZeroSecond());
    }
}

auto
    FCk_AudioTrack_DebugWindow::
    Get_TrackName(
        const FCk_Handle_AudioTrack& InTrack) const
    -> FString
{
    const auto& TrackName = UCk_Utils_AudioTrack_UE::Get_TrackName(InTrack);
    return TrackName.GetTagName().ToString();
}

auto
    FCk_AudioTrack_DebugWindow::
    Get_TrackStateString(
        const FCk_Handle_AudioTrack& InTrack) const
    -> FString
{
    const auto& State = UCk_Utils_AudioTrack_UE::Get_State(InTrack);
    return ck::Format_UE(TEXT("{}"), State);
}

auto
    FCk_AudioTrack_DebugWindow::
    Get_PriorityBadgeColor(
        int32 InPriority) const
    -> ImU32
{
    if (InPriority >= 75)
    {
        return IM_COL32(244, 67, 54, 255); // High priority - Red
    }
    else if (InPriority >= 50)
    {
        return IM_COL32(255, 152, 0, 255); // Medium priority - Orange
    }
    else
    {
        return IM_COL32(76, 175, 80, 255); // Low priority - Green
    }
}

auto
    FCk_AudioTrack_DebugWindow::
    Get_SpatialInfoString(
        const FCk_Handle_AudioTrack& InTrack) const
    -> FString
{
    const auto IsSpatial = UCk_Utils_AudioTrack_UE::Get_IsSpatial(InTrack);
    if (IsSpatial)
    {
        return TEXT("3D Audio");
    }
    else
    {
        return TEXT("Non-spatial");
    }
}

// --------------------------------------------------------------------------------------------------------------------
