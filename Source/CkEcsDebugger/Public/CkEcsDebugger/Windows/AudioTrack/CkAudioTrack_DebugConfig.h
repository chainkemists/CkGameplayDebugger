#pragma once

#include "CogCommonConfig.h"

#include "CkAudio/AudioTrack/CkAudioTrack_Fragment_Data.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkAudioTrack_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_AudioTrack_DebugWindowConfig : public UCogCommonConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_AudioTrack_DebugWindowConfig);

public:
    auto
    Reset() -> void override;

    auto
    Get_TrackColor(
        const FCk_Handle_AudioTrack& InTrack) const -> const FVector4f&;

private:
    inline static FVector4f _PlayingColor{0.33f, 0.59f, 1.0f, 1.0f};      // Blue
    inline static FVector4f _FadingColor{1.0f, 0.76f, 0.03f, 1.0f};      // Yellow
    inline static FVector4f _StoppedColor{0.62f, 0.62f, 0.62f, 1.0f};    // Gray
    inline static FVector4f _PausedColor{1.0f, 0.5f, 0.5f, 1.0f};        // Red

public:
    UPROPERTY(Config)
    bool SortByName = false;

    UPROPERTY(Config)
    bool SortByPriority = true;

    UPROPERTY(Config)
    bool ShowOnlyPlaying = false;

    UPROPERTY(Config)
    bool ShowSpatialInfo = true;

    UPROPERTY(Config)
    FVector4f PlayingColor = _PlayingColor;

    UPROPERTY(Config)
    FVector4f FadingColor = _FadingColor;

    UPROPERTY(Config)
    FVector4f StoppedColor = _StoppedColor;

    UPROPERTY(Config)
    FVector4f PausedColor = _PausedColor;
};

//--------------------------------------------------------------------------------------------------------------------------