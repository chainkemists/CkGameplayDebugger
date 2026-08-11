#if WITH_EDITOR

#include "CkSmDebugNode_Transition.h"

#include "CkSmDebugNode_State.h"
#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Transition::
    AllocateDefaultPins()
    -> void
{
    CreatePin(EGPD_Input, TEXT("Transition"), TEXT("In"));
    CreatePin(EGPD_Output, TEXT("Transition"), TEXT("Out"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Transition::
    GetNodeTitle(
        ENodeTitleType::Type InTitleType) const
    -> FText
{
    return FText::GetEmpty();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Transition::
    PopulateFromTransitionInfo(
        const FCkSmDebugger_TransitionInfo& InTransition,
        int32 InIndex)
    -> void
{
    _TransitionIndex = InIndex;
    _SourceStateName = InTransition.SourceStateName;
    _TargetStateName = InTransition.TargetStateName;
    _SatisfiedCount = InTransition.SatisfiedCount;
    _TotalCount = InTransition.TotalCount;
    _AreAllConditionsSatisfied = InTransition.AreAllConditionsSatisfied;
    _IsSubSmTransition = InTransition.IsSubSmTransition;
    _Conditions = InTransition.Conditions;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Transition::
    UpdateRuntimeData(
        const FCkSmDebugger_TransitionInfo& InTransition)
    -> void
{
    _SatisfiedCount = InTransition.SatisfiedCount;
    _TotalCount = InTransition.TotalCount;
    _AreAllConditionsSatisfied = InTransition.AreAllConditionsSatisfied;
    _Conditions = InTransition.Conditions;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Transition::
    InitMockup(
        const FString& InSourceName,
        const FString& InTargetName,
        int32 InIndex,
        int32 InSourceIdx,
        int32 InTargetIdx)
    -> void
{
    _SourceStateName = InSourceName;
    _TargetStateName = InTargetName;
    _TransitionIndex = InIndex;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Transition::
    GetSourceNode() const
    -> UCkSmDebugNode_State*
{
    if (Pins.Num() < 1)
    { return nullptr; }

    auto* InputPin = Pins[0];
    if (NOT InputPin || InputPin->LinkedTo.Num() == 0)
    { return nullptr; }

    return Cast<UCkSmDebugNode_State>(InputPin->LinkedTo[0]->GetOwningNode());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Transition::
    GetTargetNode() const
    -> UCkSmDebugNode_State*
{
    if (Pins.Num() < 2)
    { return nullptr; }

    auto* OutputPin = Pins[1];
    if (NOT OutputPin || OutputPin->LinkedTo.Num() == 0)
    { return nullptr; }

    return Cast<UCkSmDebugNode_State>(OutputPin->LinkedTo[0]->GetOwningNode());
}

// --------------------------------------------------------------------------------------------------------------------

#endif
