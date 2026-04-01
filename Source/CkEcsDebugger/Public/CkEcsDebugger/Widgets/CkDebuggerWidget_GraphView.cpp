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

    // Clip all content (nodes, edges, labels) to the graph view bounds
    SetClipping(EWidgetClipping::ClipToBounds);

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

    if (NOT NodeCanvas.IsValid())
    { return; }

    NodeCanvas->ClearChildren();

    if (InModel.IsEmpty())
    {
        if (EmptyStateWidget.IsValid())
        {
            EmptyStateWidget->SetVisibility(EVisibility::SelfHitTestInvisible);
        }
        return;
    }

    if (EmptyStateWidget.IsValid())
    {
        EmptyStateWidget->SetVisibility(EVisibility::Collapsed);
    }

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

    if (NodeCanvas.IsValid())
    {
        NodeCanvas->ClearChildren();
    }
    if (EmptyStateWidget.IsValid())
    {
        EmptyStateWidget->SetVisibility(EVisibility::SelfHitTestInvisible);
    }
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
    const auto HalfNodeY = FCkDebuggerStyle::GraphNode_Height * 0.5f;

    // Paint children (background + nodes) first
    const int32 ChildLayerId = SCompoundWidget::OnPaint(
        Args, AllottedGeometry, MyCullingRect, OutDrawElements,
        LayerId, InWidgetStyle, bParentEnabled);

    const auto EdgeLayerId = ChildLayerId + 1;
    const auto LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);

    // Count duplicate edges per source-target pair so only truly overlapping edges curve
    TMap<uint64, int32> EdgePairCount;
    for (const auto& Edge : EdgeEntries)
    {
        const uint64 PairKey = (static_cast<uint64>(FMath::Min(Edge.SourceIndex, Edge.TargetIndex)) << 32)
            | static_cast<uint64>(FMath::Max(Edge.SourceIndex, Edge.TargetIndex));
        EdgePairCount.FindOrAdd(PairKey, 0)++;
    }

    // Track how many edges we've drawn per pair for offset calculation
    TMap<uint64, int32> EdgePairDrawn;

    for (const auto& Edge : EdgeEntries)
    {
        if (NOT NodeEntries.IsValidIndex(Edge.SourceIndex)
            || NOT NodeEntries.IsValidIndex(Edge.TargetIndex))
        {
            continue;
        }

        const auto SourceCenter = GraphToScreen(
            NodeEntries[Edge.SourceIndex].GraphPosition, ViewSize);
        const auto TargetCenter = GraphToScreen(
            NodeEntries[Edge.TargetIndex].GraphPosition, ViewSize);

        const auto EdgeColor = Edge.Color.CopyWithNewOpacity(0.7f);
        const auto LineThickness = FMath::Max(1.5f, 2.0f * ZoomLevel);
        const auto Direction = (TargetCenter - SourceCenter).GetSafeNormal();
        const auto Perp = FVector2D(-Direction.Y, Direction.X);
        const auto Distance = FVector2D::Distance(SourceCenter, TargetCenter);

        // Compute curve offset for duplicate edges
        const uint64 PairKey = (static_cast<uint64>(FMath::Min(Edge.SourceIndex, Edge.TargetIndex)) << 32)
            | static_cast<uint64>(FMath::Max(Edge.SourceIndex, Edge.TargetIndex));
        const auto PairTotal = EdgePairCount.FindRef(PairKey);
        auto& DrawnCount = EdgePairDrawn.FindOrAdd(PairKey, 0);
        const auto PairIndex = DrawnCount++;

        auto CurveOffset = 0.0f;
        if (PairTotal > 1)
        {
            CurveOffset = (static_cast<float>(PairIndex) - static_cast<float>(PairTotal - 1) * 0.5f) * 25.0f;
        }

        // Cubic bezier control points — subtle curve for all edges, larger for duplicates
        const auto BaseOffset = FMath::Min(Distance * 0.12f, 30.0f);
        const auto P0 = SourceCenter;
        const auto P3 = TargetCenter;
        const auto P1 = P0 + Direction * Distance * 0.33f + Perp * (BaseOffset + CurveOffset);
        const auto P2 = P0 + Direction * Distance * 0.67f + Perp * (BaseOffset + CurveOffset);

        // Tessellate bezier into line segments
        constexpr int32 NumSegments = 20;
        TArray<FVector2D> CurvePoints;
        CurvePoints.Reserve(NumSegments + 1);
        for (int32 Seg = 0; Seg <= NumSegments; ++Seg)
        {
            const auto T = static_cast<float>(Seg) / static_cast<float>(NumSegments);
            const auto OneMinusT = 1.0f - T;
            const auto Point = OneMinusT * OneMinusT * OneMinusT * P0
                + 3.0f * OneMinusT * OneMinusT * T * P1
                + 3.0f * OneMinusT * T * T * P2
                + T * T * T * P3;
            CurvePoints.Add(Point);
        }

        const auto PaintGeometry = AllottedGeometry.ToPaintGeometry(
            AllottedGeometry.GetLocalSize(), FSlateLayoutTransform());

        FSlateDrawElement::MakeLines(
            OutDrawElements, EdgeLayerId, PaintGeometry, CurvePoints,
            ESlateDrawEffect::None, EdgeColor, true, LineThickness);

        // Arrow head aligned with bezier tangent at endpoint
        const auto ArrowDir = (CurvePoints.Last() - CurvePoints[CurvePoints.Num() - 2]).GetSafeNormal();
        const auto ArrowTip = TargetCenter - ArrowDir * (HalfNodeY + 4.0f);
        const auto ArrowPerp = FVector2D(-ArrowDir.Y, ArrowDir.X);
        constexpr float ArrowSize = 10.0f;

        TArray<FVector2D> ArrowLeft;
        ArrowLeft.Add(ArrowTip);
        ArrowLeft.Add(ArrowTip - ArrowDir * ArrowSize + ArrowPerp * ArrowSize * 0.5f);

        TArray<FVector2D> ArrowRight;
        ArrowRight.Add(ArrowTip);
        ArrowRight.Add(ArrowTip - ArrowDir * ArrowSize - ArrowPerp * ArrowSize * 0.5f);

        FSlateDrawElement::MakeLines(OutDrawElements, EdgeLayerId, PaintGeometry, ArrowLeft,
            ESlateDrawEffect::None, EdgeColor, true, LineThickness);

        FSlateDrawElement::MakeLines(OutDrawElements, EdgeLayerId, PaintGeometry, ArrowRight,
            ESlateDrawEffect::None, EdgeColor, true, LineThickness);

        // Edge label at bezier midpoint (T=0.5)
        if (NOT Edge.Label.IsEmpty())
        {
            const auto LabelText = Edge.Label.ToString();
            const auto TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()
                ->Measure(LabelText, LabelFont);

            // Bezier at T=0.5: coefficients are 0.125, 0.375, 0.375, 0.125
            const auto BezierMid = 0.125f * P0 + 0.375f * P1 + 0.375f * P2 + 0.125f * P3;
            // Tangent at T=0.5 for perpendicular offset
            const auto BezierTangent = (-0.75f * P0 + -0.75f * P1 + 0.75f * P2 + 0.75f * P3).GetSafeNormal();
            const auto LabelPerp = FVector2D(-BezierTangent.Y, BezierTangent.X);
            const auto LabelCenter = BezierMid + LabelPerp * 8.0f;
            const auto LabelPos = LabelCenter - FVector2D(TextSize.X, TextSize.Y) * 0.5f;

            FSlateDrawElement::MakeText(
                OutDrawElements,
                EdgeLayerId + 1,
                AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(LabelPos))),
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
        SetCursor(EMouseCursor::GrabHandClosed);
        return FReply::Handled().CaptureMouse(AsShared());
    }

    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const auto AbsolutePos = MouseEvent.GetScreenSpacePosition();
        const auto ViewSize = MyGeometry.GetLocalSize();
        const auto HitIndex = HitTestNode(AbsolutePos, ViewSize);

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
        SetCursor(EMouseCursor::Default);
        return FReply::Handled().ReleaseMouseCapture();
    }

    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && DraggedNodeIndex != INDEX_NONE)
    {
        const auto ClickedIndex = DraggedNodeIndex;
        DraggedNodeIndex = INDEX_NONE;
        SetCursor(EMouseCursor::Default);

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
                SetCursor(EMouseCursor::GrabHandClosed);
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
    const FVector2D& InAbsolutePos,
    const FVector2D& InViewSize) const -> int32
{
    // Test against each node widget's actual painted geometry to avoid
    // coordinate space mismatches between the view and the canvas.
    for (int32 i = NodeEntries.Num() - 1; i >= 0; --i)
    {
        if (NOT NodeEntries[i].Widget.IsValid())
        {
            continue;
        }

        const auto& NodeGeometry = NodeEntries[i].Widget->GetCachedGeometry();
        if (NodeGeometry.IsUnderLocation(InAbsolutePos))
        {
            return i;
        }
    }

    return INDEX_NONE;
}

// =====================================================================================================================

auto SCkDebuggerWidget_GraphView::UpdateNodeScreenPositions(const FVector2D& InViewSize) -> void
{
    // Center the node widget on its graph position
    const auto HalfNode = FVector2D(
        FCkDebuggerStyle::GraphNode_Width,
        FCkDebuggerStyle::GraphNode_Height) * 0.5f;

    for (auto& Entry : NodeEntries)
    {
        if (Entry.Slot == nullptr)
        {
            continue;
        }

        const auto ScreenCenter = GraphToScreen(Entry.GraphPosition, InViewSize);
        const auto TopLeft = ScreenCenter - HalfNode;

        Entry.Slot->SetOffset(FMargin(TopLeft.X, TopLeft.Y, 0.0f, 0.0f));
    }

    if (NodeCanvas.IsValid())
    {
        NodeCanvas->Invalidate(EInvalidateWidgetReason::Layout);
    }
}
