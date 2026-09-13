#include "CkInspector_Audio.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "CkAudio/AudioTrack/CkAudioTrack_Fragment.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"
#include "CkAudio/AudioDirector/CkAudioDirector_Fragment.h"
#include "CkAudio/AudioDirector/CkAudioDirector_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
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

    auto IsDestroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity) || InEntity.Has_Any<
            ck::FTag_DestroyEntity_Initiate,
            ck::FTag_DestroyEntity_EndPlay,
            ck::FTag_DestroyEntity_Teardown,
            ck::FTag_DestroyEntity_Await,
            ck::FTag_DestroyEntity_Finalize>();
    }

    auto TryGetTrack(const FCk_Handle& InEntity, FCk_Handle_AudioTrack& OutTrack) -> bool
    {
        OutTrack = {};
        if (IsDestroying(InEntity)
            || NOT InEntity.Has_All<ck::FFragment_AudioTrack_Params, ck::FFragment_AudioTrack_Current>())
        { return false; }

        auto Mutable = InEntity;
        OutTrack = UCk_Utils_AudioTrack_UE::Cast(Mutable);
        return ck::IsValid(OutTrack);
    }

    auto HasTrackCurrent(const FCk_Handle& InEntity) -> bool
    {
        return NOT IsDestroying(InEntity)
            && InEntity.Has<ck::FFragment_AudioTrack_Current>();
    }

    auto TryGetDirector(const FCk_Handle& InEntity, FCk_Handle_AudioDirector& OutDirector) -> bool
    {
        OutDirector = {};
        if (IsDestroying(InEntity)
            || NOT InEntity.Has_All<ck::FFragment_AudioDirector_Params, ck::FFragment_AudioDirector_Current>())
        { return false; }

        auto Mutable = InEntity;
        OutDirector = UCk_Utils_AudioDirector_UE::Cast(Mutable);
        return ck::IsValid(OutDirector);
    }

    auto HasDirectorCurrent(const FCk_Handle& InEntity) -> bool
    {
        return NOT IsDestroying(InEntity)
            && InEntity.Has<ck::FFragment_AudioDirector_Current>();
    }

    auto GateReason(const FCk_Handle& InEntity, const ECk_DebugRequest_Requirement InRequirement) -> FString
    {
        return ck::DebugRequestGate::Evaluate(InEntity, InRequirement).Reason.ToString();
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

auto FCkInspector_Audio::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    if (ck_inspector_audio::IsDestroying(Entity))
    { return Builder.Build(Entity); }

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
        auto CapturedTrack = FCk_Handle_AudioTrack{};

        if (ck_inspector_audio::TryGetTrack(Entity, CapturedTrack))
        {
            // Fade time 0 — the debugger wants the state change NOW, not a designer's ramp.
            Builder.AddActionRow(
                FText::FromString(TEXT("Playback:")),
                {
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Play")),
                        FText::FromString(TEXT("UCk_Utils_AudioTrack_UE::Request_Play (no fade)")),
                        [CapturedEntity]()
                        {
                            auto Track = FCk_Handle_AudioTrack{};
                            if (NOT ck_inspector_audio::TryGetTrack(CapturedEntity, Track))
                            { return; }

                            UCk_Utils_AudioTrack_UE::Request_Play(Track, FCk_Time::ZeroSecond(), {});
                        },
                        ECk_DebugRequest_Requirement::LocalOk
                    },
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Stop")),
                        FText::FromString(TEXT("UCk_Utils_AudioTrack_UE::Request_Stop (no fade)")),
                        [CapturedEntity]()
                        {
                            auto Track = FCk_Handle_AudioTrack{};
                            if (NOT ck_inspector_audio::TryGetTrack(CapturedEntity, Track))
                            { return; }

                            UCk_Utils_AudioTrack_UE::Request_Stop(Track, FCk_Time::ZeroSecond(), {});
                        },
                        ECk_DebugRequest_Requirement::LocalOk
                    },
                });

            // Reads CURRENT volume, writes TARGET volume: with a zero fade the two converge on the
            // next audio tick, and the meter row above shows the pair while they differ.
            Builder.AddNumericRow(
                FText::FromString(TEXT("Set Volume:")),
                TAttribute<float>::CreateLambda([CapturedEntity]()
                {
                    auto Track = FCk_Handle_AudioTrack{};
                    return ck_inspector_audio::TryGetTrack(CapturedEntity, Track)
                        ? UCk_Utils_AudioTrack_UE::Get_CurrentVolume(Track)
                        : 0.0f;
                }),
                [CapturedEntity](float InVolume)
                {
                    auto Track = FCk_Handle_AudioTrack{};
                    if (NOT ck_inspector_audio::TryGetTrack(CapturedEntity, Track))
                    { return; }

                    UCk_Utils_AudioTrack_UE::Request_SetVolume(Track, InVolume, FCk_Time::ZeroSecond(), {});
                },
                0.0f,
                TOptional<float>{},
                ECk_DebugRequest_Requirement::LocalOk);

            // CosmeticOnly — a dedicated server has no viewport to draw the track's debug text on.
            // The Utils expose two verbs rather than one toggle; Get_IsDebugDrawEnabled is the
            // read-back that makes them one switch.
            Builder.AddToggleRow(
                FText::FromString(TEXT("Debug Draw:")),
                TAttribute<bool>::CreateLambda([CapturedEntity]()
                {
                    auto Track = FCk_Handle_AudioTrack{};
                    return ck_inspector_audio::TryGetTrack(CapturedEntity, Track)
                        && UCk_Utils_AudioTrack_UE::Get_IsDebugDrawEnabled(Track);
                }),
                [CapturedEntity](bool InIsEnabled)
                {
                    auto Track = FCk_Handle_AudioTrack{};
                    if (NOT ck_inspector_audio::TryGetTrack(CapturedEntity, Track))
                    { return; }

                    if (InIsEnabled)
                    { UCk_Utils_AudioTrack_UE::Request_EnableDebugDraw(Track); }
                    else
                    { UCk_Utils_AudioTrack_UE::Request_DisableDebugDraw(Track); }
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

        auto CapturedDirector = FCk_Handle_AudioDirector{};

        if (ck_inspector_audio::TryGetDirector(Entity, CapturedDirector))
        {
            Builder.AddActionRow(
                FText::FromString(TEXT("All Tracks:")),
                {
                    FCkInspector_Action
                    {
                        FText::FromString(TEXT("Stop All")),
                        FText::FromString(TEXT("UCk_Utils_AudioDirector_UE::Request_StopAllTracks (no fade)")),
                        [CapturedEntity]()
                        {
                            auto Director = FCk_Handle_AudioDirector{};
                            if (NOT ck_inspector_audio::TryGetDirector(CapturedEntity, Director))
                            { return; }

                            UCk_Utils_AudioDirector_UE::Request_StopAllTracks(
                                Director,
                                FCk_Time::ZeroSecond(),
                                {});
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
                        FText::FromString(ck::Format_UE(TEXT("{} Actions"), NameStr)),
                        {
                            FCkInspector_Action
                            {
                                FText::FromString(TEXT("Stop")),
                                FText::FromString(ck::Format_UE(
                                    TEXT("UCk_Utils_AudioDirector_UE::Request_StopTrack({}) (no fade)"), NameStr)),
                                [CapturedEntity, TrackName]()
                                {
                                    auto Director = FCk_Handle_AudioDirector{};
                                    if (NOT ck_inspector_audio::TryGetDirector(CapturedEntity, Director)
                                        || NOT Director.Get<ck::FFragment_AudioDirector_Current>()
                                            .Get_TracksByName().Contains(TrackName))
                                    { return; }

                                    UCk_Utils_AudioDirector_UE::Request_StopTrack(Director, TrackName,
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
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_AudioAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

// =====================================================================================================================

auto SCkInspector_AudioAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Root = SNew(SBox);
    ChildSlot[Root];
    if (Refresh_Tracks() && Build_AuthoredView())
    { Root->SetContent(_View->GetRegion(TEXT("main"))); return; }
    Release();
    Root->SetContent(SNullWidget::NullWidget);
}

SCkInspector_AudioAuthored::~SCkInspector_AudioAuthored()
{
    Release();
}

auto SCkInspector_AudioAuthored::Get_IsTrackAvailable() const -> bool
{
    return _Active && ck_inspector_audio::HasTrackCurrent(_Entity);
}

auto SCkInspector_AudioAuthored::Get_IsDirectorAvailable() const -> bool
{
    return _Active && ck_inspector_audio::HasDirectorCurrent(_Entity);
}

auto SCkInspector_AudioAuthored::Get_TrackStateText() const -> FString
{
    return Get_IsTrackAvailable()
        ? ck::Format_UE(TEXT("{}"), _Entity.Get<ck::FFragment_AudioTrack_Current>().Get_State())
        : TEXT("--");
}

auto SCkInspector_AudioAuthored::Get_TrackVolumeText() const -> FString
{
    if (NOT Get_IsTrackAvailable())
    { return TEXT("--"); }

    const auto& Current = _Entity.Get<ck::FFragment_AudioTrack_Current>();
    return ck::Format_UE(
        TEXT("{:.3f} → {:.3f}"),
        Current.Get_CurrentVolume(),
        Current.Get_TargetVolume());
}

auto SCkInspector_AudioAuthored::Get_TrackFadeSpeedText() const -> FString
{
    return Get_IsTrackAvailable()
        ? FString::Printf(TEXT("%.3f"), _Entity.Get<ck::FFragment_AudioTrack_Current>().Get_FadeSpeed())
        : TEXT("--");
}

auto SCkInspector_AudioAuthored::Get_TrackPlaybackText() const -> FString
{
    return Get_IsTrackAvailable()
        ? ck::Format_UE(
            TEXT("{:.1f}%"),
            _Entity.Get<ck::FFragment_AudioTrack_Current>().Get_PlaybackPercent() * 100.0f)
        : TEXT("--");
}

auto SCkInspector_AudioAuthored::Get_TrackVirtualizedText() const -> FString
{
    return Get_IsTrackAvailable()
        ? (_Entity.Get<ck::FFragment_AudioTrack_Current>().Get_IsVirtualized() ? TEXT("Yes") : TEXT("No"))
        : TEXT("--");
}

auto SCkInspector_AudioAuthored::Get_DirectorActiveTracksText() const -> FString
{
    return Get_IsDirectorAvailable()
        ? FString::FromInt(_Entity.Get<ck::FFragment_AudioDirector_Current>().Get_ActiveTracks().Num())
        : TEXT("--");
}

auto SCkInspector_AudioAuthored::Get_DirectorPriorityText() const -> FString
{
    return Get_IsDirectorAvailable()
        ? FString::FromInt(_Entity.Get<ck::FFragment_AudioDirector_Current>().Get_CurrentHighestPriority())
        : TEXT("--");
}

auto SCkInspector_AudioAuthored::Get_DirectorAllFinishedText() const -> FString
{
    return Get_IsDirectorAvailable()
        ? (_Entity.Get<ck::FFragment_AudioDirector_Current>().Get_HasFiredAllTracksFinished()
            ? TEXT("Yes")
            : TEXT("No"))
        : TEXT("--");
}

auto SCkInspector_AudioAuthored::Get_CanRequestTrack() const -> bool
{
    auto Track = FCk_Handle_AudioTrack{};
    return _Active
        && ck_inspector_audio::TryGetTrack(_Entity, Track)
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_AudioAuthored::Get_CanRequestDirector() const -> bool
{
    auto Director = FCk_Handle_AudioDirector{};
    return _Active
        && ck_inspector_audio::TryGetDirector(_Entity, Director)
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_AudioAuthored::Get_CanDebugDraw() const -> bool
{
    auto Track = FCk_Handle_AudioTrack{};
    return _Active
        && ck_inspector_audio::TryGetTrack(_Entity, Track)
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled;
}

auto SCkInspector_AudioAuthored::Get_RequestDisabledReasonTrack() const -> FString
{
    auto Track = FCk_Handle_AudioTrack{};
    return ck_inspector_audio::TryGetTrack(_Entity, Track)
        ? ck_inspector_audio::GateReason(_Entity, ECk_DebugRequest_Requirement::LocalOk)
        : TEXT("Audio Track is unavailable.");
}

auto SCkInspector_AudioAuthored::Get_RequestDisabledReasonDirector() const -> FString
{
    auto Director = FCk_Handle_AudioDirector{};
    return ck_inspector_audio::TryGetDirector(_Entity, Director)
        ? ck_inspector_audio::GateReason(_Entity, ECk_DebugRequest_Requirement::LocalOk)
        : TEXT("Audio Director is unavailable.");
}

auto SCkInspector_AudioAuthored::Get_DebugDrawDisabledReason() const -> FString
{
    auto Track = FCk_Handle_AudioTrack{};
    return ck_inspector_audio::TryGetTrack(_Entity, Track)
        ? ck_inspector_audio::GateReason(_Entity, ECk_DebugRequest_Requirement::CosmeticOnly)
        : TEXT("Audio Track is unavailable.");
}

auto SCkInspector_AudioAuthored::Get_IsDebugDrawEnabled() const -> bool
{
    auto Track = FCk_Handle_AudioTrack{};
    return ck_inspector_audio::TryGetTrack(_Entity, Track)
        && UCk_Utils_AudioTrack_UE::Get_IsDebugDrawEnabled(Track);
}

auto SCkInspector_AudioAuthored::Get_DiffColor(const FString& InLabel) const -> FLinearColor
{
    return _DiffLabels.Contains(InLabel) ? CkStyle::Accent() : CkStyle::Text();
}

auto SCkInspector_AudioAuthored::Refresh_Tracks() -> bool
{
    if (NOT _Tracks.IsValid())
    {
        const FCkUiLoadResult Create = FCkUiCollection::TryCreate({
            FCkUiFieldSchema{TEXT("name"), ECkUiFieldKind::Text},
            FCkUiFieldSchema{TEXT("handle"), ECkUiFieldKind::Text},
            FCkUiFieldSchema{TEXT("stop-label"), ECkUiFieldKind::Text},
            FCkUiFieldSchema{TEXT("stop-tooltip"), ECkUiFieldKind::Text},
            FCkUiFieldSchema{TEXT("name-color"), ECkUiFieldKind::Color},
        }, _Tracks);
        if (NOT Create.Succeeded || NOT _Tracks.IsValid())
        {
            _LoadError = FString::Join(Create.Errors, TEXT("\n"));
            return false;
        }
    }
    auto Records = TArray<FCkUiRecordData>{};
    if (Get_IsDirectorAvailable())
    {
        const auto& Tracks = _Entity.Get<ck::FFragment_AudioDirector_Current>().Get_TracksByName();
        Records.Reserve(Tracks.Num());
        for (const auto& [Name, Handle] : Tracks)
        {
            const FString Key = Name.ToString();
            auto Record = FCkUiRecordData{};
            Record.Key = Key;
            Record.Fields.Add(TEXT("name"), FCkUiFieldValue{
                .Kind = ECkUiFieldKind::Text,
                .Text = FText::FromString(Key)});
            Record.Fields.Add(TEXT("handle"), FCkUiFieldValue{
                .Kind = ECkUiFieldKind::Text,
                .Text = FText::FromString(ck::IsValid(Handle)
                    ? ck::Format_UE(TEXT("[{}]"), Handle)
                    : FString{TEXT("(Invalid)")})});
            Record.Fields.Add(TEXT("stop-label"), FCkUiFieldValue{
                .Kind = ECkUiFieldKind::Text,
                .Text = FText::FromString(TEXT("Stop"))});
            Record.Fields.Add(TEXT("stop-tooltip"), FCkUiFieldValue{
                .Kind = ECkUiFieldKind::Text,
                .Text = FText::FromString(ck::Format_UE(
                    TEXT("UCk_Utils_AudioDirector_UE::Request_StopTrack({}) (no fade)"),
                    Key))});
            Record.Fields.Add(TEXT("name-color"), FCkUiFieldValue{
                .Kind = ECkUiFieldKind::Color,
                .Color = Get_DiffColor(Key)});
            Records.Add(MoveTemp(Record));
        }
    }
    const FCkUiLoadResult Set = _Tracks->TrySetRecords(MoveTemp(Records));
    if (NOT Set.Succeeded)
    {
        _LoadError = FString::Join(Set.Errors, TEXT("\n"));
        return false;
    }
    return true;
}

auto SCkInspector_AudioAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid())
    {
        _LoadError = FString::Join(RegistryResult.Errors, TEXT("\n"));
        return false;
    }
    if (NOT Plugin.IsValid())
    {
        _LoadError = TEXT("CkDebugger plugin could not be resolved for Audio inspector authored resources.");
        return false;
    }

    const TWeakPtr<SCkInspector_AudioAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};

    const auto BindText = [&Data, Weak](
        const FString& InKey,
        FString (SCkInspector_AudioAuthored::* InGetter)() const)
    {
        Data.Text.Add(InKey, TAttribute<FText>::CreateLambda([Weak, InGetter]()
        {
            const auto Widget = Weak.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString((Widget.Get()->*InGetter)())
                : FText::GetEmpty();
        }));
    };
    BindText(TEXT("audio-track-state"), &SCkInspector_AudioAuthored::Get_TrackStateText);
    BindText(TEXT("audio-track-volume"), &SCkInspector_AudioAuthored::Get_TrackVolumeText);
    BindText(TEXT("audio-track-fade-speed"), &SCkInspector_AudioAuthored::Get_TrackFadeSpeedText);
    BindText(TEXT("audio-track-playback"), &SCkInspector_AudioAuthored::Get_TrackPlaybackText);
    BindText(TEXT("audio-track-virtualized"), &SCkInspector_AudioAuthored::Get_TrackVirtualizedText);
    BindText(TEXT("audio-director-active"), &SCkInspector_AudioAuthored::Get_DirectorActiveTracksText);
    BindText(TEXT("audio-director-priority"), &SCkInspector_AudioAuthored::Get_DirectorPriorityText);
    BindText(TEXT("audio-director-finished"), &SCkInspector_AudioAuthored::Get_DirectorAllFinishedText);

    const auto BindDiff = [&Data, Weak](const FString& Key, const FString& Label)
    {
        Data.Color.Add(Key, TAttribute<FLinearColor>::CreateLambda([Weak, Label]()
        {
            const auto Widget = Weak.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? Widget->Get_DiffColor(Label)
                : FLinearColor::Transparent;
        }));
    };
    BindDiff(TEXT("audio-track-state-diff"), TEXT("State:"));
    BindDiff(TEXT("audio-track-volume-diff"), TEXT("Volume (cur → target):"));
    BindDiff(TEXT("audio-track-fade-diff"), TEXT("Fade Speed:"));
    BindDiff(TEXT("audio-track-playback-diff"), TEXT("Playback:"));
    BindDiff(TEXT("audio-track-virtualized-diff"), TEXT("Virtualized:"));
    BindDiff(TEXT("audio-director-active-diff"), TEXT("Active Tracks:"));
    BindDiff(TEXT("audio-director-priority-diff"), TEXT("Highest Priority:"));
    BindDiff(TEXT("audio-director-finished-diff"), TEXT("All Tracks Finished:"));

    const auto BindTone = [&Data, Weak](
        const FString& InForeground,
        const FString& InBackground,
        TFunction<ECk_Tone(const SCkInspector_AudioAuthored&)> InGetTone)
    {
        Data.Color.Add(InForeground, TAttribute<FLinearColor>::CreateLambda([Weak, InGetTone]()
        {
            const auto Widget = Weak.Pin();
            return Widget.IsValid()
                ? CkStyle::GetToneColor(InGetTone(*Widget))
                : CkStyle::GetToneColor(ECk_Tone::Neutral);
        }));
        Data.Color.Add(InBackground, TAttribute<FLinearColor>::CreateLambda([Weak, InGetTone]()
        {
            const auto Widget = Weak.Pin();
            return Widget.IsValid()
                ? CkStyle::GetToneDimColor(InGetTone(*Widget))
                : CkStyle::GetToneDimColor(ECk_Tone::Neutral);
        }));
    };
    BindTone(
        TEXT("audio-track-state-foreground"),
        TEXT("audio-track-state-background"),
        [](const SCkInspector_AudioAuthored& InWidget)
        {
            return InWidget.Get_IsTrackAvailable()
                ? ck_inspector_audio::Get_TrackStateTone(
                    InWidget._Entity.Get<ck::FFragment_AudioTrack_Current>().Get_State())
                : ECk_Tone::Neutral;
        });
    BindTone(
        TEXT("audio-track-virtualized-foreground"),
        TEXT("audio-track-virtualized-background"),
        [](const SCkInspector_AudioAuthored& InWidget)
        {
            return InWidget.Get_IsTrackAvailable()
                && InWidget._Entity.Get<ck::FFragment_AudioTrack_Current>().Get_IsVirtualized()
                    ? ECk_Tone::Warn
                    : ECk_Tone::Neutral;
        });
    BindTone(
        TEXT("audio-director-active-foreground"),
        TEXT("audio-director-active-background"),
        [](const SCkInspector_AudioAuthored&) { return ECk_Tone::Info; });
    BindTone(
        TEXT("audio-director-finished-foreground"),
        TEXT("audio-director-finished-background"),
        [](const SCkInspector_AudioAuthored& InWidget)
        {
            return InWidget.Get_IsDirectorAvailable()
                && InWidget._Entity.Get<ck::FFragment_AudioDirector_Current>().Get_HasFiredAllTracksFinished()
                    ? ECk_Tone::Ok
                    : ECk_Tone::Neutral;
        });

    Data.Text.Add(TEXT("audio-track-play-label"), FText::FromString(TEXT("Play")));
    Data.Text.Add(TEXT("audio-track-stop-label"), FText::FromString(TEXT("Stop")));
    Data.Text.Add(TEXT("audio-director-stop-all-label"), FText::FromString(TEXT("Stop All")));
    Data.Text.Add(TEXT("audio-track-play-tooltip"), FText::FromString(
        TEXT("UCk_Utils_AudioTrack_UE::Request_Play (no fade)")));
    Data.Text.Add(TEXT("audio-track-stop-tooltip"), FText::FromString(
        TEXT("UCk_Utils_AudioTrack_UE::Request_Stop (no fade)")));
    Data.Text.Add(TEXT("audio-director-stop-all-tooltip"), FText::FromString(
        TEXT("UCk_Utils_AudioDirector_UE::Request_StopAllTracks (no fade)")));
    Data.Text.Add(TEXT("audio-track-disabled"), TAttribute<FText>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? FText::FromString(Widget->Get_RequestDisabledReasonTrack())
            : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("audio-director-disabled"), TAttribute<FText>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? FText::FromString(Widget->Get_RequestDisabledReasonDirector())
            : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("audio-debug-draw-disabled"), TAttribute<FText>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert()
            ? FText::FromString(Widget->Get_DebugDrawDisabledReason())
            : FText::GetEmpty();
    }));

    Data.Visibility.Add(TEXT("audio-track-available"), TAttribute<bool>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && Widget->Get_IsTrackAvailable();
    }));
    Data.Visibility.Add(TEXT("audio-director-available"), TAttribute<bool>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && Widget->Get_IsDirectorAvailable();
    }));
    Data.Visibility.Add(TEXT("audio-unavailable"), TAttribute<bool>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid()
            && NOT Widget->Get_IsTrackAvailable()
            && NOT Widget->Get_IsDirectorAvailable();
    }));
    Data.Visibility.Add(TEXT("audio-track-can-request"), TAttribute<bool>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && Widget->Get_CanRequestTrack();
    }));
    Data.Visibility.Add(TEXT("audio-director-can-request"), TAttribute<bool>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && Widget->Get_CanRequestDirector();
    }));
    Data.Visibility.Add(TEXT("audio-debug-draw-can-request"), TAttribute<bool>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && Widget->Get_CanDebugDraw();
    }));
    Data.Visibility.Add(TEXT("audio-debug-draw"), TAttribute<bool>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && Widget->Get_IsDebugDrawEnabled();
    }));

    const auto GetTrackCurrentNumber = [Weak](
        TFunction<float(const ck::FFragment_AudioTrack_Current&)> InGetValue)
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && Widget->Get_IsTrackAvailable()
            ? InGetValue(Widget->_Entity.Get<ck::FFragment_AudioTrack_Current>())
            : 0.0f;
    };
    Data.Number.Add(TEXT("audio-track-volume-input"), TAttribute<float>::CreateLambda(
        [GetTrackCurrentNumber]()
        {
            return GetTrackCurrentNumber([](const ck::FFragment_AudioTrack_Current& InCurrent)
            { return InCurrent.Get_CurrentVolume(); });
        }));
    Data.Number.Add(TEXT("audio-track-volume-fraction"), TAttribute<float>::CreateLambda(
        [GetTrackCurrentNumber]()
        {
            return GetTrackCurrentNumber([](const ck::FFragment_AudioTrack_Current& InCurrent)
            { return InCurrent.Get_CurrentVolume(); });
        }));
    Data.Number.Add(TEXT("audio-track-playback-fraction"), TAttribute<float>::CreateLambda(
        [GetTrackCurrentNumber]()
        {
            return GetTrackCurrentNumber([](const ck::FFragment_AudioTrack_Current& InCurrent)
            { return InCurrent.Get_PlaybackPercent(); });
        }));
    Data.Color.Add(TEXT("audio-track-volume-fill"), CkStyle::GetToneColor(ECk_Tone::Accent));
    Data.Color.Add(TEXT("audio-track-playback-fill"), CkStyle::GetToneColor(ECk_Tone::Info));
    Data.NumberCommitted.Add(
        TEXT("audio-track-volume-committed"),
        FCkUiOnNumberCommitted::CreateLambda([Weak](const float InValue, ETextCommit::Type)
        {
            if (const auto Widget = Weak.Pin(); Widget.IsValid())
            { Widget->Commit_Volume(InValue); }
        }));
    Data.BoolChanged.Add(
        TEXT("audio-debug-draw-changed"),
        FCkUiOnBoolChanged::CreateLambda([Weak](const bool bInEnabled)
        {
            if (const auto Widget = Weak.Pin(); Widget.IsValid())
            { Widget->Set_DebugDraw(bInEnabled); }
        }));

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("audio-track-play"), FSimpleDelegate::CreateLambda([Weak]()
    {
        if (const auto Widget = Weak.Pin(); Widget.IsValid())
        { Widget->Request_Play(); }
    }));
    Actions.Add(TEXT("audio-track-stop"), FSimpleDelegate::CreateLambda([Weak]()
    {
        if (const auto Widget = Weak.Pin(); Widget.IsValid())
        { Widget->Request_Stop(); }
    }));
    Actions.Add(TEXT("audio-director-stop-all"), FSimpleDelegate::CreateLambda([Weak]()
    {
        if (const auto Widget = Weak.Pin(); Widget.IsValid())
        { Widget->Request_StopAll(); }
    }));
    Data.Collections.Add(TEXT("audio-director-tracks"), _Tracks);
    Data.ItemActions.Add(
        TEXT("audio-director-stop-track"),
        FCkUiOnItemAction::CreateLambda([Weak](const FString& InKey)
        {
            if (const auto Widget = Weak.Pin(); Widget.IsValid())
            { Widget->Request_StopTrack(InKey); }
        }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {},
        MoveTemp(Actions),
        {},
        FCoreStyle::Get().GetFontStyle("NormalFont"),
        MoveTemp(Data),
        Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(Root, TEXT("EcsInspectorAudio.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorAudio.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }

    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_AudioAuthored::Request_Play() -> void
{
    auto Track = FCk_Handle_AudioTrack{};
    if (NOT _Active
        || NOT ck_inspector_audio::TryGetTrack(_Entity, Track)
        || NOT ck::DebugRequestGate::Evaluate(
            _Entity,
            ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
    { return; }

    UCk_Utils_AudioTrack_UE::Request_Play(Track, FCk_Time::ZeroSecond(), {});
}

auto SCkInspector_AudioAuthored::Request_Stop() -> void
{
    auto Track = FCk_Handle_AudioTrack{};
    if (NOT _Active
        || NOT ck_inspector_audio::TryGetTrack(_Entity, Track)
        || NOT ck::DebugRequestGate::Evaluate(
            _Entity,
            ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
    { return; }

    UCk_Utils_AudioTrack_UE::Request_Stop(Track, FCk_Time::ZeroSecond(), {});
}

auto SCkInspector_AudioAuthored::Commit_Volume(const float InVolume) -> void
{
    auto Track = FCk_Handle_AudioTrack{};
    if (NOT _Active
        || NOT ck_inspector_audio::TryGetTrack(_Entity, Track)
        || NOT ck::DebugRequestGate::Evaluate(
            _Entity,
            ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
    { return; }

    UCk_Utils_AudioTrack_UE::Request_SetVolume(
        Track,
        InVolume,
        FCk_Time::ZeroSecond(),
        {});
}

auto SCkInspector_AudioAuthored::Set_DebugDraw(const bool bInEnabled) -> void
{
    auto Track = FCk_Handle_AudioTrack{};
    if (NOT _Active
        || NOT ck_inspector_audio::TryGetTrack(_Entity, Track)
        || NOT ck::DebugRequestGate::Evaluate(
            _Entity,
            ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled)
    { return; }

    if (bInEnabled)
    { UCk_Utils_AudioTrack_UE::Request_EnableDebugDraw(Track); }
    else
    { UCk_Utils_AudioTrack_UE::Request_DisableDebugDraw(Track); }
}

auto SCkInspector_AudioAuthored::Request_StopAll() -> void
{
    auto Director = FCk_Handle_AudioDirector{};
    if (NOT _Active
        || NOT ck_inspector_audio::TryGetDirector(_Entity, Director)
        || NOT ck::DebugRequestGate::Evaluate(
            _Entity,
            ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
    { return; }

    UCk_Utils_AudioDirector_UE::Request_StopAllTracks(
        Director,
        FCk_Time::ZeroSecond(),
        {});
}

auto SCkInspector_AudioAuthored::Request_StopTrack(const FString& InTrackKey) -> void
{
    auto Director = FCk_Handle_AudioDirector{};
    const FName TrackName{*InTrackKey};
    if (NOT _Active
        || TrackName.IsNone()
        || NOT ck_inspector_audio::TryGetDirector(_Entity, Director)
        || NOT Director.Get<ck::FFragment_AudioDirector_Current>()
            .Get_TracksByName().Contains(TrackName)
        || NOT ck::DebugRequestGate::Evaluate(
            _Entity,
            ECk_DebugRequest_Requirement::LocalOk).IsEnabled)
    { return; }

    UCk_Utils_AudioDirector_UE::Request_StopTrack(
        Director,
        TrackName,
        FCk_Time::ZeroSecond(),
        {});
}

auto SCkInspector_AudioAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid() || NOT Refresh_Tracks())
    { return; }

    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else
    { _LoadError.Reset(); }
}

auto SCkInspector_AudioAuthored::Release() -> void
{
    if (NOT _Active)
    { return; }

    _Active = false;
    _Entity = {};
    _DiffLabels.Reset();
    _Tracks.Reset();
    _View.Reset();
    _Mounted = false;
}

FCkInspector_Audio::~FCkInspector_Audio()
{
    OnDeactivated();
}
auto FCkInspector_Audio::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> Native = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    { return Native; }

    auto DiffLabels = TSet<FString>{};
    for (const TCHAR* Label : {
        TEXT("State:"),
        TEXT("Volume (cur → target):"),
        TEXT("Fade Speed:"),
        TEXT("Playback:"),
        TEXT("Virtualized:"),
        TEXT("Active Tracks:"),
        TEXT("Highest Priority:"),
        TEXT("All Tracks Finished:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
        { DiffLabels.Add(Label); }
    }
    if (ck_inspector_audio::HasDirectorCurrent(Entity))
    {
        for (const auto& [Name, Handle] : Entity.Get<ck::FFragment_AudioDirector_Current>().Get_TracksByName())
        {
            const FString Label = Name.ToString();
            if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
            { DiffLabels.Add(Label); }
        }
    }

    const TSharedRef<SCkInspector_AudioAuthored> Authored = SNew(SCkInspector_AudioAuthored)
        .Entity(Entity)
        .DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return Native;
    }

    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_Audio::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_AudioAuthored>& Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}
