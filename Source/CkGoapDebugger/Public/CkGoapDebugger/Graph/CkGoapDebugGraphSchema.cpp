#include "CkGoapDebugGraphSchema.h"
#include "CkGoapDebugConnectionPolicy.h"

#include "ToolMenu.h"

// ====================================================================================================================

auto
	UCkGoapDebugGraphSchema::
	GetGraphType(const UEdGraph* InGraph) const
	-> EGraphType
{
	return GT_StateMachine;
}

auto
	UCkGoapDebugGraphSchema::
	GetGraphContextActions(
		FGraphContextMenuBuilder& InContextMenuBuilder) const
	-> void
{
}

auto
	UCkGoapDebugGraphSchema::
	GetContextMenuActions(
		UToolMenu* InMenu,
		UGraphNodeContextMenuContext* InContext) const
	-> void
{
}

auto
	UCkGoapDebugGraphSchema::
	CanCreateConnection(
		const UEdGraphPin* InPinA,
		const UEdGraphPin* InPinB) const
	-> const FPinConnectionResponse
{
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Read-only debug graph"));
}

auto
	UCkGoapDebugGraphSchema::
	TryCreateConnection(
		UEdGraphPin* InPinA,
		UEdGraphPin* InPinB) const
	-> bool
{
	return false;
}

auto
	UCkGoapDebugGraphSchema::
	CreateConnectionDrawingPolicy(
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float InZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements,
		UEdGraph* InGraphObj) const
	-> FConnectionDrawingPolicy*
{
	return new FCkGoapDebugConnectionPolicy(
		InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

auto
	UCkGoapDebugGraphSchema::
	GetGraphDisplayInformation(
		const UEdGraph& InGraph,
		FGraphDisplayInfo& OutDisplayInfo) const
	-> void
{
	OutDisplayInfo.PlainName = FText::FromString(TEXT("GOAP Debug"));
	OutDisplayInfo.DisplayName = FText::FromString(TEXT("GOAP Debug"));
}

// ====================================================================================================================
