#pragma once

#include "CoreMinimal.h"

#include "CkInsightsDebugger/Analysis/CkInsightsFrameRequest.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

DECLARE_DELEGATE_OneParam(FOnFrameTimingTimerSelectionChanged, TOptional<uint32> /*TimerIndex*/);

/**
 * Compact nested timer view for the frame currently shown in the Insights details panel.
 *
 * The analysis request owns the immutable, UI-ready timing data. This widget only maps the
 * supplied millisecond interval and nesting depth to Slate geometry; it never reaches into
 * TraceServices or creates Slate resources while painting.
 */
class CKINSIGHTSDEBUGGER_API SCkFrameTimingGraph : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkFrameTimingGraph)
    {}
        SLATE_EVENT(FOnFrameTimingTimerSelectionChanged, OnTimerSelectionChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto SetView(TSharedPtr<const FCkInsightsFrameTimingView, ESPMode::ThreadSafe> InView) -> void;
    auto ClearView() -> void;
    auto HasData() const -> bool;
    auto GetView() const -> TSharedPtr<const FCkInsightsFrameTimingView, ESPMode::ThreadSafe> { return _View; }

    /** Timer selected by a graph click; unset means the user clicked empty graph space. */
    auto GetSelectedTimerIndex() const -> TOptional<uint32> { return _SelectedTimerIndex; }
    /** Apply a linked selection without rebroadcasting it to the source tree. */
    auto SetTimerSelection(TOptional<uint32> InTimerIndex) -> void;
    /** Number of displayed occurrences sharing the selected trace timer identity. */
    auto GetSelectedOccurrenceCount() const -> int32;
    /** Left edge of the visible timing range, exposed for focused interaction tests. */
    auto GetViewStartMs() const -> double { return _ViewStartMs; }
    /** Duration of the visible timing range, exposed for focused interaction tests. */
    auto GetVisibleDurationMs() const -> double { return _VisibleDurationMs; }
    auto GetViewTopRow() const -> double { return _ViewTopRow; }
    auto GetVisibleRowCount() const -> double { return _VisibleRowCount; }
    /** Focuses the timer's occurrence union when it fits, otherwise its first occurrence. */
    auto FocusTimer(uint32 InTimerIndex) -> void;
    /** Whether a bar has enough interior width to attempt a clipped label. */
    static auto ShouldAttemptLabel(float InBarWidth) -> bool;
    static auto FormatDuration(double InMilliseconds) -> FString;
    static auto FormatEventLabel(const FString& InDisplayName, double InMilliseconds) -> FString;

    virtual auto ComputeDesiredSize(float LayoutScaleMultiplier) const -> FVector2D override;
    virtual auto OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                         const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                         int32 LayerId, const FWidgetStyle& InWidgetStyle,
                         bool bParentEnabled) const -> int32 override;
    virtual auto OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseLeave(const FPointerEvent& MouseEvent) -> void override;

private:
    /** Event below the pointer, or INDEX_NONE when no timing bar contains the pointer. */
    auto Get_EventAt(const FGeometry& InGeometry, const FVector2D& InLocalPosition) const -> int32;
    /** Pixel-aligned visible event bounds shared by painting and hit testing. */
    auto GetEventRect(const FCkInsightsFrameTimingEvent& InEvent, const FGeometry& InGeometry) const -> FSlateRect;
    auto GetGraphWidth(const FGeometry& InGeometry) const -> float;
    auto GetGraphHeight(const FGeometry& InGeometry) const -> float;
    auto GetDurationMs() const -> double;
    auto GetMillisecondsPerPixel(const FGeometry& InGeometry) const -> double;
    auto GetRowsPerPixel(const FGeometry& InGeometry) const -> double;
    auto GetRowTop(uint32 InDepth, const FGeometry& InGeometry) const -> float;
    auto GetRowHeight(const FGeometry& InGeometry) const -> float;
    auto UpdateVisibleRowCount(const FGeometry& InGeometry) -> void;
    auto ClampViewport() -> void;
    auto ApplyZoom(const FGeometry& InGeometry, const FVector2D& InLocalPosition, float InDelta) -> void;
    auto UpdateTimeScaleLabels() -> void;
    static auto FormatTimeScaleLabel(double InMilliseconds, double InTickStepMilliseconds) -> FString;
    auto SetSelectedTimerIndex(TOptional<uint32> InTimerIndex, bool bNotify) -> void;
    auto UpdateTooltip(int32 InEventIndex) -> void;

private:
    TSharedPtr<const FCkInsightsFrameTimingView, ESPMode::ThreadSafe> _View;
    int32 _HoveredEvent = INDEX_NONE;
    TOptional<uint32> _SelectedTimerIndex;
    double _ViewStartMs = 0.0;
    double _VisibleDurationMs = 0.0;
    double _ViewTopRow = 0.0;
    double _VisibleRowCount = 0.0;
    TArray<FString> _TimeScaleLabels;

    bool _bPointerDown = false;
    bool _bPanning = false;
    FKey _DragButton = EKeys::Invalid;
    FVector2D _DragStartPosition = FVector2D::ZeroVector;
    double _DragStartViewMs = 0.0;
    double _DragStartTopRow = 0.0;

    FOnFrameTimingTimerSelectionChanged _OnTimerSelectionChanged;

    TSharedPtr<SToolTip> _EventTooltip;
    TSharedPtr<STextBlock> _TooltipTextBlock;
};
