#include "CkGoapDebugNode_Goal.h"

// ====================================================================================================================

auto
	UCkGoapDebugNode_Goal::
	AllocateDefaultPins()
	-> void
{
	CreatePin(EGPD_Input, TEXT("Transition"), TEXT("In"));
}

auto
	UCkGoapDebugNode_Goal::
	GetNodeTitle(ENodeTitleType::Type InTitleType) const
	-> FText
{
	return FText::FromString(_DisplayName.IsEmpty() ? _GoalName : _DisplayName);
}

auto
	UCkGoapDebugNode_Goal::
	PopulateFromGoalInfo(
		const FCkGoapDebugger_GoalInfo& InGoal)
	-> void
{
	_GoalName = InGoal.ClassName;
	_Priority = InGoal.Priority;
	_Conditions = InGoal.Conditions;
	_IsActiveGoal = InGoal.IsActiveGoal;
}

// ====================================================================================================================
