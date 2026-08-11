#include "CkDebuggerCommon/Graph/SCkDebug_GraphCanvas.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"
#include "Animation/CurveSequence.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#include <array>

// =====================================================================================================================

namespace ck_debug_graph_canvas
{
    constexpr auto HoverDeemphasisAlpha = 0.25f;
    constexpr auto LegacyGridSnapSize = 16.0f;
    constexpr auto LegacyGridRulePeriod = 8;
    constexpr auto LegacySmallestGridSize = 8.0f;
    constexpr auto LegacyZoomLevels = std::array{
        0.100f, 0.125f, 0.150f, 0.175f, 0.200f, 0.225f, 0.250f, 0.375f, 0.500f, 0.675f,
        0.750f, 0.875f, 1.000f, 1.250f, 1.375f, 1.500f, 1.675f, 1.750f, 1.875f, 2.000f};
    constexpr auto LegacyZoomTexts = std::array{
        TEXT("-12"), TEXT("-11"), TEXT("-10"), TEXT("-9"), TEXT("-8"), TEXT("-7"),
        TEXT("-6"), TEXT("-5"), TEXT("-4"), TEXT("-3"), TEXT("-2"), TEXT("-1"),
        TEXT("1:1"), TEXT("+1"), TEXT("+2"), TEXT("+3"), TEXT("+4"), TEXT("+5"),
        TEXT("+6"), TEXT("+7")};

    enum class EMarqueeOperation : uint8 { Replace, Add, Invert, Remove };

    auto Get_MarqueeOperation(const FPointerEvent& InMouseEvent) -> EMarqueeOperation
    {
        if (InMouseEvent.IsAltDown()) return EMarqueeOperation::Remove;
        if (InMouseEvent.IsControlDown()) return EMarqueeOperation::Invert;
        return InMouseEvent.IsShiftDown() ? EMarqueeOperation::Add : EMarqueeOperation::Replace;
    }

    auto Fancy_Mod(float InValue, float InSize) -> float
    {
        return (InValue >= 0.0f ? 0.0f : InSize) + FMath::Fmod(InValue, InSize);
    }

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
    _OnNodeMoved = InArgs._OnNodeMoved;
    _OnNodeContextMenu = InArgs._OnNodeContextMenu;
    _OnBackgroundContextMenu = InArgs._OnBackgroundContextMenu;
    _LegacyZoomLevel = Get_LegacyZoomLevel(_Transform.Zoom);
    _ZoomLevelFade = FCurveSequence(0.0f, 1.0f);
    _ZoomLevelFade.Play(AsShared());
}

auto SCkDebug_GraphCanvas::Set_Scene(FCkDebug_GraphCanvasScene InScene) -> void
{
    InScene.Nodes.StableSort(
        [](const FCkDebug_GraphCanvasNode& InA, const FCkDebug_GraphCanvasNode& InB)
        {
            return InA.Layer < InB.Layer;
        });

    auto CanReuseChildSlots = _Scene.Nodes.Num() == InScene.Nodes.Num();
    if (CanReuseChildSlots)
    {
        for (auto Index = 0; Index < InScene.Nodes.Num(); ++Index)
        {
            const auto& Previous = _Scene.Nodes[Index];
            const auto& Next = InScene.Nodes[Index];
            if (Previous.Id != Next.Id || Previous.Widget != Next.Widget)
            {
                CanReuseChildSlots = false;
                break;
            }
        }
    }

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
    _SelectedNodeIds = MoveTemp(PreservedSelection);
    if (NOT CanReuseChildSlots)
    {
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

        // Scene consumers may replace card widgets while preserving stable IDs. Re-emit only when
        // the child slots actually changed so new cards rehydrate their selected visual state.
        Notify_SelectionChanged();
    }

    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

auto SCkDebug_GraphCanvas::Set_Transform(const FCkDebug_GraphCanvasTransform& InTransform) -> void
{
    _Transform = InTransform;
    _Transform.Zoom = FMath::Clamp(_Transform.Zoom, _MinZoom, _MaxZoom);
    _LegacyZoomLevel = Get_LegacyZoomLevel(_Transform.Zoom);
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
    _OnNodeMoved.Unbind();
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
    }
    else
    {
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
    }

    // The retained GraphEditor connection policies offset straight reciprocal wires by 4.5 px,
    // then shorten both ends by the 8 px arrow radius. Adapters opt into those exact values; routed
    // SM edges keep their authored route points and therefore leave LineSeparation at zero.
    if (NOT FMath::IsNearlyZero(InEdge.LineSeparation) && Result.Points.Num() == 2)
    {
        const auto Delta = Result.End - Result.Start;
        if (Delta.SizeSquared() > KINDA_SMALL_NUMBER)
        {
            const auto UnitDelta = Delta.GetSafeNormal();
            const auto Normal = FVector2D{Delta.Y, -Delta.X}.GetSafeNormal();
            const auto DirectionBias = Normal * InEdge.LineSeparation;
            const auto LengthBias = UnitDelta * FMath::Max(0.0f, InEdge.ArrowRadius);
            Result.Start += DirectionBias + LengthBias;
            Result.End += DirectionBias - LengthBias;
            Result.Points[0] = Result.Start;
            Result.Points[1] = Result.End;
        }
    }
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
    auto EdgeLayer = Draw_Background(InAllottedGeometry, OutDrawElements, InLayerId);
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
    Draw_MarqueePreview(OutDrawElements, EdgeLayer + 2);
    Draw_ZoomLabel(InAllottedGeometry, OutDrawElements, EdgeLayer + 3);
    return SPanel::OnPaint(InArgs,
                           InAllottedGeometry,
                           InCullingRect,
                           OutDrawElements,
                           EdgeLayer + 4,
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
            _NodeDragExceededDeadZone = false;
            _DraggedNodeId = HitNodeId;
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
    _MarqueeBaseSelection = _SelectedNodeIds;
    _MarqueePreviewSelection.Reset();
    _MarqueeOperation = static_cast<uint8>(ck_debug_graph_canvas::Get_MarqueeOperation(InMouseEvent));
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
        auto MovedNodes = TArray<TPair<uint64, FVector2D>>{};
        if (_NodeDragExceededDeadZone)
        {
            for (const auto& [Id, StartPosition] : _NodeDragStartPositions)
            {
                if (const auto* Node = Find_Node(Id); Node != nullptr && NOT Node->Position.Equals(StartPosition))
                {
                    MovedNodes.Emplace(Id, Node->Position);
                }
            }
            MovedNodes.Sort([](const auto& InA, const auto& InB) { return InA.Key < InB.Key; });
        }
        _IsDraggingNodes = false;
        _NodeDragStartPositions.Reset();
        _DraggedNodeId = 0;
        _NodeDragExceededDeadZone = false;
        for (const auto& [Id, Position] : MovedNodes)
        {
            _OnNodeMoved.ExecuteIfBound(Id, Position);
        }
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
        auto NewSelection = Get_MarqueeSelection();
        const auto MarqueeSize = FVector2D{Marquee.Right - Marquee.Left,
                                            Marquee.Bottom - Marquee.Top};
        const auto IsClick = MarqueeSize.SizeSquared() <=
                              FMath::Square(FSlateApplication::Get().GetDragTriggerDistance());
        if (IsClick)
        {
            NewSelection.Empty();
        }
        else { NewSelection = Get_MarqueeSelection(); }
        _IsMarqueeSelecting = false;
        _MarqueePreviewSelection.Reset();
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
        if (Delta.SizeSquared() > FMath::Square(FSlateApplication::Get().GetDragTriggerDistance()))
        {
            _PanWasDragged = true;
        }
        _Transform.Pan = _PanStart + Delta;
        Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
        return FReply::Handled();
    }

    if (_IsDraggingNodes)
    {
        if (!_NodeDragExceededDeadZone &&
            (LocalPosition - _PointerDownLocal).SizeSquared() <=
                FMath::Square(FSlateApplication::Get().GetDragTriggerDistance()))
        {
            return FReply::Handled();
        }
        _NodeDragExceededDeadZone = true;
        const auto WorldDelta = Screen_To_World(LocalPosition, _Transform) - _NodeDragStartWorld;
        auto SnappedWorldDelta = WorldDelta;
        if (const auto* DraggedStartPosition = _NodeDragStartPositions.Find(_DraggedNodeId))
        {
            const auto UnsnappedPosition = *DraggedStartPosition + WorldDelta;
            const auto SnappedPosition = FVector2D{
                ck_debug_graph_canvas::LegacyGridSnapSize * FMath::RoundToFloat(
                    UnsnappedPosition.X / ck_debug_graph_canvas::LegacyGridSnapSize),
                ck_debug_graph_canvas::LegacyGridSnapSize * FMath::RoundToFloat(
                    UnsnappedPosition.Y / ck_debug_graph_canvas::LegacyGridSnapSize)};
            SnappedWorldDelta = SnappedPosition - *DraggedStartPosition;
        }
        for (auto& Node : _Scene.Nodes)
        {
            if (const auto* StartPosition = _NodeDragStartPositions.Find(Node.Id))
            {
                Node.Position = *StartPosition + SnappedWorldDelta;
            }
        }
        Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
        return FReply::Handled();
    }

    if (_IsMarqueeSelecting)
    {
        _MarqueePreviewSelection = Get_MarqueeSelection();
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
    Apply_LegacyZoom(FMath::TruncToInt(FMath::RoundFromZero(InMouseEvent.GetWheelDelta())),
                     LocalPosition,
                     InMouseEvent.IsControlDown());
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
    _MarqueePreviewSelection.Reset();
    _NodeDragStartPositions.Reset();
    _DraggedNodeId = 0;
    _NodeDragExceededDeadZone = false;
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
    if (InEdge.DeemphasizeWhenUnrelatedHovered && _HoveredNodeId != 0 && _HoveredNodeId != InEdge.SourceId &&
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
            const auto DashLength = FMath::Max(0.5f, InEdge.DashLength);
            const auto DashGap = FMath::Max(0.0f, InEdge.DashGap);
            for (auto Distance = 0.0f; Distance < Length;
                 Distance += DashLength + DashGap)
            {
                const auto DashEnd = FMath::Min(Length,
                                                Distance + DashLength);
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

    // Match FConnectionDrawingPolicy: the 16 px Graph.Arrow brush is centered on the endpoint and
    // rotated along the final segment. The Common-local key keeps Editor and packaged output equal.
    const auto* ArrowImage = FCkDebuggerStyle::Get().GetBrush(TEXT("CkDebugger.Graph.Arrow"));
    if (ArrowImage == nullptr || ArrowImage->GetDrawType() == ESlateBrushDrawType::NoDrawType)
    {
        return;
    }

    const auto ArrowRadius = FVector2D{FMath::Max(0.0f, InEdge.ArrowRadius),
                                       FMath::Max(0.0f, InEdge.ArrowRadius)};
    const auto ArrowDrawPosition = End - ArrowRadius;
    const auto AngleInRadians = FMath::Atan2(Delta.Y, Delta.X);
    FSlateDrawElement::MakeRotatedBox(OutDrawElements,
                                      InLayerId + 1,
                                      FPaintGeometry(ArrowDrawPosition,
                                                     ArrowImage->ImageSize * _Transform.Zoom,
                                                     _Transform.Zoom),
                                      ArrowImage,
                                      ESlateDrawEffect::None,
                                      AngleInRadians,
                                      TOptional<FVector2D>{},
                                      FSlateDrawElement::RelativeToElement,
                                      Color);
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
    if (Size.SizeSquared() <= FMath::Square(FSlateApplication::Get().GetDragTriggerDistance()))
    {
        return;
    }

    const auto* MarqueeBrush = FCkDebuggerStyle::Get().GetBrush(TEXT("CkDebugger.Graph.MarqueeSelection"));
    FSlateDrawElement::MakeBox(OutDrawElements,
                               InLayerId,
                               FPaintGeometry(FVector2D{Rect.Left, Rect.Top}, Size, 1.0f),
                               MarqueeBrush);
}

auto SCkDebug_GraphCanvas::Draw_MarqueePreview(FSlateWindowElementList& OutDrawElements,
                                                int32 InLayerId) const -> void
{
    if (NOT _IsMarqueeSelecting)
    {
        return;
    }

    const auto PreviewColor = FLinearColor(0.828f, 0.364f, 0.003f, 0.35f);
    for (const auto& Node : _Scene.Nodes)
    {
        if (NOT _MarqueePreviewSelection.Contains(Node.Id))
        {
            continue;
        }
        const auto Rect = Get_ScreenRect(Node);
        FSlateDrawElement::MakeBox(OutDrawElements,
                                   InLayerId,
                                   FPaintGeometry(FVector2D{Rect.Left - 2.0f, Rect.Top - 2.0f},
                                                  FVector2D{Rect.Right - Rect.Left + 4.0f,
                                                            Rect.Bottom - Rect.Top + 4.0f},
                                                  1.0f),
                                   FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
                                   ESlateDrawEffect::None,
                                   PreviewColor);
    }
}

auto SCkDebug_GraphCanvas::Draw_Background(const FGeometry& InGeometry,
                                           FSlateWindowElementList& OutDrawElements,
                                           int32 InLayerId) const -> int32
{
    const auto* BackgroundBrush = FCkDebuggerStyle::Get().GetBrush(
        TEXT("CkDebugger.Graph.Panel.SolidBackground"));
    FSlateDrawElement::MakeBox(OutDrawElements,
                               InLayerId,
                               InGeometry.ToPaintGeometry(),
                               BackgroundBrush);

    const auto ViewportSize = InGeometry.GetLocalSize();
    auto Inflation = 1.0f;
    while (_Transform.Zoom * Inflation * ck_debug_graph_canvas::LegacyGridSnapSize <=
           ck_debug_graph_canvas::LegacySmallestGridSize)
    {
        Inflation *= 2.0f;
    }

    const auto GridCellSize = ck_debug_graph_canvas::LegacyGridSnapSize * _Transform.Zoom * Inflation;
    const auto ViewOffset = -_Transform.Pan / FMath::Max(KINDA_SMALL_NUMBER, _Transform.Zoom);
    auto GridX = ck_debug_graph_canvas::Fancy_Mod(
        static_cast<float>(ViewOffset.X),
        Inflation * ck_debug_graph_canvas::LegacyGridSnapSize * ck_debug_graph_canvas::LegacyGridRulePeriod) *
                 -_Transform.Zoom;
    auto GridY = ck_debug_graph_canvas::Fancy_Mod(
        static_cast<float>(ViewOffset.Y),
        Inflation * ck_debug_graph_canvas::LegacyGridSnapSize * ck_debug_graph_canvas::LegacyGridRulePeriod) *
                 -_Transform.Zoom;
    const auto ZeroSpace = World_To_Screen(FVector2D::ZeroVector, _Transform);
    const auto RegularColor = FLinearColor(0.024f, 0.024f, 0.024f);
    const auto RuleColor = FLinearColor(0.010f, 0.010f, 0.010f);
    const auto CenterColor = FLinearColor(0.005f, 0.005f, 0.005f);
    auto LinePoints = TArray<FVector2D>{FVector2D::ZeroVector, FVector2D::ZeroVector};

    for (auto Index = 0; GridY < ViewportSize.Y; GridY += GridCellSize, ++Index)
    {
        if (GridY < 0.0f) continue;
        const auto IsRule = Index % ck_debug_graph_canvas::LegacyGridRulePeriod == 0;
        const auto& Color = FMath::IsNearlyEqual(static_cast<float>(ZeroSpace.Y), GridY, 1.0f)
                                ? CenterColor
                                : (IsRule ? RuleColor : RegularColor);
        LinePoints[0] = FVector2D{0.0f, GridY};
        LinePoints[1] = FVector2D{ViewportSize.X, GridY};
        FSlateDrawElement::MakeLines(OutDrawElements, InLayerId + (IsRule ? 1 : 0),
                                     InGeometry.ToPaintGeometry(), LinePoints,
                                     ESlateDrawEffect::None, Color, true);
    }
    for (auto Index = 0; GridX < ViewportSize.X; GridX += GridCellSize, ++Index)
    {
        if (GridX < 0.0f) continue;
        const auto IsRule = Index % ck_debug_graph_canvas::LegacyGridRulePeriod == 0;
        const auto& Color = FMath::IsNearlyEqual(static_cast<float>(ZeroSpace.X), GridX, 1.0f)
                                ? CenterColor
                                : (IsRule ? RuleColor : RegularColor);
        LinePoints[0] = FVector2D{GridX, 0.0f};
        LinePoints[1] = FVector2D{GridX, ViewportSize.Y};
        FSlateDrawElement::MakeLines(OutDrawElements, InLayerId + (IsRule ? 1 : 0),
                                     InGeometry.ToPaintGeometry(), LinePoints,
                                     ESlateDrawEffect::None, Color, true);
    }
    return InLayerId + 2;
}

auto SCkDebug_GraphCanvas::Draw_ZoomLabel(const FGeometry& InGeometry,
                                          FSlateWindowElementList& OutDrawElements,
                                          int32 InLayerId) const -> void
{
    const auto& TextStyle = FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>(
        TEXT("CkDebugger.Graph.ZoomText"));
    const auto Font = TextStyle.Font;
    const auto Text = FText::FromString(ck_debug_graph_canvas::LegacyZoomTexts[_LegacyZoomLevel]);
    FSlateDrawElement::MakeText(OutDrawElements,
                                InLayerId,
                                InGeometry.ToPaintGeometry(
                                    FVector2f{40.0f, 20.0f},
                                    FSlateLayoutTransform{FVector2f{InGeometry.GetLocalSize().X - 45.0f, 5.0f}}),
                                Text,
                                Font,
                                ESlateDrawEffect::None,
                                FLinearColor(1.0f, 1.0f, 1.0f, 1.25f - _ZoomLevelFade.GetLerp()));
}

auto SCkDebug_GraphCanvas::Get_MarqueeSelection() const -> TSet<uint64>
{
    const auto LocalPointer = GetCachedGeometry().AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
    const auto Marquee = ck_debug_graph_canvas::Make_NormalizedRect(_PointerDownLocal, LocalPointer);
    auto Affected = TSet<uint64>{};
    for (const auto& Node : _Scene.Nodes)
    {
        if (ck_debug_graph_canvas::Rects_Intersect(Marquee, Get_ScreenRect(Node))) Affected.Add(Node.Id);
    }

    auto Result = _MarqueeBaseSelection;
    switch (static_cast<ck_debug_graph_canvas::EMarqueeOperation>(_MarqueeOperation))
    {
        default:
        case ck_debug_graph_canvas::EMarqueeOperation::Replace: return Affected;
        case ck_debug_graph_canvas::EMarqueeOperation::Add: Result.Append(Affected); return Result;
        case ck_debug_graph_canvas::EMarqueeOperation::Remove:
            for (const auto Id : Affected) Result.Remove(Id);
            return Result;
        case ck_debug_graph_canvas::EMarqueeOperation::Invert:
            for (const auto Id : Affected) { if (Result.Contains(Id)) Result.Remove(Id); else Result.Add(Id); }
            return Result;
    }
}

auto SCkDebug_GraphCanvas::Get_LegacyZoomLevel(float InZoom) const -> int32
{
    for (auto Index = 0; Index < static_cast<int32>(ck_debug_graph_canvas::LegacyZoomLevels.size()); ++Index)
    {
        if (InZoom <= ck_debug_graph_canvas::LegacyZoomLevels[Index]) return Index;
    }
    return 12;
}

auto SCkDebug_GraphCanvas::Apply_LegacyZoom(int32 InZoomDelta,
                                             const FVector2D& InLocalZoomOrigin,
                                             bool bInAllowFullRange) -> void
{
    const auto WorldAtCursor = Screen_To_World(InLocalZoomOrigin, _Transform);
    const auto DefaultZoomLevel = 12;
    const auto CanZoomPastDefault = (_LegacyZoomLevel == DefaultZoomLevel && InZoomDelta > 0 && bInAllowFullRange) ||
                                    _LegacyZoomLevel > DefaultZoomLevel;
    const auto MaxZoomLevel = CanZoomPastDefault
                                  ? static_cast<int32>(ck_debug_graph_canvas::LegacyZoomLevels.size()) - 1
                                  : DefaultZoomLevel;
    _LegacyZoomLevel = FMath::Clamp(_LegacyZoomLevel + InZoomDelta, 0, MaxZoomLevel);
    _Transform.Zoom = FMath::Clamp(ck_debug_graph_canvas::LegacyZoomLevels[_LegacyZoomLevel], _MinZoom, _MaxZoom);
    _LegacyZoomLevel = Get_LegacyZoomLevel(_Transform.Zoom);
    _Transform.Pan = InLocalZoomOrigin - WorldAtCursor * _Transform.Zoom;
    _ZoomLevelFade.Play(AsShared());
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}
