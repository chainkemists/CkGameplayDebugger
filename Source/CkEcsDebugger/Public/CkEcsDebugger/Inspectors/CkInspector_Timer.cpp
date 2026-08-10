#include "CkInspector_Timer.h"

#include "CkCore/Chrono/CkChrono.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Time/CkTime.h"
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
    Builder.SetEditGuard(Get_EditGuard());

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

    // ---- Controls ----
    //
    // Every verb below leaves through the public Timer Utils with the typed handle captured BY VALUE
    // and re-validated on fire. Nothing reads a request back: the live rows above are the display, and
    // the queued verbs land a frame later.
    //
    // All of them are LocalOk — FProcessor_Timer_HandleRequests runs on every net mode, and a timer
    // that is not replicated at all is the common case.

    Builder.AddHeader(FText::FromString(TEXT("Controls")));

    Builder.AddActionRow(
        FText::FromString(TEXT("Playback:")),
        {
            FCkInspector_Action
            {
                FText::FromString(TEXT("Pause")),
                FText::FromString(TEXT("Request_Pause — stops advancing, keeps the elapsed value")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_Pause(Timer, {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Resume")),
                FText::FromString(TEXT("Request_Resume — resumes advancing from the current elapsed value")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_Resume(Timer, {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Stop")),
                FText::FromString(TEXT("Request_Stop — pauses AND rewinds to the starting value")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_Stop(Timer, {});
                }
            }
        });

    Builder.AddActionRow(
        FText::FromString(TEXT("Position:")),
        {
            FCkInspector_Action
            {
                FText::FromString(TEXT("Reset")),
                FText::FromString(TEXT("Request_Reset — rewinds to the starting value, leaving the run state alone")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_Reset(Timer, {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Complete")),
                FText::FromString(TEXT("Request_Complete — jumps straight to Done and fires the timer's completion signals")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_Complete(Timer, {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Reverse")),
                FText::FromString(TEXT("Request_ReverseDirection — flips CountUp <-> CountDown immediately (no queue)")),
                [CapturedTimer]
                {
                    auto Timer = CapturedTimer;
                    if (ck::Is_NOT_Valid(Timer))
                    { return; }

                    UCk_Utils_Timer_UE::Request_ReverseDirection(Timer, {});
                }
            }
        });

    // Immediate mutator, so the read-only "Direction:" row above agrees with this dropdown on the very
    // next paint rather than a frame later.
    Builder.AddEnumDropdownRow(
        FText::FromString(TEXT("Set Direction:")),
        {
            FText::FromString(TEXT("CountUp")),
            FText::FromString(TEXT("CountDown"))
        },
        TAttribute<int32>::CreateLambda([CapturedTimer]()
        {
            if (ck::Is_NOT_Valid(CapturedTimer))
            { return 0; }

            return static_cast<int32>(UCk_Utils_Timer_UE::Get_CountDirection(CapturedTimer));
        }),
        [CapturedTimer](int32 InIndex)
        {
            auto Timer = CapturedTimer;
            if (ck::Is_NOT_Valid(Timer))
            { return; }

            UCk_Utils_Timer_UE::Request_ChangeCountDirection(
                Timer, static_cast<ECk_Timer_CountDirection>(InIndex), {});
        });

    // Jump mode is a pending ARGUMENT, not timer state — it selects how the next committed jump amount
    // is read (delta vs target elapsed), so it writes only to this inspector's box.
    Builder.AddEnumDropdownRow(
        FText::FromString(TEXT("Jump Mode:")),
        {
            FText::FromString(TEXT("Relative")),
            FText::FromString(TEXT("Absolute"))
        },
        TAttribute<int32>::CreateLambda([Mode = _JumpMode]()
        {
            return static_cast<int32>(*Mode);
        }),
        [Mode = _JumpMode](int32 InIndex)
        {
            *Mode = static_cast<ECk_RelativeAbsolute>(InIndex);
        });

    // Committing the amount IS the verb — the editor commits on enter / lost focus, never per
    // keystroke, so one jump is enqueued per deliberate edit.
    Builder.AddNumericRow(
        FText::FromString(TEXT("Jump (s):")),
        TAttribute<float>::CreateLambda([Seconds = _JumpSeconds]() { return *Seconds; }),
        [CapturedTimer, Seconds = _JumpSeconds, Mode = _JumpMode](float InSeconds)
        {
            *Seconds = InSeconds;

            auto Timer = CapturedTimer;
            if (ck::Is_NOT_Valid(Timer))
            { return; }

            UCk_Utils_Timer_UE::Request_Jump(
                Timer,
                FCk_Request_Timer_Jump{FCk_Time{InSeconds}}.Set_JumpMode(*Mode),
                {});
        });

    Builder.AddNumericRow(
        FText::FromString(TEXT("Consume (s):")),
        TAttribute<float>::CreateLambda([Seconds = _ConsumeSeconds]() { return *Seconds; }),
        [CapturedTimer, Seconds = _ConsumeSeconds](float InSeconds)
        {
            *Seconds = InSeconds;

            auto Timer = CapturedTimer;
            if (ck::Is_NOT_Valid(Timer))
            { return; }

            UCk_Utils_Timer_UE::Request_Consume(Timer, FCk_Request_Timer_Consume{FCk_Time{InSeconds}}, {});
        });

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================
