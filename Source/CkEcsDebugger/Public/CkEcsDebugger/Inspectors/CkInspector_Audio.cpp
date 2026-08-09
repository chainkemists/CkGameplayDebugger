#include "CkInspector_Audio.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkAudio/AudioTrack/CkAudioTrack_Fragment.h"
#include "CkAudio/AudioDirector/CkAudioDirector_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Audio)

// =====================================================================================================================

namespace ck_inspector_audio
{
    // Audible = Ok, in-transit = Info, held = Warn, silent = Neutral.
    static auto Get_TrackStateTone(ECk_AudioTrack_State InState) -> ECk_Tone
    {
        switch (InState)
        {
            case ECk_AudioTrack_State::Playing:    return ECk_Tone::Ok;
            case ECk_AudioTrack_State::FadingIn:   return ECk_Tone::Info;
            case ECk_AudioTrack_State::FadingOut:  return ECk_Tone::Info;
            case ECk_AudioTrack_State::Paused:     return ECk_Tone::Warn;
            case ECk_AudioTrack_State::Stopped:    return ECk_Tone::Neutral;
            default:                               return ECk_Tone::Neutral;
        }
    }
}

// =====================================================================================================================

auto FCkInspector_Audio::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Audio"));
}

auto FCkInspector_Audio::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<
        ck::FFragment_AudioTrack_Current,
        ck::FFragment_AudioDirector_Current>();
}

// =====================================================================================================================

auto FCkInspector_Audio::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- Audio Track ----
    if (Entity.Has<ck::FFragment_AudioTrack_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Track")));

        const auto CapturedEntity = Entity;

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("State:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto State = CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_State();
                return FText::FromString(ck::Format_UE(TEXT("{}"), State));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return ECk_Tone::Neutral; }
                return ck_inspector_audio::Get_TrackStateTone(
                    CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_State());
            }));

        Builder.AddMeterRow(
            FText::FromString(TEXT("Volume (cur → target):")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return 0.0f; }
                return CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_CurrentVolume();
            }),
            ECk_Tone::Accent,
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto& Current = CapturedEntity.Get<ck::FFragment_AudioTrack_Current>();
                return FText::FromString(ck::Format_UE(TEXT("{:.3f} → {:.3f}"),
                    Current.Get_CurrentVolume(), Current.Get_TargetVolume()));
            }));

        Builder.AddRow(
            FText::FromString(TEXT("Fade Speed:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Speed = CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_FadeSpeed();
                return FText::FromString(FString::Printf(TEXT("%.3f"), Speed));
            },
            CkStyle::Value_Numeric());

        // Get_PlaybackPercent is a 0..1 fraction despite the name (the CkAudio debug draw scales it
        // by 100 to display) — the previous row printed the raw fraction with a % sign.
        Builder.AddMeterRow(
            FText::FromString(TEXT("Playback:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return 0.0f; }
                return CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_PlaybackPercent();
            }),
            ECk_Tone::Info,
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Pct = CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_PlaybackPercent();
                return FText::FromString(ck::Format_UE(TEXT("{:.1f}%"), Pct * 100.0f));
            }));

        Builder.AddConditionalRow(
            FText::FromString(TEXT("Virtualized:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto IsVirt = CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_IsVirtualized();
                return FText::FromString(IsVirt ? TEXT("Yes") : TEXT("No"));
            },
            [CapturedEntity](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return CkStyle::None(); }
                const auto IsVirt = CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_IsVirtualized();
                return IsVirt ? CkStyle::Warn() : CkStyle::Value_Bool_False();
            });
    }

    // ---- Audio Director ----
    if (Entity.Has<ck::FFragment_AudioDirector_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Director")));

        const auto CapturedEntity = Entity;

        Builder.AddCountBadgeRow(
            FText::FromString(TEXT("Active Tracks:")),
            TAttribute<int32>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioDirector_Current>())
                { return 0; }
                return CapturedEntity.Get<ck::FFragment_AudioDirector_Current>().Get_ActiveTracks().Num();
            }),
            ECk_Tone::Info);

        Builder.AddRow(
            FText::FromString(TEXT("Highest Priority:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioDirector_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Priority = CapturedEntity.Get<ck::FFragment_AudioDirector_Current>().Get_CurrentHighestPriority();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Priority));
            },
            CkStyle::Value_Numeric());

        Builder.AddConditionalRow(
            FText::FromString(TEXT("All Tracks Finished:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioDirector_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Fired = CapturedEntity.Get<ck::FFragment_AudioDirector_Current>().Get_HasFiredAllTracksFinished();
                return FText::FromString(Fired ? TEXT("Yes") : TEXT("No"));
            },
            [CapturedEntity](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioDirector_Current>())
                { return CkStyle::None(); }
                const auto Fired = CapturedEntity.Get<ck::FFragment_AudioDirector_Current>().Get_HasFiredAllTracksFinished();
                return Fired ? CkStyle::Value_Bool_True() : CkStyle::Value_Bool_False();
            });

        // Track name list
        const auto& TracksByName = Entity.Get<ck::FFragment_AudioDirector_Current>().Get_TracksByName();
        if (NOT TracksByName.IsEmpty())
        {
            Builder.AddHeader(FText::FromString(ck::Format_UE(TEXT("Tracks ({})"), TracksByName.Num())));

            for (const auto& [Name, Handle] : TracksByName)
            {
                const auto NameStr = Name.ToString();
                Builder.AddRow(
                    FText::FromString(NameStr),
                    [Handle](const FCk_Handle&)
                    {
                        return FText::FromString(ck::IsValid(Handle)
                            ? ck::Format_UE(TEXT("[{}]"), Handle)
                            : FString(TEXT("(Invalid)")));
                    },
                    CkStyle::Value_Handle());
            }
        }
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Audio::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
