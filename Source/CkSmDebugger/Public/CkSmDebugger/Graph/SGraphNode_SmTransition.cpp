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

    auto BpRed    = FCkSmDebuggerStyle::Color_Sm_Breakpoint;
    auto BpHollow = FLinearColor(BpRed.R, BpRed.G, BpRed.B, 0.25f);
    auto BpStyle  = _TransitionNode ? _TransitionNode->Get_BreakpointStyle() : 0;

    // LogicDriverPro pattern: two-layer overlay with ColorSpill background + Icon foreground.
    auto TransOverlay = SNew(SOverlay)

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
            ];

    // ---- Style 0: filled red circle, top-right (only when set) ----
    if (BpStyle == 0)
    {
        TransOverlay->AddSlot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Top)
            [
                SNew(SBox)
                    .WidthOverride(8.0f)
                    .HeightOverride(8.0f)
                    .Visibility(TAttribute<EVisibility>::CreateLambda([TransNodePtr = _TransitionNode]()
                    {
                        return (TransNodePtr && TransNodePtr->Get_HasBreakpoint())
                            ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                    }))
                    [
                        SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor(BpRed)
                    ]
            ];
    }
    // ---- Style 1: filled red circle, top-left (only when set) ----
    else if (BpStyle == 1)
    {
        TransOverlay->AddSlot()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Top)
            [
                SNew(SBox)
                    .WidthOverride(8.0f)
                    .HeightOverride(8.0f)
                    .Visibility(TAttribute<EVisibility>::CreateLambda([TransNodePtr = _TransitionNode]()
                    {
                        return (TransNodePtr && TransNodePtr->Get_HasBreakpoint())
                            ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                    }))
                    [
                        SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor(BpRed)
                    ]
            ];
    }
    // ---- Style 2: diamond, top-right (only when set) ----
    else if (BpStyle == 2)
    {
        TransOverlay->AddSlot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Top)
            [
                SNew(SBox)
                    .WidthOverride(8.0f)
                    .HeightOverride(8.0f)
                    .Visibility(TAttribute<EVisibility>::CreateLambda([TransNodePtr = _TransitionNode]()
                    {
                        return (TransNodePtr && TransNodePtr->Get_HasBreakpoint())
                            ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                    }))
                    [
                        SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor(BpRed)
                            .RenderTransformPivot(FVector2D(0.5, 0.5))
                            .RenderTransform(FSlateRenderTransform(FQuat2D(PI / 4.0)))
                    ]
            ];
    }
    // ---- Style 3: diamond, top-left (only when set) ----
    else if (BpStyle == 3)
    {
        TransOverlay->AddSlot()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Top)
            [
                SNew(SBox)
                    .WidthOverride(8.0f)
                    .HeightOverride(8.0f)
                    .Visibility(TAttribute<EVisibility>::CreateLambda([TransNodePtr = _TransitionNode]()
                    {
                        return (TransNodePtr && TransNodePtr->Get_HasBreakpoint())
                            ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                    }))
                    [
                        SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor(BpRed)
                            .RenderTransformPivot(FVector2D(0.5, 0.5))
                            .RenderTransform(FSlateRenderTransform(FQuat2D(PI / 4.0)))
                    ]
            ];
    }
    // ---- Style 4: always-visible diamond, top-right (hollow=unset, filled=set) — matches State Style 23 ----
    else if (BpStyle == 4)
    {
        TransOverlay->AddSlot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Top)
            [
                SNew(SBox)
                    .WidthOverride(8.0f)
                    .HeightOverride(8.0f)
                    [
                        SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor_Lambda([TransNodePtr = _TransitionNode, BpRed, BpHollow]()
                            {
                                return (TransNodePtr && TransNodePtr->Get_HasBreakpoint())
                                    ? BpRed : BpHollow;
                            })
                            .RenderTransformPivot(FVector2D(0.5, 0.5))
                            .RenderTransform(FSlateRenderTransform(FQuat2D(PI / 4.0)))
                    ]
            ];
    }
    // ---- Style 5: always-visible diamond, top-left ----
    else if (BpStyle == 5)
    {
        TransOverlay->AddSlot()
            .HAlign(HAlign_Left)
            .VAlign(VAlign_Top)
            [
                SNew(SBox)
                    .WidthOverride(8.0f)
                    .HeightOverride(8.0f)
                    [
                        SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor_Lambda([TransNodePtr = _TransitionNode, BpRed, BpHollow]()
                            {
                                return (TransNodePtr && TransNodePtr->Get_HasBreakpoint())
                                    ? BpRed : BpHollow;
                            })
                            .RenderTransformPivot(FVector2D(0.5, 0.5))
                            .RenderTransform(FSlateRenderTransform(FQuat2D(PI / 4.0)))
                    ]
            ];
    }
    // ---- Style 6: always-visible diamond, bottom-right ----
    else if (BpStyle == 6)
    {
        TransOverlay->AddSlot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Bottom)
            [
                SNew(SBox)
                    .WidthOverride(8.0f)
                    .HeightOverride(8.0f)
                    [
                        SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor_Lambda([TransNodePtr = _TransitionNode, BpRed, BpHollow]()
                            {
                                return (TransNodePtr && TransNodePtr->Get_HasBreakpoint())
                                    ? BpRed : BpHollow;
                            })
                            .RenderTransformPivot(FVector2D(0.5, 0.5))
                            .RenderTransform(FSlateRenderTransform(FQuat2D(PI / 4.0)))
                    ]
            ];
    }
    // ---- Style 7: always-visible square, top-right ----
    else if (BpStyle == 7)
    {
        TransOverlay->AddSlot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Top)
            [
                SNew(SBox)
                    .WidthOverride(8.0f)
                    .HeightOverride(8.0f)
                    [
                        SNew(SBorder)
                            .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                            .BorderBackgroundColor_Lambda([TransNodePtr = _TransitionNode, BpRed, BpHollow]()
                            {
                                return (TransNodePtr && TransNodePtr->Get_HasBreakpoint())
                                    ? BpRed : BpHollow;
                            })
                    ]
            ];
    }
    // ---- Style 8: red border ring around the whole badge (only when set) ----
    else if (BpStyle == 8)
    {
        TransOverlay->AddSlot()
            [
                SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor_Lambda([TransNodePtr = _TransitionNode, BpRed]()
                    {
                        return (TransNodePtr && TransNodePtr->Get_HasBreakpoint())
                            ? FLinearColor(BpRed.R, BpRed.G, BpRed.B, 0.35f)
                            : FLinearColor::Transparent;
                    })
            ];
    }

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            TransOverlay
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
