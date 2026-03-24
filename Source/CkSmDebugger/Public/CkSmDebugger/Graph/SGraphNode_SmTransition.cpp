#include "SGraphNode_SmTransition.h"

#include "CkSmDebugNode_Transition.h"
#include "CkSmDebugNode_State.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Styling/AppStyle.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmTransition::
    Construct(
        const FArguments& InArgs,
        UCkSmDebugNode_Transition* InNode)
    -> void
{
    _TransitionNode = InNode;
    GraphNode = InNode;

    SetCursor(EMouseCursor::CardinalCross);
    UpdateGraphNode();
}

// --------------------------------------------------------------------------------------------------------------------

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

auto
    SGraphNode_SmTransition::
    UpdateGraphNode()
    -> void
{
    InputPins.Empty();
    OutputPins.Empty();
    RightNodeBox.Reset();
    LeftNodeBox.Reset();

    // LogicDriverPro pattern: two-layer overlay with ColorSpill background + Icon foreground.
    // The ColorSpill brush is a circular gradient, the Icon brush is a directional arrow.
    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SOverlay)

            // Circular background
            + SOverlay::Slot()
                [
                    SNew(SImage)
                        .Image(FAppStyle::GetBrush(TEXT("Graph.TransitionNode.ColorSpill")))
                        .ColorAndOpacity(FCkSmDebuggerStyle::Color_Sm_TransitionBadge)
                ]

            // Transition icon (directional arrow)
            + SOverlay::Slot()
                [
                    SNew(SImage)
                        .Image(FAppStyle::GetBrush(TEXT("Graph.TransitionNode.Icon")))
                ]
        ];

    // No pin widgets — wires are drawn via state-to-state resolution in the connection policy.
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmTransition::
    CreatePinWidgets()
    -> void
{
    // Transition badge has no visible pins.
    // Wires bypass this node via DetermineLinkGeometry → state-to-state resolution.
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmTransition::
    PerformSecondPassLayout(
        const TMap<UObject*, TSharedRef<SNode>>& InNodeToWidgetLookup) const
    -> void
{
    auto Center = GetCenterBetweenNodes(InNodeToWidgetLookup);

    // Offset by half the badge size so it is centered on the wire midpoint.
    // Standard TransitionNode.ColorSpill brush is ~16x16.
    constexpr auto BadgeHalf = 8.0f;
    auto NewPos = Center - FVector2D(BadgeHalf, BadgeHalf);

    if (_TransitionNode)
    {
        _TransitionNode->NodePosX = static_cast<int32>(NewPos.X);
        _TransitionNode->NodePosY = static_cast<int32>(NewPos.Y);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmTransition::
    MoveTo(
        const FVector2D& InNewPosition,
        FNodeSet& InNodeFilter,
        bool bMarkDirty)
    -> void
{
    // Transition badge position is computed — do not allow user dragging.
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmTransition::
    GetCenterBetweenNodes(
        const TMap<UObject*, TSharedRef<SNode>>& InNodeToWidgetLookup) const
    -> FVector2D
{
    auto DefaultCenter = FVector2D(0.0, 0.0);

    if (NOT _TransitionNode)
    { return DefaultCenter; }

    auto* SourceNode = _TransitionNode->GetSourceNode();
    auto* TargetNode = _TransitionNode->GetTargetNode();

    if (NOT SourceNode || NOT TargetNode)
    { return DefaultCenter; }

    auto SourceWidgetPtr = InNodeToWidgetLookup.Find(SourceNode);
    auto TargetWidgetPtr = InNodeToWidgetLookup.Find(TargetNode);

    if (NOT SourceWidgetPtr || NOT TargetWidgetPtr)
    { return DefaultCenter; }

    auto SourceGeometry = (*SourceWidgetPtr)->GetCachedGeometry();
    auto TargetGeometry = (*TargetWidgetPtr)->GetCachedGeometry();

    auto SourceCenter = FVector2D(
        SourceNode->NodePosX + SourceGeometry.GetLocalSize().X * 0.5,
        SourceNode->NodePosY + SourceGeometry.GetLocalSize().Y * 0.5);

    auto TargetCenter = FVector2D(
        TargetNode->NodePosX + TargetGeometry.GetLocalSize().X * 0.5,
        TargetNode->NodePosY + TargetGeometry.GetLocalSize().Y * 0.5);

    return (SourceCenter + TargetCenter) * 0.5;
}

// --------------------------------------------------------------------------------------------------------------------
