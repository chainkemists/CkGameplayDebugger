#pragma once

#include "CkAudio/AudioDirector/CkAudioDirector_Fragment_Data.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Fragment_Data.h"

#include "CkEcs/Handle/CkHandle.h"

#include "Engine/Attenuation.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

/** One live audio track, as a read-only snapshot.
 *
 *  Plain data on purpose: the collector runs on a refresh gate and the window renders from what it captured, so a
 *  track destroyed between the two must not take a dangling read with it. The one handle kept is the track ENTITY,
 *  which the row is keyed by and which is validity-checked before use. */
struct FCkAudioDebugger_TrackInfo
{
    FCk_Handle TrackEntity;

    FString TrackName;
    FString SoundPath;

    ECk_AudioTrack_State State = ECk_AudioTrack_State::Stopped;

    // The three numbers the mixer page exists for. `CurrentVolume` alone never explains itself — it is the pair with
    // `TargetVolume`, plus the rate between them, that says whether a track is steady, fading in, or fading out.
    float CurrentVolume = 0.0f;
    float TargetVolume = 0.0f;
    float FadeSpeed = 0.0f;

    float PlaybackPercent = 0.0f;

    /** Playing at a real volume and inaudible anyway. The single most common audio bug, and the reason this is a
     *  first-class field rather than something the reader infers. */
    bool IsVirtualized = false;

    /** False when the pooled component behind the track has already been released — the track entity outlives it, and
     *  a row that printed a volume for a track with no component would be reporting a number nothing is playing. */
    bool HasAudioComponent = false;

    int32 Priority = 0;
    ECk_AudioTrack_OverrideBehavior OverrideBehavior = ECk_AudioTrack_OverrideBehavior::Interrupt;
    ECk_LoopBehavior LoopBehavior = ECk_LoopBehavior::Loop;

    /** Everything the Spatial page needs to answer "why is it that loud".
     *
     *  `HasSpatialData` gates the whole block and is false whenever the component is gone or carries no attenuation:
     *  a page that drew a radar from zeroed fields would put a silent 2D track at the listener's feet and invite the
     *  reader to conclude something about a position that was never computed. */
    bool HasSpatialData = false;

    /** Distinct on purpose. `bAttenuate` off means distance does not change the gain; `bSpatialize` off means the
     *  sound has no position to place. A track can be one without the other, and the two failure reports differ. */
    bool IsAttenuated = false;
    bool IsSpatialized = false;

    FVector WorldLocation = FVector::ZeroVector;

    /** Centimetres, as the engine stores them — converted to metres only where they are rendered. */
    float DistanceToListener = 0.0f;
    float InnerRadius = 0.0f;
    float FalloffDistance = 0.0f;
    float MaxFalloffDistance = 0.0f;

    /** Signed degrees off the listener's forward, positive to the right. */
    float BearingDegrees = 0.0f;

    /** From the engine's OWN `FBaseAttenuationSettings::Evaluate` at the live listener position, never re-derived
     *  here. A debugger that reimplemented the falloff maths would eventually disagree with the mixer, and would be
     *  believed over it. */
    float AttenuationGain = 1.0f;

    /** The same `Evaluate`, swept over distance — so the plotted curve and the marked point cannot disagree. */
    TArray<float> FalloffCurve;

    FString AttenuationAssetName;

    EAttenuationDistanceModel DistanceModel = EAttenuationDistanceModel::Linear;
    TEnumAsByte<EAttenuationShape::Type> AttenuationShape = EAttenuationShape::Sphere;

    /** What the listener actually hears: the attenuation gain times the track's own volume. The whole point of the
     *  Spatial page is that these two are different numbers and only their product is audibility. */
    auto Get_AudibleVolume() const -> float
    {
        return IsVirtualized ? 0.0f : CurrentVolume * (IsAttenuated ? AttenuationGain : 1.0f);
    }

    /** Playing, positioned, and past the point where the curve has bottomed out. Reported separately from
     *  virtualization because the two have different fixes — move the sound vs raise the concurrency budget. */
    auto Get_IsOutOfRange() const -> bool
    {
        return HasSpatialData
            && IsAttenuated
            && State != ECk_AudioTrack_State::Stopped
            && MaxFalloffDistance > 0.0f
            && DistanceToListener > MaxFalloffDistance;
    }
};

// --------------------------------------------------------------------------------------------------------------------

/** One AudioDirector and the tracks it owns. */
struct FCkAudioDebugger_DirectorInfo
{
    FCk_Handle DirectorEntity;

    FString DirectorName;

    int32 MaxConcurrentTracks = 0;

    /** The policy the header states, because it is what decides what happens to the rows underneath it: whether a
     *  same-priority track is admitted at all, and how long the fade it arrives on lasts. Unset crossfade means the
     *  director has no default and each track's own fade times govern. */
    TOptional<float> DefaultCrossfadeSeconds;

    ECk_SamePriorityBehavior SamePriorityBehavior = ECk_SamePriorityBehavior::Block;

    TArray<FCkAudioDebugger_TrackInfo> Tracks;

    /** Tracks that are not Stopped — what the director's concurrency budget is actually spent on. Counted here rather
     *  than derived in the window so the header and the list cannot disagree. */
    auto Get_ActiveTrackCount() const -> int32
    {
        auto Count = 0;

        for (const auto& Track : Tracks)
        {
            if (Track.State != ECk_AudioTrack_State::Stopped)
            { ++Count; }
        }

        return Count;
    }
};

// --------------------------------------------------------------------------------------------------------------------

/** One track plotted on the radar, in radar terms rather than world terms.
 *
 *  Reduced to bearing + distance at collection time so the widget does no world maths: a paint pass that had to know
 *  about listener orientation would be a second place for the projection to be wrong. */
struct FCkAudioDebugger_SpatialBlip
{
    FString TrackName;

    float BearingDegrees = 0.0f;
    float DistanceCm = 0.0f;

    bool IsSelected = false;
    bool IsVirtualized = false;
    bool IsOutOfRange = false;
    bool IsAudible = false;
};

// --------------------------------------------------------------------------------------------------------------------

/** What the Spatial page's two custom widgets paint from.
 *
 *  Owned by the window, allocated once and mutated in place on the refresh gate, so the radar and the curve follow
 *  live values without either widget being rebuilt (same contract as `SCkDebug_Sparkline`'s sample ring). */
struct FCkAudioDebugger_SpatialView
{
    bool HasSelection = false;

    /** Distinct from `HasSelection`: a track can be selected and still have nothing to plot, because it is 2D or its
     *  component is gone. The page says which of those it is rather than drawing an empty radar. */
    bool HasSpatialData = false;

    FString TrackName;
    FString AttenuationAssetName;
    FString ListenerSource;

    float DistanceCm = 0.0f;
    float InnerRadiusCm = 0.0f;
    float FalloffCm = 0.0f;
    float MaxFalloffCm = 0.0f;
    float BearingDegrees = 0.0f;

    float AttenuationGain = 1.0f;
    float TrackVolume = 0.0f;
    float AudibleVolume = 0.0f;

    bool IsAttenuated = false;
    bool IsSpatialized = false;
    bool IsVirtualized = false;
    bool IsOutOfRange = false;

    /** Gain sampled over `0 .. RadarRangeCm`-ish, produced by the engine's own attenuation evaluation. */
    TArray<float> FalloffCurve;

    TArray<FCkAudioDebugger_SpatialBlip> Blips;

    /** The radar's outer edge. Never smaller than the selected track's distance, so the thing being inspected can
     *  never fall off the picture that exists to show where it is. */
    float RadarRangeCm = 2000.0f;
};

// --------------------------------------------------------------------------------------------------------------------

/** Everything one refresh saw.
 *
 *  `HasWorld` is the difference between "this world has no audio directors" and "there is no world to look at" — a
 *  window that rendered both as an empty list would show a PIE-less editor as a silent game. */
struct FCkAudioDebugger_Snapshot
{
    bool HasWorld = false;

    /** The listener every spatial number on the page is measured FROM, plus where it came from. Named rather than
     *  assumed: the audio device's own listener and the player camera are usually the same point and occasionally
     *  are not, and a distance is meaningless until the reader knows which one it is relative to. */
    bool    HasListener = false;
    FVector ListenerLocation = FVector::ZeroVector;
    FVector ListenerForward = FVector::ForwardVector;
    FString ListenerSource;

    TArray<FCkAudioDebugger_DirectorInfo> Directors;

    auto Get_TrackCount() const -> int32
    {
        auto Count = 0;

        for (const auto& Director : Directors)
        { Count += Director.Tracks.Num(); }

        return Count;
    }

    auto Get_AudibleCount() const -> int32
    {
        auto Count = 0;

        for (const auto& Director : Directors)
        {
            for (const auto& Track : Director.Tracks)
            {
                // Audible means all three: playing, not virtualized, and actually at a volume. A track failing any
                // one of those is inaudible however healthy the other two look — which is exactly the state this
                // count exists to make visible.
                if (Track.State != ECk_AudioTrack_State::Stopped
                    && NOT Track.IsVirtualized
                    && Track.CurrentVolume > 0.0f)
                { ++Count; }
            }
        }

        return Count;
    }

    auto Get_VirtualizedCount() const -> int32
    {
        auto Count = 0;

        for (const auto& Director : Directors)
        {
            for (const auto& Track : Director.Tracks)
            {
                if (Track.IsVirtualized)
                { ++Count; }
            }
        }

        return Count;
    }

    auto Get_FadingCount() const -> int32
    {
        auto Count = 0;

        for (const auto& Director : Directors)
        {
            for (const auto& Track : Director.Tracks)
            {
                if (Track.State == ECk_AudioTrack_State::FadingIn
                    || Track.State == ECk_AudioTrack_State::FadingOut)
                { ++Count; }
            }
        }

        return Count;
    }
};

// --------------------------------------------------------------------------------------------------------------------
