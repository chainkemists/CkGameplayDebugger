#pragma once

#include "CkAStarDebugger/Data/CkAStarDebugger_Types.h"

#include "Widgets/SLeafWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkAStarDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Grid view — custom Slate widget rendering A* grid cells, open/closed sets, path overlay.
// Supports pan (right-drag), zoom (mouse wheel), and cell selection (left-click).
// --------------------------------------------------------------------------------------------------------------------

class SCkAStarDebugger_GridView : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkAStarDebugger_GridView) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkAStarDebugger_ViewModel> InViewModel) -> void;
    auto SetSearchInfo(const FCkAStarDebugger_SearchInfo& InInfo) -> void;
    auto Reset_ForWorldChange() -> void;

    auto OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry, const FSlateRect& InMyCullingRect, FSlateWindowElementList& InOutDrawElements, int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool InbParentEnabled) const -> int32 override;
    auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

    auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;

    auto IsInteractable() const -> bool override { return true; }
    auto SupportsKeyboardFocus() const -> bool override { return true; }

private:
    auto GetCellColor(int32 InCellIndex) const -> FLinearColor;
    auto CellToScreen(int32 InCellX, int32 InCellY) const -> FVector2D;
    auto ScreenToCell(FVector2D InScreenPos, const FGeometry& InGeometry) const -> int32;

    auto PaintGrid(const FGeometry& InGeometry, FSlateWindowElementList& InOutDrawElements, int32 InLayerId) const -> void;
    auto PaintPathOverlay(const FGeometry& InGeometry, FSlateWindowElementList& InOutDrawElements, int32 InLayerId) const -> void;
    auto PaintLegend(const FGeometry& InGeometry, FSlateWindowElementList& InOutDrawElements, int32 InLayerId) const -> void;

private:
    TSharedPtr<FCkAStarDebugger_ViewModel> _ViewModel;
    FCkAStarDebugger_SearchInfo _SearchInfo;

    float _CellSize = 16.0f;
    float _CellGap = 1.0f;
    FVector2D _PanOffset = FVector2D{10.0, 10.0};

    bool _IsPanning = false;
    FVector2D _PanStartMousePos = FVector2D::ZeroVector;
    FVector2D _PanStartOffset = FVector2D::ZeroVector;
};

// --------------------------------------------------------------------------------------------------------------------
