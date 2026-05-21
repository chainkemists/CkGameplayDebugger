#include "CkGoapDebugNode_Goal.h"

// ====================================================================================================================

auto
    UCkGoapDebugNode_Goal::
    AllocateDefaultPins()
    -> void
{
    for (const auto& Cond : _Goal)
    {
        const auto PinName = FName(*Cond.Key.ToString());
        CreatePin(EGPD_Input, TEXT("GoapCondition"), PinName);
    }

    // Always offer a fallback input pin so an empty-goal anchor still has
    // somewhere for the connection policy to attach.
    if (_Goal.Num() == 0)
    { CreatePin(EGPD_Input, TEXT("GoapDefault"), NAME_None); }
}

// ====================================================================================================================

auto
    UCkGoapDebugNode_Goal::
    GetNodeTitle(
        ENodeTitleType::Type InTitleType) const
    -> FText
{
    return FText::FromString(TEXT("Goal"));
}

// ====================================================================================================================

auto
    UCkGoapDebugNode_Goal::
    PopulateFromGoal(
        const FString& InOwningActionName,
        const TArray<FCkGoapDebugger_Condition>& InGoal)
    -> void
{
    _OwningActionName = InOwningActionName;
    _Goal = InGoal;
}

// ====================================================================================================================
