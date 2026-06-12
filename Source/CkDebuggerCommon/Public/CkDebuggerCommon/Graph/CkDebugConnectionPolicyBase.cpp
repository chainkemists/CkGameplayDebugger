#include "CkDebuggerCommon/Graph/CkDebugConnectionPolicyBase.h"

// Graph connection-drawing policy is editor-only (FConnectionDrawingPolicy /
// GraphEditor). GraphEditor is gated to bBuildEditor in the Build.cs, so this
// must not compile in non-editor (DeveloperTool cooked) targets.
#if WITH_EDITOR

#include "SGraphNode.h"
#include "Styling/AppStyle.h"
#include "Rendering/DrawElements.h"

// --------------------------------------------------------------------------------------------------------------------

FCkDebugConnectionPolicyBase::FCkDebugConnectionPolicyBase(
	int32 InBackLayerID,
	int32 InFrontLayerID,
	float InZoomFactor,
	const FSlateRect& InClippingRect,
	FSlateWindowElementList& InDrawElements)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
{
	ArrowImage = FAppStyle::GetBrush(TEXT("Graph.Arrow"));
	ArrowRadius = FVector2D(8.0, 8.0);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkDebugConnectionPolicyBase::
	Draw(
		TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries,
		FArrangedChildren& InArrangedNodes)
	-> void
{
	_NodeWidgetMap.Empty();

	for (auto NodeIndex = 0; NodeIndex < InArrangedNodes.Num(); ++NodeIndex)
	{
		auto& CurWidget = InArrangedNodes[NodeIndex];
		auto ChildNode = StaticCastSharedRef<SGraphNode>(CurWidget.Widget);
		_NodeWidgetMap.Add(ChildNode->GetNodeObj(), NodeIndex);
	}

	FConnectionDrawingPolicy::Draw(InPinGeometries, InArrangedNodes);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkDebugConnectionPolicyBase::
	DetermineLinkGeometry(
		FArrangedChildren& InArrangedNodes,
		TSharedRef<SWidget>& InOutputPinWidget,
		UEdGraphPin* InOutputPin,
		UEdGraphPin* InInputPin,
		FArrangedWidget*& OutStartWidgetGeometry,
		FArrangedWidget*& OutEndWidgetGeometry)
	-> void
{
	auto* SourceNode = InOutputPin ? InOutputPin->GetOwningNode() : nullptr;
	auto* TargetNode = InInputPin ? InInputPin->GetOwningNode() : nullptr;

	if (SourceNode && TargetNode)
	{
		auto* StartIdx = _NodeWidgetMap.Find(SourceNode);
		auto* EndIdx = _NodeWidgetMap.Find(TargetNode);

		if (StartIdx && EndIdx)
		{
			OutStartWidgetGeometry = &InArrangedNodes[*StartIdx];
			OutEndWidgetGeometry = &InArrangedNodes[*EndIdx];
			return;
		}
	}

	// Fallback: pin-based geometry
	OutStartWidgetGeometry = PinGeometries->Find(InOutputPinWidget);

	if (auto* TargetWidget = PinToPinWidgetMap.Find(InInputPin))
	{
		auto InputWidget = (*TargetWidget).ToSharedRef();
		OutEndWidgetGeometry = PinGeometries->Find(InputWidget);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkDebugConnectionPolicyBase::
	DetermineWiringStyle(
		UEdGraphPin* InOutputPin,
		UEdGraphPin* InInputPin,
		FConnectionParams& OutParams)
	-> void
{
	OutParams.AssociatedPin1 = InOutputPin;
	OutParams.AssociatedPin2 = InInputPin;
	OutParams.WireThickness = 1.5f;
	OutParams.WireColor = FLinearColor(0.5f, 0.5f, 0.6f);
	OutParams.bDrawBubbles = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkDebugConnectionPolicyBase::
	DrawSplineWithArrow(
		const FVector2D& InStartPoint,
		const FVector2D& InEndPoint,
		const FConnectionParams& InParams)
	-> void
{
	DrawLineWithArrow(InStartPoint, InEndPoint, InParams);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkDebugConnectionPolicyBase::
	DrawSplineWithArrow(
		const FGeometry& InStartGeom,
		const FGeometry& InEndGeom,
		const FConnectionParams& InParams)
	-> void
{
	auto StartCenter = FGeometryHelper::CenterOf(InStartGeom);
	auto EndCenter   = FGeometryHelper::CenterOf(InEndGeom);
	auto SeedPoint   = (StartCenter + EndCenter) * 0.5;

	auto StartPoint = FGeometryHelper::FindClosestPointOnGeom(InStartGeom, SeedPoint);
	auto EndPoint   = FGeometryHelper::FindClosestPointOnGeom(InEndGeom,   SeedPoint);

	DrawSplineWithArrow(StartPoint, EndPoint, InParams);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkDebugConnectionPolicyBase::
	ComputeSplineTangent(
		const FVector2D& InStart,
		const FVector2D& InEnd) const
	-> FVector2D
{
	return (InEnd - InStart).GetSafeNormal();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkDebugConnectionPolicyBase::
	DrawLineWithArrow(
		const FVector2D& InStart,
		const FVector2D& InEnd,
		const FConnectionParams& InParams)
	-> void
{
	constexpr auto LineSeparation = 4.5f;

	auto Delta     = InEnd - InStart;
	auto UnitDelta = Delta.GetSafeNormal();
	auto Normal    = FVector2D(Delta.Y, -Delta.X).GetSafeNormal();

	auto DirectionBias = Normal * LineSeparation;
	auto LengthBias    = ArrowRadius.X * UnitDelta;
	auto StartPoint    = InStart + DirectionBias + LengthBias;
	auto EndPoint      = InEnd   + DirectionBias - LengthBias;

	auto LinePoints = TArray<FVector2D>{ StartPoint, EndPoint };
	FSlateDrawElement::MakeLines(
		DrawElementsList,
		WireLayerID,
		FPaintGeometry(),
		LinePoints,
		ESlateDrawEffect::None,
		InParams.WireColor,
		true,
		InParams.WireThickness);
	DrawArrow(EndPoint, Delta, InParams);
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkDebugConnectionPolicyBase::
	DrawArrow(
		const FVector2D& InPos,
		const FVector2D& InDelta,
		const FConnectionParams& InParams)
	-> void
{
	auto ArrowDrawPos  = InPos - ArrowRadius;
	auto AngleInRadians = FMath::Atan2(InDelta.Y, InDelta.X);

	FSlateDrawElement::MakeRotatedBox(
		DrawElementsList,
		ArrowLayerID,
		FPaintGeometry(ArrowDrawPos, ArrowImage->ImageSize * ZoomFactor, ZoomFactor),
		ArrowImage,
		ESlateDrawEffect::None,
		AngleInRadians,
		TOptional<FVector2D>(),
		FSlateDrawElement::RelativeToElement,
		InParams.WireColor);
}

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR
