#include "CkInspector_Audio.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkAudio/AudioTrack/CkAudioTrack_Fragment.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"
#include "CkAudio/AudioDirector/CkAudioDirector_Fragment.h"
#include "CkAudio/AudioDirector/CkAudioDirector_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

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
    Builder.SetEditGuard(Get_EditGuard());

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

        // ---- Track verbs ----
        // The inspected entity IS the track here (FFragment_AudioTrack_Current lives on it), so the
        // controls address it directly. LocalOk: audio playback is driven wherever the audio device
        // is; there is no authority gate on these requests.
        auto MutableTrackEntity = Entity;
        const auto CapturedTrack = UCk_Utils_AudioTrack_UE::Cast(MutableTrackEntity);

        if (ck::IsValid(CapturedTrack))
        {
            // Fade time 0 — the debugger wants the state change NOW, not a designer's ramp.
            Builder.AddActionRow(
                FText::FromString(TEXT("Playback:")),
                {
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Play")),
                        FText::FromString(TEXT("UCk_Utils_AudioTrack_UE::Request_Play (no fade)")),
                        [CapturedTrack]()
                        {
                            auto MutableTrack = CapturedTrack;
                            if (ck::Is_NOT_Valid(MutableTrack)) { return; }
                            UCk_Utils_AudioTrack_UE::Request_Play(MutableTrack, FCk_Time::ZeroSecond(), {});
                        },
                        ECk_DebugRequest_Requirement::LocalOk
                    },
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Stop")),
                        FText::FromString(TEXT("UCk_Utils_AudioTrack_UE::Request_Stop (no fade)")),
                        [CapturedTrack]()
                        {
                            auto MutableTrack = CapturedTrack;
                            if (ck::Is_NOT_Valid(MutableTrack)) { return; }
                            UCk_Utils_AudioTrack_UE::Request_Stop(MutableTrack, FCk_Time::ZeroSecond(), {});
                        },
                        ECk_DebugRequest_Requirement::LocalOk
                    },
                });

            // Reads CURRENT volume, writes TARGET volume: with a zero fade the two converge on the
            // next audio tick, and the meter row above shows the pair while they differ.
            Builder.AddNumericRow(
                FText::FromString(TEXT("Set Volume:")),
                TAttribute<float>::CreateLambda([CapturedTrack]()
                {
                    if (ck::Is_NOT_Valid(CapturedTrack)) { return 0.0f; }
                    return UCk_Utils_AudioTrack_UE::Get_CurrentVolume(CapturedTrack);
                }),
                [CapturedTrack](float InVolume)
                {
                    auto MutableTrack = CapturedTrack;
                    if (ck::Is_NOT_Valid(MutableTrack)) { return; }
                    UCk_Utils_AudioTrack_UE::Request_SetVolume(MutableTrack, InVolume, FCk_Time::ZeroSecond(), {});
                },
                0.0f,
                TOptional<float>{},
                ECk_DebugRequest_Requirement::LocalOk);

            // CosmeticOnly — a dedicated server has no viewport to draw the track's debug text on.
            // The Utils expose two verbs rather than one toggle; Get_IsDebugDrawEnabled is the
            // read-back that makes them one switch.
            Builder.AddToggleRow(
                FText::FromString(TEXT("Debug Draw:")),
                TAttribute<bool>::CreateLambda([CapturedTrack]()
                {
                    if (ck::Is_NOT_Valid(CapturedTrack)) { return false; }
                    return UCk_Utils_AudioTrack_UE::Get_IsDebugDrawEnabled(CapturedTrack);
                }),
                [CapturedTrack](bool InIsEnabled)
                {
                    auto MutableTrack = CapturedTrack;
                    if (ck::Is_NOT_Valid(MutableTrack)) { return; }

                    if (InIsEnabled)
                    { UCk_Utils_AudioTrack_UE::Request_EnableDebugDraw(MutableTrack); }
                    else
                    { UCk_Utils_AudioTrack_UE::Request_DisableDebugDraw(MutableTrack); }
                },
                ECk_DebugRequest_Requirement::CosmeticOnly);
        }
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

        auto MutableDirectorEntity = Entity;
        const auto CapturedDirector = UCk_Utils_AudioDirector_UE::Cast(MutableDirectorEntity);

        if (ck::IsValid(CapturedDirector))
        {
            Builder.AddActionRow(
                FText::FromString(TEXT("All Tracks:")),
                {
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Stop All")),
                        FText::FromString(TEXT("UCk_Utils_AudioDirector_UE::Request_StopAllTracks (no fade)")),
                        [CapturedDirector]()
                        {
                            auto MutableDirector = CapturedDirector;
                            if (ck::Is_NOT_Valid(MutableDirector)) { return; }
                            UCk_Utils_AudioDirector_UE::Request_StopAllTracks(MutableDirector, FCk_Time::ZeroSecond(), {});
                        },
                        ECk_DebugRequest_Requirement::LocalOk
                    },
                });
        }

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

                // Per-track Stop rides the SAME map iteration as the row above — the director
                // addresses its tracks by FName, so no handle plumbing is needed.
                if (ck::IsValid(CapturedDirector))
                {
                    const auto TrackName = Name;

                    Builder.AddActionRow(
                        FText::FromString(NameStr),
                        {
                            FCkInspector_Action
                            {
                                FText::FromString(TEXT("Stop")),
                                FText::FromString(ck::Format_UE(
                                    TEXT("UCk_Utils_AudioDirector_UE::Request_StopTrack({}) (no fade)"), NameStr)),
                                [CapturedDirector, TrackName]()
                                {
                                    auto MutableDirector = CapturedDirector;
                                    if (ck::Is_NOT_Valid(MutableDirector)) { return; }

                                    UCk_Utils_AudioDirector_UE::Request_StopTrack(MutableDirector, TrackName,
                                        FCk_Time::ZeroSecond(), {});
                                },
                                ECk_DebugRequest_Requirement::LocalOk
                            },
                        });
                }
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
