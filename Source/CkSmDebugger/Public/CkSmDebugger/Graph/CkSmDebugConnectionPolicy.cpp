#include "CkSmDebugConnectionPolicy.h"

#include "CkSmDebugNode_Compound.h"
#include "CkSmDebugNode_Entry.h"
#include "CkSmDebugNode_State.h"
#include "CkSmDebugNode_Transition.h"
#include "CkSmDebugGraph.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"
#include "CkCore/Macros/CkMacros.h"

#include "Styling/AppStyle.h"
#include "Rendering/DrawElements.h"

// --------------------------------------------------------------------------------------------------------------------

FCkSmDebugConnectionPolicy::FCkSmDebugConnectionPolicy(
    int32 InBackLayerID,
    int32 InFrontLayerID,
    float InZoomFactor,
    const FSlateRect& InClippingRect,
    FSlateWindowElementList& InDrawElements,
    UEdGraph* InGraphObj)
    : FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
    , _Graph(Cast<UCkSmDebugGraph>(InGraphObj))
{
    ArrowImage = FAppStyle::GetBrush(TEXT("Graph.Arrow"));
    ArrowRadius = FVector2D(8.0, 8.0);
}

// --------------------------------------------------------------------------------------------------------------------

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
    OutParams.WireColor = FCkSmDebuggerStyle::Color_Sm_TransitionWire;
    OutParams.bDrawBubbles = false;

    if (NOT InInputPin)
    { return; }

    // Entry → State: neutral wire, no special styling
    if (Cast<UCkSmDebugNode_Entry>(InOutputPin->GetOwningNode()))
    { return; }

    // State → Compound: connector wire with sub-SM styling
    if (Cast<UCkSmDebugNode_Compound>(InInputPin->GetOwningNode()))
    {
        OutParams.WireColor = FCkSmDebuggerStyle::Color_Sm_SubSmConnector;
        OutParams.WireThickness = 1.5f;
        return;
    }

    // Transition wires: highlight state + self-loop flag
    if (auto* TransitionNode = Cast<UCkSmDebugNode_Transition>(InInputPin->GetOwningNode()))
    {
        if (TransitionNode->GetSourceNode() == TransitionNode->GetTargetNode())
        {
            OutParams.bUserFlag2 = true;
        }

        // Scrub highlight: thick bright wire for the fired transition
        if (TransitionNode->Get_IsScrubHighlighted())
        {
            OutParams.WireColor = FCkSmDebuggerStyle::Color_Sm_ScrubTransitionWire;
            OutParams.WireThickness = 3.0f;
        }
        // Live flash: interpolate from flash color back to normal
        else if (TransitionNode->Get_LiveFlashAlpha() > 0.0f)
        {
            auto Alpha = TransitionNode->Get_LiveFlashAlpha();
            OutParams.WireColor = FMath::Lerp(
                FCkSmDebuggerStyle::Color_Sm_TransitionWire,
                FCkSmDebuggerStyle::Color_Sm_LiveFlashWire,
                Alpha);
            OutParams.WireThickness = FMath::Lerp(1.5f, 3.5f, Alpha);
        }
    }

    // Deemphasis for unhovered pins
    auto bDeemphasizeUnhoveredPins = HoveredPins.Num() > 0;
    if (bDeemphasizeUnhoveredPins)
    {
        ApplyHoverDeemphasis(InOutputPin, InInputPin, OutParams.WireThickness, OutParams.WireColor);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugConnectionPolicy::
    Draw(
        TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries,
        FArrangedChildren& InArrangedNodes)
    -> void
{
    // Build node → arranged-index map for geometry lookups
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
    // -----------------------------------------------------------------------------------
    // Case 1: Transition edge — resolve to source/target STATE node geometry.
    // Wires draw state-to-state; the transition badge sits on top at the midpoint.
    // -----------------------------------------------------------------------------------
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

    // -----------------------------------------------------------------------------------
    // Case 2: State → Compound — resolve to state and compound node geometries.
    // -----------------------------------------------------------------------------------
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

    // -----------------------------------------------------------------------------------
    // Case 3: Entry → State — resolve to entry node and target state geometries.
    // -----------------------------------------------------------------------------------
    if (Cast<UCkSmDebugNode_Entry>(InOutputPin->GetOwningNode()))
    {
        auto* EntryIdx = _NodeWidgetMap.Find(InOutputPin->GetOwningNode());
        auto* StateIdx = _NodeWidgetMap.Find(InInputPin->GetOwningNode());

        if (EntryIdx && StateIdx)
        {
            OutStartWidgetGeometry = &InArrangedNodes[*EntryIdx];
            OutEndWidgetGeometry   = &InArrangedNodes[*StateIdx];
            return;
        }
    }

    // -----------------------------------------------------------------------------------
    // Fallback: pin-based geometry
    // -----------------------------------------------------------------------------------
    OutStartWidgetGeometry = PinGeometries->Find(InOutputPinWidget);

    if (auto* pTargetWidget = PinToPinWidgetMap.Find(InInputPin))
    {
        auto InputWidget = (*pTargetWidget).ToSharedRef();
        OutEndWidgetGeometry = PinGeometries->Find(InputWidget);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugConnectionPolicy::
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
    FCkSmDebugConnectionPolicy::
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
    FCkSmDebugConnectionPolicy::
    ComputeSplineTangent(
        const FVector2D& InStart,
        const FVector2D& InEnd) const
    -> FVector2D
{
    return (InEnd - InStart).GetSafeNormal();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSmDebugConnectionPolicy::
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
    FCkSmDebugConnectionPolicy::
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
