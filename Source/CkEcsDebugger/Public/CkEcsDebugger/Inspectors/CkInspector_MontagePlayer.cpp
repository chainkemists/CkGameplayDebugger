#include "CkInspector_MontagePlayer.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkAnimation/MontagePlayer/CkMontagePlayer_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_MontagePlayer)

// =====================================================================================================================

namespace ck_inspector_montage_player
{
    static auto Get_StateKindTone(ECk_MontagePlayer_StateKind InKind) -> ECk_Tone
    {
        switch (InKind)
        {
            case ECk_MontagePlayer_StateKind::Play:          return ECk_Tone::Ok;
            case ECk_MontagePlayer_StateKind::Resume:        return ECk_Tone::Ok;
            case ECk_MontagePlayer_StateKind::Pause:         return ECk_Tone::Warn;
            case ECk_MontagePlayer_StateKind::JumpToSection: return ECk_Tone::Info;
            case ECk_MontagePlayer_StateKind::Stop:          return ECk_Tone::Neutral;
            default:                                         return ECk_Tone::Neutral;
        }
    }

    static auto Get_StateKindString(ECk_MontagePlayer_StateKind InKind) -> FString
    {
        switch (InKind)
        {
            case ECk_MontagePlayer_StateKind::Play:          return TEXT("Play");
            case ECk_MontagePlayer_StateKind::Stop:          return TEXT("Stop");
            case ECk_MontagePlayer_StateKind::Pause:         return TEXT("Pause");
            case ECk_MontagePlayer_StateKind::Resume:        return TEXT("Resume");
            case ECk_MontagePlayer_StateKind::JumpToSection: return TEXT("JumpToSection");
            default:                                         return TEXT("Unknown");
        }
    }

    // The fragment records the requested state, not the playhead — the live position only exists on
    // the AnimInstance driving the montage. Both are weak, so every read re-resolves them.
    static auto Get_PlaybackSeconds(
        const FCk_Handle& InEntity,
        float& OutLength)
        -> float
    {
        OutLength = 0.0f;

        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_MontagePlayer_Current>())
        { return 0.0f; }

        const auto& Current      = InEntity.Get<ck::FFragment_MontagePlayer_Current>();
        const auto* Montage      = Current.Get_ActiveMontage().Get();
        const auto* AnimInstance = Current.Get_LastSeenAnimInstance().Get();

        if (ck::Is_NOT_Valid(Montage, ck::IsValid_Policy_NullptrOnly{}) ||
            ck::Is_NOT_Valid(AnimInstance, ck::IsValid_Policy_NullptrOnly{}))
        { return 0.0f; }

        OutLength = Montage->GetPlayLength();
        return AnimInstance->Montage_GetPosition(Montage);
    }
}

// =====================================================================================================================

auto FCkInspector_MontagePlayer::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Montage Player"));
}

auto FCkInspector_MontagePlayer::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has<ck::FFragment_MontagePlayer_Current>();
}

// =====================================================================================================================

auto FCkInspector_MontagePlayer::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    const auto CapturedEntity = Entity;

    Builder.AddHeader(FText::FromString(TEXT("Montage Player")));

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("State:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_MontagePlayer_Current>())
            { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck_inspector_montage_player::Get_StateKindString(
                CapturedEntity.Get<ck::FFragment_MontagePlayer_Current>().Get_State().Get_Kind()));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_MontagePlayer_Current>())
            { return ECk_Tone::Neutral; }
            return ck_inspector_montage_player::Get_StateKindTone(
                CapturedEntity.Get<ck::FFragment_MontagePlayer_Current>().Get_State().Get_Kind());
        }));

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Active Montage:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_MontagePlayer_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto Montage = CapturedEntity.Get<ck::FFragment_MontagePlayer_Current>().Get_ActiveMontage();
            if (NOT Montage.IsValid())
            { return FText::FromString(TEXT("(none)")); }
            return FText::FromString(Montage->GetFName().ToString());
        },
        [CapturedEntity](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_MontagePlayer_Current>())
            { return CkStyle::None(); }
            return CapturedEntity.Get<ck::FFragment_MontagePlayer_Current>().Get_ActiveMontage().IsValid()
                ? CkStyle::Value_Object()
                : CkStyle::TextMute();
        });

    Builder.AddMeterRow(
        FText::FromString(TEXT("Position:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        {
            auto Length = 0.0f;
            const auto Position = ck_inspector_montage_player::Get_PlaybackSeconds(CapturedEntity, Length);
            if (Length <= 0.0f) { return 0.0f; }
            return Position / Length;
        }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            auto Length = 0.0f;
            const auto Position = ck_inspector_montage_player::Get_PlaybackSeconds(CapturedEntity, Length);
            if (Length <= 0.0f) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{:.2f} / {:.2f}s"), Position, Length));
        }));

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Anim Instance:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_MontagePlayer_Current>())
            { return FText::FromString(TEXT("--")); }
            return FText::FromString(
                CapturedEntity.Get<ck::FFragment_MontagePlayer_Current>().Get_LastSeenAnimInstance().IsValid()
                    ? TEXT("Valid")
                    : TEXT("Invalid"));
        },
        [CapturedEntity](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_MontagePlayer_Current>())
            { return CkStyle::None(); }
            return CapturedEntity.Get<ck::FFragment_MontagePlayer_Current>().Get_LastSeenAnimInstance().IsValid()
                ? CkStyle::Ok()
                : CkStyle::Err();
        });

    Builder.AddRow(
        FText::FromString(TEXT("Play Rate:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_MontagePlayer_Current>())
            { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{:.2f}"),
                CapturedEntity.Get<ck::FFragment_MontagePlayer_Current>().Get_State().Get_PlayRate()));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Catch-up Remaining:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_MontagePlayer_Current>())
            { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{:.3f} s"),
                CapturedEntity.Get<ck::FFragment_MontagePlayer_Current>().Get_CatchUpRemaining().Get_Seconds()));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_MontagePlayer::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
