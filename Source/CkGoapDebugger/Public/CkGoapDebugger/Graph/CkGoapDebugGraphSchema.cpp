#include "CkGoapDebugGraphSchema.h"
#include "CkGoapDebugConnectionPolicy.h"
#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugNode_Goal.h"

#include "EdGraph/EdGraphNode.h"
#include "ToolMenu.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

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
	if (InMenu == nullptr || InContext == nullptr || InContext->Node == nullptr)
	{ return; }

	// Goal nodes use a different class and exposed fields than Action nodes —
	// handle them with their own Copy menu before falling through.
	if (const auto* GoalNode = Cast<UCkGoapDebugNode_Goal>(InContext->Node))
	{
		const auto DisplayName = GoalNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		const auto GoalClassName = GoalNode->Get_GoalName();
		const auto PriorityStr = FString::Printf(TEXT("%d"), GoalNode->Get_Priority());

		auto ConditionLines = TArray<FString>{};
		for (const auto& C : GoalNode->Get_Conditions())
		{ ConditionLines.Add(C.AsString()); }
		const auto ConditionStr = FString::Join(ConditionLines, TEXT("\n"));

		const auto AllStr = FString::Printf(
			TEXT("Goal: %s\nClass: %s\nPriority: %s\nActive: %s\nConditions:\n%s"),
			*DisplayName,
			*GoalClassName,
			*PriorityStr,
			GoalNode->Get_IsActiveGoal() ? TEXT("yes") : TEXT("no"),
			ConditionLines.Num() > 0 ? *ConditionStr : TEXT("  (none)"));

		ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
			TEXT("CopyGoalDisplayName"),
			FText::FromString(TEXT("Copy Display Name")),
			FText::FromString(TEXT("Copy the visible goal title")),
			DisplayName);

		ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
			TEXT("CopyGoalClassName"),
			FText::FromString(TEXT("Copy Goal Class Name")),
			FText::FromString(TEXT("Copy the underlying goal class name")),
			GoalClassName);

		ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
			TEXT("CopyGoalPriority"),
			FText::FromString(TEXT("Copy Priority")),
			FText::FromString(TEXT("Copy this goal's priority value")),
			PriorityStr);

		if (ConditionLines.Num() > 0)
		{
			ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
				TEXT("CopyGoalConditions"),
				FText::FromString(TEXT("Copy Conditions")),
				FText::FromString(TEXT("Copy the goal's condition list (one per line)")),
				ConditionStr);
		}

		ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
			TEXT("CopyGoalAll"),
			FText::FromString(TEXT("Copy All")),
			FText::FromString(TEXT("Copy a multi-line summary of this goal")),
			AllStr);

		return;
	}

	const auto* Node = Cast<UCkGoapDebugNode_Action>(InContext->Node);
	if (Node == nullptr)
	{ return; }

	const auto DisplayName = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	const auto ActionName = Node->Get_ActionName();
	const auto CostStr = FString::Printf(TEXT("%.2f"), Node->Get_Cost());

	auto PreconditionLines = TArray<FString>{};
	for (const auto& C : Node->Get_Preconditions())
	{ PreconditionLines.Add(C.AsString()); }
	const auto PreconditionStr = FString::Join(PreconditionLines, TEXT("\n"));

	auto EffectLines = TArray<FString>{};
	for (const auto& E : Node->Get_Effects())
	{ EffectLines.Add(E.AsString()); }
	const auto EffectStr = FString::Join(EffectLines, TEXT("\n"));

	const auto AllStr = FString::Printf(TEXT("%s\nClass: %s\nCost: %s\nPreconditions:\n%s\nEffects:\n%s"),
		*DisplayName,
		*ActionName,
		*CostStr,
		PreconditionLines.Num() > 0 ? *PreconditionStr : TEXT("  (none)"),
		EffectLines.Num()       > 0 ? *EffectStr       : TEXT("  (none)"));

	ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
		TEXT("CopyText"),
		FText::FromString(TEXT("Copy Display Name")),
		FText::FromString(TEXT("Copy the visible node title")),
		DisplayName);

	ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
		TEXT("CopyActionName"),
		FText::FromString(TEXT("Copy Action Class Name")),
		FText::FromString(TEXT("Copy the underlying action class name")),
		ActionName);

	ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
		TEXT("CopyCost"),
		FText::FromString(TEXT("Copy Cost")),
		FText::FromString(TEXT("Copy this action's cost value")),
		CostStr);

	if (PreconditionLines.Num() > 0)
	{
		ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
			TEXT("CopyPreconditions"),
			FText::FromString(TEXT("Copy Preconditions")),
			FText::FromString(TEXT("Copy the precondition list (one per line)")),
			PreconditionStr);
	}

	if (EffectLines.Num() > 0)
	{
		ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
			TEXT("CopyEffects"),
			FText::FromString(TEXT("Copy Effects")),
			FText::FromString(TEXT("Copy the effect list (one per line)")),
			EffectStr);
	}

	ck::DebugCopyMenu::AddCopyEntryToToolMenu(InMenu,
		TEXT("CopyAll"),
		FText::FromString(TEXT("Copy All")),
		FText::FromString(TEXT("Copy a multi-line summary of this node")),
		AllStr);
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
