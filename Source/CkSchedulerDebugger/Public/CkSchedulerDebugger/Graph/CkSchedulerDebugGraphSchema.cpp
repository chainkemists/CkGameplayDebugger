#include "CkSchedulerDebugger/Graph/CkSchedulerDebugGraphSchema.h"
#include "CkSchedulerDebugger/Graph/CkSchedulerDebugConnectionPolicy.h"
#include "CkSchedulerDebugger/Graph/CkSchedulerDebugNode_Processor.h"

#include "EdGraph/EdGraphNode.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

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
	if (InMenu == nullptr || InContext == nullptr || InContext->Node == nullptr)
	{ return; }

	const auto* Node = Cast<UCkSchedulerDebugNode_Processor>(InContext->Node);
	if (Node == nullptr)
	{ return; }

	const auto DisplayName  = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	const auto ClassName    = Node->ProcessorName.ToString();
	const auto GroupName    = Node->GroupName.ToString();
	const auto ExecOrderStr = FString::Printf(TEXT("#%d"), Node->ExecutionOrder);
	const auto TimingStr    = FString::Printf(TEXT("%.3f ms"), Node->LastFrameTimeMs);

	auto FlagsStr = FString{};
	if (Node->IsGhost)         { FlagsStr += TEXT("Ghost "); }
	if (Node->IsParallel)      { FlagsStr += TEXT("Parallel "); }
	if (Node->HasDirtyMarker)  { FlagsStr += TEXT("Dirty "); }
	if (Node->IsGroupStart)    { FlagsStr += TEXT("GroupStart "); }
	if (Node->IsGroupEnd)      { FlagsStr += TEXT("GroupEnd "); }
	FlagsStr.TrimEndInline();

	const auto AllStr = FString::Printf(
		TEXT("%s\nClass: %s\nGroup: %s\nExec Order: %s\nTiming: %s\nPump Count: %d%s"),
		*DisplayName,
		*ClassName,
		*GroupName,
		*ExecOrderStr,
		*TimingStr,
		Node->PumpCountThisFrame,
		FlagsStr.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("\nFlags: %s"), *FlagsStr));

	ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
		TEXT("CopyText"),
		FText::FromString(TEXT("Copy Display Name")),
		FText::FromString(TEXT("Copy the visible node title")),
		DisplayName);

	ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
		TEXT("CopyClassName"),
		FText::FromString(TEXT("Copy Processor Class Name")),
		FText::FromString(TEXT("Copy the underlying processor class name")),
		ClassName);

	ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
		TEXT("CopyGroup"),
		FText::FromString(TEXT("Copy Group Name")),
		FText::FromString(TEXT("Copy the group this processor belongs to")),
		GroupName);

	ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
		TEXT("CopyExecOrder"),
		FText::FromString(TEXT("Copy Exec Order")),
		FText::FromString(TEXT("Copy this processor's execution order")),
		ExecOrderStr);

	ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
		TEXT("CopyTiming"),
		FText::FromString(TEXT("Copy Timing")),
		FText::FromString(TEXT("Copy last frame's timing for this processor")),
		TimingStr);

	ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
		TEXT("CopyAll"),
		FText::FromString(TEXT("Copy All")),
		FText::FromString(TEXT("Copy a multi-line summary of this node")),
		AllStr);
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
