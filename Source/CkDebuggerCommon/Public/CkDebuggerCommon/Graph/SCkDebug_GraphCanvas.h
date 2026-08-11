#pragma once

#include "CoreMinimal.h"
#include "Widgets/SPanel.h"

// =====================================================================================================================
// Runtime Slate graph canvas. The scene is value-only: feature modules own their data and hand this
// widget stable IDs, geometry, edges, and arbitrary card widgets. No editor graph-module types are
// allowed here.
// =====================================================================================================================

enum class ECkDebug_GraphAnchor : uint8
{
    Center,
    Left,
    Right,
    Top,
    Bottom,
};

// ---------------------------------------------------------------------------------------------------------------------

struct FCkDebug_GraphCanvasTransform
{
    FVector2D Pan = FVector2D::ZeroVector;
    float Zoom = 1.0f;
};

struct FCkDebug_GraphCanvasNodeGeometry
{
    uint64 Id = 0;
    FVector2D Position = FVector2D::ZeroVector;
    FVector2D Size = FVector2D{160.0f, 64.0f};
    int32 Layer = 0;
};

struct FCkDebug_GraphCanvasNode : FCkDebug_GraphCanvasNodeGeometry
{
    TSharedPtr<SWidget> Widget;
};

struct FCkDebug_GraphCanvasEdge
{
    uint64 SourceId = 0;
    uint64 TargetId = 0;
    ECkDebug_GraphAnchor SourceAnchor = ECkDebug_GraphAnchor::Center;
    ECkDebug_GraphAnchor TargetAnchor = ECkDebug_GraphAnchor::Center;
    FLinearColor Color = FLinearColor::White;
    float Thickness = 1.5f;
    bool IsDirected = true;
    bool IsDashed = false;
    TArray<FVector2D> RoutePoints;
};

struct FCkDebug_GraphCanvasScene
{
    TArray<FCkDebug_GraphCanvasNode> Nodes;
    TArray<FCkDebug_GraphCanvasEdge> Edges;
};

struct FCkDebug_GraphCanvasEdgeGeometry
{
    FVector2D Start = FVector2D::ZeroVector;
    FVector2D End = FVector2D::ZeroVector;
    TArray<FVector2D> Points;
};

DECLARE_DELEGATE_OneParam(FOnCkDebug_GraphCanvasSelectionChanged, const TSet<uint64>&);
DECLARE_DELEGATE_OneParam(FOnCkDebug_GraphCanvasNodeDoubleClicked, uint64);
DECLARE_DELEGATE_TwoParams(FOnCkDebug_GraphCanvasNodeContextMenu, uint64, const FPointerEvent&);
DECLARE_DELEGATE_TwoParams(FOnCkDebug_GraphCanvasBackgroundContextMenu,
                           const FVector2D&,
                           const FPointerEvent&);

// ---------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_GraphCanvas : public SPanel
{
  public:
    SCkDebug_GraphCanvas();

    SLATE_BEGIN_ARGS(SCkDebug_GraphCanvas)
        : _MinZoom(0.25f), _MaxZoom(3.0f), _FitPadding(32.0f), _AllowNodeDragging(false)
    {
    }
    SLATE_ARGUMENT(float, MinZoom)
    SLATE_ARGUMENT(float, MaxZoom)
    SLATE_ARGUMENT(float, FitPadding)
    SLATE_ARGUMENT(bool, AllowNodeDragging)
    SLATE_EVENT(FOnCkDebug_GraphCanvasSelectionChanged, OnSelectionChanged)
    SLATE_EVENT(FOnCkDebug_GraphCanvasNodeDoubleClicked, OnNodeDoubleClicked)
    SLATE_EVENT(FOnCkDebug_GraphCanvasNodeContextMenu, OnNodeContextMenu)
    SLATE_EVENT(FOnCkDebug_GraphCanvasBackgroundContextMenu, OnBackgroundContextMenu)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** Replaces scene values while retaining pan/zoom and any still-present selected IDs. */
    auto Set_Scene(FCkDebug_GraphCanvasScene InScene) -> void;
    auto Get_Scene() const -> const FCkDebug_GraphCanvasScene&
    {
        return _Scene;
    }

    auto Set_Transform(const FCkDebug_GraphCanvasTransform& InTransform) -> void;
    auto Get_Transform() const -> const FCkDebug_GraphCanvasTransform&
    {
        return _Transform;
    }
    auto Frame_All() -> void;
    auto Reset_View() -> void;

    auto Set_SelectedNodeIds(TSet<uint64> InSelectedIds, bool InNotify = true) -> void;
    auto Get_SelectedNodeIds() const -> const TSet<uint64>&
    {
        return _SelectedNodeIds;
    }
    auto Clear_InteractionDelegates() -> void;

    // ---- Pure geometry helpers; deliberately public for deterministic automation coverage.
    // ------------------------

    static auto World_To_Screen(const FVector2D& InWorld,
                                const FCkDebug_GraphCanvasTransform& InTransform) -> FVector2D;
    static auto Screen_To_World(const FVector2D& InScreen,
                                const FCkDebug_GraphCanvasTransform& InTransform) -> FVector2D;
    static auto Compute_FitTransform(const TArray<FCkDebug_GraphCanvasNodeGeometry>& InNodes,
                                     const FVector2D& InViewportSize,
                                     float InPadding,
                                     float InMinZoom,
                                     float InMaxZoom) -> FCkDebug_GraphCanvasTransform;
    static auto Compute_EdgeGeometry(const FCkDebug_GraphCanvasNodeGeometry& InSource,
                                     const FCkDebug_GraphCanvasNodeGeometry& InTarget,
                                     const FCkDebug_GraphCanvasEdge& InEdge,
                                     const FCkDebug_GraphCanvasTransform& InTransform)
        -> FCkDebug_GraphCanvasEdgeGeometry;

    // ---- SWidget
    // ---------------------------------------------------------------------------------------------------

    auto OnPaint(const FPaintArgs& InArgs,
                 const FGeometry& InAllottedGeometry,
                 const FSlateRect& InCullingRect,
                 FSlateWindowElementList& OutDrawElements,
                 int32 InLayerId,
                 const FWidgetStyle& InWidgetStyle,
                 bool InParentEnabled) const -> int32 override;
    auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;
    auto OnArrangeChildren(const FGeometry& InAllottedGeometry,
                           FArrangedChildren& OutArrangedChildren) const -> void override;
    auto GetChildren() -> FChildren* override
    {
        return &_Children;
    }
    auto OnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
        -> FReply override;
    auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
        -> FReply override;
    auto OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
        -> FReply override;
    auto OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
        -> FReply override;
    auto OnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
        -> FReply override;
    auto OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
        -> FReply override;
    auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;
    auto OnMouseCaptureLost(const FCaptureLostEvent& InCaptureLostEvent) -> void override;
    auto SupportsKeyboardFocus() const -> bool override
    {
        return true;
    }

  private:
    class FSlot : public TSlotBase<FSlot>
    {
      public:
        SLATE_SLOT_BEGIN_ARGS(FSlot, TSlotBase<FSlot>)
        SLATE_SLOT_END_ARGS()
    };

    auto Find_Node(uint64 InId) const -> const FCkDebug_GraphCanvasNode*;
    auto Find_NodeAt(const FVector2D& InLocalPosition) const -> uint64;
    auto Get_ScreenRect(const FCkDebug_GraphCanvasNodeGeometry& InNode) const -> FSlateRect;
    auto Is_RectVisible(const FSlateRect& InRect, const FSlateRect& InCullingRect) const -> bool;
    auto Notify_SelectionChanged() -> void;
    auto Draw_Edge(const FCkDebug_GraphCanvasEdge& InEdge,
                   const FCkDebug_GraphCanvasEdgeGeometry& InGeometry,
                   FSlateWindowElementList& OutDrawElements,
                   int32 InLayerId) const -> void;
    auto Draw_Marquee(const FGeometry& InGeometry,
                      FSlateWindowElementList& OutDrawElements,
                      int32 InLayerId) const -> void;

  private:
    TPanelChildren<FSlot> _Children;
    FCkDebug_GraphCanvasScene _Scene;
    FCkDebug_GraphCanvasTransform _Transform;
    TSet<uint64> _SelectedNodeIds;

    float _MinZoom = 0.25f;
    float _MaxZoom = 3.0f;
    float _FitPadding = 32.0f;
    bool _AllowNodeDragging = false;

    FOnCkDebug_GraphCanvasSelectionChanged _OnSelectionChanged;
    FOnCkDebug_GraphCanvasNodeDoubleClicked _OnNodeDoubleClicked;
    FOnCkDebug_GraphCanvasNodeContextMenu _OnNodeContextMenu;
    FOnCkDebug_GraphCanvasBackgroundContextMenu _OnBackgroundContextMenu;

    bool _IsPanning = false;
    bool _IsMarqueeSelecting = false;
    bool _IsDraggingNodes = false;
    bool _PanWasDragged = false;
    FVector2D _PointerDownLocal = FVector2D::ZeroVector;
    FVector2D _PanStart = FVector2D::ZeroVector;
    uint64 _ContextNodeId = 0;
    TSet<uint64> _MarqueeBaseSelection;
    FVector2D _NodeDragStartWorld = FVector2D::ZeroVector;
    TMap<uint64, FVector2D> _NodeDragStartPositions;
    uint64 _HoveredNodeId = 0;
};
