#include "CkAStarDebugger/GridView/SCkAStarDebugger_GridView.h"
#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"
#include "CkAStarDebugger/CkAStarDebuggerStyle.h"

#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

// ====================================================================================================================
// Construction
// ====================================================================================================================

auto
    SCkAStarDebugger_GridView::
    Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkAStarDebugger_ViewModel> InViewModel)
    -> void
{
    _ViewModel = InViewModel;
    _CellSize = FCkAStarDebuggerStyle::Grid_CellSize;
    _CellGap = FCkAStarDebuggerStyle::Grid_CellGap;
}

auto
    SCkAStarDebugger_GridView::
    SetSearchInfo(
        const FCkAStarDebugger_SearchInfo& InInfo)
    -> void
{
    _SearchInfo = InInfo;
}

auto
    SCkAStarDebugger_GridView::
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const
    -> FVector2D
{
    return FVector2D(400.0f, 400.0f);
}

// ====================================================================================================================
// Cell color determination
// ====================================================================================================================

auto
    SCkAStarDebugger_GridView::
    GetCellColor(
        int32 InCellIndex) const
    -> FLinearColor
{
    if (InCellIndex == _SearchInfo.StartNode)
    { return FCkAStarDebuggerStyle::Color_Cell_Start; }

    if (InCellIndex == _SearchInfo.GoalNode)
    { return FCkAStarDebuggerStyle::Color_Cell_Goal; }

    if (_SearchInfo.BlockedCells.Contains(InCellIndex))
    { return FCkAStarDebuggerStyle::Color_Cell_Blocked; }

    if (_SearchInfo.Path.Contains(InCellIndex))
    { return FCkAStarDebuggerStyle::Color_Cell_Path; }

    if (_SearchInfo.HasCellData)
    {
        if (_SearchInfo.OpenSetCells.Contains(InCellIndex))
        { return FCkAStarDebuggerStyle::Color_Cell_OpenSet; }

        if (_SearchInfo.ClosedSetCells.Contains(InCellIndex))
        { return FCkAStarDebuggerStyle::Color_Cell_ClosedSet; }
    }

    return FCkAStarDebuggerStyle::Color_Cell_Empty;
}

// ====================================================================================================================
// Coordinate conversion
// ====================================================================================================================

auto
    SCkAStarDebugger_GridView::
    CellToScreen(
        int32 InCellX,
        int32 InCellY) const
    -> FVector2D
{
    return FVector2D(
        _PanOffset.X + InCellX * (_CellSize + _CellGap),
        _PanOffset.Y + InCellY * (_CellSize + _CellGap));
}

auto
    SCkAStarDebugger_GridView::
    ScreenToCell(
        FVector2D InScreenPos,
        const FGeometry& InGeometry) const
    -> int32
{
    auto LocalPos = InGeometry.AbsoluteToLocal(InScreenPos);
    auto CellX = static_cast<int32>((LocalPos.X - _PanOffset.X) / (_CellSize + _CellGap));
    auto CellY = static_cast<int32>((LocalPos.Y - _PanOffset.Y) / (_CellSize + _CellGap));

    if (CellX < 0 || CellX >= _SearchInfo.GridWidth || CellY < 0 || CellY >= _SearchInfo.GridHeight)
    { return -1; }

    return CellY * _SearchInfo.GridWidth + CellX;
}

// ====================================================================================================================
// OnPaint — the core rendering
// ====================================================================================================================

auto
    SCkAStarDebugger_GridView::
    OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InMyCullingRect,
        FSlateWindowElementList& InOutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InbParentEnabled) const
    -> int32
{
    FSlateDrawElement::MakeBox(
        InOutDrawElements,
        InLayerId,
        InAllottedGeometry.ToPaintGeometry(),
        FAppStyle::GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        FCkAStarDebuggerStyle::Color_Grid_Background);

    if (_SearchInfo.GridWidth <= 0 || _SearchInfo.GridHeight <= 0)
    {
        auto NoDataText = FString{TEXT("No A* grid data available. Stats are shown in the panel.")};

        FSlateDrawElement::MakeText(
            InOutDrawElements,
            InLayerId + 1,
            InAllottedGeometry.ToPaintGeometry(
                FVector2D(20.0f, InAllottedGeometry.GetLocalSize().Y * 0.5f - 8.0f),
                InAllottedGeometry.GetLocalSize()),
            NoDataText,
            FCoreStyle::GetDefaultFontStyle("Regular", 10),
            ESlateDrawEffect::None,
            FCkAStarDebuggerStyle::Color_Text_Muted);

        return InLayerId + 1;
    }

    PaintGrid(InAllottedGeometry, InOutDrawElements, InLayerId + 1);
    PaintPathOverlay(InAllottedGeometry, InOutDrawElements, InLayerId + 2);
    PaintLegend(InAllottedGeometry, InOutDrawElements, InLayerId + 3);

    return InLayerId + 4;
}

// ====================================================================================================================
// Grid cell painting
// ====================================================================================================================

auto
    SCkAStarDebugger_GridView::
    PaintGrid(
        const FGeometry& InGeometry,
        FSlateWindowElementList& InOutDrawElements,
        int32 InLayerId) const
    -> void
{
    auto WidgetSize = InGeometry.GetLocalSize();
    auto WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
    auto SelectedCell = _ViewModel.IsValid() ? _ViewModel->Get_SelectedCellIndex() : -1;

    for (int32 Y = 0; Y < _SearchInfo.GridHeight; ++Y)
    {
        for (int32 X = 0; X < _SearchInfo.GridWidth; ++X)
        {
            auto ScreenPos = CellToScreen(X, Y);

            if (ScreenPos.X + _CellSize < 0.0f || ScreenPos.Y + _CellSize < 0.0f
                || ScreenPos.X > WidgetSize.X || ScreenPos.Y > WidgetSize.Y)
            { continue; }

            auto CellIndex = Y * _SearchInfo.GridWidth + X;
            auto Color = GetCellColor(CellIndex);

            FSlateDrawElement::MakeBox(
                InOutDrawElements,
                InLayerId,
                InGeometry.ToPaintGeometry(ScreenPos, FVector2D(_CellSize, _CellSize)),
                WhiteBrush,
                ESlateDrawEffect::None,
                Color);

            if (CellIndex == SelectedCell)
            {
                auto BorderSize = FVector2D{_CellSize + 2.0f, _CellSize + 2.0f};
                auto BorderPos = ScreenPos - FVector2D{1.0f, 1.0f};
                auto BorderColor = FLinearColor::White;

                // Top
                FSlateDrawElement::MakeBox(
                    InOutDrawElements, InLayerId + 1,
                    InGeometry.ToPaintGeometry(BorderPos, FVector2D(BorderSize.X, 1.0f)),
                    WhiteBrush, ESlateDrawEffect::None, BorderColor);
                // Bottom
                FSlateDrawElement::MakeBox(
                    InOutDrawElements, InLayerId + 1,
                    InGeometry.ToPaintGeometry(BorderPos + FVector2D(0.0f, BorderSize.Y - 1.0f), FVector2D(BorderSize.X, 1.0f)),
                    WhiteBrush, ESlateDrawEffect::None, BorderColor);
                // Left
                FSlateDrawElement::MakeBox(
                    InOutDrawElements, InLayerId + 1,
                    InGeometry.ToPaintGeometry(BorderPos, FVector2D(1.0f, BorderSize.Y)),
                    WhiteBrush, ESlateDrawEffect::None, BorderColor);
                // Right
                FSlateDrawElement::MakeBox(
                    InOutDrawElements, InLayerId + 1,
                    InGeometry.ToPaintGeometry(BorderPos + FVector2D(BorderSize.X - 1.0f, 0.0f), FVector2D(1.0f, BorderSize.Y)),
                    WhiteBrush, ESlateDrawEffect::None, BorderColor);
            }
        }
    }
}

// ====================================================================================================================
// Path overlay — draws connected line segments through path cell centers
// ====================================================================================================================

auto
    SCkAStarDebugger_GridView::
    PaintPathOverlay(
        const FGeometry& InGeometry,
        FSlateWindowElementList& InOutDrawElements,
        int32 InLayerId) const
    -> void
{
    if (_SearchInfo.Path.Num() < 2 || _SearchInfo.GridWidth <= 0)
    { return; }

    auto LinePoints = TArray<FVector2D>{};
    LinePoints.Reserve(_SearchInfo.Path.Num());

    for (auto NodeIndex : _SearchInfo.Path)
    {
        auto CellX = NodeIndex % _SearchInfo.GridWidth;
        auto CellY = NodeIndex / _SearchInfo.GridWidth;
        auto ScreenPos = CellToScreen(CellX, CellY);
        auto CenterOffset = _CellSize * 0.5f;
        LinePoints.Add(ScreenPos + FVector2D(CenterOffset, CenterOffset));
    }

    FSlateDrawElement::MakeLines(
        InOutDrawElements,
        InLayerId,
        InGeometry.ToPaintGeometry(),
        LinePoints,
        ESlateDrawEffect::None,
        FCkAStarDebuggerStyle::Color_Path_Line,
        true,
        FCkAStarDebuggerStyle::Path_LineThickness);
}

// ====================================================================================================================
// Legend — color key at bottom of grid
// ====================================================================================================================

auto
    SCkAStarDebugger_GridView::
    PaintLegend(
        const FGeometry& InGeometry,
        FSlateWindowElementList& InOutDrawElements,
        int32 InLayerId) const
    -> void
{
    auto WidgetSize = InGeometry.GetLocalSize();
    auto WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
    auto Font = FCoreStyle::GetDefaultFontStyle("Regular", 9);

    struct FLegendItem
    {
        FLinearColor Color;
        FString Label;
    };

    auto Items = TArray<FLegendItem>{
        { FCkAStarDebuggerStyle::Color_Cell_Start,     TEXT("Start") },
        { FCkAStarDebuggerStyle::Color_Cell_Goal,      TEXT("Goal") },
        { FCkAStarDebuggerStyle::Color_Cell_OpenSet,   TEXT("Open") },
        { FCkAStarDebuggerStyle::Color_Cell_ClosedSet, TEXT("Closed") },
        { FCkAStarDebuggerStyle::Color_Cell_Path,      TEXT("Path") },
        { FCkAStarDebuggerStyle::Color_Cell_Blocked,   TEXT("Blocked") },
    };

    constexpr auto SwatchSize = 10.0f;
    constexpr auto ItemSpacing = 12.0f;
    constexpr auto LegendY_Offset = 24.0f;
    auto Y = WidgetSize.Y - LegendY_Offset;
    auto X = 10.0f;

    for (const auto& Item : Items)
    {
        FSlateDrawElement::MakeBox(
            InOutDrawElements, InLayerId,
            InGeometry.ToPaintGeometry(FVector2D{X, Y}, FVector2D{SwatchSize, SwatchSize}),
            WhiteBrush, ESlateDrawEffect::None, Item.Color);

        FSlateDrawElement::MakeText(
            InOutDrawElements, InLayerId,
            InGeometry.ToPaintGeometry(
                FVector2D{X + SwatchSize + 3.0f, Y - 1.0f},
                FVector2D{60.0f, 14.0f}),
            Item.Label, Font, ESlateDrawEffect::None,
            FCkAStarDebuggerStyle::Color_Text_Secondary);

        X += SwatchSize + 3.0f + 40.0f + ItemSpacing;
    }
}

// ====================================================================================================================
// Input handling
// ====================================================================================================================

auto
    SCkAStarDebugger_GridView::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        _IsPanning = true;
        _PanStartMousePos = InMouseEvent.GetScreenSpacePosition();
        _PanStartOffset = _PanOffset;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        auto CellIndex = ScreenToCell(InMouseEvent.GetScreenSpacePosition(), InGeometry);

        if (_ViewModel.IsValid())
        {
            _ViewModel->Set_SelectedCellIndex(CellIndex);
        }

        return FReply::Handled();
    }

    return FReply::Unhandled();
}

auto
    SCkAStarDebugger_GridView::
    OnMouseButtonUp(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && _IsPanning)
    {
        _IsPanning = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    return FReply::Unhandled();
}

auto
    SCkAStarDebugger_GridView::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (_IsPanning)
    {
        auto Delta = InMouseEvent.GetScreenSpacePosition() - _PanStartMousePos;
        _PanOffset = _PanStartOffset + Delta;
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

auto
    SCkAStarDebugger_GridView::
    OnMouseWheel(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    auto OldCellSize = _CellSize;
    auto ZoomDelta = (InMouseEvent.GetWheelDelta() > 0.0f) ? 2.0f : -2.0f;
    _CellSize = FMath::Clamp(
        _CellSize + ZoomDelta,
        FCkAStarDebuggerStyle::Grid_MinCellSize,
        FCkAStarDebuggerStyle::Grid_MaxCellSize);

    auto LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    auto Scale = _CellSize / OldCellSize;
    _PanOffset = LocalMouse - (LocalMouse - _PanOffset) * Scale;

    return FReply::Handled();
}

// ====================================================================================================================
