#if WITH_EDITOR
#include "CkSmDebugConnectionPolicy.h"

#include "CkSmDebugNode_Compound.h"
#include "CkSmDebugNode_Entry.h"
#include "CkSmDebugNode_State.h"
#include "CkSmDebugNode_Transition.h"
#include "CkSmDebugGraph.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

FCkSmDebugConnectionPolicy::FCkSmDebugConnectionPolicy(
	int32 InBackLayerID,
	int32 InFrontLayerID,
	float InZoomFactor,
	const FSlateRect& InClippingRect,
	FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj)
	: FCkDebugConnectionPolicyBase(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
	, _Graph(Cast<UCkSmDebugGraph>(InGraphObj))
{
}

auto
	FCkSmDebugConnectionPolicy::
	DetermineWiringStyle(
		UEdGraphPin* InOutputPin,
		UEdGraphPin* InInputPin,
		FConnectionParams& OutParams)
	-> void
{
	OutParams.AssociatedPin1 = InOutputPin;
	OutParams.AssociatedPin2 = InInputPin;
	OutParams.WireThickness = 1.5f;
	// Neutral wire: the muted-text role, which is where the suite's low-emphasis lines live.
	OutParams.WireColor = CkStyle::TextDim();
	OutParams.bDrawBubbles = false;

	if (NOT InInputPin)
	{ return; }

	// Entry -> State: neutral wire
	if (Cast<UCkSmDebugNode_Entry>(InOutputPin->GetOwningNode()))
	{ return; }

	// State -> Compound: sub-SM connector
	if (Cast<UCkSmDebugNode_Compound>(InInputPin->GetOwningNode()))
	{
		OutParams.WireColor = CkStyle::OverlayOf(CkStyle::Accent(), 0.502f);
		OutParams.WireThickness = 1.5f;
		return;
	}

	// Transition wires: highlight + self-loop flag
	if (auto* TransitionNode = Cast<UCkSmDebugNode_Transition>(InInputPin->GetOwningNode()))
	{
		if (TransitionNode->GetSourceNode() == TransitionNode->GetTargetNode())
		{
			OutParams.bUserFlag2 = true;
		}

		if (TransitionNode->Get_IsScrubHighlighted())
		{
			OutParams.WireColor = CkStyle::Accent();
			OutParams.WireThickness = 3.0f;
		}
		else if (TransitionNode->Get_LiveFlashAlpha() > 0.0f)
		{
			auto Alpha = TransitionNode->Get_LiveFlashAlpha();
			OutParams.WireColor = FMath::Lerp(
				CkStyle::TextDim(),
				CkStyle::Warn(),
				Alpha);
			OutParams.WireThickness = FMath::Lerp(1.5f, 3.5f, Alpha);
		}
	}

	auto bDeemphasizeUnhoveredPins = HoveredPins.Num() > 0;
	if (bDeemphasizeUnhoveredPins)
	{
		ApplyHoverDeemphasis(InOutputPin, InInputPin, OutParams.WireThickness, OutParams.WireColor);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkSmDebugConnectionPolicy::
	DetermineLinkGeometry(
		FArrangedChildren& InArrangedNodes,
		TSharedRef<SWidget>& InOutputPinWidget,
		UEdGraphPin* InOutputPin,
		UEdGraphPin* InInputPin,
		FArrangedWidget*& OutStartWidgetGeometry,
		FArrangedWidget*& OutEndWidgetGeometry)
	-> void
{
	// Transition edge: resolve to source/target STATE node geometry
	if (auto* TransitionNode = Cast<UCkSmDebugNode_Transition>(InInputPin->GetOwningNode()))
	{
		auto* SourceState = TransitionNode->GetSourceNode();
		auto* TargetState = TransitionNode->GetTargetNode();

		if (SourceState && TargetState)
		{
			auto* StartIdx = _NodeWidgetMap.Find(SourceState);
			auto* EndIdx   = _NodeWidgetMap.Find(TargetState);

			if (StartIdx && EndIdx)
			{
				OutStartWidgetGeometry = &InArrangedNodes[*StartIdx];
				OutEndWidgetGeometry   = &InArrangedNodes[*EndIdx];
				return;
			}
		}
	}

	// State -> Compound
	if (Cast<UCkSmDebugNode_Compound>(InInputPin->GetOwningNode()))
	{
		auto* StateIdx = _NodeWidgetMap.Find(InOutputPin->GetOwningNode());
		auto* CompIdx  = _NodeWidgetMap.Find(InInputPin->GetOwningNode());

		if (StateIdx && CompIdx)
		{
			OutStartWidgetGeometry = &InArrangedNodes[*StateIdx];
			OutEndWidgetGeometry   = &InArrangedNodes[*CompIdx];
			return;
		}
	}

	// Entry -> State, and fallback: use base class node-to-node resolution
	FCkDebugConnectionPolicyBase::DetermineLinkGeometry(
		InArrangedNodes, InOutputPinWidget, InOutputPin, InInputPin,
		OutStartWidgetGeometry, OutEndWidgetGeometry);
}

// --------------------------------------------------------------------------------------------------------------------
#endif
