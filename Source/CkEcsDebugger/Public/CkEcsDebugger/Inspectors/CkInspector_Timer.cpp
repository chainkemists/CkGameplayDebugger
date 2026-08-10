#include "CkInspector_Timer.h"

#include "CkCore/Chrono/CkChrono.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkTimer/CkTimer_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Timer)

// =====================================================================================================================

namespace ck_inspector_timer
{
    // Elapsed / goal, matching the ratio the numeric rows report. A zero (or negative) goal has no meaningful
    // progress — read it as empty rather than dividing by zero.
    auto Get_Progress(
        const FCk_Handle_Timer& InTimer)
        -> float
    {
        if (ck::Is_NOT_Valid(InTimer))
        { return 0.0f; }

        const auto Chrono  = UCk_Utils_Timer_UE::Get_CurrentTimerValue(InTimer);
        const auto GoalMs  = Chrono.Get_GoalValue().Get_Milliseconds();

        if (GoalMs <= 0.0)
        { return 0.0f; }

        return FMath::Clamp(static_cast<float>(Chrono.Get_TimeElapsed().Get_Milliseconds() / GoalMs), 0.0f, 1.0f);
    }

    // The meter's tone is fixed at compose time, so it cannot dim while the timer is paused — a full-brightness bar
    // that is not advancing reads as a live timer. This pill carries that signal instead, and states the reason.
    auto Get_StateTone(
        const FCk_Handle_Timer& InTimer)
        -> ECk_Tone
    {
        if (ck::Is_NOT_Valid(InTimer))
        { return ECk_Tone::Neutral; }

        return UCk_Utils_Timer_UE::Get_CurrentState(InTimer) == ECk_Timer_State::Running
            ? ECk_Tone::Accent
            : ECk_Tone::Neutral;
    }
}

// =====================================================================================================================

auto FCkInspector_Timer::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Timer"));
}

auto FCkInspector_Timer::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Timer_UE::Has(Entity);
}

auto FCkInspector_Timer::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;
    const auto TimerHandle = UCk_Utils_Timer_UE::Cast(MutableEntity);

    if (ck::Is_NOT_Valid(TimerHandle))
    { return Builder.Build(Entity, FString()); }

    const auto CapturedTimer = TimerHandle;

    // ---- Identity ----

    Builder.AddRow(
        FText::FromString(TEXT("Name:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto& Name = UCk_Utils_Timer_UE::Get_Name(CapturedTimer);
            return FText::FromString(Name.IsValid() ? Name.ToString() : TEXT("None"));
        },
        CkStyle::Value_Tag());

    // ---- Configuration ----

    Builder.AddRow(
        FText::FromString(TEXT("Direction:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Direction = UCk_Utils_Timer_UE::Get_CountDirection(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Direction));
        },
        CkStyle::Value_Enum());

    Builder.AddRow(
        FText::FromString(TEXT("Behavior:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Behavior = UCk_Utils_Timer_UE::Get_Behavior(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Behavior));
        },
        CkStyle::Value_Enum());

    // ---- Live state ----

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("State:")),
        TAttribute<FText>::CreateLambda([CapturedTimer]()
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto State = UCk_Utils_Timer_UE::Get_CurrentState(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), State));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedTimer]()
        {
            return ck_inspector_timer::Get_StateTone(CapturedTimer);
        }));

    Builder.AddRow(
        FText::FromString(TEXT("Goal:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Chrono.Get_GoalValue()));
        },
        CkStyle::Value_Numeric());

    Builder.AddMeterRow(
        FText::FromString(TEXT("Elapsed:")),
        TAttribute<float>::CreateLambda([CapturedTimer]()
        {
            return ck_inspector_timer::Get_Progress(CapturedTimer);
        }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([CapturedTimer]()
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{} / {}"), Chrono.Get_TimeElapsed(), Chrono.Get_GoalValue()));
        }));

    Builder.AddRow(
        FText::FromString(TEXT("Remaining:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Chrono.Get_TimeRemaining()));
        },
        CkStyle::Value_Numeric());

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Done:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return FText::FromString(Chrono.Get_IsDone() ? TEXT("Yes") : TEXT("No"));
        },
        [CapturedTimer](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return CkStyle::None(); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return Chrono.Get_IsDone()
                ? CkStyle::Value_Bool_True()
                : CkStyle::Value_Bool_False();
        });

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================
