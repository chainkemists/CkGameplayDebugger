#include "CkInspector_StateMachine.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment_Data.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Utils.h"
#include "CkStateMachine/State/CkSmState_Fragment.h"
#include "CkStateMachine/Task/CkSmTask_Fragment.h"
#include "CkStateMachine/Task/EntityScripts/CkSmTask_EntityScript.h"
#include "CkStateMachine/Transition/CkSmTransition_Fragment.h"
#include "CkStateMachine/Condition/CkSmCondition_Fragment.h"
#include "CkStateMachine/Condition/EntityScripts/CkSmCondition_EntityScript.h"
#include "CkStateMachine/Debug/CkStateMachine_Debug_Fragment.h"
#include "CkStateMachine/Debug/CkStateMachine_Debug_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_StateMachine)

// =====================================================================================================================

namespace
{
    // One lane + the timeline's own 8px top pad and 18px axis band — enough to read the markers
    // without the History section swallowing the inspector.
    constexpr auto SmTimelineHeight = 60.0f;

    // Tones replace the previous per-result FLinearColor mapping now that these values render as
    // status pills. Running is Accent (the "live" tone the pill widget was built for) rather than
    // Ok, so a running SM reads as *active* and only Succeeded/Pass read as *good*.
    auto Format_RunStatus_Tone(ECk_SmRunStatus InStatus) -> ECk_Tone
    {
        switch (InStatus)
        {
            case ECk_SmRunStatus::Running: return ECk_Tone::Accent;
            case ECk_SmRunStatus::Paused:  return ECk_Tone::Warn;
            case ECk_SmRunStatus::Stopped:
            default:                       return ECk_Tone::Neutral;
        }
    }

    auto Format_TaskResult_Tone(ECk_SmTaskResult InResult) -> ECk_Tone
    {
        switch (InResult)
        {
            case ECk_SmTaskResult::Running:   return ECk_Tone::Accent;
            case ECk_SmTaskResult::Succeeded: return ECk_Tone::Ok;
            case ECk_SmTaskResult::Failed:
            default:                          return ECk_Tone::Err;
        }
    }

    auto Format_ConditionResult_Tone(ECk_SmConditionResult InResult) -> ECk_Tone
    {
        switch (InResult)
        {
            case ECk_SmConditionResult::Pass:        return ECk_Tone::Ok;
            case ECk_SmConditionResult::Fail:        return ECk_Tone::Err;
            case ECk_SmConditionResult::Undetermined:
            default:                                 return ECk_Tone::Neutral;
        }
    }

    auto Format_TransitionResult_Tone(ECk_SmTransitionResult InResult) -> ECk_Tone
    {
        switch (InResult)
        {
            case ECk_SmTransitionResult::Pass:        return ECk_Tone::Ok;
            case ECk_SmTransitionResult::Fail:        return ECk_Tone::Err;
            case ECk_SmTransitionResult::Undetermined:
            default:                                  return ECk_Tone::Neutral;
        }
    }

    // Depth-tuned via the canonical shortener at the shared SM name-depth
    // setting — the "SM Name Depth" spinbox in the window's overlay popover now
    // drives the inspector too (0 = full name).
    auto Format_Sm_ClassName(const UClass* InClass) -> FString
    {
        if (InClass == nullptr) { return FString(TEXT("(None)")); }
        return SCkDebug_NameLabel::Get_ShortName(
            InClass->GetName(),
            GetDefault<UCk_DebugOverlay_Settings>()->SmStateNameDepth);
    }

    auto Format_Sm_StateName(const FString& InStateName) -> FString
    {
        if (InStateName.IsEmpty()) { return FString(TEXT("(None)")); }
        return SCkDebug_NameLabel::Get_ShortName(
            InStateName,
            GetDefault<UCk_DebugOverlay_Settings>()->SmStateNameDepth);
    }
}

// =====================================================================================================================

auto FCkInspector_StateMachine::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("State Machine"));
}

auto FCkInspector_StateMachine::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<
        ck::FFragment_Sm_Current,
        ck::FFragment_Sm_Debug,
        ck::FFragment_SmState_Params,
        ck::FFragment_SmTask_Current,
        ck::FFragment_SmTask_Params,
        ck::FFragment_SmTransition_Current,
        ck::FFragment_SmTransition_Params,
        ck::FFragment_SmCondition_Current,
        ck::FFragment_SmCondition_Params>();
}

// =====================================================================================================================

auto FCkInspector_StateMachine::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    // Keep the Sm_Debug poll processor running while this inspector is showing SM data —
    // it gates itself off when no debugger has consumed its data recently.
    UCk_Utils_StateMachineDebug_UE::NotifyDebugDataConsumed();

    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    // ---- State Machine root entity ----
    if (Entity.Has<ck::FFragment_Sm_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("State Machine")));

        const auto CapturedEntity = Entity;

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Status:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sm_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Status = CapturedEntity.Get<ck::FFragment_Sm_Current>().Get_RunStatus();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Status));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sm_Current>())
                { return ECk_Tone::Neutral; }
                return Format_RunStatus_Tone(CapturedEntity.Get<ck::FFragment_Sm_Current>().Get_RunStatus());
            }));

        // The current state is the single most-read value in this inspector — Info-toned pill so it
        // separates from the run status beside it without competing with Ok/Err result pills below.
        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Current State:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sm_Current>())
                { return FText::FromString(TEXT("--")); }
                const UClass* StateClass = CapturedEntity.Get<ck::FFragment_Sm_Current>().Get_CurrentStateClass();
                return FText::FromString(Format_Sm_ClassName(StateClass));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sm_Current>())
                { return ECk_Tone::Neutral; }
                const UClass* StateClass = CapturedEntity.Get<ck::FFragment_Sm_Current>().Get_CurrentStateClass();
                return StateClass != nullptr ? ECk_Tone::Info : ECk_Tone::Neutral;
            }));

        // Run-status verbs. AuthorityOnly across the board: the SM request processor is authority-
        // gated and ensure-and-drops off-authority, so a live button on a client would read as "the
        // debugger is broken". The gate greys them with the reason instead.
        //
        // NO transition control here by design: the only public API is
        // Request_Transition(TSubclassOf<UCk_SmState_EntityScript>), which needs a class picker this
        // vocabulary does not have (design doc "Deliberately excluded").
        auto MutableSmEntity = Entity;
        const auto CapturedSm = UCk_Utils_StateMachine_UE::Cast(MutableSmEntity);

        if (ck::IsValid(CapturedSm))
        {
            const auto Make_SmAction = [CapturedSm](
                const FString& InLabel,
                const FString& InTooltip,
                auto InRequest) -> FCkInspector_Action
            {
                return FCkInspector_Action
                {
                    FText::FromString(InLabel),
                    FText::FromString(InTooltip),
                    [CapturedSm, InRequest]()
                    {
                        auto MutableSm = CapturedSm;
                        if (ck::Is_NOT_Valid(MutableSm)) { return; }
                        InRequest(MutableSm);
                    },
                    ECk_DebugRequest_Requirement::AuthorityOnly
                };
            };

            Builder.AddActionRow(
                FText::FromString(TEXT("Control:")),
                {
                    Make_SmAction(TEXT("Start"), TEXT("UCk_Utils_StateMachine_UE::Request_Start"),
                        [](FCk_Handle_StateMachine& InSm) { UCk_Utils_StateMachine_UE::Request_Start(InSm, {}); }),
                    Make_SmAction(TEXT("Stop"), TEXT("UCk_Utils_StateMachine_UE::Request_Stop"),
                        [](FCk_Handle_StateMachine& InSm) { UCk_Utils_StateMachine_UE::Request_Stop(InSm, {}); }),
                    Make_SmAction(TEXT("Pause"), TEXT("UCk_Utils_StateMachine_UE::Request_Pause"),
                        [](FCk_Handle_StateMachine& InSm) { UCk_Utils_StateMachine_UE::Request_Pause(InSm, {}); }),
                    Make_SmAction(TEXT("Resume"), TEXT("UCk_Utils_StateMachine_UE::Request_Resume"),
                        [](FCk_Handle_StateMachine& InSm) { UCk_Utils_StateMachine_UE::Request_Resume(InSm, {}); }),
                });
        }
    }

    // ---- Pending Transition ----
    if (Entity.Has<ck::FFragment_Sm_PendingTransition>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Pending Transition")));

        const auto CapturedEntity = Entity;

        Builder.AddRow(
            FText::FromString(TEXT("From:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sm_PendingTransition>())
                { return FText::FromString(TEXT("--")); }
                const UClass* Class = CapturedEntity.Get<ck::FFragment_Sm_PendingTransition>().Get_PreviousStateClass();
                return FText::FromString(Format_Sm_ClassName(Class));
            },
            CkStyle::Value_Object());

        Builder.AddRow(
            FText::FromString(TEXT("To:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sm_PendingTransition>())
                { return FText::FromString(TEXT("--")); }
                const UClass* Class = CapturedEntity.Get<ck::FFragment_Sm_PendingTransition>().Get_TargetStateClass();
                return FText::FromString(Format_Sm_ClassName(Class));
            },
            CkStyle::Value_Object());
    }

    // ---- Debug history ----
    if (Entity.Has<ck::FFragment_Sm_Debug>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Debug")));

        const auto& Debug       = Entity.Get<ck::FFragment_Sm_Debug>();
        const auto  RunCounter  = Debug.Get_RunCounter();
        const auto  EnteredAt   = Debug.Get_CurrentStateEnteredAtRealTime();

        Builder.AddRow(
            FText::FromString(TEXT("Run #:")),
            [RunCounter](const FCk_Handle&)
            { return FText::FromString(ck::Format_UE(TEXT("{}"), RunCounter)); },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("State Entered At:")),
            [EnteredAt](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f s"), EnteredAt)); },
            CkStyle::Value_Numeric());

        const auto& History    = Debug.Get_History();
        const auto  HistoryNum = History.Num();

        Builder.AddHeader(FText::FromString(ck::Format_UE(TEXT("History ({})"), HistoryNum)));

        if (History.IsEmpty())
        {
            Builder.AddRow(
                FText::FromString(TEXT("(empty)")),
                [](const FCk_Handle&) { return FText::FromString(TEXT("--")); },
                CkStyle::TextMute());
        }
        else
        {
            // Timeline first: the whole run's transitions positioned on their real-time axis, which
            // the numbered rows below cannot show (they are evenly spaced regardless of when the
            // transition happened — a burst of five transitions in one frame looks the same as five
            // spread over a minute). Markers are informational (no SelectionId), tooltip carries the
            // full From -> To plus frame number and the conditions that fired.
            //
            // SNAPSHOT, like every AddChipsRow/AddTimelineRow: composed once per (entity x inspector)
            // build. The panel deliberately does not rebuild from Tick
            // (CkDebuggerPanel_Inspector.cpp "POLICY"), so new transitions appear on the next
            // re-selection / filter keystroke. The numbered rows below have always had exactly the
            // same staleness — their text is captured by value at build time too.
            auto Content = FCkInspector_TimelineContent{};
            Content.TimeMin = History[0].RealTimeSeconds;
            Content.TimeMax = History[HistoryNum - 1].RealTimeSeconds;
            Content.Events.Reserve(HistoryNum);

            for (auto Index = int32{0}; Index < HistoryNum; ++Index)
            {
                const auto& Entry = History[Index];

                auto Tooltip = FString::Printf(TEXT("[%d] %s → %s  (frame %llu, %.2f s)"),
                    Index,
                    *Format_Sm_StateName(Entry.FromStateName),
                    *Format_Sm_StateName(Entry.ToStateName),
                    Entry.FrameNumber,
                    Entry.RealTimeSeconds);

                if (NOT Entry.TransitionConditionNames.IsEmpty())
                {
                    Tooltip.Append(TEXT("\n"));
                    Tooltip.Append(FString::Join(Entry.TransitionConditionNames, TEXT(", ")));
                }

                auto Event = FCkDebug_TimelineEvent{};
                Event.LaneIndex   = 0;
                Event.TimeSeconds = Entry.RealTimeSeconds;
                Event.Shape       = ECkDebug_TimelineMarker::Square;
                Event.Color       = CkStyle::Accent();
                Event.Tooltip     = MoveTemp(Tooltip);

                Content.Events.Add(MoveTemp(Event));
            }

            Builder.AddTimelineRow(
                FText::FromString(TEXT("Timeline:")),
                TArray<FString>{ FString(TEXT("Transitions")) },
                Content,
                SmTimelineHeight);

            constexpr auto MaxEntriesToShow = int32{ 8 };
            const auto     StartIndex       = FMath::Max(0, HistoryNum - MaxEntriesToShow);

            for (auto Index = StartIndex; Index < HistoryNum; ++Index)
            {
                const auto& Entry      = History[Index];
                const auto  FromName   = Format_Sm_StateName(Entry.FromStateName);
                const auto  ToName     = Format_Sm_StateName(Entry.ToStateName);
                const auto  EntryLabel = FString::Printf(TEXT("[%d]"), Index);
                const auto  EntryText  = FString::Printf(TEXT("%s → %s"), *FromName, *ToName);

                Builder.AddRow(
                    FText::FromString(EntryLabel),
                    [EntryText](const FCk_Handle&) { return FText::FromString(EntryText); },
                    CkStyle::Value_Object());
            }
        }
    }

    // ---- State entity ----
    if (Entity.Has<ck::FFragment_SmState_Params>())
    {
        Builder.AddHeader(FText::FromString(TEXT("State")));

        const auto& Params            = Entity.Get<ck::FFragment_SmState_Params>();
        const UClass* ResolvedClass     = Params.Get_ResolvedScriptClass();
        const UClass* RequestedClass    = Params.Get_RequestedScriptClass();
        const auto  ResolvedName      = Format_Sm_ClassName(ResolvedClass);
        const auto  RequestedName     = Format_Sm_ClassName(RequestedClass);

        Builder.AddRow(
            FText::FromString(TEXT("Class:")),
            [ResolvedName](const FCk_Handle&) { return FText::FromString(ResolvedName); },
            CkStyle::Value_Object());

        if (ResolvedClass != RequestedClass)
        {
            Builder.AddRow(
                FText::FromString(TEXT("Requested:")),
                [RequestedName](const FCk_Handle&) { return FText::FromString(RequestedName); },
                CkStyle::Value_Object());
        }

        if (Entity.Has<ck::FFragment_SmState_Hierarchy>())
        {
            const auto& Hierarchy = Entity.Get<ck::FFragment_SmState_Hierarchy>().Get_Hierarchy();
            if (NOT Hierarchy.IsEmpty())
            {
                auto HierarchyStr = FString();
                for (auto HIdx = int32{0}; HIdx < Hierarchy.Num(); ++HIdx)
                {
                    if (HIdx > 0) { HierarchyStr.Append(TEXT(" / ")); }
                    HierarchyStr.Append(Hierarchy[HIdx].IsValid()
                        ? Hierarchy[HIdx].GetTagName().ToString()
                        : FString(TEXT("(invalid)")));
                }
                Builder.AddRow(
                    FText::FromString(TEXT("Hierarchy:")),
                    [HierarchyStr](const FCk_Handle&) { return FText::FromString(HierarchyStr); },
                    CkStyle::Value_Tag());
            }
        }
    }

    // ---- Task entity ----
    if (Entity.Has_Any<ck::FFragment_SmTask_Current, ck::FFragment_SmTask_Params>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Task")));

        if (Entity.Has<ck::FFragment_SmTask_Params>())
        {
            const UClass* ScriptClass = Entity.Get<ck::FFragment_SmTask_Params>().Get_ScriptClass();
            const auto  ClassName   = Format_Sm_ClassName(ScriptClass);

            Builder.AddRow(
                FText::FromString(TEXT("Class:")),
                [ClassName](const FCk_Handle&) { return FText::FromString(ClassName); },
                CkStyle::Value_Object());
        }

        if (Entity.Has<ck::FFragment_SmTask_Current>())
        {
            const auto CapturedEntity = Entity;
            Builder.AddStatusPillRow(
                FText::FromString(TEXT("Result:")),
                TAttribute<FText>::CreateLambda([CapturedEntity]()
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmTask_Current>())
                    { return FText::FromString(TEXT("--")); }
                    const auto Result = CapturedEntity.Get<ck::FFragment_SmTask_Current>().Get_LastResult();
                    return FText::FromString(ck::Format_UE(TEXT("{}"), Result));
                }),
                TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmTask_Current>())
                    { return ECk_Tone::Neutral; }
                    return Format_TaskResult_Tone(CapturedEntity.Get<ck::FFragment_SmTask_Current>().Get_LastResult());
                }));
        }

        if (Entity.Has<ck::FFragment_SmTask_SubStateMachine>())
        {
            const auto CapturedEntity = Entity;

            // The sub-SM is an entity, so it gets the entity pill rather than a formatted string —
            // clicking it navigates. EntityRef renders "None" on an invalid handle by itself.
            const auto Get_SubSm = [CapturedEntity]() -> FCk_Handle
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmTask_SubStateMachine>())
                { return FCk_Handle{}; }
                return CapturedEntity.Get<ck::FFragment_SmTask_SubStateMachine>().Get_SubStateMachineHandle();
            };

            Builder.AddWidgetRow(
                FText::FromString(TEXT("Sub SM:")),
                SNew(SCkDebug_EntityRef)
                    .Entity_Lambda(Get_SubSm)
                    .ShowName(true),
                [Get_SubSm]()
                {
                    const auto SubSm = Get_SubSm();
                    return ck::IsValid(SubSm) ? ck::Format_UE(TEXT("{}"), SubSm) : FString{};
                });
        }
    }

    // ---- Transition entity ----
    if (Entity.Has_Any<ck::FFragment_SmTransition_Current, ck::FFragment_SmTransition_Params>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Transition")));

        if (Entity.Has<ck::FFragment_SmTransition_Params>())
        {
            const UClass* TargetClass = Entity.Get<ck::FFragment_SmTransition_Params>().Get_TargetStateClass();
            const auto  ClassName   = Format_Sm_ClassName(TargetClass);
            Builder.AddRow(
                FText::FromString(TEXT("Target:")),
                [ClassName](const FCk_Handle&) { return FText::FromString(ClassName); },
                CkStyle::Value_Object());
        }

        if (Entity.Has<ck::FFragment_SmTransition_Current>())
        {
            const auto CapturedEntity = Entity;
            Builder.AddStatusPillRow(
                FText::FromString(TEXT("Result:")),
                TAttribute<FText>::CreateLambda([CapturedEntity]()
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmTransition_Current>())
                    { return FText::FromString(TEXT("--")); }
                    const auto Result = CapturedEntity.Get<ck::FFragment_SmTransition_Current>().Get_Result();
                    return FText::FromString(ck::Format_UE(TEXT("{}"), Result));
                }),
                TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmTransition_Current>())
                    { return ECk_Tone::Neutral; }
                    return Format_TransitionResult_Tone(CapturedEntity.Get<ck::FFragment_SmTransition_Current>().Get_Result());
                }));
        }
    }

    // ---- Condition entity ----
    if (Entity.Has_Any<ck::FFragment_SmCondition_Current, ck::FFragment_SmCondition_Params>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Condition")));

        if (Entity.Has<ck::FFragment_SmCondition_Params>())
        {
            const UClass* ScriptClass = Entity.Get<ck::FFragment_SmCondition_Params>().Get_ScriptClass();
            const auto  ClassName   = Format_Sm_ClassName(ScriptClass);
            Builder.AddRow(
                FText::FromString(TEXT("Class:")),
                [ClassName](const FCk_Handle&) { return FText::FromString(ClassName); },
                CkStyle::Value_Object());
        }

        if (Entity.Has<ck::FFragment_SmCondition_Current>())
        {
            const auto CapturedEntity = Entity;
            Builder.AddStatusPillRow(
                FText::FromString(TEXT("Result:")),
                TAttribute<FText>::CreateLambda([CapturedEntity]()
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmCondition_Current>())
                    { return FText::FromString(TEXT("--")); }
                    const auto Result = CapturedEntity.Get<ck::FFragment_SmCondition_Current>().Get_Result();
                    return FText::FromString(ck::Format_UE(TEXT("{}"), Result));
                }),
                TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmCondition_Current>())
                    { return ECk_Tone::Neutral; }
                    return Format_ConditionResult_Tone(CapturedEntity.Get<ck::FFragment_SmCondition_Current>().Get_Result());
                }));
        }
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_StateMachine::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // The row lambdas re-read FFragment_Sm_Debug every frame while this section is
    // visible — keep signalling demand so the poll processor stays on.
    UCk_Utils_StateMachineDebug_UE::NotifyDebugDataConsumed();
}

// =====================================================================================================================
