#include "CkInspector_Tween.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkTween/CkTween_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Tween)

// =====================================================================================================================

namespace ck_inspector_tween
{
    static auto Get_StateTone(ECk_TweenState InState) -> ECk_Tone
    {
        switch (InState)
        {
            case ECk_TweenState::Playing:   return ECk_Tone::Ok;
            case ECk_TweenState::Paused:    return ECk_Tone::Warn;
            case ECk_TweenState::Completed: return ECk_Tone::Info;
            case ECk_TweenState::Cancelled: return ECk_Tone::Err;
            default:                        return ECk_Tone::Neutral;
        }
    }

    static auto Get_DurationSeconds(const FCk_Handle& InEntity) -> float
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_Tween_Params>())
        { return 0.0f; }

        return InEntity.Get<ck::FFragment_Tween_Params>().Get_Duration();
    }

    static auto Get_CurrentTimeSeconds(const FCk_Handle& InEntity) -> float
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_Tween_Current>())
        { return 0.0f; }

        return InEntity.Get<ck::FFragment_Tween_Current>().Get_CurrentTime();
    }
}

// =====================================================================================================================

auto FCkInspector_Tween::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Tween"));
}

auto FCkInspector_Tween::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && Entity.Has<ck::FFragment_Tween_Current>();
}

// =====================================================================================================================

auto FCkInspector_Tween::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    if (NOT Entity.Has<ck::FFragment_Tween_Current>())
    { return Builder.Build(Entity); }

    Builder.AddHeader(FText::FromString(TEXT("Tween")));

    const auto CapturedEntity = Entity;

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("State:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto State = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_State();
            return FText::FromString(ck::Format_UE(TEXT("{}"), State));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return ECk_Tone::Neutral; }
            return ck_inspector_tween::Get_StateTone(
                CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_State());
        }));

    // The Params duration is fixed for the tween's life, so the row shape can be chosen once here;
    // without it there is no bound to meter against and the raw time is all there is to show.
    const auto HasDuration = ck_inspector_tween::Get_DurationSeconds(Entity) > 0.0f;

    if (HasDuration)
    {
        Builder.AddMeterRow(
            FText::FromString(TEXT("Time:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            {
                const auto Duration = ck_inspector_tween::Get_DurationSeconds(CapturedEntity);
                if (Duration <= 0.0f) { return 0.0f; }
                return ck_inspector_tween::Get_CurrentTimeSeconds(CapturedEntity) / Duration;
            }),
            ECk_Tone::Accent,
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{:.4f} / {:.4f}s"),
                    ck_inspector_tween::Get_CurrentTimeSeconds(CapturedEntity),
                    ck_inspector_tween::Get_DurationSeconds(CapturedEntity)));
            }));
    }
    else
    {
        Builder.AddRow(
            FText::FromString(TEXT("Time:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto T = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_CurrentTime();
                return FText::FromString(FString::Printf(TEXT("%.4f"), T));
            },
            CkStyle::Value_Numeric());
    }

    Builder.AddRow(
        FText::FromString(TEXT("Loop:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto Loop = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_CurrentLoop();
            return FText::FromString(ck::Format_UE(TEXT("{}"), Loop));
        },
        CkStyle::Value_Numeric());

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Reversed:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto IsReversed = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_IsReversed();
            return FText::FromString(IsReversed ? TEXT("Yes") : TEXT("No"));
        },
        [CapturedEntity](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return CkStyle::None(); }
            const auto IsReversed = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_IsReversed();
            return IsReversed ? CkStyle::Status_Active() : CkStyle::Value_Bool_False();
        });

    Builder.AddRow(
        FText::FromString(TEXT("Time Multiplier:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto Multiplier = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_TimeMultiplier();
            return FText::FromString(FString::Printf(TEXT("%.3f"), Multiplier));
        },
        CkStyle::Value_Numeric());

    // ---- Chain ----
    if (Entity.Has<ck::FFragment_Tween_Chain>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Chain")));

        Builder.AddRow(
            FText::FromString(TEXT("Next Tween:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Chain>())
                { return FText::FromString(TEXT("--")); }
                const auto& NextTween = CapturedEntity.Get<ck::FFragment_Tween_Chain>().Get_NextTween();
                if (NOT NextTween.IsSet())
                { return FText::FromString(TEXT("(None)")); }
                return FText::FromString(ck::IsValid(NextTween.GetValue())
                    ? ck::Format_UE(TEXT("[{}]"), NextTween.GetValue())
                    : FString(TEXT("(Invalid)")));
            },
            CkStyle::Value_Handle());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Tween::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
