#pragma once

#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class FCkSchedulerDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Waterfall timeline - custom OnPaint leaf widget rendering processor execution bars.
// Supports zoom, pan, group collapse, and click-to-select.
// --------------------------------------------------------------------------------------------------------------------

class SCkSchedulerDebugger_Timeline : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkSchedulerDebugger_Timeline) {}
        SLATE_ARGUMENT(TSharedPtr<FCkSchedulerDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // ---- SWidget

    virtual auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

    virtual auto OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const -> int32 override;

    virtual auto OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;

    virtual auto IsInteractable() const -> bool override { return true; }
    virtual auto SupportsKeyboardFocus() const -> bool override { return true; }

private:
    TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;

    // ---- Rendering state

    mutable float _PixelsPerMs = 400.0f;
    mutable float _ScrollOffsetX = 0.0f;
    float _LabelColumnWidth = 200.0f;
    float _RowHeight = 28.0f;
    float _GroupHeaderHeight = 24.0f;
    float _TimeAxisHeight = 30.0f;

    // ---- Collapsed groups

    TSet<FName> _CollapsedGroups;

    // ---- Hit testing

    struct FBarHitInfo
    {
        int32 ProcessorIndex;
        FVector2D Position;
        FVector2D Size;
    };

    mutable TArray<FBarHitInfo> _BarHitAreas;

    // ---- Panning

    bool _IsPanning = false;
    float _PanStartX = 0.0f;
    float _PanStartOffsetX = 0.0f;

    // ---- Paint helpers

    auto DoPaintTimeAxis(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        float InTotalTimeMs) const -> int32;

    auto DoPaintGroupLabel(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FCkSchedulerDebugger_GroupInfo& InGroup,
        float InY,
        bool InIsCollapsed) const -> int32;

    auto DoPaintProcessorBar(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FCkSchedulerDebugger_ProcessorInfo& InProcessor,
        const FName& InGroupName,
        float InX,
        float InY) const -> int32;

    auto DoPaintPumpSeparators(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        float InMainPassEndX) const -> int32;

    auto DoPaintCollapsedAggregateBar(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FCkSchedulerDebugger_GroupInfo& InGroup,
        float InY) const -> int32;

    auto TimeToPixelX(double InTimeMs) const -> float;
};

// --------------------------------------------------------------------------------------------------------------------
