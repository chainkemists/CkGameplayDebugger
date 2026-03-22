#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "CkEcs/Handle/CkHandle.h"

class FCkEcsGraphModel;
class ICkEcsGraphLayoutStrategy;
class SCkDebuggerWidget_GraphNode;

DECLARE_DELEGATE_OneParam(FCkGraphView_OnNodeClicked, const FCk_Handle&);

class SCkDebuggerWidget_GraphView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerWidget_GraphView) {}
        SLATE_EVENT(FCkGraphView_OnNodeClicked, OnNodeClicked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** Rebuild all node widgets and edge data from the current model state. */
    auto RebuildFromModel(
        const FCkEcsGraphModel& InModel,
        ICkEcsGraphLayoutStrategy& InLayout) -> void;

    /** Clear all nodes and edges, show empty state. */
    auto ClearGraph() -> void;

    // SWidget overrides
    auto OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const -> int32 override;

    auto Tick(
        const FGeometry& AllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime) -> void override;

    auto OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    auto OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    auto OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    auto OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;

private:
    // Coordinate transforms
    auto GraphToScreen(const FVector2D& InGraphPos, const FVector2D& InViewSize) const -> FVector2D;
    auto ScreenToGraph(const FVector2D& InScreenPos, const FVector2D& InViewSize) const -> FVector2D;

    auto OnNodeWidgetClicked(FCk_Handle InEntity) -> void;

    /** Recalculate screen positions and update SConstraintCanvas slot offsets. */
    auto UpdateNodeScreenPositions(const FVector2D& InViewSize) -> void;

    /** Find which node index is under the given screen position, or INDEX_NONE. */
    auto HitTestNode(const FVector2D& InScreenPos, const FVector2D& InViewSize) const -> int32;

    // Layout entries
    struct FNodeLayoutEntry
    {
        FVector2D GraphPosition = FVector2D::ZeroVector;
        FCk_Handle Entity;
        TSharedPtr<SCkDebuggerWidget_GraphNode> Widget;
        SConstraintCanvas::FSlot* Slot = nullptr;
    };
    TArray<FNodeLayoutEntry> NodeEntries;

    struct FEdgeLayoutEntry
    {
        int32 SourceIndex = INDEX_NONE;
        int32 TargetIndex = INDEX_NONE;
        FLinearColor Color = FLinearColor::White;
        FText Label;
    };
    TArray<FEdgeLayoutEntry> EdgeEntries;

    // Pan/Zoom state
    FVector2D ViewOffset = FVector2D::ZeroVector;
    float ZoomLevel = 1.0f;
    static constexpr float ZoomMin = 0.3f;
    static constexpr float ZoomMax = 3.0f;
    static constexpr float ZoomStep = 0.1f;

    // Pan (right-click drag)
    bool bIsPanning = false;
    FVector2D PanStartMousePosition = FVector2D::ZeroVector;
    FVector2D PanStartViewOffset = FVector2D::ZeroVector;

    // Node drag (left-click drag)
    int32 DraggedNodeIndex = INDEX_NONE;
    FVector2D DragStartMousePosition = FVector2D::ZeroVector;
    FVector2D DragStartNodePosition = FVector2D::ZeroVector;
    bool bDragThresholdMet = false;
    static constexpr float DragThreshold = 4.0f;

    // The canvas that hosts node widgets
    TSharedPtr<SConstraintCanvas> NodeCanvas;

    // Overlay widgets
    TSharedPtr<SWidget> EmptyStateWidget;

    // Delegate
    FCkGraphView_OnNodeClicked OnNodeClickedDelegate;

    // Flag to update positions
    bool bPositionsDirty = false;
    FVector2D CachedViewSize = FVector2D::ZeroVector;
};
