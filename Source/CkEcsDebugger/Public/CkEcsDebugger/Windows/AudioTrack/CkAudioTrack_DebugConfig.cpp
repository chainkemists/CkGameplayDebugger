#include "CkAudioTrack_DebugConfig.h"

#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    UCk_AudioTrack_DebugWindowConfig::
    Reset()
    -> void
{
    Super::Reset();

    SortByName = false;
    SortByPriority = true;
    ShowOnlyPlaying = false;
    ShowSpatialInfo = true;

    PlayingColor = _PlayingColor;
    FadingColor = _FadingColor;
    StoppedColor = _StoppedColor;
    PausedColor = _PausedColor;
}

auto
    UCk_AudioTrack_DebugWindowConfig::
    Get_TrackColor(
        const FCk_Handle_AudioTrack& InTrack) const
    -> const FVector4f&
{
    const auto& State = UCk_Utils_AudioTrack_UE::Get_State(InTrack);

    switch (State)
    {
        case ECk_AudioTrack_State::Playing:
            return PlayingColor;
        case ECk_AudioTrack_State::FadingIn:
        case ECk_AudioTrack_State::FadingOut:
            return FadingColor;
        case ECk_AudioTrack_State::Paused:
            return PausedColor;
        case ECk_AudioTrack_State::Stopped:
        default:
            return StoppedColor;
    }
}

//--------------------------------------------------------------------------------------------------------------------------