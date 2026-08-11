#if WITH_EDITOR

#include "CkSchedulerDebugger/Graph/CkSchedulerDebugNode_Processor.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugNode_Processor::
	AllocateDefaultPins()
	-> void
{
	CreatePin(EGPD_Input, TEXT("Transition"), TEXT("In"));
	CreatePin(EGPD_Output, TEXT("Transition"), TEXT("Out"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugNode_Processor::
	GetNodeTitle(ENodeTitleType::Type InTitleType) const
	-> FText
{
	return FText::FromString(DisplayName);
}

// --------------------------------------------------------------------------------------------------------------------

#endif
