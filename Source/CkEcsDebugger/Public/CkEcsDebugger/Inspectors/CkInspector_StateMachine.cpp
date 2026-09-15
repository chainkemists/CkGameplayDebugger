#include "CkInspector_StateMachine.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

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
#include "CkDebuggerCommon/Widgets/SCkDebug_EventTimeline.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"

#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_StateMachine)

// =====================================================================================================================

namespace ck_inspector_state_machine
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

    auto IsDestroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity) || InEntity.Has_Any<
            ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
            ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
            ck::FTag_DestroyEntity_Finalize>();
    }

    auto HasInspectableFragments(const FCk_Handle& InEntity) -> bool
    {
        return NOT IsDestroying(InEntity) && InEntity.Has_Any<
            ck::FFragment_Sm_Current, ck::FFragment_Sm_Debug, ck::FFragment_SmState_Params,
            ck::FFragment_SmTask_Current, ck::FFragment_SmTask_Params,
            ck::FFragment_SmTransition_Current, ck::FFragment_SmTransition_Params,
            ck::FFragment_SmCondition_Current, ck::FFragment_SmCondition_Params>();
    }

    auto RouteKey(const FCk_Handle& InEntity) -> FString
    {
        if (ck::Is_NOT_Valid(InEntity))
        { return {}; }
        const auto& Entity = InEntity.Get_Entity();
        return ck::Format_UE(TEXT("{}:{}"), static_cast<uint32>(Entity.Get_ID()),
            static_cast<uint32>(Entity.Get_VersionNumber()));
    }

    auto TryGetStateMachine(const FCk_Handle& InEntity, FCk_Handle_StateMachine& OutStateMachine) -> bool
    {
        OutStateMachine = {};
        if (IsDestroying(InEntity) || NOT InEntity.Has<ck::FFragment_Sm_Current>()
            || NOT InEntity.Has<ck::FFragment_Sm_Params>())
        { return false; }
        if (ck::Is_NOT_Valid(InEntity.Get<ck::FFragment_Sm_Params>().Get_InitialStateClass()))
        { return false; }
        auto Mutable = InEntity;
        OutStateMachine = UCk_Utils_StateMachine_UE::Cast(Mutable);
        return ck::IsValid(OutStateMachine);
    }
}

// =====================================================================================================================

auto FCkInspector_StateMachine::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("State Machine"));
}

auto FCkInspector_StateMachine::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck_inspector_state_machine::HasInspectableFragments(Entity);
}

// =====================================================================================================================

auto FCkInspector_StateMachine::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    using namespace ck_inspector_state_machine;
    if (IsDestroying(Entity))
    { return SNullWidget::NullWidget; }
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
        auto CapturedSm = FCk_Handle_StateMachine{};

        if (TryGetStateMachine(Entity, CapturedSm))
        {
            const auto Make_SmAction = [CapturedEntity](
                const FString& InLabel,
                const FString& InTooltip,
                auto InRequest) -> FCkInspector_Action
            {
                return FCkInspector_Action
                {
                    FText::FromString(InLabel),
                    FText::FromString(InTooltip),
                    [CapturedEntity, InRequest]()
                    {
                        auto CurrentSm = FCk_Handle_StateMachine{};
                        if (NOT TryGetStateMachine(CapturedEntity, CurrentSm)
                            || NOT ck::DebugRequestGate::Evaluate(
                                CapturedEntity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
                        { return; }
                        InRequest(CurrentSm);
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

auto SCkInspector_StateMachineAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _RouteKey = ck_inspector_state_machine::RouteKey(_Entity);
    _DiffLabels = InArgs._DiffLabels;
    _TimelinePort = SNew(SBox);
    if (Get_IsAvailable() && Capture_History() && Build_AuthoredView())
    {
        ChildSlot[_View->GetRegion(TEXT("main"))];
        return;
    }
    if (_LoadError.IsEmpty())
    { _LoadError = TEXT("State Machine composition is unavailable."); }
    Release();
    ChildSlot[SNullWidget::NullWidget];
}

SCkInspector_StateMachineAuthored::~SCkInspector_StateMachineAuthored()
{
    Release();
}

auto SCkInspector_StateMachineAuthored::Get_IsAvailable() const -> bool
{
    return _Active && ck_inspector_state_machine::HasInspectableFragments(_Entity)
        && NOT _RouteKey.IsEmpty() && _RouteKey == ck_inspector_state_machine::RouteKey(_Entity);
}

auto SCkInspector_StateMachineAuthored::Get_CanRequest() const -> bool
{
    auto StateMachine = FCk_Handle_StateMachine{};
    return Get_IsAvailable() && ck_inspector_state_machine::TryGetStateMachine(_Entity, StateMachine)
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled;
}

auto SCkInspector_StateMachineAuthored::Get_RequestDisabledReason() const -> FString
{
    auto StateMachine = FCk_Handle_StateMachine{};
    if (NOT Get_IsAvailable() || NOT ck_inspector_state_machine::TryGetStateMachine(_Entity, StateMachine))
    { return TEXT("State Machine request target is unavailable."); }
    return ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).Reason.ToString();
}

auto SCkInspector_StateMachineAuthored::Get_SubStateMachine() const -> FCk_Handle
{
    if (NOT Get_IsAvailable() || NOT _Entity.Has<ck::FFragment_SmTask_SubStateMachine>())
    { return {}; }
    const auto SubStateMachine = _Entity.Get<ck::FFragment_SmTask_SubStateMachine>().Get_SubStateMachineHandle();
    return ck_inspector_state_machine::IsDestroying(SubStateMachine)
        ? FCk_Handle{} : FCk_Handle{SubStateMachine};
}

auto SCkInspector_StateMachineAuthored::Get_Text(const FString& InKey) const -> FString
{
    using namespace ck_inspector_state_machine;
    if (NOT Get_IsAvailable())
    { return InKey == TEXT("sub-sm-id") || InKey == TEXT("sub-sm-name") ? FString{} : FString{TEXT("--")}; }
    if (InKey == TEXT("status") && _Entity.Has<ck::FFragment_Sm_Current>())
    { return ck::Format_UE(TEXT("{}"), _Entity.Get<ck::FFragment_Sm_Current>().Get_RunStatus()); }
    if (InKey == TEXT("current-state") && _Entity.Has<ck::FFragment_Sm_Current>())
    { return Format_Sm_ClassName(_Entity.Get<ck::FFragment_Sm_Current>().Get_CurrentStateClass()); }
    if (_Entity.Has<ck::FFragment_Sm_PendingTransition>())
    {
        const auto& Pending = _Entity.Get<ck::FFragment_Sm_PendingTransition>();
        if (InKey == TEXT("pending-from"))
        { return Format_Sm_ClassName(Pending.Get_PreviousStateClass()); }
        if (InKey == TEXT("pending-to"))
        { return Format_Sm_ClassName(Pending.Get_TargetStateClass()); }
    }
    if (Get_Bool(TEXT("debug")))
    {
        if (const auto* Captured = _HistoryText.Find(InKey))
        { return *Captured; }
    }
    if (_Entity.Has<ck::FFragment_SmState_Params>())
    {
        const auto& Params = _Entity.Get<ck::FFragment_SmState_Params>();
        if (InKey == TEXT("state-class"))
        { return Format_Sm_ClassName(Params.Get_ResolvedScriptClass()); }
        if (InKey == TEXT("state-requested"))
        { return Format_Sm_ClassName(Params.Get_RequestedScriptClass()); }
        if (InKey == TEXT("state-hierarchy") && _Entity.Has<ck::FFragment_SmState_Hierarchy>())
        {
            auto Names = TArray<FString>{};
            for (const auto& HierarchyTag : _Entity.Get<ck::FFragment_SmState_Hierarchy>().Get_Hierarchy())
            { Names.Add(HierarchyTag.IsValid() ? HierarchyTag.GetTagName().ToString() : FString{TEXT("(invalid)")}); }
            return FString::Join(Names, TEXT(" / "));
        }
    }
    if (InKey == TEXT("task-class") && _Entity.Has<ck::FFragment_SmTask_Params>())
    { return Format_Sm_ClassName(_Entity.Get<ck::FFragment_SmTask_Params>().Get_ScriptClass()); }
    if (InKey == TEXT("task-result") && _Entity.Has<ck::FFragment_SmTask_Current>())
    { return ck::Format_UE(TEXT("{}"), _Entity.Get<ck::FFragment_SmTask_Current>().Get_LastResult()); }
    if (InKey == TEXT("sub-sm-id") || InKey == TEXT("sub-sm-name"))
    {
        const auto SubStateMachine = Get_SubStateMachine();
        if (ck::Is_NOT_Valid(SubStateMachine))
        { return FString{}; }
        if (InKey == TEXT("sub-sm-id"))
        { return ck::Format_UE(TEXT("{}"), SubStateMachine); }
        const auto Name = UCk_Utils_Handle_UE::Get_DebugName(SubStateMachine);
        return Name.IsNone() ? FString{} : ck::DebugNameClean::Get_CleanName(Name.ToString());
    }
    if (InKey == TEXT("transition-target") && _Entity.Has<ck::FFragment_SmTransition_Params>())
    { return Format_Sm_ClassName(_Entity.Get<ck::FFragment_SmTransition_Params>().Get_TargetStateClass()); }
    if (InKey == TEXT("transition-result") && _Entity.Has<ck::FFragment_SmTransition_Current>())
    { return ck::Format_UE(TEXT("{}"), _Entity.Get<ck::FFragment_SmTransition_Current>().Get_Result()); }
    if (InKey == TEXT("condition-class") && _Entity.Has<ck::FFragment_SmCondition_Params>())
    { return Format_Sm_ClassName(_Entity.Get<ck::FFragment_SmCondition_Params>().Get_ScriptClass()); }
    if (InKey == TEXT("condition-result") && _Entity.Has<ck::FFragment_SmCondition_Current>())
    { return ck::Format_UE(TEXT("{}"), _Entity.Get<ck::FFragment_SmCondition_Current>().Get_Result()); }
    return TEXT("--");
}

auto SCkInspector_StateMachineAuthored::Get_Bool(const FString& InKey) const -> bool
{
    if (NOT Get_IsAvailable())
    { return false; }
    if (InKey == TEXT("root"))
    { return _Entity.Has<ck::FFragment_Sm_Current>(); }
    if (InKey == TEXT("controls-visible"))
    { return Get_Bool(TEXT("root")) && ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection()); }
    if (InKey == TEXT("pending"))
    { return _Entity.Has<ck::FFragment_Sm_PendingTransition>(); }
    if (InKey == TEXT("debug"))
    { return _HistoryCaptured && _Entity.Has<ck::FFragment_Sm_Debug>(); }
    if (InKey == TEXT("history-empty"))
    { return Get_Bool(TEXT("debug")) && _History.IsValid() && _History->GetRecords().IsEmpty(); }
    if (InKey == TEXT("history-populated"))
    { return Get_Bool(TEXT("debug")) && _History.IsValid() && NOT _History->GetRecords().IsEmpty(); }
    if (InKey == TEXT("state"))
    { return _Entity.Has<ck::FFragment_SmState_Params>(); }
    if (InKey == TEXT("state-requested"))
    {
        if (NOT Get_Bool(TEXT("state")))
        { return false; }
        const auto& Params = _Entity.Get<ck::FFragment_SmState_Params>();
        return Params.Get_ResolvedScriptClass() != Params.Get_RequestedScriptClass();
    }
    if (InKey == TEXT("state-hierarchy"))
    {
        return Get_Bool(TEXT("state")) && _Entity.Has<ck::FFragment_SmState_Hierarchy>()
            && NOT _Entity.Get<ck::FFragment_SmState_Hierarchy>().Get_Hierarchy().IsEmpty();
    }
    if (InKey == TEXT("task"))
    { return _Entity.Has_Any<ck::FFragment_SmTask_Params, ck::FFragment_SmTask_Current>(); }
    if (InKey == TEXT("task-class"))
    { return _Entity.Has<ck::FFragment_SmTask_Params>(); }
    if (InKey == TEXT("task-result"))
    { return _Entity.Has<ck::FFragment_SmTask_Current>(); }
    if (InKey == TEXT("sub-sm"))
    { return Get_Bool(TEXT("task")) && _Entity.Has<ck::FFragment_SmTask_SubStateMachine>(); }
    if (InKey == TEXT("transition"))
    { return _Entity.Has_Any<ck::FFragment_SmTransition_Params, ck::FFragment_SmTransition_Current>(); }
    if (InKey == TEXT("transition-target"))
    { return _Entity.Has<ck::FFragment_SmTransition_Params>(); }
    if (InKey == TEXT("transition-result"))
    { return _Entity.Has<ck::FFragment_SmTransition_Current>(); }
    if (InKey == TEXT("condition"))
    { return _Entity.Has_Any<ck::FFragment_SmCondition_Params, ck::FFragment_SmCondition_Current>(); }
    if (InKey == TEXT("condition-class"))
    { return _Entity.Has<ck::FFragment_SmCondition_Params>(); }
    if (InKey == TEXT("condition-result"))
    { return _Entity.Has<ck::FFragment_SmCondition_Current>(); }
    return false;
}

auto SCkInspector_StateMachineAuthored::Get_Tone(const FString& InKey) const -> ECk_Tone
{
    using namespace ck_inspector_state_machine;
    if (NOT Get_IsAvailable())
    { return ECk_Tone::Neutral; }
    if (InKey == TEXT("status") && _Entity.Has<ck::FFragment_Sm_Current>())
    { return Format_RunStatus_Tone(_Entity.Get<ck::FFragment_Sm_Current>().Get_RunStatus()); }
    if (InKey == TEXT("current-state") && _Entity.Has<ck::FFragment_Sm_Current>())
    { return _Entity.Get<ck::FFragment_Sm_Current>().Get_CurrentStateClass() != nullptr ? ECk_Tone::Info : ECk_Tone::Neutral; }
    if (InKey == TEXT("task-result") && _Entity.Has<ck::FFragment_SmTask_Current>())
    { return Format_TaskResult_Tone(_Entity.Get<ck::FFragment_SmTask_Current>().Get_LastResult()); }
    if (InKey == TEXT("transition-result") && _Entity.Has<ck::FFragment_SmTransition_Current>())
    { return Format_TransitionResult_Tone(_Entity.Get<ck::FFragment_SmTransition_Current>().Get_Result()); }
    if (InKey == TEXT("condition-result") && _Entity.Has<ck::FFragment_SmCondition_Current>())
    { return Format_ConditionResult_Tone(_Entity.Get<ck::FFragment_SmCondition_Current>().Get_Result()); }
    return ECk_Tone::Neutral;
}

auto SCkInspector_StateMachineAuthored::Get_DiffColor(const FString& InLabel) const -> FLinearColor
{
    return _Active && _DiffLabels.Contains(InLabel) ? CkStyle::Accent() : CkStyle::TextDim();
}

auto SCkInspector_StateMachineAuthored::Capture_History() -> bool
{
    using namespace ck_inspector_state_machine;
    const auto SchemaResult = FCkUiCollection::TryCreate({
        {TEXT("label"), ECkUiFieldKind::Text}, {TEXT("value"), ECkUiFieldKind::Text},
        {TEXT("diff-color"), ECkUiFieldKind::Color}}, _History);
    if (NOT SchemaResult.Succeeded || NOT _History.IsValid())
    {
        _LoadError = FString::Join(SchemaResult.Errors, TEXT("\n"));
        return false;
    }
    if (NOT _Entity.Has<ck::FFragment_Sm_Debug>())
    { return true; }

    const auto& Debug = _Entity.Get<ck::FFragment_Sm_Debug>();
    const auto& History = Debug.Get_History();
    _HistoryText.Add(TEXT("run"), ck::Format_UE(TEXT("{}"), Debug.Get_RunCounter()));
    _HistoryText.Add(TEXT("entered-at"), ck::Format_UE(TEXT("{:.2f} s"), Debug.Get_CurrentStateEnteredAtRealTime()));
    _HistoryText.Add(TEXT("history-title"), ck::Format_UE(TEXT("History ({})"), History.Num()));

    auto Content = FCkInspector_TimelineContent{};
    auto Records = TArray<FCkUiRecordData>{};
    if (NOT History.IsEmpty())
    {
        Content.TimeMin = History[0].RealTimeSeconds;
        Content.TimeMax = History.Last().RealTimeSeconds;
        for (auto Index = int32{0}; Index < History.Num(); ++Index)
        {
            const auto& Entry = History[Index];
            const auto FromName = Format_Sm_StateName(Entry.FromStateName);
            const auto ToName = Format_Sm_StateName(Entry.ToStateName);
            auto Event = FCkDebug_TimelineEvent{};
            Event.TimeSeconds = Entry.RealTimeSeconds;
            Event.Shape = ECkDebug_TimelineMarker::Square;
            Event.Color = CkStyle::Accent();
            Event.Tooltip = ck::Format_UE(TEXT("[{}] {} → {}  (frame {}, {:.2f} s)"),
                Index, FromName, ToName, Entry.FrameNumber, Entry.RealTimeSeconds);
            if (NOT Entry.TransitionConditionNames.IsEmpty())
            { Event.Tooltip += TEXT("\n") + FString::Join(Entry.TransitionConditionNames, TEXT(", ")); }
            Content.Events.Add(MoveTemp(Event));
            if (Index < FMath::Max(0, History.Num() - 8))
            { continue; }
            const auto Label = ck::Format_UE(TEXT("[{}]"), Index);
            auto Record = FCkUiRecordData{};
            Record.Key = ck::Format_UE(TEXT("{}:history:{}:{}"), _RouteKey, Debug.Get_RunCounter(), Index);
            auto LabelField = FCkUiFieldValue{};
            LabelField.Text = FText::FromString(Label);
            Record.Fields.Add(TEXT("label"), MoveTemp(LabelField));
            auto ValueField = FCkUiFieldValue{};
            ValueField.Text = FText::FromString(ck::Format_UE(TEXT("{} → {}"), FromName, ToName));
            Record.Fields.Add(TEXT("value"), MoveTemp(ValueField));
            auto DiffField = FCkUiFieldValue{};
            DiffField.Kind = ECkUiFieldKind::Color;
            DiffField.Color = Get_DiffColor(Label);
            Record.Fields.Add(TEXT("diff-color"), MoveTemp(DiffField));
            Records.Add(MoveTemp(Record));
        }
    }
    const auto RecordsResult = _History->TrySetRecords(MoveTemp(Records));
    if (NOT RecordsResult.Succeeded)
    {
        _LoadError = FString::Join(RecordsResult.Errors, TEXT("\n"));
        return false;
    }
    // History and its timeline are one build-time snapshot; the inspector host does not rebuild on Tick.
    const auto Timeline = SNew(SCkDebug_EventTimeline)
        .LaneLabels(TArray<FString>{FString{TEXT("Transitions")}})
        .DesiredHeight(SmTimelineHeight);
    Timeline->Set_Content(Content.TimeMin, Content.TimeMax, Content.Events, Content.Spans);
    _TimelinePort->SetContent(Timeline);
    _HistoryCaptured = true;
    return true;
}

auto SCkInspector_StateMachineAuthored::Build_AuthoredView() -> bool
{
    auto Registry = TSharedPtr<const FCkUiWidgetRegistrySnapshot>{};
    const auto RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        _LoadError = NOT Plugin.IsValid() ? TEXT("CkDebugger plugin is unavailable.")
            : FString::Join(RegistryResult.Errors, TEXT("\n"));
        return false;
    }
    const auto WeakWidget = TWeakPtr<SCkInspector_StateMachineAuthored>{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    });
    for (const auto* Key : {
        TEXT("root"), TEXT("controls-visible"), TEXT("pending"), TEXT("debug"), TEXT("history-empty"), TEXT("history-populated"),
        TEXT("state"), TEXT("state-requested"), TEXT("state-hierarchy"), TEXT("task"), TEXT("task-class"),
        TEXT("task-result"), TEXT("sub-sm"), TEXT("transition"), TEXT("transition-target"),
        TEXT("transition-result"), TEXT("condition"), TEXT("condition-class"), TEXT("condition-result")})
    {
        const auto Name = FString{Key};
        Data.Visibility.Add(TEXT("sm-") + Name, TAttribute<bool>::CreateLambda([WeakWidget, Name]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() && Widget->Get_Bool(Name);
        }));
    }
    Data.Visibility.Add(TEXT("sm-unavailable"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return NOT Widget.IsValid() || NOT Widget->Get_IsAvailable();
    }));
    Data.Visibility.Add(TEXT("sm-can-request"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_CanRequest();
    }));
    for (const auto* Key : {
        TEXT("status"), TEXT("current-state"), TEXT("pending-from"), TEXT("pending-to"), TEXT("run"),
        TEXT("entered-at"), TEXT("history-title"), TEXT("state-class"), TEXT("state-requested"),
        TEXT("state-hierarchy"), TEXT("task-class"), TEXT("task-result"), TEXT("sub-sm-id"),
        TEXT("sub-sm-name"), TEXT("transition-target"), TEXT("transition-result"), TEXT("condition-class"),
        TEXT("condition-result")})
    {
        const auto Name = FString{Key};
        Data.Text.Add(TEXT("sm-") + Name, TAttribute<FText>::CreateLambda([WeakWidget, Name]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() ? FText::FromString(Widget->Get_Text(Name)) : FText::GetEmpty();
        }));
    }
    Data.Text.Add(TEXT("sm-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty();
    }));
    for (const auto* Key : {TEXT("status"), TEXT("current-state"), TEXT("task-result"), TEXT("transition-result"), TEXT("condition-result")})
    {
        const auto Name = FString{Key};
        Data.Color.Add(TEXT("sm-") + Name + TEXT("-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakWidget, Name]()
        {
            const auto Widget = WeakWidget.Pin();
            return CkStyle::GetToneColor(Widget.IsValid() ? Widget->Get_Tone(Name) : ECk_Tone::Neutral);
        }));
        Data.Color.Add(TEXT("sm-") + Name + TEXT("-background"), TAttribute<FLinearColor>::CreateLambda([WeakWidget, Name]()
        {
            const auto Widget = WeakWidget.Pin();
            return CkStyle::GetToneDimColor(Widget.IsValid() ? Widget->Get_Tone(Name) : ECk_Tone::Neutral);
        }));
    }
    const auto DiffBindings = TMap<FString, FString>{
        {TEXT("status"), TEXT("Status:")}, {TEXT("current-state"), TEXT("Current State:")},
        {TEXT("control"), TEXT("Control:")}, {TEXT("pending-from"), TEXT("From:")},
        {TEXT("pending-to"), TEXT("To:")}, {TEXT("run"), TEXT("Run #:")},
        {TEXT("entered-at"), TEXT("State Entered At:")}, {TEXT("timeline"), TEXT("Timeline:")},
        {TEXT("empty"), TEXT("(empty)")}, {TEXT("class"), TEXT("Class:")},
        {TEXT("requested"), TEXT("Requested:")}, {TEXT("hierarchy"), TEXT("Hierarchy:")},
        {TEXT("result"), TEXT("Result:")}, {TEXT("sub-sm"), TEXT("Sub SM:")}, {TEXT("target"), TEXT("Target:")}};
    for (const auto& Pair : DiffBindings)
    {
        const auto Label = Pair.Value;
        Data.Color.Add(TEXT("sm-") + Pair.Key + TEXT("-diff"), TAttribute<FLinearColor>::CreateLambda([WeakWidget, Label]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() ? Widget->Get_DiffColor(Label) : CkStyle::TextDim();
        }));
    }
    Data.Color.Add(TEXT("sm-object-color"), CkStyle::Value_Object());
    Data.Color.Add(TEXT("sm-numeric-color"), CkStyle::Value_Numeric());
    Data.Color.Add(TEXT("sm-tag-color"), CkStyle::Value_Tag());
    Data.Collections.Add(TEXT("sm-history"), _History);
    auto Actions = FCkUiView::FActions{};
    const auto RequestBindings = TMap<FString, ERequest>{
        {TEXT("Start"), ERequest::Start}, {TEXT("Stop"), ERequest::Stop},
        {TEXT("Pause"), ERequest::Pause}, {TEXT("Resume"), ERequest::Resume}};
    for (const auto& Pair : RequestBindings)
    {
        const auto Key = TEXT("sm-") + Pair.Key.ToLower();
        Data.Text.Add(Key + TEXT("-label"), FText::FromString(Pair.Key));
        Data.Text.Add(Key + TEXT("-tooltip"), FText::FromString(TEXT("UCk_Utils_StateMachine_UE::Request_") + Pair.Key));
        Actions.Add(Key, FSimpleDelegate::CreateLambda([WeakWidget, Route = _RouteKey, RequestKind = Pair.Value]()
        {
            if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
            { Widget->Request(Route, RequestKind); }
        }));
    }
    Actions.Add(TEXT("sm-sub-sm-navigate"), FSimpleDelegate::CreateLambda([WeakWidget]()
    {
        if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
        { Widget->Navigate_SubStateMachine(); }
    }));
    auto Native = FCkUiView::FNativeBindings{};
    Native.Add(TEXT("sm-timeline-port"), _TimelinePort.ToSharedRef());
    const auto Candidate = FCkUiView::Create(MoveTemp(Native), MoveTemp(Actions), {},
        FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const auto ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("EcsInspectorStateMachine.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorStateMachine.ui.css")));
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

auto SCkInspector_StateMachineAuthored::Navigate_SubStateMachine() -> void
{
    const auto Current = Get_SubStateMachine();
    if (ck::IsValid(Current))
    { ck::DebugNav::Goto_Entity(Current); }
}

auto SCkInspector_StateMachineAuthored::Request(const FString& InRouteKey, ERequest InRequest) -> void
{
    auto StateMachine = FCk_Handle_StateMachine{};
    if (NOT Get_IsAvailable() || InRouteKey != _RouteKey
        || InRouteKey != ck_inspector_state_machine::RouteKey(_Entity)
        || NOT ck_inspector_state_machine::TryGetStateMachine(_Entity, StateMachine)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled)
    { return; }
    switch (InRequest)
    {
        case ERequest::Start: UCk_Utils_StateMachine_UE::Request_Start(StateMachine, {}); break;
        case ERequest::Stop: UCk_Utils_StateMachine_UE::Request_Stop(StateMachine, {}); break;
        case ERequest::Pause: UCk_Utils_StateMachine_UE::Request_Pause(StateMachine, {}); break;
        case ERequest::Resume: UCk_Utils_StateMachine_UE::Request_Resume(StateMachine, {}); break;
    }
}

auto SCkInspector_StateMachineAuthored::Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active)
    { return; }
    if (NOT Get_IsAvailable())
    { Release(); return; }
    if (NOT _View.IsValid())
    { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else
    { _LoadError.Reset(); }
}

auto SCkInspector_StateMachineAuthored::Release() -> void
{
    if (NOT _Active)
    { return; }
    _Active = false;
    if (_TimelinePort.IsValid())
    { _TimelinePort->SetContent(SNullWidget::NullWidget); }
    _Entity = {};
    _RouteKey.Reset();
    _DiffLabels.Reset();
    _HistoryText.Reset();
    _History.Reset();
    _TimelinePort.Reset();
    _View.Reset();
    _HistoryCaptured = false;
    _Mounted = false;
    ChildSlot[SNullWidget::NullWidget];
}

FCkInspector_StateMachine::~FCkInspector_StateMachine()
{
    OnDeactivated();
}

auto FCkInspector_StateMachine::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const auto Native = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    { return Native; }
    auto DiffLabels = TSet<FString>{};
    for (const auto* Label : {TEXT("Status:"), TEXT("Current State:"), TEXT("Control:"), TEXT("From:"),
        TEXT("To:"), TEXT("Run #:"), TEXT("State Entered At:"), TEXT("Timeline:"), TEXT("(empty)"),
        TEXT("Class:"), TEXT("Requested:"), TEXT("Hierarchy:"), TEXT("Result:"), TEXT("Sub SM:"), TEXT("Target:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
        { DiffLabels.Add(Label); }
    }
    if (CanInspect(Entity) && Entity.Has<ck::FFragment_Sm_Debug>())
    {
        const auto Count = Entity.Get<ck::FFragment_Sm_Debug>().Get_History().Num();
        for (auto Index = FMath::Max(0, Count - 8); Index < Count; ++Index)
        {
            const auto Label = ck::Format_UE(TEXT("[{}]"), Index);
            if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
            { DiffLabels.Add(Label); }
        }
    }
    const auto Authored = SNew(SCkInspector_StateMachineAuthored).Entity(Entity).DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return Native;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_StateMachine::OnDeactivated() -> void
{
    for (const auto& WeakWidget : _AuthoredInstances)
    {
        if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
        { Widget->Release(); }
    }
    _AuthoredInstances.Reset();
}

auto FCkInspector_StateMachine::Wants_TickWhenNotInspectable(const FCk_Handle& Entity) const -> bool
{
    return _AuthoredInstances.ContainsByPredicate([&Entity](const auto& WeakWidget)
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert() && Widget->Is_ForEntity(Entity);
    });
}

auto FCkInspector_StateMachine::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    for (const auto& WeakWidget : _AuthoredInstances)
    {
        if (const auto Widget = WeakWidget.Pin(); Widget.IsValid() && NOT Widget->Get_IsAvailable())
        { Widget->Release(); }
    }
    _AuthoredInstances.RemoveAll([](const auto& WeakWidget)
    {
        const auto Widget = WeakWidget.Pin();
        return NOT Widget.IsValid() || Widget->Is_Inert();
    });
    _LastAuthoredLoadError.Reset();
    for (const auto& WeakWidget : _AuthoredInstances)
    {
        if (const auto Widget = WeakWidget.Pin(); Widget.IsValid() && NOT Widget->Get_LoadError().IsEmpty())
        {
            _LastAuthoredLoadError = Widget->Get_LoadError();
            break;
        }
    }
    if (CanInspect(Entity))
    { UCk_Utils_StateMachineDebug_UE::NotifyDebugDataConsumed(); }
}

// =====================================================================================================================
