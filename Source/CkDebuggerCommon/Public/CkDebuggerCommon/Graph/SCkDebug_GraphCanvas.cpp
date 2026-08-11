#include "CkDebuggerCommon/Graph/SCkDebug_GraphCanvas.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SNullWidget.h"

// =====================================================================================================================

namespace ck_debug_graph_canvas
{
    constexpr auto ZoomStep = 1.15f;
    constexpr auto PanDeadZonePx = 3.0f;
    constexpr auto MarqueeBorderThickness = 1.0f;
    constexpr auto HoverDeemphasisAlpha = 0.25f;
    constexpr auto DashLength = 8.0f;
    constexpr auto DashGap = 5.0f;

    auto Get_AnchorPoint(const FSlateRect& InRect, ECkDebug_GraphAnchor InAnchor) -> FVector2D
    {
        const auto Center = FVector2D{(InRect.Left + InRect.Right) * 0.5f,
                                      (InRect.Top + InRect.Bottom) * 0.5f};
        switch (InAnchor)
        {
            case ECkDebug_GraphAnchor::Left:
                return FVector2D{InRect.Left, Center.Y};
            case ECkDebug_GraphAnchor::Right:
                return FVector2D{InRect.Right, Center.Y};
            case ECkDebug_GraphAnchor::Top:
                return FVector2D{Center.X, InRect.Top};
            case ECkDebug_GraphAnchor::Bottom:
                return FVector2D{Center.X, InRect.Bottom};
            default:
                return Center;
        }
    }

    auto Get_ClosestPointOnRect(const FSlateRect& InRect, const FVector2D& InToward) -> FVector2D
    {
        const auto Center = FVector2D{(InRect.Left + InRect.Right) * 0.5f,
                                      (InRect.Top + InRect.Bottom) * 0.5f};
        const auto Delta = InToward - Center;
        if (FMath::IsNearlyZero(Delta.X) && FMath::IsNearlyZero(Delta.Y))
        {
            return Center;
        }

        const auto HalfWidth = FMath::Max(0.5f, (InRect.Right - InRect.Left) * 0.5f);
        const auto HalfHeight = FMath::Max(0.5f, (InRect.Bottom - InRect.Top) * 0.5f);
        const auto Scale = 1.0f / FMath::Max(FMath::Abs(Delta.X) / HalfWidth,
                                             FMath::Abs(Delta.Y) / HalfHeight);
        return Center + Delta * Scale;
    }

    auto Make_NormalizedRect(const FVector2D& InA, const FVector2D& InB) -> FSlateRect
    {
        return FSlateRect{static_cast<float>(FMath::Min(InA.X, InB.X)),
                          static_cast<float>(FMath::Min(InA.Y, InB.Y)),
                          static_cast<float>(FMath::Max(InA.X, InB.X)),
                          static_cast<float>(FMath::Max(InA.Y, InB.Y))};
    }

    auto Rects_Intersect(const FSlateRect& InA, const FSlateRect& InB) -> bool
    {
        return InA.Left <= InB.Right && InA.Right >= InB.Left && InA.Top <= InB.Bottom &&
               InA.Bottom >= InB.Top;
    }

    auto Have_SameIds(const TSet<uint64>& InA, const TSet<uint64>& InB) -> bool
    {
        if (InA.Num() != InB.Num())
        {
            return false;
        }

        for (const auto Id : InA)
        {
            if (NOT InB.Contains(Id))
            {
                return false;
            }
        }
        return true;
    }
} // namespace ck_debug_graph_canvas

// =====================================================================================================================
// CONSTRUCTION / SCENE
// =====================================================================================================================

SCkDebug_GraphCanvas::SCkDebug_GraphCanvas() : _Children(this)
{
    SetCanTick(false);
    bCanSupportFocus = true;
}

auto SCkDebug_GraphCanvas::Construct(const FArguments& InArgs) -> void
{
    _MinZoom = FMath::Max(0.01f, InArgs._MinZoom);
    _MaxZoom = FMath::Max(_MinZoom, InArgs._MaxZoom);
    _FitPadding = FMath::Max(0.0f, InArgs._FitPadding);
    _AllowNodeDragging = InArgs._AllowNodeDragging;
    _OnSelectionChanged = InArgs._OnSelectionChanged;
    _OnNodeDoubleClicked = InArgs._OnNodeDoubleClicked;
    _OnNodeContextMenu = InArgs._OnNodeContextMenu;
    _OnBackgroundContextMenu = InArgs._OnBackgroundContextMenu;
}

auto SCkDebug_GraphCanvas::Set_Scene(FCkDebug_GraphCanvasScene InScene) -> void
{
    auto PresentIds = TSet<uint64>{};
    for (const auto& Node : InScene.Nodes)
    {
        PresentIds.Add(Node.Id);
    }

    auto PreservedSelection = TSet<uint64>{};
    for (const auto Id : _SelectedNodeIds)
    {
        if (PresentIds.Contains(Id))
        {
            PreservedSelection.Add(Id);
        }
    }

    _Scene = MoveTemp(InScene);
    _Scene.Nodes.StableSort(
        [](const FCkDebug_GraphCanvasNode& InA, const FCkDebug_GraphCanvasNode& InB)
        {
            return InA.Layer < InB.Layer;
        });
    _SelectedNodeIds = MoveTemp(PreservedSelection);
    _Children.Empty();

    for (const auto& Node : _Scene.Nodes)
    {
        auto Slot = FSlot::FSlotArguments(MakeUnique<FSlot>());
        if (Node.Widget.IsValid())
        {
            _Children.AddSlot(MoveTemp(Slot[Node.Widget.ToSharedRef()]));
        }
        else
        {
            _Children.AddSlot(MoveTemp(Slot[SNullWidget::NullWidget]));
        }
    }

    // Scene consumers commonly recreate their card widgets while preserving stable IDs. Re-emit
    // even when the ID set is unchanged so those new cards rehydrate their selected visual state.
    Notify_SelectionChanged();

    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

auto SCkDebug_GraphCanvas::Set_Transform(const FCkDebug_GraphCanvasTransform& InTransform) -> void
{
    _Transform = InTransform;
    _Transform.Zoom = FMath::Clamp(_Transform.Zoom, _MinZoom, _MaxZoom);
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

auto SCkDebug_GraphCanvas::Frame_All() -> void
{
    auto Nodes = TArray<FCkDebug_GraphCanvasNodeGeometry>{};
    Nodes.Reserve(_Scene.Nodes.Num());
    for (const auto& Node : _Scene.Nodes)
    {
        Nodes.Add(Node);
    }

    const auto Size = GetCachedGeometry().GetLocalSize();
    if (Size.X <= 0.0f || Size.Y <= 0.0f)
    {
        return;
    }

    Set_Transform(Compute_FitTransform(Nodes, Size, _FitPadding, _MinZoom, _MaxZoom));
}

auto SCkDebug_GraphCanvas::Reset_View() -> void
{
    Set_Transform(FCkDebug_GraphCanvasTransform{});
}

auto SCkDebug_GraphCanvas::Set_SelectedNodeIds(TSet<uint64> InSelectedIds, bool InNotify) -> void
{
    auto ValidIds = TSet<uint64>{};
    for (const auto Id : InSelectedIds)
    {
        if (Find_Node(Id) != nullptr)
        {
            ValidIds.Add(Id);
        }
    }

    if (ck_debug_graph_canvas::Have_SameIds(_SelectedNodeIds, ValidIds))
    {
        return;
    }

    _SelectedNodeIds = MoveTemp(ValidIds);
    if (InNotify)
    {
        Notify_SelectionChanged();
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto SCkDebug_GraphCanvas::Clear_InteractionDelegates() -> void
{
    _OnSelectionChanged.Unbind();
    _OnNodeDoubleClicked.Unbind();
    _OnNodeContextMenu.Unbind();
    _OnBackgroundContextMenu.Unbind();
}

// =====================================================================================================================
// PURE GEOMETRY
// =====================================================================================================================

auto SCkDebug_GraphCanvas::World_To_Screen(const FVector2D& InWorld,
                                           const FCkDebug_GraphCanvasTransform& InTransform)
    -> FVector2D
{
    return InWorld * InTransform.Zoom + InTransform.Pan;
}

auto SCkDebug_GraphCanvas::Screen_To_World(const FVector2D& InScreen,
                                           const FCkDebug_GraphCanvasTransform& InTransform)
    -> FVector2D
{
    const auto SafeZoom = FMath::Max(KINDA_SMALL_NUMBER, InTransform.Zoom);
    return (InScreen - InTransform.Pan) / SafeZoom;
}

auto SCkDebug_GraphCanvas::Compute_FitTransform(
    const TArray<FCkDebug_GraphCanvasNodeGeometry>& InNodes,
    const FVector2D& InViewportSize,
    float InPadding,
    float InMinZoom,
    float InMaxZoom) -> FCkDebug_GraphCanvasTransform
{
    auto Result = FCkDebug_GraphCanvasTransform{};
    if (InNodes.IsEmpty() || InViewportSize.X <= 0.0f || InViewportSize.Y <= 0.0f)
    {
        return Result;
    }

    auto Bounds = FSlateRect{TNumericLimits<float>::Max(),
                             TNumericLimits<float>::Max(),
                             TNumericLimits<float>::Lowest(),
                             TNumericLimits<float>::Lowest()};
    for (const auto& Node : InNodes)
    {
        Bounds.Left = FMath::Min(Bounds.Left, Node.Position.X);
        Bounds.Top = FMath::Min(Bounds.Top, Node.Position.Y);
        Bounds.Right = FMath::Max(Bounds.Right, Node.Position.X + Node.Size.X);
        Bounds.Bottom = FMath::Max(Bounds.Bottom, Node.Position.Y + Node.Size.Y);
    }

    const auto GraphSize = FVector2D{FMath::Max(1.0f, Bounds.Right - Bounds.Left),
                                     FMath::Max(1.0f, Bounds.Bottom - Bounds.Top)};
    const auto Available = FVector2D{FMath::Max(1.0f, InViewportSize.X - 2.0f * InPadding),
                                     FMath::Max(1.0f, InViewportSize.Y - 2.0f * InPadding)};
    Result.Zoom = FMath::Clamp(FMath::Min(Available.X / GraphSize.X, Available.Y / GraphSize.Y),
                               InMinZoom,
                               InMaxZoom);

    const auto GraphCenter = FVector2D{(Bounds.Left + Bounds.Right) * 0.5f,
                                       (Bounds.Top + Bounds.Bottom) * 0.5f};
    Result.Pan = InViewportSize * 0.5f - GraphCenter * Result.Zoom;
    return Result;
}

auto SCkDebug_GraphCanvas::Compute_EdgeGeometry(const FCkDebug_GraphCanvasNodeGeometry& InSource,
                                                const FCkDebug_GraphCanvasNodeGeometry& InTarget,
                                                const FCkDebug_GraphCanvasEdge& InEdge,
                                                const FCkDebug_GraphCanvasTransform& InTransform)
    -> FCkDebug_GraphCanvasEdgeGeometry
{
    const auto SourceMin = World_To_Screen(InSource.Position, InTransform);
    const auto SourceMax = World_To_Screen(InSource.Position + InSource.Size, InTransform);
    const auto TargetMin = World_To_Screen(InTarget.Position, InTransform);
    const auto TargetMax = World_To_Screen(InTarget.Position + InTarget.Size, InTransform);
    const auto SourceRect = FSlateRect{static_cast<float>(SourceMin.X),
                                       static_cast<float>(SourceMin.Y),
                                       static_cast<float>(SourceMax.X),
                                       static_cast<float>(SourceMax.Y)};
    const auto TargetRect = FSlateRect{static_cast<float>(TargetMin.X),
                                       static_cast<float>(TargetMin.Y),
                                       static_cast<float>(TargetMax.X),
                                       static_cast<float>(TargetMax.Y)};

    auto Result = FCkDebug_GraphCanvasEdgeGeometry{};
    if (InEdge.RoutePoints.IsEmpty())
    {
        const auto TargetSeed = ck_debug_graph_canvas::Get_AnchorPoint(TargetRect,
                                                                       InEdge.TargetAnchor);
        const auto SourceSeed = ck_debug_graph_canvas::Get_AnchorPoint(SourceRect,
                                                                       InEdge.SourceAnchor);
        Result.Start = InEdge.SourceAnchor == ECkDebug_GraphAnchor::Center
                           ? ck_debug_graph_canvas::Get_ClosestPointOnRect(SourceRect, TargetSeed)
                           : SourceSeed;
        Result.End = InEdge.TargetAnchor == ECkDebug_GraphAnchor::Center
                         ? ck_debug_graph_canvas::Get_ClosestPointOnRect(TargetRect, Result.Start)
                         : TargetSeed;
        Result.Points = {Result.Start, Result.End};
        return Result;
    }

    const auto FirstRoute = World_To_Screen(InEdge.RoutePoints[0], InTransform);
    const auto LastRoute = World_To_Screen(InEdge.RoutePoints.Last(), InTransform);
    Result.Start = InEdge.SourceAnchor == ECkDebug_GraphAnchor::Center
                       ? ck_debug_graph_canvas::Get_ClosestPointOnRect(SourceRect, FirstRoute)
                       : ck_debug_graph_canvas::Get_AnchorPoint(SourceRect, InEdge.SourceAnchor);
    Result.End = InEdge.TargetAnchor == ECkDebug_GraphAnchor::Center
                     ? ck_debug_graph_canvas::Get_ClosestPointOnRect(TargetRect, LastRoute)
                     : ck_debug_graph_canvas::Get_AnchorPoint(TargetRect, InEdge.TargetAnchor);
    Result.Points.Add(Result.Start);
    for (const auto& Point : InEdge.RoutePoints)
    {
        Result.Points.Add(World_To_Screen(Point, InTransform));
    }
    Result.Points.Add(Result.End);
    return Result;
}

// =====================================================================================================================
// PANEL / PAINT
// =====================================================================================================================

auto SCkDebug_GraphCanvas::ComputeDesiredSize(float) const -> FVector2D
{
    return FVector2D{320.0f, 240.0f};
}

auto SCkDebug_GraphCanvas::OnArrangeChildren(const FGeometry& InAllottedGeometry,
                                             FArrangedChildren& OutArrangedChildren) const -> void
{
    const auto ViewportSize = InAllottedGeometry.GetLocalSize();
    const auto VisibleRect = FSlateRect{0.0f, 0.0f, ViewportSize.X, ViewportSize.Y};
    for (auto Index = 0; Index < _Scene.Nodes.Num() && Index < _Children.Num(); ++Index)
    {
        const auto& Node = _Scene.Nodes[Index];
        const auto Rect = Get_ScreenRect(Node);
        if (NOT Is_RectVisible(Rect, VisibleRect))
        {
            continue;
        }

        const auto ScreenPosition = World_To_Screen(Node.Position, _Transform);
        OutArrangedChildren.AddWidget(InAllottedGeometry.MakeChild(
            _Children[Index].GetWidget(),
            FVector2f{static_cast<float>(Node.Size.X), static_cast<float>(Node.Size.Y)},
            FSlateLayoutTransform{_Transform.Zoom,
                                  FVector2f{static_cast<float>(ScreenPosition.X),
                                            static_cast<float>(ScreenPosition.Y)}}));
    }
}

auto SCkDebug_GraphCanvas::OnPaint(const FPaintArgs& InArgs,
                                   const FGeometry& InAllottedGeometry,
                                   const FSlateRect& InCullingRect,
                                   FSlateWindowElementList& OutDrawElements,
                                   int32 InLayerId,
                                   const FWidgetStyle& InWidgetStyle,
                                   bool InParentEnabled) const -> int32
{
    const auto BackgroundBrush = FCkDebuggerStyle::Get().GetBrush(
        TEXT("CkDebugger.Graph.Background"));
    if (BackgroundBrush != nullptr)
    {
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            InLayerId,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f{static_cast<float>(InAllottedGeometry.GetLocalSize().X),
                          static_cast<float>(InAllottedGeometry.GetLocalSize().Y)},
                FSlateLayoutTransform{}),
            BackgroundBrush,
            ESlateDrawEffect::None,
            InWidgetStyle.GetColorAndOpacityTint());
    }

    auto EdgeLayer = InLayerId + 1;
    for (const auto& Edge : _Scene.Edges)
    {
        const auto* Source = Find_Node(Edge.SourceId);
        const auto* Target = Find_Node(Edge.TargetId);
        if (Source == nullptr || Target == nullptr)
        {
            continue;
        }

        const auto Geometry = Compute_EdgeGeometry(*Source, *Target, Edge, _Transform);
        if (Geometry.Points.Num() < 2)
        {
            continue;
        }
        Draw_Edge(Edge, Geometry, OutDrawElements, EdgeLayer);
    }

    Draw_Marquee(InAllottedGeometry, OutDrawElements, EdgeLayer + 1);
    return SPanel::OnPaint(InArgs,
                           InAllottedGeometry,
                           InCullingRect,
                           OutDrawElements,
                           EdgeLayer + 2,
                           InWidgetStyle,
                           InParentEnabled);
}

// =====================================================================================================================
// INPUT
// =====================================================================================================================

auto SCkDebug_GraphCanvas::OnPreviewMouseButtonDown(const FGeometry& InGeometry,
                                                    const FPointerEvent& InMouseEvent) -> FReply
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    const auto LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto HitNodeId = Find_NodeAt(LocalPosition);
    if (HitNodeId == 0)
    {
        return FReply::Unhandled();
    }

    auto NewSelection = _SelectedNodeIds;
    if (InMouseEvent.IsControlDown())
    {
        if (NewSelection.Contains(HitNodeId))
        {
            NewSelection.Remove(HitNodeId);
        }
        else
        {
            NewSelection.Add(HitNodeId);
        }
    }
    else
    {
        NewSelection.Empty();
        NewSelection.Add(HitNodeId);
    }
    Set_SelectedNodeIds(MoveTemp(NewSelection));

    // Selection is observed during the preview route so interactive card children cannot swallow
    // it. Leave the event unhandled so those children retain their existing click behavior.
    return FReply::Unhandled();
}

auto SCkDebug_GraphCanvas::OnMouseButtonDown(const FGeometry& InGeometry,
                                             const FPointerEvent& InMouseEvent) -> FReply
{
    const auto LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto HitNodeId = Find_NodeAt(LocalPosition);

    _PointerDownLocal = LocalPosition;
    _PanStart = _Transform.Pan;
    _PanWasDragged = false;

    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        _IsPanning = true;
        _ContextNodeId = HitNodeId;
        return FReply::Handled()
            .CaptureMouse(SharedThis(this))
            .SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
    {
        _IsPanning = true;
        _ContextNodeId = 0;
        return FReply::Handled()
            .CaptureMouse(SharedThis(this))
            .SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }

    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    if (HitNodeId != 0)
    {
        // Selection is already applied in the preview route. Keeping it there lets a card child
        // handle the click without suppressing graph selection; applying it again here would undo
        // Ctrl toggles.
        if (_AllowNodeDragging)
        {
            _IsDraggingNodes = true;
            _NodeDragStartWorld = Screen_To_World(LocalPosition, _Transform);
            _NodeDragStartPositions.Reset();
            for (const auto SelectedId : _SelectedNodeIds)
            {
                if (const auto* SelectedNode = Find_Node(SelectedId))
                {
                    _NodeDragStartPositions.Add(SelectedId, SelectedNode->Position);
                }
            }
            return FReply::Handled()
                .CaptureMouse(SharedThis(this))
                .SetUserFocus(SharedThis(this), EFocusCause::Mouse);
        }
        return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }

    _IsMarqueeSelecting = true;
    _MarqueeBaseSelection = InMouseEvent.IsControlDown() ? _SelectedNodeIds : TSet<uint64>{};
    return FReply::Handled()
        .CaptureMouse(SharedThis(this))
        .SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

auto SCkDebug_GraphCanvas::OnMouseButtonUp(const FGeometry& InGeometry,
                                           const FPointerEvent& InMouseEvent) -> FReply
{
    const auto LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && _IsDraggingNodes)
    {
        _IsDraggingNodes = false;
        _NodeDragStartPositions.Reset();
        return FReply::Handled().ReleaseMouseCapture();
    }

    if ((InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton ||
         InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton) &&
        _IsPanning)
    {
        _IsPanning = false;
        if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && NOT _PanWasDragged)
        {
            if (_ContextNodeId != 0)
            {
                _OnNodeContextMenu.ExecuteIfBound(_ContextNodeId, InMouseEvent);
            }
            else
            {
                _OnBackgroundContextMenu.ExecuteIfBound(Screen_To_World(LocalPosition, _Transform),
                                                        InMouseEvent);
            }
        }
        _ContextNodeId = 0;
        return FReply::Handled().ReleaseMouseCapture();
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && _IsMarqueeSelecting)
    {
        const auto Marquee = ck_debug_graph_canvas::Make_NormalizedRect(_PointerDownLocal,
                                                                        LocalPosition);
        auto NewSelection = _MarqueeBaseSelection;
        const auto MarqueeSize = FVector2D{Marquee.Right - Marquee.Left,
                                           Marquee.Bottom - Marquee.Top};
        const auto IsClick = MarqueeSize.SizeSquared() <=
                             FMath::Square(ck_debug_graph_canvas::PanDeadZonePx);
        if (IsClick)
        {
            NewSelection.Empty();
        }
        else
        {
            for (const auto& Node : _Scene.Nodes)
            {
                if (ck_debug_graph_canvas::Rects_Intersect(Marquee, Get_ScreenRect(Node)))
                {
                    NewSelection.Add(Node.Id);
                }
            }
        }
        _IsMarqueeSelecting = false;
        Set_SelectedNodeIds(MoveTemp(NewSelection));
        return FReply::Handled().ReleaseMouseCapture();
    }

    return SPanel::OnMouseButtonUp(InGeometry, InMouseEvent);
}

auto SCkDebug_GraphCanvas::OnMouseMove(const FGeometry& InGeometry,
                                       const FPointerEvent& InMouseEvent) -> FReply
{
    const auto LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto NewHoveredNodeId = Find_NodeAt(LocalPosition);
    if (_HoveredNodeId != NewHoveredNodeId)
    {
        _HoveredNodeId = NewHoveredNodeId;
        Invalidate(EInvalidateWidgetReason::Paint);
    }

    if (_IsPanning)
    {
        const auto Delta = LocalPosition - _PointerDownLocal;
        if (Delta.SizeSquared() > FMath::Square(ck_debug_graph_canvas::PanDeadZonePx))
        {
            _PanWasDragged = true;
        }
        _Transform.Pan = _PanStart + Delta;
        Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
        return FReply::Handled();
    }

    if (_IsDraggingNodes)
    {
        const auto WorldDelta = Screen_To_World(LocalPosition, _Transform) - _NodeDragStartWorld;
        for (auto& Node : _Scene.Nodes)
        {
            if (const auto* StartPosition = _NodeDragStartPositions.Find(Node.Id))
            {
                Node.Position = *StartPosition + WorldDelta;
            }
        }
        Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
        return FReply::Handled();
    }

    if (_IsMarqueeSelecting)
    {
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    return SPanel::OnMouseMove(InGeometry, InMouseEvent);
}

auto SCkDebug_GraphCanvas::OnMouseButtonDoubleClick(const FGeometry& InGeometry,
                                                    const FPointerEvent& InMouseEvent) -> FReply
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return SPanel::OnMouseButtonDoubleClick(InGeometry, InMouseEvent);
    }

    const auto LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto HitNodeId = Find_NodeAt(LocalPosition);
    if (HitNodeId == 0)
    {
        return FReply::Unhandled();
    }

    _OnNodeDoubleClicked.ExecuteIfBound(HitNodeId);
    return FReply::Handled();
}

auto SCkDebug_GraphCanvas::OnMouseWheel(const FGeometry& InGeometry,
                                        const FPointerEvent& InMouseEvent) -> FReply
{
    const auto LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto WorldAtCursor = Screen_To_World(LocalPosition, _Transform);
    _Transform.Zoom = FMath::Clamp(_Transform.Zoom * FMath::Pow(ck_debug_graph_canvas::ZoomStep,
                                                                InMouseEvent.GetWheelDelta()),
                                   _MinZoom,
                                   _MaxZoom);
    _Transform.Pan = LocalPosition - WorldAtCursor * _Transform.Zoom;
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
    return FReply::Handled();
}

auto SCkDebug_GraphCanvas::OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
    -> FReply
{
    if (InKeyEvent.GetKey() == EKeys::F)
    {
        Frame_All();
        return FReply::Handled();
    }
    if (InKeyEvent.GetKey() == EKeys::Home)
    {
        Reset_View();
        return FReply::Handled();
    }
    return SPanel::OnKeyDown(InGeometry, InKeyEvent);
}

auto SCkDebug_GraphCanvas::OnMouseCaptureLost(const FCaptureLostEvent& InCaptureLostEvent) -> void
{
    _IsPanning = false;
    _IsMarqueeSelecting = false;
    _IsDraggingNodes = false;
    _PanWasDragged = false;
    _ContextNodeId = 0;
    _NodeDragStartPositions.Reset();
    SPanel::OnMouseCaptureLost(InCaptureLostEvent);
}

// =====================================================================================================================
// PRIVATE HELPERS / DRAWING
// =====================================================================================================================

auto SCkDebug_GraphCanvas::Find_Node(uint64 InId) const -> const FCkDebug_GraphCanvasNode*
{
    return _Scene.Nodes.FindByPredicate(
        [InId](const FCkDebug_GraphCanvasNode& InNode)
        {
            return InNode.Id == InId;
        });
}

auto SCkDebug_GraphCanvas::Find_NodeAt(const FVector2D& InLocalPosition) const -> uint64
{
    auto ResultId = uint64{0};
    auto ResultLayer = TNumericLimits<int32>::Lowest();
    for (const auto& Node : _Scene.Nodes)
    {
        const auto Rect = Get_ScreenRect(Node);
        if (InLocalPosition.X >= Rect.Left && InLocalPosition.X <= Rect.Right &&
            InLocalPosition.Y >= Rect.Top && InLocalPosition.Y <= Rect.Bottom &&
            Node.Layer >= ResultLayer)
        {
            ResultId = Node.Id;
            ResultLayer = Node.Layer;
        }
    }
    return ResultId;
}

auto SCkDebug_GraphCanvas::Get_ScreenRect(const FCkDebug_GraphCanvasNodeGeometry& InNode) const
    -> FSlateRect
{
    const auto Position = World_To_Screen(InNode.Position, _Transform);
    const auto Size = InNode.Size * _Transform.Zoom;
    return FSlateRect{static_cast<float>(Position.X),
                      static_cast<float>(Position.Y),
                      static_cast<float>(Position.X + Size.X),
                      static_cast<float>(Position.Y + Size.Y)};
}

auto SCkDebug_GraphCanvas::Is_RectVisible(const FSlateRect& InRect,
                                          const FSlateRect& InCullingRect) const -> bool
{
    return ck_debug_graph_canvas::Rects_Intersect(InRect, InCullingRect);
}

auto SCkDebug_GraphCanvas::Notify_SelectionChanged() -> void
{
    _OnSelectionChanged.ExecuteIfBound(_SelectedNodeIds);
}

auto SCkDebug_GraphCanvas::Draw_Edge(const FCkDebug_GraphCanvasEdge& InEdge,
                                     const FCkDebug_GraphCanvasEdgeGeometry& InGeometry,
                                     FSlateWindowElementList& OutDrawElements,
                                     int32 InLayerId) const -> void
{
    auto Color = InEdge.Color;
    if (_HoveredNodeId != 0 && _HoveredNodeId != InEdge.SourceId &&
        _HoveredNodeId != InEdge.TargetId)
    {
        Color.A *= ck_debug_graph_canvas::HoverDeemphasisAlpha;
    }

    const auto Thickness = FMath::Max(0.5f, InEdge.Thickness * _Transform.Zoom);
    if (InEdge.IsDashed)
    {
        for (auto Index = 1; Index < InGeometry.Points.Num(); ++Index)
        {
            const auto Start = InGeometry.Points[Index - 1];
            const auto End = InGeometry.Points[Index];
            const auto Delta = End - Start;
            const auto Length = Delta.Size();
            if (Length <= KINDA_SMALL_NUMBER)
            {
                continue;
            }
            const auto Direction = Delta / Length;
            for (auto Distance = 0.0f; Distance < Length;
                 Distance += ck_debug_graph_canvas::DashLength + ck_debug_graph_canvas::DashGap)
            {
                const auto DashEnd = FMath::Min(Length,
                                                Distance + ck_debug_graph_canvas::DashLength);
                const auto DashPoints = TArray<FVector2D>{Start + Direction * Distance,
                                                          Start + Direction * DashEnd};
                FSlateDrawElement::MakeLines(OutDrawElements,
                                             InLayerId,
                                             FPaintGeometry(),
                                             DashPoints,
                                             ESlateDrawEffect::None,
                                             Color,
                                             true,
                                             Thickness);
            }
        }
    }
    else
    {
        FSlateDrawElement::MakeLines(OutDrawElements,
                                     InLayerId,
                                     FPaintGeometry(),
                                     InGeometry.Points,
                                     ESlateDrawEffect::None,
                                     Color,
                                     true,
                                     Thickness);
    }

    if (NOT InEdge.IsDirected || InGeometry.Points.Num() < 2)
    {
        return;
    }

    const auto End = InGeometry.Points.Last();
    const auto Previous = InGeometry.Points[InGeometry.Points.Num() - 2];
    const auto Delta = End - Previous;
    if (Delta.SizeSquared() <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const auto Direction = Delta.GetSafeNormal();
    const auto Normal = FVector2D{Direction.Y, -Direction.X};
    const auto ArrowLength = 10.0f * _Transform.Zoom;
    const auto ArrowHalfWidth = 4.0f * _Transform.Zoom;
    const auto ArrowBase = End - Direction * ArrowLength;
    const auto ArrowPoints = TArray<FVector2D>{ArrowBase + Normal * ArrowHalfWidth,
                                               End,
                                               ArrowBase - Normal * ArrowHalfWidth};
    FSlateDrawElement::MakeLines(OutDrawElements,
                                 InLayerId + 1,
                                 FPaintGeometry(),
                                 ArrowPoints,
                                 ESlateDrawEffect::None,
                                 Color,
                                 true,
                                 Thickness);
}

auto SCkDebug_GraphCanvas::Draw_Marquee(const FGeometry& InGeometry,
                                        FSlateWindowElementList& OutDrawElements,
                                        int32 InLayerId) const -> void
{
    if (NOT _IsMarqueeSelecting)
    {
        return;
    }

    const auto LocalPointer = InGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
    const auto Rect = ck_debug_graph_canvas::Make_NormalizedRect(_PointerDownLocal, LocalPointer);
    const auto Size = FVector2D{Rect.Right - Rect.Left, Rect.Bottom - Rect.Top};
    if (Size.SizeSquared() <= FMath::Square(ck_debug_graph_canvas::PanDeadZonePx))
    {
        return;
    }

    FSlateDrawElement::MakeBox(OutDrawElements,
                               InLayerId,
                               FPaintGeometry(FVector2D{Rect.Left, Rect.Top}, Size, 1.0f),
                               CkStyle::GetFilledBrush(),
                               ESlateDrawEffect::None,
                               CkStyle::Info().CopyWithNewOpacity(0.25f));
}
