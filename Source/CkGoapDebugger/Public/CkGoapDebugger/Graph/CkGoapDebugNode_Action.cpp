#include "CkGoapDebugNode_Action.h"

// ====================================================================================================================

auto
	UCkGoapDebugNode_Action::
	AllocateDefaultPins()
	-> void
{
	CreatePin(EGPD_Input, TEXT("Transition"), TEXT("In"));
	CreatePin(EGPD_Output, TEXT("Transition"), TEXT("Out"));
}

auto
	UCkGoapDebugNode_Action::
	GetNodeTitle(ENodeTitleType::Type InTitleType) const
	-> FText
{
	return FText::FromString(_ActionName);
}

auto
	UCkGoapDebugNode_Action::
	PopulateFromActionInfo(
		const FCkGoapDebugger_ActionInfo& InAction,
		int32 InIndex)
	-> void
{
	_ActionName = InAction.ClassName;
	_Cost = InAction.Cost;
	_Preconditions = InAction.Preconditions;
	_Effects = InAction.Effects;
	_ActionClass = InAction.ActionClass;
	_ActionIndex = InIndex;
}

auto
	UCkGoapDebugNode_Action::
	UpdatePlanState(
		bool InIsInPlan,
		int32 InPlanStepIndex)
	-> void
{
	_InPlan = InIsInPlan;
	_PlanStepIndex = InPlanStepIndex;
}

// ====================================================================================================================
