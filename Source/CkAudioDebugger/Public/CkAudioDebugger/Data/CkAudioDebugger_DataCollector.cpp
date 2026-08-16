#include "CkAudioDebugger/Data/CkAudioDebugger_DataCollector.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkRecord/Record/CkRecord_Utils.h"

#include "CkAudio/AudioDirector/CkAudioDirector_Fragment.h"
#include "CkAudio/AudioDirector/CkAudioDirector_Utils.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Fragment.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"

#include "AudioDevice.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundAttenuation.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_audio_debugger_collector
{
    /** Points on the plotted falloff curve. Enough that a Logarithmic knee reads as a curve rather than as a bend,
     *  cheap enough to resample every gated refresh — the attenuation asset can be swapped live. */
    constexpr auto k_FalloffCurveSamples = 48;

    /** The curve is drawn past the falloff distance on purpose: the reader has to be able to SEE that it has bottomed
     *  out, and a plot that stopped exactly at the last audible metre would hide the difference between "quiet" and
     *  "silent past here". */
    constexpr auto k_FalloffCurveOvershoot = 1.15f;

    /** The attenuation the mixer will actually apply, which is NOT simply the track's library asset: a SoundCue with
     *  `bOverrideAttenuation` wins over it, and the Setup processor writes a synthetic default when neither exists.
     *  Reading the resolved settings off the COMPONENT is the only way to get the same answer the engine has. */
    auto
        TryGet_ResolvedAttenuation(
            const UAudioComponent* InComponent)
        -> const FSoundAttenuationSettings*
    {
        if (ck::Is_NOT_Valid(InComponent))
        { return nullptr; }

        if (InComponent->bOverrideAttenuation)
        { return &InComponent->AttenuationOverrides; }

        if (ck::IsValid(InComponent->AttenuationSettings))
        { return &InComponent->AttenuationSettings->Attenuation; }

        return nullptr;
    }

    /** The audio device's own listener first, because it is the point the mixer measured from — including any
     *  attenuation-listener override a game has set. The player camera is a FALLBACK, not an equivalent: the two
     *  coincide in most projects and diverge in exactly the ones where a distance is worth debugging, so whichever
     *  answered is named on the page rather than left for the reader to assume. */
    auto
        Resolve_Listener(
            FCkAudioDebugger_Snapshot& OutSnapshot,
            UWorld* InWorld)
        -> void
    {
        if (ck::Is_NOT_Valid(InWorld))
        { return; }

        if (const auto AudioDevice = InWorld->GetAudioDevice();
            AudioDevice.IsValid())
        {
            auto ListenerTransform = FTransform{};

            if (AudioDevice->GetListenerTransform(0, ListenerTransform))
            {
                OutSnapshot.HasListener = true;
                OutSnapshot.ListenerLocation = ListenerTransform.GetLocation();
                OutSnapshot.ListenerForward = ListenerTransform.GetRotation().GetForwardVector();
                OutSnapshot.ListenerSource = FString{TEXT("audio device listener 0")};

                return;
            }
        }

        const auto* PlayerController = InWorld->GetFirstPlayerController();

        if (ck::Is_NOT_Valid(PlayerController) || ck::Is_NOT_Valid(PlayerController->PlayerCameraManager))
        { return; }

        OutSnapshot.HasListener = true;
        OutSnapshot.ListenerLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
        OutSnapshot.ListenerForward = PlayerController->PlayerCameraManager->GetCameraRotation().Vector();
        OutSnapshot.ListenerSource = FString{TEXT("PlayerCameraManager (audio device had no listener)")};
    }

    auto
        Build_SpatialInfo(
            FCkAudioDebugger_TrackInfo& OutInfo,
            const UAudioComponent* InComponent,
            const FCkAudioDebugger_Snapshot& InSnapshot)
        -> void
    {
        if (ck::Is_NOT_Valid(InComponent) || NOT InSnapshot.HasListener)
        { return; }

        const auto* Attenuation = TryGet_ResolvedAttenuation(InComponent);

        if (Attenuation == nullptr)
        { return; }

        OutInfo.HasSpatialData = true;
        OutInfo.IsAttenuated = Attenuation->bAttenuate;
        OutInfo.IsSpatialized = Attenuation->bSpatialize;
        OutInfo.DistanceModel = Attenuation->DistanceAlgorithm;
        OutInfo.AttenuationShape = Attenuation->AttenuationShape;
        OutInfo.FalloffDistance = Attenuation->FalloffDistance;
        OutInfo.MaxFalloffDistance = Attenuation->GetMaxFalloffDistance();

        // Sphere's extent X IS the inner radius — the distance the sound stays at full gain before falloff starts.
        // Other shapes have no single radius, so the ring is suppressed rather than guessed at.
        OutInfo.InnerRadius = Attenuation->AttenuationShape == EAttenuationShape::Sphere
            ? static_cast<float>(Attenuation->AttenuationShapeExtents.X)
            : 0.0f;

        OutInfo.AttenuationAssetName = ck::IsValid(InComponent->AttenuationSettings)
            ? InComponent->AttenuationSettings->GetName()
            : (InComponent->bOverrideAttenuation
                ? FString{TEXT("(inline override)")}
                : FString{});

        const auto ComponentTransform = InComponent->GetComponentTransform();

        OutInfo.WorldLocation = ComponentTransform.GetLocation();

        const auto ToTrack = OutInfo.WorldLocation - InSnapshot.ListenerLocation;

        OutInfo.DistanceToListener = static_cast<float>(ToTrack.Size());

        const auto FlatToTrack = FVector{ToTrack.X, ToTrack.Y, 0.0}.GetSafeNormal();
        const auto FlatForward = FVector{
            InSnapshot.ListenerForward.X, InSnapshot.ListenerForward.Y, 0.0}.GetSafeNormal();

        if (NOT FlatToTrack.IsNearlyZero() && NOT FlatForward.IsNearlyZero())
        {
            const auto FlatRight = FVector::CrossProduct(FVector::UpVector, FlatForward);

            OutInfo.BearingDegrees = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(
                FVector::DotProduct(FlatToTrack, FlatRight),
                FVector::DotProduct(FlatToTrack, FlatForward))));
        }

        // The engine's own evaluation at the REAL listener position. This is the number the mixer used.
        OutInfo.AttenuationGain = Attenuation->Evaluate(ComponentTransform, InSnapshot.ListenerLocation);

        if (const auto CurveRange = OutInfo.MaxFalloffDistance * k_FalloffCurveOvershoot;
            CurveRange > KINDA_SMALL_NUMBER)
        {
            // Swept by walking a SYNTHETIC listener along the real listener's bearing, so the plotted curve is the
            // same function, through the same shape maths, as the marked point — not a parallel approximation of it.
            const auto Direction = FlatToTrack.IsNearlyZero() ? FVector::ForwardVector : -FlatToTrack;

            OutInfo.FalloffCurve.Reserve(k_FalloffCurveSamples);

            for (auto Index = 0; Index < k_FalloffCurveSamples; ++Index)
            {
                const auto Alpha = static_cast<float>(Index) / static_cast<float>(k_FalloffCurveSamples - 1);
                const auto SampleAt = OutInfo.WorldLocation + Direction * (CurveRange * Alpha);

                OutInfo.FalloffCurve.Add(Attenuation->Evaluate(ComponentTransform, SampleAt));
            }
        }
    }

    auto
        Build_TrackInfo(
            FCk_Handle_AudioTrack InTrack,
            const FCkAudioDebugger_Snapshot& InSnapshot)
        -> FCkAudioDebugger_TrackInfo
    {
        auto Info = FCkAudioDebugger_TrackInfo{};

        Info.TrackEntity = InTrack;
        Info.TrackName = UCk_Utils_AudioTrack_UE::Get_TrackName(InTrack).ToString();

        Info.State = UCk_Utils_AudioTrack_UE::Get_State(InTrack);
        Info.CurrentVolume = UCk_Utils_AudioTrack_UE::Get_CurrentVolume(InTrack);
        Info.PlaybackPercent = UCk_Utils_AudioTrack_UE::Get_PlaybackPercent(InTrack);
        Info.IsVirtualized = UCk_Utils_AudioTrack_UE::Get_IsVirtualized(InTrack);
        Info.Priority = UCk_Utils_AudioTrack_UE::Get_Priority(InTrack);
        Info.OverrideBehavior = UCk_Utils_AudioTrack_UE::Get_OverrideBehavior(InTrack);

        // Target and fade rate have no Utils accessor — they are `CK_PROPERTY_GET` on the Current fragment, read
        // here directly. They are what makes the mixer readable: a current volume alone never says whether the track
        // is steady or halfway through a crossfade.
        if (InTrack.Has<ck::FFragment_AudioTrack_Current>())
        {
            const auto& Current = InTrack.Get<ck::FFragment_AudioTrack_Current>();

            Info.TargetVolume = Current.Get_TargetVolume();
            Info.FadeSpeed = Current.Get_FadeSpeed();

            // The component is pooled and weakly held, so it can already be released while the track entity lives on.
            // Recorded rather than assumed, so a row never prints a volume for something nothing is playing.
            Info.HasAudioComponent = Current.Get_AudioComponent().IsValid();

            Build_SpatialInfo(Info, Current.Get_AudioComponent().Get(), InSnapshot);
        }

        if (InTrack.Has<ck::FFragment_AudioTrack_Params>())
        {
            const auto& Params = InTrack.Get<ck::FFragment_AudioTrack_Params>();

            Info.LoopBehavior = Params.Get_LoopBehavior();

            // The soft path, NEVER resolved. A debugger that loaded a sound to name it would be pulling assets into
            // memory to describe a page about what is resident.
            Info.SoundPath = Params.Get_Sound().ToSoftObjectPath().ToString();
        }

        return Info;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAudioDebugger_DataCollector::
    Collect(
        UWorld* InWorld)
    -> void
{
    _Snapshot = FCkAudioDebugger_Snapshot{};

    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);

    if (ck::Is_NOT_Valid(TransientEntity))
    { return; }

    _Snapshot.HasWorld = true;

    // MUST precede the director walk: every track's spatial block is measured from this listener, and a track built
    // before it was resolved would silently report distance-from-origin.
    ck_audio_debugger_collector::Resolve_Listener(_Snapshot, InWorld);

    TransientEntity.View<ck::FFragment_AudioDirector_Current>().ForEach(
        [this, &TransientEntity](FCk_Entity InEntity, const ck::FFragment_AudioDirector_Current&)
        {
            auto Handle = ck::MakeHandle(InEntity, TransientEntity);

            if (ck::Is_NOT_Valid(Handle))
            { return; }

            const auto Director = UCk_Utils_AudioDirector_UE::Cast(Handle);

            if (ck::Is_NOT_Valid(Director))
            { return; }

            auto Info = FCkAudioDebugger_DirectorInfo{};
            Info.DirectorEntity = Handle;
            Info.DirectorName = UCk_Utils_Handle_UE::Get_DebugName(Handle).ToString();

            if (Handle.Has<ck::FFragment_AudioDirector_Params>())
            {
                const auto& Params = Handle.Get<ck::FFragment_AudioDirector_Params>();

                Info.MaxConcurrentTracks   = Params.Get_MaxConcurrentTracks();
                Info.SamePriorityBehavior  = Params.Get_SamePriorityBehavior();

                if (const auto& Crossfade = Params.Get_DefaultCrossfadeDuration();
                    Crossfade.IsSet())
                { Info.DefaultCrossfadeSeconds = Crossfade.GetValue().Get_Seconds(); }
            }

            // The record utils are a typedef over the generic template, not a name the macro exports — CkAudio's
            // own AudioTrack utils spell it the same way.
            using RecordOfAudioTracks_Utils = ck::TUtils_RecordOfEntities<ck::FFragment_RecordOfAudioTracks>;

            RecordOfAudioTracks_Utils::ForEach_ValidEntry(Handle,
                [this, &Info](FCk_Handle_AudioTrack InTrack)
                {
                    Info.Tracks.Add(ck_audio_debugger_collector::Build_TrackInfo(InTrack, _Snapshot));
                });

            // Sorted by priority (highest first) then name, because a director's record order is whatever order the
            // tracks were added in — an implementation detail that would make two identical sessions print the mixer
            // in different orders, and would move a row under the reader's cursor between refreshes.
            Info.Tracks.Sort([](const FCkAudioDebugger_TrackInfo& InLhs, const FCkAudioDebugger_TrackInfo& InRhs)
            {
                if (InLhs.Priority != InRhs.Priority)
                { return InLhs.Priority > InRhs.Priority; }

                return InLhs.TrackName.Compare(InRhs.TrackName, ESearchCase::IgnoreCase) < 0;
            });

            _Snapshot.Directors.Add(MoveTemp(Info));
        });

    // Same reasoning one level up: view iteration order follows EnTT's pool layout.
    _Snapshot.Directors.Sort([](const FCkAudioDebugger_DirectorInfo& InLhs,
                                const FCkAudioDebugger_DirectorInfo& InRhs)
    {
        return InLhs.DirectorName.Compare(InRhs.DirectorName, ESearchCase::IgnoreCase) < 0;
    });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAudioDebugger_DataCollector::
    Reset()
    -> void
{
    _Snapshot = FCkAudioDebugger_Snapshot{};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAudioDebugger_DataCollector::
    Get_Snapshot() const
    -> const FCkAudioDebugger_Snapshot&
{
    return _Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
