#include "CkSchedulerDebugger/Graph/CkSchedulerDebugGraphSchema.h"
#include "CkSchedulerDebugger/Graph/CkSchedulerDebugConnectionPolicy.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraphSchema::
	GetGraphType(const UEdGraph* InTestEdGraph) const
	-> EGraphType
{
	return GT_StateMachine;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraphSchema::
	CanCreateConnection(
		const UEdGraphPin* InPinA,
		const UEdGraphPin* InPinB) const
	-> const FPinConnectionResponse
{
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Read-only graph"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraphSchema::
	TryCreateConnection(
		UEdGraphPin* InPinA,
		UEdGraphPin* InPinB) const
	-> bool
{
	return false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraphSchema::
	GetGraphContextActions(
		FGraphContextMenuBuilder& InContextMenuBuilder) const
	-> void
{
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraphSchema::
	GetContextMenuActions(
		UToolMenu* InMenu,
		UGraphNodeContextMenuContext* InContext) const
	-> void
{
}

// --------------------------------------------------------------------------------------------------------------------

auto
	UCkSchedulerDebugGraphSchema::
	CreateConnectionDrawingPolicy(
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float InZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements,
		UEdGraph* InGraphObj) const
	-> FConnectionDrawingPolicy*
{
	return new FCkSchedulerDebugConnectionPolicy(
		InBackLayerID,
		InFrontLayerID,
		InZoomFactor,
		InClippingRect,
		InDrawElements);
}

// --------------------------------------------------------------------------------------------------------------------
