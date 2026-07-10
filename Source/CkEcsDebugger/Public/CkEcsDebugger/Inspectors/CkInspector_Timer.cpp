#include "CkInspector_Timer.h"

#include "CkCore/Chrono/CkChrono.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkTimer/CkTimer_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Timer)

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

    Builder.AddConditionalRow(
        FText::FromString(TEXT("State:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto State = UCk_Utils_Timer_UE::Get_CurrentState(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), State));
        },
        [CapturedTimer](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return CkStyle::None(); }
            return UCk_Utils_Timer_UE::Get_CurrentState(CapturedTimer) == ECk_Timer_State::Running
                ? CkStyle::Status_Active()
                : CkStyle::Value_Enum();
        });

    Builder.AddRow(
        FText::FromString(TEXT("Goal:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Chrono.Get_GoalValue()));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Elapsed:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Chrono.Get_TimeElapsed()));
        },
        CkStyle::Value_Numeric());

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
        FText::FromString(TEXT("Progress:")),
        [CapturedTimer](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return FText::FromString(TEXT("--")); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            const auto GoalMs    = Chrono.Get_GoalValue().Get_Milliseconds();
            const auto ElapsedMs = Chrono.Get_TimeElapsed().Get_Milliseconds();
            const auto Ratio     = GoalMs > 0.0 ? (ElapsedMs / GoalMs) : 0.0;
            return FText::FromString(ck::Format_UE(TEXT("{:.1f}%"), Ratio * 100.0));
        },
        [CapturedTimer](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedTimer)) { return CkStyle::None(); }
            const auto Chrono = UCk_Utils_Timer_UE::Get_CurrentTimerValue(CapturedTimer);
            return Chrono.Get_IsDone()
                ? CkStyle::Status_Active()
                : CkStyle::Value_Numeric();
        });

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
