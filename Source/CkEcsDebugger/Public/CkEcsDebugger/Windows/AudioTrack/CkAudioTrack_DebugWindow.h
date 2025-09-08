#pragma once

#include "CkAudio/AudioTrack/CkAudioTrack_Fragment_Data.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"
#include "CkEcsDebugger/Windows/AudioTrack/CkAudioTrack_DebugConfig.h"

// --------------------------------------------------------------------------------------------------------------------

class FCk_AudioTrack_DebugWindow : public FCk_Ecs_DebugWindow
{
    using Super = FCk_Ecs_DebugWindow;

public:
    auto
    Initialize() -> void override;

protected:
    auto
    ResetConfig() -> void override;

    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;

    auto
    GameTick(
        float InDeltaT) -> void override;

    auto
    RenderMenu() -> void;

    auto
    RenderTable(
        const TArray<FCk_Handle>& InSelectionEntities) -> void;

    auto
    RenderTracksTable(
        const TArray<FCk_Handle_AudioTrack>& InTracks,
        const FCk_Handle& InSourceEntity) -> void;

    auto
    RenderTrackInfo(
        const FCk_Handle_AudioTrack& InTrack) -> void;

    auto
    ProcessTrackRequest(
        FCk_Handle_AudioTrack& InTrack,
        const FString& InAction) -> void;

private:
    auto
    Get_TrackName(
        const FCk_Handle_AudioTrack& InTrack) const -> FString;

    auto
    Get_TrackStateString(
        const FCk_Handle_AudioTrack& InTrack) const -> FString;

    auto
    Get_PriorityBadgeColor(
        int32 InPriority) const -> ImU32;

    auto
    Get_SpatialInfoString(
        const FCk_Handle_AudioTrack& InTrack) const -> FString;

private:
    TObjectPtr<UCk_AudioTrack_DebugWindowConfig> _Config;

    ImGuiTextFilter _Filter;
    FString _PendingAction;
    FCk_Handle_AudioTrack _PendingTrack;
};

// --------------------------------------------------------------------------------------------------------------------
