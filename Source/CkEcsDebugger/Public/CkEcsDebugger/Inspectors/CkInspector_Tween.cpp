#include "CkInspector_Tween.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkTween/CkTween_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Tween)

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

    Builder.AddRow(
        FText::FromString(TEXT("State:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto State = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_State();
            return FText::FromString(ck::Format_UE(TEXT("{}"), State));
        },
        CkStyle::Value_Enum());

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
