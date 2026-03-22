#include "CkDebuggerWidget_GraphView.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"

#include "CkEcsDebugger/Graph/CkEcsGraphModel.h"
#include "CkEcsDebugger/Graph/CkEcsGraphLayoutStrategy.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Widgets/CkDebuggerWidget_GraphNode.h"

// =====================================================================================================================

auto SCkDebuggerWidget_GraphView::Construct(const FArguments& InArgs) -> void
{
    OnNodeClickedDelegate = InArgs._OnNodeClicked;

    EmptyStateWidget =
        SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Visibility(EVisibility::SelfHitTestInvisible)
        [
            SNew(STextBlock)
            .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Header"))
            .Text(FText::FromString(TEXT("Select an entity to view its relationships")))
            .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Muted)
        ];

    ChildSlot
    [
        SNew(SOverlay)

        // Background
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Graph.Background"))
            .Padding(0.0f)
            [
                SAssignNew(NodeCanvas, SConstraintCanvas)
            ]
        ]

        // Empty state overlay
        + SOverlay::Slot()
        [
            EmptyStateWidget.ToSharedRef()
        ]
    ];
}

// =====================================================================================================================

auto SCkDebuggerWidget_GraphView::RebuildFromModel(
    const FCkEcsGraphModel& InModel,
    ICkEcsGraphLayoutStrategy& InLayout) -> void
{
    // Reset pan on graph change so center node is always visible
    ViewOffset = FVector2D::ZeroVector;

    // Clear existing state
    NodeEntries.Reset();
    EdgeEntries.Reset();
    NodeCanvas->ClearChildren();

    if (InModel.IsEmpty())
    {
        EmptyStateWidget->SetVisibility(EVisibility::SelfHitTestInvisible);
        return;
    }

    EmptyStateWidget->SetVisibility(EVisibility::Collapsed);

    // Compute layout positions
    const auto& ModelNodes = InModel.Get_Nodes();
    const auto& ModelEdges = InModel.Get_Edges();
    const auto LayoutPositions = InLayout.ComputeLayout(
        ModelNodes, ModelEdges, InModel.Get_CenterNodeIndex());

    // Build position lookup
    TMap<int32, FVector2D> PositionMap;
    PositionMap.Reserve(LayoutPositions.Num());
    for (const auto& LP : LayoutPositions)
    {
        PositionMap.Add(LP.NodeIndex, LP.Position);
    }

    // Create node widgets
    NodeEntries.Reserve(ModelNodes.Num());
    for (int32 i = 0; i < ModelNodes.Num(); ++i)
    {
        const auto& ModelNode = ModelNodes[i];

        auto& Entry = NodeEntries.AddDefaulted_GetRef();
        Entry.Entity = ModelNode.Entity;
        Entry.GraphPosition = PositionMap.FindRef(i);

        SAssignNew(Entry.Widget, SCkDebuggerWidget_GraphNode)
            .Label(ModelNode.Label)
            .NodeColor(ModelNode.Color)
            .IsCenter(ModelNode.IsCenterNode);

        // Add to canvas — initial offset will be set in UpdateNodeScreenPositions
        NodeCanvas->AddSlot()
            .AutoSize(true)
            .Offset(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
            .Expose(Entry.Slot)
            [
                Entry.Widget.ToSharedRef()
            ];
    }

    // Store edge data for OnPaint
    EdgeEntries.Reserve(ModelEdges.Num());
    for (const auto& ModelEdge : ModelEdges)
    {
        auto& Entry = EdgeEntries.AddDefaulted_GetRef();
        Entry.SourceIndex = ModelEdge.SourceNodeIndex;
        Entry.TargetIndex = ModelEdge.TargetNodeIndex;
        Entry.Color = ModelEdge.Color;
        Entry.Label = ModelEdge.Label;
    }

    bPositionsDirty = true;
}

// =====================================================================================================================

auto SCkDebuggerWidget_GraphView::ClearGraph() -> void
{
    NodeEntries.Reset();
    EdgeEntries.Reset();
    NodeCanvas->ClearChildren();
    EmptyStateWidget->SetVisibility(EVisibility::SelfHitTestInvisible);
}

// =====================================================================================================================

auto SCkDebuggerWidget_GraphView::Tick(
    const FGeometry& AllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    const auto CurrentSize = AllottedGeometry.GetLocalSize();

    // Detect size changes (window resize)
    if (NOT CurrentSize.Equals(CachedViewSize, 1.0f))
    {
        CachedViewSize = CurrentSize;
        bPositionsDirty = true;
    }

    if (bPositionsDirty && NOT NodeEntries.IsEmpty())
    {
        UpdateNodeScreenPositions(CurrentSize);
        bPositionsDirty = false;
    }
}

// =====================================================================================================================

auto SCkDebuggerWidget_GraphView::OnPaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled) const -> int32
{
    // Draw edges BEFORE children so they appear behind nodes
    const auto ViewSize = AllottedGeometry.GetLocalSize();
    // Node size is constant (not affected by zoom) for readability
    const auto HalfNode = FVector2D(
        FCkDebuggerStyle::GraphNode_Width,
        FCkDebuggerStyle::GraphNode_Height) * 0.5f;

    // Paint children (background + nodes) first
    const int32 ChildLayerId = SCompoundWidget::OnPaint(
        Args, AllottedGeometry, MyCullingRect, OutDrawElements,
        LayerId, InWidgetStyle, bParentEnabled);

    // Draw edges ON TOP of the background but they'll visually interleave with nodes.
    // Using ChildLayerId ensures edges paint above the background border.
    const auto EdgeLayerId = ChildLayerId + 1;
    const auto LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);

    for (const auto& Edge : EdgeEntries)
    {
        if (NOT NodeEntries.IsValidIndex(Edge.SourceIndex)
            || NOT NodeEntries.IsValidIndex(Edge.TargetIndex))
        {
            continue;
        }

        const auto SourceScreen = GraphToScreen(
            NodeEntries[Edge.SourceIndex].GraphPosition, ViewSize) + HalfNode;
        const auto TargetScreen = GraphToScreen(
            NodeEntries[Edge.TargetIndex].GraphPosition, ViewSize) + HalfNode;

        const auto EdgeColor = Edge.Color.CopyWithNewOpacity(0.6f);
        const auto LineThickness = FMath::Max(1.5f, 2.0f * ZoomLevel);

        // Draw the main line
        TArray<FVector2D> LinePoints;
        LinePoints.Add(SourceScreen);
        LinePoints.Add(TargetScreen);

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            EdgeLayerId,
            AllottedGeometry.ToPaintGeometry(),
            LinePoints,
            ESlateDrawEffect::None,
            EdgeColor,
            true,
            LineThickness);

        // Draw a directional arrow near the target end
        const auto Direction = (TargetScreen - SourceScreen).GetSafeNormal();
        const auto ArrowTip = TargetScreen - Direction * (HalfNode.Y + 4.0f);
        const auto ArrowPerp = FVector2D(-Direction.Y, Direction.X);
        constexpr float ArrowSize = 10.0f;

        TArray<FVector2D> ArrowLeft;
        ArrowLeft.Add(ArrowTip);
        ArrowLeft.Add(ArrowTip - Direction * ArrowSize + ArrowPerp * ArrowSize * 0.5f);

        TArray<FVector2D> ArrowRight;
        ArrowRight.Add(ArrowTip);
        ArrowRight.Add(ArrowTip - Direction * ArrowSize - ArrowPerp * ArrowSize * 0.5f);

        FSlateDrawElement::MakeLines(OutDrawElements, EdgeLayerId,
            AllottedGeometry.ToPaintGeometry(), ArrowLeft,
            ESlateDrawEffect::None, EdgeColor, true, LineThickness);

        FSlateDrawElement::MakeLines(OutDrawElements, EdgeLayerId,
            AllottedGeometry.ToPaintGeometry(), ArrowRight,
            ESlateDrawEffect::None, EdgeColor, true, LineThickness);

        // Draw edge label offset from midpoint perpendicular to the edge
        if (NOT Edge.Label.IsEmpty())
        {
            const auto MidPoint = (SourceScreen + TargetScreen) * 0.5f;
            const auto EdgePerp = FVector2D(-Direction.Y, Direction.X);

            // Offset label to the right side of the edge
            constexpr float LabelOffset = 8.0f;
            const auto OffsetMidPoint = MidPoint + EdgePerp * LabelOffset;

            const auto LabelText = Edge.Label.ToString();
            const auto TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()
                ->Measure(LabelText, LabelFont);

            const auto LabelPos = OffsetMidPoint - FVector2D(TextSize.X, TextSize.Y) * 0.5f;

            FSlateDrawElement::MakeText(
                OutDrawElements,
                EdgeLayerId + 1,
                AllottedGeometry.ToPaintGeometry(LabelPos, TextSize),
                LabelText,
                LabelFont,
                ESlateDrawEffect::None,
                Edge.Color.CopyWithNewOpacity(0.8f));
        }
    }

    return EdgeLayerId + 2;
}

// =====================================================================================================================

auto SCkDebuggerWidget_GraphView::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent) -> FReply
{
    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        bIsPanning = true;
        PanStartMousePosition = MouseEvent.GetScreenSpacePosition();
        PanStartViewOffset = ViewOffset;
        return FReply::Handled().CaptureMouse(AsShared());
    }

    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const auto LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
        const auto ViewSize = MyGeometry.GetLocalSize();
        const auto HitIndex = HitTestNode(LocalPos, ViewSize);

        if (HitIndex != INDEX_NONE)
        {
            DraggedNodeIndex = HitIndex;
            DragStartMousePosition = MouseEvent.GetScreenSpacePosition();
            DragStartNodePosition = NodeEntries[HitIndex].GraphPosition;
            bDragThresholdMet = false;
            return FReply::Handled().CaptureMouse(AsShared());
        }
    }

    return FReply::Unhandled();
}

auto SCkDebuggerWidget_GraphView::OnMouseButtonUp(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent) -> FReply
{
    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bIsPanning)
    {
        bIsPanning = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && DraggedNodeIndex != INDEX_NONE)
    {
        const auto ClickedIndex = DraggedNodeIndex;
        DraggedNodeIndex = INDEX_NONE;

        // If we didn't drag past the threshold, treat as a click
        if (NOT bDragThresholdMet && NodeEntries.IsValidIndex(ClickedIndex))
        {
            OnNodeWidgetClicked(NodeEntries[ClickedIndex].Entity);
        }

        return FReply::Handled().ReleaseMouseCapture();
    }

    return FReply::Unhandled();
}

auto SCkDebuggerWidget_GraphView::OnMouseMove(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent) -> FReply
{
    if (bIsPanning)
    {
        const auto CurrentMousePos = MouseEvent.GetScreenSpacePosition();
        const auto Delta = (CurrentMousePos - PanStartMousePosition) / ZoomLevel;
        ViewOffset = PanStartViewOffset + Delta;
        bPositionsDirty = true;
        return FReply::Handled();
    }

    if (DraggedNodeIndex != INDEX_NONE && NodeEntries.IsValidIndex(DraggedNodeIndex))
    {
        const auto CurrentMousePos = MouseEvent.GetScreenSpacePosition();
        const auto MouseDelta = CurrentMousePos - DragStartMousePosition;

        // Check drag threshold
        if (NOT bDragThresholdMet)
        {
            if (MouseDelta.Size() >= DragThreshold)
            {
                bDragThresholdMet = true;
            }
            else
            {
                return FReply::Handled();
            }
        }

        // Move the node in graph space
        const auto GraphDelta = MouseDelta / ZoomLevel;
        NodeEntries[DraggedNodeIndex].GraphPosition = DragStartNodePosition + GraphDelta;
        bPositionsDirty = true;
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

auto SCkDebuggerWidget_GraphView::OnMouseWheel(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent) -> FReply
{
    const auto MouseLocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    const auto ViewSize = MyGeometry.GetLocalSize();
    const auto GraphPosBeforeZoom = ScreenToGraph(MouseLocalPos, ViewSize);

    const auto WheelDelta = MouseEvent.GetWheelDelta();
    const auto NewZoom = FMath::Clamp(ZoomLevel + ZoomStep * WheelDelta, ZoomMin, ZoomMax);

    if (FMath::IsNearlyEqual(NewZoom, ZoomLevel))
    {
        return FReply::Handled();
    }

    ZoomLevel = NewZoom;

    // Adjust ViewOffset so the point under the cursor stays fixed
    const auto GraphPosAfterZoom = ScreenToGraph(MouseLocalPos, ViewSize);
    ViewOffset += (GraphPosAfterZoom - GraphPosBeforeZoom);

    bPositionsDirty = true;
    return FReply::Handled();
}

// =====================================================================================================================

auto SCkDebuggerWidget_GraphView::GraphToScreen(
    const FVector2D& InGraphPos,
    const FVector2D& InViewSize) const -> FVector2D
{
    const auto Center = InViewSize * 0.5f;
    return Center + (InGraphPos + ViewOffset) * ZoomLevel;
}

auto SCkDebuggerWidget_GraphView::ScreenToGraph(
    const FVector2D& InScreenPos,
    const FVector2D& InViewSize) const -> FVector2D
{
    const auto Center = InViewSize * 0.5f;
    return (InScreenPos - Center) / ZoomLevel - ViewOffset;
}

// =====================================================================================================================

auto SCkDebuggerWidget_GraphView::OnNodeWidgetClicked(FCk_Handle InEntity) -> void
{
    OnNodeClickedDelegate.ExecuteIfBound(InEntity);
}

// =====================================================================================================================

auto SCkDebuggerWidget_GraphView::HitTestNode(
    const FVector2D& InScreenPos,
    const FVector2D& InViewSize) const -> int32
{
    const auto NodeW = FCkDebuggerStyle::GraphNode_Width;
    const auto NodeH = FCkDebuggerStyle::GraphNode_Height;

    // Iterate in reverse so topmost (last-drawn) nodes are hit first
    for (int32 i = NodeEntries.Num() - 1; i >= 0; --i)
    {
        const auto NodeScreenPos = GraphToScreen(NodeEntries[i].GraphPosition, InViewSize);

        if (InScreenPos.X >= NodeScreenPos.X && InScreenPos.X <= NodeScreenPos.X + NodeW
            && InScreenPos.Y >= NodeScreenPos.Y && InScreenPos.Y <= NodeScreenPos.Y + NodeH)
        {
            return i;
        }
    }

    return INDEX_NONE;
}

// =====================================================================================================================

auto SCkDebuggerWidget_GraphView::UpdateNodeScreenPositions(const FVector2D& InViewSize) -> void
{
    for (auto& Entry : NodeEntries)
    {
        if (Entry.Slot == nullptr)
        {
            continue;
        }

        const auto ScreenPos = GraphToScreen(Entry.GraphPosition, InViewSize);

        // SConstraintCanvas with AutoSize=true: Left=X, Top=Y position the widget.
        // Right and Bottom are ignored; widget sizes itself via its DesiredSize.
        Entry.Slot->SetOffset(FMargin(ScreenPos.X, ScreenPos.Y, 0.0f, 0.0f));
    }

    NodeCanvas->Invalidate(EInvalidateWidgetReason::Layout);
}
