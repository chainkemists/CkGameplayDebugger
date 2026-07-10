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

        Builder.AddRow(
            FText::FromString(TEXT("State:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto State = CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_State();
                return FText::FromString(ck::Format_UE(TEXT("{}"), State));
            },
            CkStyle::Value_Enum());

        Builder.AddRow(
            FText::FromString(TEXT("Volume (cur):")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Vol = CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_CurrentVolume();
                return FText::FromString(FString::Printf(TEXT("%.3f"), Vol));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Volume (target):")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Vol = CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_TargetVolume();
                return FText::FromString(FString::Printf(TEXT("%.3f"), Vol));
            },
            CkStyle::Value_Numeric());

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

        Builder.AddRow(
            FText::FromString(TEXT("Playback:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioTrack_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Pct = CapturedEntity.Get<ck::FFragment_AudioTrack_Current>().Get_PlaybackPercent();
                return FText::FromString(FString::Printf(TEXT("%.1f%%"), Pct));
            },
            CkStyle::Value_Numeric());

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

        Builder.AddRow(
            FText::FromString(TEXT("Active Tracks:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_AudioDirector_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Count = CapturedEntity.Get<ck::FFragment_AudioDirector_Current>().Get_ActiveTracks().Num();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Count));
            },
            CkStyle::Value_Numeric());

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
