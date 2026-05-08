#include "CkInspector_StateMachine.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment_Data.h"
#include "CkStateMachine/State/CkSmState_Fragment.h"
#include "CkStateMachine/Task/CkSmTask_Fragment.h"
#include "CkStateMachine/Task/EntityScripts/CkSmTask_EntityScript.h"
#include "CkStateMachine/Transition/CkSmTransition_Fragment.h"
#include "CkStateMachine/Condition/CkSmCondition_Fragment.h"
#include "CkStateMachine/Condition/EntityScripts/CkSmCondition_EntityScript.h"
#include "CkStateMachine/Debug/CkStateMachine_Debug_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_StateMachine)

// =====================================================================================================================

namespace
{
    auto Format_RunStatus_Color(ECk_SmRunStatus InStatus) -> FLinearColor
    {
        switch (InStatus)
        {
            case ECk_SmRunStatus::Running: return CkDebugStyle::Status_Active();
            case ECk_SmRunStatus::Paused:  return CkDebugStyle::Warn();
            case ECk_SmRunStatus::Stopped:
            default:                       return CkDebugStyle::TextMute();
        }
    }

    auto Format_TaskResult_Color(ECk_SmTaskResult InResult) -> FLinearColor
    {
        switch (InResult)
        {
            case ECk_SmTaskResult::Running:   return CkDebugStyle::Status_Active();
            case ECk_SmTaskResult::Succeeded: return CkDebugStyle::Ok();
            case ECk_SmTaskResult::Failed:
            default:                          return CkDebugStyle::Err();
        }
    }

    auto Format_ConditionResult_Color(ECk_SmConditionResult InResult) -> FLinearColor
    {
        switch (InResult)
        {
            case ECk_SmConditionResult::Pass:        return CkDebugStyle::Ok();
            case ECk_SmConditionResult::Fail:        return CkDebugStyle::Err();
            case ECk_SmConditionResult::Undetermined:
            default:                                 return CkDebugStyle::TextMute();
        }
    }

    auto Format_TransitionResult_Color(ECk_SmTransitionResult InResult) -> FLinearColor
    {
        switch (InResult)
        {
            case ECk_SmTransitionResult::Pass:        return CkDebugStyle::Ok();
            case ECk_SmTransitionResult::Fail:        return CkDebugStyle::Err();
            case ECk_SmTransitionResult::Undetermined:
            default:                                  return CkDebugStyle::TextMute();
        }
    }

    auto Format_Sm_ClassName(const UClass* InClass) -> FString
    {
        return InClass != nullptr ? InClass->GetName() : FString(TEXT("(None)"));
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
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- State Machine root entity ----
    if (Entity.Has<ck::FFragment_Sm_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("State Machine")));

        const auto CapturedEntity = Entity;

        Builder.AddConditionalRow(
            FText::FromString(TEXT("Status:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sm_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Status = CapturedEntity.Get<ck::FFragment_Sm_Current>().Get_RunStatus();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Status));
            },
            [CapturedEntity](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sm_Current>())
                { return CkDebugStyle::None(); }
                return Format_RunStatus_Color(CapturedEntity.Get<ck::FFragment_Sm_Current>().Get_RunStatus());
            });

        Builder.AddRow(
            FText::FromString(TEXT("Current State:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sm_Current>())
                { return FText::FromString(TEXT("--")); }
                const UClass* StateClass = CapturedEntity.Get<ck::FFragment_Sm_Current>().Get_CurrentStateClass();
                return FText::FromString(Format_Sm_ClassName(StateClass));
            },
            CkDebugStyle::Value_Object());
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
            CkDebugStyle::Value_Object());

        Builder.AddRow(
            FText::FromString(TEXT("To:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sm_PendingTransition>())
                { return FText::FromString(TEXT("--")); }
                const UClass* Class = CapturedEntity.Get<ck::FFragment_Sm_PendingTransition>().Get_TargetStateClass();
                return FText::FromString(Format_Sm_ClassName(Class));
            },
            CkDebugStyle::Value_Object());
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
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("State Entered At:")),
            [EnteredAt](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.2f s"), EnteredAt)); },
            CkDebugStyle::Value_Numeric());

        const auto& History    = Debug.Get_History();
        const auto  HistoryNum = History.Num();

        Builder.AddHeader(FText::FromString(ck::Format_UE(TEXT("History ({})"), HistoryNum)));

        if (History.IsEmpty())
        {
            Builder.AddRow(
                FText::FromString(TEXT("(empty)")),
                [](const FCk_Handle&) { return FText::FromString(TEXT("--")); },
                CkDebugStyle::TextMute());
        }
        else
        {
            constexpr auto MaxEntriesToShow = int32{ 8 };
            const auto     StartIndex       = FMath::Max(0, HistoryNum - MaxEntriesToShow);

            for (auto Index = StartIndex; Index < HistoryNum; ++Index)
            {
                const auto& Entry      = History[Index];
                const auto  FromName   = Entry.FromStateName.IsEmpty() ? FString(TEXT("(None)")) : Entry.FromStateName;
                const auto  ToName     = Entry.ToStateName.IsEmpty() ? FString(TEXT("(None)")) : Entry.ToStateName;
                const auto  EntryLabel = FString::Printf(TEXT("[%d]"), Index);
                const auto  EntryText  = FString::Printf(TEXT("%s → %s"), *FromName, *ToName);

                Builder.AddRow(
                    FText::FromString(EntryLabel),
                    [EntryText](const FCk_Handle&) { return FText::FromString(EntryText); },
                    CkDebugStyle::Value_Object());
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
            CkDebugStyle::Value_Object());

        if (ResolvedClass != RequestedClass)
        {
            Builder.AddRow(
                FText::FromString(TEXT("Requested:")),
                [RequestedName](const FCk_Handle&) { return FText::FromString(RequestedName); },
                CkDebugStyle::Value_Object());
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
                    CkDebugStyle::Value_Tag());
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
                CkDebugStyle::Value_Object());
        }

        if (Entity.Has<ck::FFragment_SmTask_Current>())
        {
            const auto CapturedEntity = Entity;
            Builder.AddConditionalRow(
                FText::FromString(TEXT("Result:")),
                [CapturedEntity](const FCk_Handle&)
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmTask_Current>())
                    { return FText::FromString(TEXT("--")); }
                    const auto Result = CapturedEntity.Get<ck::FFragment_SmTask_Current>().Get_LastResult();
                    return FText::FromString(ck::Format_UE(TEXT("{}"), Result));
                },
                [CapturedEntity](const FCk_Handle&) -> FLinearColor
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmTask_Current>())
                    { return CkDebugStyle::None(); }
                    return Format_TaskResult_Color(CapturedEntity.Get<ck::FFragment_SmTask_Current>().Get_LastResult());
                });
        }

        if (Entity.Has<ck::FFragment_SmTask_SubStateMachine>())
        {
            const auto CapturedEntity = Entity;
            Builder.AddRow(
                FText::FromString(TEXT("Sub SM:")),
                [CapturedEntity](const FCk_Handle&)
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmTask_SubStateMachine>())
                    { return FText::FromString(TEXT("--")); }
                    const auto SubSm = CapturedEntity.Get<ck::FFragment_SmTask_SubStateMachine>().Get_SubStateMachineHandle();
                    return FText::FromString(ck::IsValid(SubSm)
                        ? ck::Format_UE(TEXT("[{}]"), SubSm)
                        : FString(TEXT("(None)")));
                },
                CkDebugStyle::Value_Handle());
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
                CkDebugStyle::Value_Object());
        }

        if (Entity.Has<ck::FFragment_SmTransition_Current>())
        {
            const auto CapturedEntity = Entity;
            Builder.AddConditionalRow(
                FText::FromString(TEXT("Result:")),
                [CapturedEntity](const FCk_Handle&)
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmTransition_Current>())
                    { return FText::FromString(TEXT("--")); }
                    const auto Result = CapturedEntity.Get<ck::FFragment_SmTransition_Current>().Get_Result();
                    return FText::FromString(ck::Format_UE(TEXT("{}"), Result));
                },
                [CapturedEntity](const FCk_Handle&) -> FLinearColor
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmTransition_Current>())
                    { return CkDebugStyle::None(); }
                    return Format_TransitionResult_Color(CapturedEntity.Get<ck::FFragment_SmTransition_Current>().Get_Result());
                });
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
                CkDebugStyle::Value_Object());
        }

        if (Entity.Has<ck::FFragment_SmCondition_Current>())
        {
            const auto CapturedEntity = Entity;
            Builder.AddConditionalRow(
                FText::FromString(TEXT("Result:")),
                [CapturedEntity](const FCk_Handle&)
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmCondition_Current>())
                    { return FText::FromString(TEXT("--")); }
                    const auto Result = CapturedEntity.Get<ck::FFragment_SmCondition_Current>().Get_Result();
                    return FText::FromString(ck::Format_UE(TEXT("{}"), Result));
                },
                [CapturedEntity](const FCk_Handle&) -> FLinearColor
                {
                    if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_SmCondition_Current>())
                    { return CkDebugStyle::None(); }
                    return Format_ConditionResult_Color(CapturedEntity.Get<ck::FFragment_SmCondition_Current>().Get_Result());
                });
        }
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_StateMachine::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
