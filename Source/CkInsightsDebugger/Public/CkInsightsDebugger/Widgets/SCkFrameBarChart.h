#pragma once

#include "CoreMinimal.h"

#include "CkInsightsAnalyzer/Report/CkMultiFrameReport.h"

#include "Layout/SlateRect.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

struct FSlateBrush;

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DELEGATE_OneParam(FOnFrameSelectionChanged, const TArray<FCk_FrameRun>& /*SelectedRuns*/);
DECLARE_DELEGATE_TwoParams(FOnScreenshotMarkerClicked, uint32 /*ScreenshotId*/, uint64 /*FrameIndex*/);

/**
 * UI-ready screenshot annotation for a frame in the chart. The analyzer tab owns decoding and
 * brush construction; this widget only retains the already-created brush and paints it.
 */
struct FCk_FrameScreenshotMarker
{
    uint32 ScreenshotId = 0;
    uint64 FrameIndex = 0;
    FString Label;
    TSharedPtr<const FSlateBrush> ThumbnailBrush;
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Lightweight frame time bar chart widget.
 *
 * Draws vertical bars for each frame, colored by severity:
 * - Green: under budget
 * - Yellow: 1x-2x over budget
 * - Red: > 2x over budget
 *
 * Supports click-to-select, drag-to-select-range, shift-to-extend, ctrl-to-add/toggle a
 * disjoint run, scroll-to-zoom, and middle-drag-to-pan. The selection is a sorted, merged,
 * non-overlapping run list; FOnFrameSelectionChanged carries it on mouse-up.
 */
class CKINSIGHTSDEBUGGER_API SCkFrameBarChart : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkFrameBarChart)
        : _TargetFrameMs(16.67)
    {}
        SLATE_ARGUMENT(double, TargetFrameMs)
        SLATE_EVENT(FOnFrameSelectionChanged, OnFrameSelectionChanged)
        SLATE_EVENT(FOnScreenshotMarkerClicked, OnScreenshotMarkerClicked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // ---- Data ----

    /** Set frame data. Resets viewport and selection. */
    auto SetFrameData(TArray<double>&& InFrameDurationsMs) -> void;

    /** Append frame data incrementally (for progressive loading). Does not reset viewport. */
    auto AppendFrameData(const TArray<double>& NewDurationsMs) -> void;

    /** Clear all frame data. */
    auto ClearFrameData() -> void;

    /** Recalculate display max using P95 (call after progressive loading completes). */
    auto RecalculateDisplayMax() -> void;

    /** Get total frame count. */
    auto GetFrameCount() const -> uint64 { return static_cast<uint64>(_FrameDurationsMs.Num()); }

    /** Set the target frame budget in ms. */
    auto SetTargetFrameMs(double InTargetMs) -> void;

    /** Replace the screenshot annotations displayed over the chart and in its thumbnail rail. */
    auto SetScreenshotMarkers(TArray<FCk_FrameScreenshotMarker>&& InMarkers) -> void;

    /** Remove screenshot annotations without changing the loaded frame data. */
    auto ClearScreenshotMarkers() -> void;

    // ---- Selection ----

    /** Current selection: sorted, merged, non-overlapping inclusive runs. Empty when nothing is selected. */
    auto Get_Selection() const -> const TArray<FCk_FrameRun>& { return _SelectedRuns; }
    auto HasSelection() const -> bool { return _SelectedRuns.Num() > 0; }

    /** Total number of frames covered by the current selection. */
    auto Get_SelectedFrameCount() const -> uint64;

    /** Replace the selection. Runs are clamped to the loaded frames, then sorted and merged. */
    auto SetSelection(TArray<FCk_FrameRun> InRuns) -> void;

    /** Replace the selection with a single frame and re-anchor shift-extend on it. */
    auto SetSelection(uint64 InFrame) -> void;

    /** Clear selection. */
    auto ClearSelection() -> void;

    /** Pan just enough to make a frame visible, centering it when it is currently off-screen. */
    auto EnsureFrameVisible(uint64 FrameIndex) -> void;

    // ---- SWidget Interface ----

    virtual auto SupportsKeyboardFocus() const -> bool override { return true; }

    virtual auto ComputeDesiredSize(float LayoutScaleMultiplier) const -> FVector2D override;
    virtual auto OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                         const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                         int32 LayerId, const FWidgetStyle& InWidgetStyle,
                         bool bParentEnabled) const -> int32 override;

    virtual auto OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> FReply override;
    virtual auto OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> void override;
    virtual auto OnMouseLeave(const FPointerEvent& MouseEvent) -> void override;
    virtual auto OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) -> FReply override;

private:
    /** Convert a local X coordinate to a frame index. Clamps to valid range. */
    auto XToFrame(const FGeometry& Geometry, float LocalX) const -> int64;

    /** Convert a frame index to a local X coordinate. */
    auto FrameToX(const FGeometry& Geometry, int64 FrameIndex) const -> float;

    /** Get bar color for a given frame duration. */
    auto GetBarColor(double DurationMs) const -> FLinearColor;

    /** Compute bar width for current zoom level. */
    auto GetBarWidth(const FGeometry& Geometry) const -> float;

    /** Compute bar step (bar width + adaptive spacing). */
    auto GetBarStep(const FGeometry& Geometry) const -> float;

    /** Apply zoom centered on a given local X position. Delta > 0 = zoom in, < 0 = zoom out. */
    auto ApplyZoom(const FGeometry& Geometry, float LocalX, float Delta) -> void;

    /** Height reserved for the screenshot rail; zero preserves the original chart layout. */
    auto GetScreenshotRailHeight(const FGeometry& Geometry) const -> float;

    /** Bottom of the bar/chart area after reserving the optional screenshot rail. */
    auto GetChartBottom(const FGeometry& Geometry) const -> float;

    /** Deterministic collision lane for a thumbnail at the current pan/zoom. */
    auto GetScreenshotThumbnailLane(const FGeometry& Geometry, int32 MarkerIndex) const -> int32;

    /** Return whether this position hits a screenshot thumbnail or its exact-frame marker. */
    auto FindScreenshotAt(const FGeometry& Geometry, const FVector2D& LocalPosition) const -> const FCk_FrameScreenshotMarker*;

    /** Determine the compact rail rectangle for a marker in stable sorted order. */
    auto GetScreenshotThumbnailRect(const FGeometry& Geometry, int32 MarkerIndex) const -> FSlateRect;

    /** Selection the in-flight drag would commit with the pointer over InFrame. */
    auto Get_DragPreviewSelection(int64 InFrame) const -> TArray<FCk_FrameRun>;

    /** Frame a shift-extend runs from; InFallbackFrame when no anchor survives. */
    auto Get_EffectiveAnchor(int64 InFallbackFrame) const -> int64;

private:
    TArray<double> _FrameDurationsMs;
    double _TargetFrameMs = 16.67;

    // Viewport state
    double _ViewOffset = 0.0;       // First visible frame (fractional for smooth scrolling)
    double _FramesPerPixel = 1.0;   // Zoom level: how many frames fit in one pixel

    // Selection — sorted, merged, non-overlapping inclusive runs
    TArray<FCk_FrameRun> _SelectedRuns;

    // Frame a shift-extend runs from: the last plain/ctrl-clicked frame or drag origin. INDEX_NONE when unset.
    int64 _SelectionAnchor = INDEX_NONE;

    // Interaction state
    bool   _bIsDragging = false;
    bool   _bIsPanning = false;
    bool   _bDragShift = false;
    bool   _bDragCtrl = false;
    int64  _DragStartFrame = 0;

    // Selection the drag started from — the base an additive ctrl-drag adds its pending run to.
    TArray<FCk_FrameRun> _DragBaseRuns;

    float  _PanStartX = 0.0f;
    double _PanStartOffset = 0.0;
    int64  _HoveredFrame = -1;

    FOnFrameSelectionChanged _OnFrameSelectionChanged;
    FOnScreenshotMarkerClicked _OnScreenshotMarkerClicked;

    // Pre-decoded, UI-owned screenshot annotations. OnPaint only reads these brushes; it never
    // decodes, allocates, or constructs Slate resources.
    TArray<FCk_FrameScreenshotMarker> _ScreenshotMarkers;

    /** Cached display max (P95-based) so outlier frames don't squish normal bars. */
    double _DisplayMaxMs = 0.0;

    // Tooltip
    TSharedPtr<SToolTip>   _FrameTooltip;
    TSharedPtr<STextBlock> _TooltipTextBlock;
};

// --------------------------------------------------------------------------------------------------------------------
