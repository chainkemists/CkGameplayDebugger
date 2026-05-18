#include "CkInspector_Goap.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkGoap/CkGoap_Fragment.h"
#include "CkGoap/Algorithm/CkGoap_Types.h"
#include "CkGoap/Algorithm/CkGoap_WorldState.h"
#include "CkGoap/EntityScripts/CkGoapAction_EntityScript.h"
#include "CkGoap/EntityScripts/CkGoapGoal_EntityScript.h"
#include "CkGoap/WorldState/CkGoap_WorldState_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Goap)

// =====================================================================================================================

namespace
{
    auto Format_PlanStatus_Color(ECk_GoapPlanStatus InStatus) -> FLinearColor
    {
        switch (InStatus)
        {
            case ECk_GoapPlanStatus::Planning:             return CkDebugStyle::Status_Active();
            case ECk_GoapPlanStatus::PlanFound:            return CkDebugStyle::Ok();
            case ECk_GoapPlanStatus::PlanFailed:           return CkDebugStyle::Err();
            case ECk_GoapPlanStatus::CostThresholdReached: return CkDebugStyle::Warn();
            case ECk_GoapPlanStatus::Idle:
            default:                                       return CkDebugStyle::Value_Enum();
        }
    }

    auto Format_ClassName(const UClass* InClass) -> FString
    {
        return InClass != nullptr
            ? InClass->GetName()
            : FString(TEXT("(None)"));
    }
}

// =====================================================================================================================

auto FCkInspector_Goap::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("GOAP"));
}

auto FCkInspector_Goap::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<
        ck::FFragment_Goap_Params,
        ck::FFragment_Goap_Current,
        ck::FFragment_Goap_Actions,
        ck::FFragment_Goap_Goals,
        ck::FFragment_Goap_Diagnostics,
        ck::FFragment_Goap_ReplanThrottle>();
}

// =====================================================================================================================

auto FCkInspector_Goap::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- Plan (current status / active goal / committed plan) ----
    if (Entity.Has<ck::FFragment_Goap_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Plan")));

        const auto CapturedEntity = Entity;

        Builder.AddConditionalRow(
            FText::FromString(TEXT("Status:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Goap_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Status = CapturedEntity.Get<ck::FFragment_Goap_Current>().Get_PlanStatus();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Status));
            },
            [CapturedEntity](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Goap_Current>())
                { return CkDebugStyle::None(); }
                return Format_PlanStatus_Color(CapturedEntity.Get<ck::FFragment_Goap_Current>().Get_PlanStatus());
            });

        Builder.AddRow(
            FText::FromString(TEXT("Active Goal:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Goap_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto& Current = CapturedEntity.Get<ck::FFragment_Goap_Current>();
                return FText::FromString(Format_ClassName(Current.Get_ActiveGoalClass()));
            },
            CkDebugStyle::Value_Object());

        Builder.AddRow(
            FText::FromString(TEXT("Plan Cost:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Goap_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Cost = CapturedEntity.Get<ck::FFragment_Goap_Current>().Get_PlanCost();
                return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), Cost));
            },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Plan Attempts:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Goap_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Count = CapturedEntity.Get<ck::FFragment_Goap_Current>().Get_PlanAttemptCount();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Count));
            },
            CkDebugStyle::Value_Numeric());

        // ---- Plan steps — one row per action in execution order
        const auto& Plan = Entity.Get<ck::FFragment_Goap_Current>().Get_Plan();
        if (Plan.IsEmpty())
        {
            Builder.AddRow(
                FText::FromString(TEXT("Plan:")),
                [](const FCk_Handle&) { return FText::FromString(TEXT("(no plan)")); },
                CkDebugStyle::TextMute());
        }
        else
        {
            for (auto Index = int32{0}; Index < Plan.Num(); ++Index)
            {
                const auto Label = ck::Format_UE(TEXT("[{}]:"), Index);
                const auto Name  = Format_ClassName(Plan[Index]);
                Builder.AddRow(
                    FText::FromString(Label),
                    [Name](const FCk_Handle&) { return FText::FromString(Name); },
                    CkDebugStyle::Value_Object());
            }
        }
    }

    // ---- Params (configuration) ----
    if (Entity.Has<ck::FFragment_Goap_Params>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Params")));

        const auto& Params = Entity.Get<ck::FFragment_Goap_Params>();
        const auto SearchBudgetUs    = Params.Get_SearchBudgetMicroseconds();
        const auto CostThreshold     = Params.Get_CostThreshold();
        const auto ReplanPolicyStr   = ck::Format_UE(TEXT("{}"), Params.Get_ReplanPolicy());
        const auto MinReplanInterval = Params.Get_MinReplanIntervalSeconds();
        const auto PlanOnStart       = Params.Get_PlanOnStart();

        Builder.AddRow(
            FText::FromString(TEXT("Search Budget (us):")),
            [SearchBudgetUs](const FCk_Handle&)
            {
                return FText::FromString(SearchBudgetUs == 0
                    ? FString(TEXT("0 (unbounded)"))
                    : FString::Printf(TEXT("%lld"), SearchBudgetUs));
            },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Cost Threshold:")),
            [CostThreshold](const FCk_Handle&)
            {
                return FText::FromString(CostThreshold > 0.0f
                    ? FString::Printf(TEXT("%.3f"), CostThreshold)
                    : FString(TEXT("0 (disabled)")));
            },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Replan Policy:")),
            [ReplanPolicyStr](const FCk_Handle&) { return FText::FromString(ReplanPolicyStr); },
            CkDebugStyle::Value_Enum());

        Builder.AddRow(
            FText::FromString(TEXT("Min Replan Interval (s):")),
            [MinReplanInterval](const FCk_Handle&)
            {
                return FText::FromString(MinReplanInterval > 0.0f
                    ? FString::Printf(TEXT("%.3f"), MinReplanInterval)
                    : FString(TEXT("0 (no throttle)")));
            },
            CkDebugStyle::Value_Numeric());

        Builder.AddConditionalRow(
            FText::FromString(TEXT("Plan On Start:")),
            [PlanOnStart](const FCk_Handle&)
            { return FText::FromString(PlanOnStart ? TEXT("true") : TEXT("false")); },
            [PlanOnStart](const FCk_Handle&) -> FLinearColor
            { return PlanOnStart ? CkDebugStyle::Value_Bool_True() : CkDebugStyle::Value_Bool_False(); });
    }

    // ---- Replan Throttle (live) ----
    if (Entity.Has<ck::FFragment_Goap_ReplanThrottle>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Replan Throttle")));

        const auto CapturedEntity = Entity;
        Builder.AddRow(
            FText::FromString(TEXT("Seconds Since Replan:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Goap_ReplanThrottle>())
                { return FText::FromString(TEXT("--")); }
                const auto Seconds = CapturedEntity.Get<ck::FFragment_Goap_ReplanThrottle>().Get_SecondsSinceLastReplan();
                return FText::FromString(FString::Printf(TEXT("%.3f"), Seconds));
            },
            CkDebugStyle::Value_Numeric());
    }

    // ---- World State (key registry → ground truth bools) ----
    // Resolve through the planner's WorldState source — WS now lives on a
    // separate entity that one or more planners reference via Params.
    auto Source = Entity.Has<ck::FFragment_Goap_Params>()
        ? Entity.Get<ck::FFragment_Goap_Params>().Get_WorldStateSource()
        : FCk_Handle_Goap_WorldState{};
    if (ck::IsValid(Source)
        && Source.Has<ck::FFragment_Goap_WorldState_KeyRegistry>()
        && Source.Has<ck::FFragment_Goap_WorldState_Values>())
    {
        const auto& Registry   = Source.Get<ck::FFragment_Goap_WorldState_KeyRegistry>().Get_Registry();
        const auto& WorldState = Source.Get<ck::FFragment_Goap_WorldState_Values>().Get_Values();
        const auto& AllTags    = Registry.GetAllTags();

        Builder.AddHeader(FText::FromString(TEXT("World State")));

        if (AllTags.IsEmpty())
        {
            Builder.AddRow(
                FText::FromString(TEXT("(empty)")),
                [](const FCk_Handle&) { return FText::FromString(TEXT("--")); },
                CkDebugStyle::TextMute());
        }
        else
        {
            for (auto Key = int32{0}; Key < AllTags.Num(); ++Key)
            {
                const auto& Tag      = AllTags[Key];
                const auto  Value    = WorldState.Get(Key);
                const auto  TagLabel = Tag.IsValid() ? Tag.GetTagName().ToString() : FString(TEXT("(invalid)"));
                Builder.AddConditionalRow(
                    FText::FromString(TagLabel),
                    [Value](const FCk_Handle&)
                    { return FText::FromString(Value ? TEXT("true") : TEXT("false")); },
                    [Value](const FCk_Handle&) -> FLinearColor
                    { return Value ? CkDebugStyle::Value_Bool_True() : CkDebugStyle::Value_Bool_False(); });
            }
        }
    }

    // ---- Actions (registered action defs) ----
    if (Entity.Has<ck::FFragment_Goap_Actions>())
    {
        const auto& ActionDefs = Entity.Get<ck::FFragment_Goap_Actions>().Get_ActionDefs();

        Builder.AddHeader(FText::FromString(ck::Format_UE(TEXT("Actions ({})"), ActionDefs.Num())));

        if (ActionDefs.IsEmpty())
        {
            Builder.AddRow(
                FText::FromString(TEXT("(none registered)")),
                [](const FCk_Handle&) { return FText::FromString(TEXT("--")); },
                CkDebugStyle::TextMute());
        }
        else
        {
            for (const auto& Def : ActionDefs)
            {
                const auto Name = Format_ClassName(Def.ActionClass);
                const auto Detail = ck::Format_UE(
                    TEXT("cost {:.2f}  |  pre {}  |  fx {}"),
                    Def.Cost,
                    Def.Preconditions.Num(),
                    Def.Effects.Num());
                Builder.AddRow(
                    FText::FromString(Name),
                    [Detail](const FCk_Handle&) { return FText::FromString(Detail); },
                    CkDebugStyle::Value_Numeric());
            }
        }
    }

    // ---- Goals (registered goal defs) ----
    if (Entity.Has<ck::FFragment_Goap_Goals>())
    {
        const auto& GoalDefs = Entity.Get<ck::FFragment_Goap_Goals>().Get_GoalDefs();

        Builder.AddHeader(FText::FromString(ck::Format_UE(TEXT("Goals ({})"), GoalDefs.Num())));

        if (GoalDefs.IsEmpty())
        {
            Builder.AddRow(
                FText::FromString(TEXT("(none registered)")),
                [](const FCk_Handle&) { return FText::FromString(TEXT("--")); },
                CkDebugStyle::TextMute());
        }
        else
        {
            for (const auto& Def : GoalDefs)
            {
                const auto Name   = Format_ClassName(Def.GoalClass);
                const auto Detail = ck::Format_UE(
                    TEXT("priority {}  |  conds {}"),
                    Def.Priority,
                    Def.Conditions.Num());
                Builder.AddRow(
                    FText::FromString(Name),
                    [Detail](const FCk_Handle&) { return FText::FromString(Detail); },
                    CkDebugStyle::Value_Numeric());
            }
        }
    }

    // ---- Diagnostics (cycles, last-failed-goal, unreachable conditions) ----
    if (Entity.Has<ck::FFragment_Goap_Diagnostics>())
    {
        const auto& Diag                  = Entity.Get<ck::FFragment_Goap_Diagnostics>();
        const auto& Cycles                = Diag.Get_DependencyCycles();
        const auto& UnreachableConditions = Diag.Get_LastUnreachableGoalConditions();
        const auto  LastFailedGoal        = Diag.Get_LastFailedGoalClass();
        const auto  HasAnything =
            (NOT Cycles.IsEmpty())
            || (NOT UnreachableConditions.IsEmpty())
            || (LastFailedGoal != nullptr);

        if (HasAnything)
        {
            Builder.AddHeader(FText::FromString(TEXT("Diagnostics")));

            if (LastFailedGoal != nullptr)
            {
                const auto Name = Format_ClassName(LastFailedGoal);
                Builder.AddRow(
                    FText::FromString(TEXT("Last Failed Goal:")),
                    [Name](const FCk_Handle&) { return FText::FromString(Name); },
                    CkDebugStyle::Err());
            }

            if (NOT UnreachableConditions.IsEmpty())
            {
                Builder.AddRow(
                    FText::FromString(TEXT("Unreachable Conds:")),
                    [Count = UnreachableConditions.Num()](const FCk_Handle&)
                    { return FText::FromString(FString::Printf(TEXT("%d"), Count)); },
                    CkDebugStyle::Warn());

                for (const auto& Pair : UnreachableConditions)
                {
                    const auto KeyStr  = Pair.Get_Key().IsValid() ? Pair.Get_Key().GetTagName().ToString() : FString(TEXT("(invalid)"));
                    const auto ValStr  = Pair.Get_Value() ? FString(TEXT("true")) : FString(TEXT("false"));
                    Builder.AddRow(
                        FText::FromString(FString::Printf(TEXT("  %s"), *KeyStr)),
                        [ValStr](const FCk_Handle&) { return FText::FromString(ValStr); },
                        CkDebugStyle::Value_Tag());
                }
            }

            if (NOT Cycles.IsEmpty())
            {
                Builder.AddRow(
                    FText::FromString(TEXT("Dependency Cycles:")),
                    [Count = Cycles.Num()](const FCk_Handle&)
                    { return FText::FromString(FString::Printf(TEXT("%d"), Count)); },
                    CkDebugStyle::Err());

                for (auto CycleIndex = int32{0}; CycleIndex < Cycles.Num(); ++CycleIndex)
                {
                    const auto& Cycle = Cycles[CycleIndex];
                    auto NamesStr = FString();
                    for (auto ActionIndex = int32{0}; ActionIndex < Cycle.Get_ActionsInCycle().Num(); ++ActionIndex)
                    {
                        if (ActionIndex > 0) { NamesStr.Append(TEXT(", ")); }
                        NamesStr.Append(Format_ClassName(Cycle.Get_ActionsInCycle()[ActionIndex]));
                    }
                    Builder.AddRow(
                        FText::FromString(FString::Printf(TEXT("  cycle %d:"), CycleIndex)),
                        [NamesStr](const FCk_Handle&) { return FText::FromString(NamesStr); },
                        CkDebugStyle::Value_Object());
                }
            }
        }
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Goap::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
