#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

// ====================================================================================================================
// Frame strip — a scrubbable horizontal history of per-frame values, drawn as heat-colored columns.
//
// Merges the two strips the suite grew independently:
//   CkSchedulerDebugger  SCkSchedulerDebugger_FrameHistoryBar  scrub · pan · highlight filter ·
//                                                              per-frame marker dot · LIVE/FROZEN label
//   CkInsightsDebugger   SCkFrameBarChart                      budget-relative green/yellow/red heat ·
//                                                              wheel zoom · hover tooltip
//
// Pure data-in: the owner pushes samples through Set_Samples on its gated refresh and owns the
// selection (bound through SelectedIndexFromEnd + reported through OnScrubbed). The widget keeps
// only view state — scroll offset and zoom.
//
// SELECTION INDEXING is from the END: 0 is the newest sample ("live"), 1 the one before it, and so
// on. That is the Scheduler's convention and it survives the ring buffer scrolling underneath.
//
// COLOR is always ck::debug_axes::Get_HeatColor over a normalized value:
//   BudgetMs > 0   normalized = Value / (2 * Budget)  → Ok at 0, Warn exactly at budget, Err at 2x
//   BudgetMs <= 0  normalized = Value / MaxInStrip    → heat relative to the strip's own range
// Pass a budget when "slow" has an absolute meaning; leave it at 0 for relative heat.
//
// INPUT: LMB drag scrubs · LMB double-click returns to live · MMB / RMB drag pans ·
// wheel zooms (when AllowZoom) · Left/Right/Home/End step the selection · RMB WITHOUT a drag opens
// the "Copy Text" menu when CopyText is set.
// ====================================================================================================================

struct FCkDebug_FrameSample
{
    double ValueMs = 0.0;

    // Draws a thin marker band at the base of the column (Scheduler's pump indicator).
    bool HasMarker = false;

    // The owner's highlight-filter verdict for this frame — draws a warm backing band.
    bool IsHighlighted = false;
};

// --------------------------------------------------------------------------------------------------------------------

enum class ECkDebug_FrameStripHeightScale : uint8
{
    // Tallest sample fills the strip. Best for "which frames spiked relative to each other".
    RelativeToMax,

    // Column height is Value / (2 * BudgetMs), clamped. Best when the budget is the story.
    // Falls back to RelativeToMax when no budget is supplied.
    Budget,
};

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DELEGATE_OneParam(FOnCkDebug_FrameStripScrubbed, int32 /* IndexFromEnd */);
DECLARE_DELEGATE(FOnCkDebug_FrameStripReturnedToLive);

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_FrameStrip : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_FrameStrip)
        : _DesiredHeight(44.0f)
        , _BarWidth(5.0f)
        , _BarGap(1.0f)
        , _BarAreaPadding(FMargin{10.0f, 4.0f, 80.0f, 4.0f})
        , _BudgetMs(0.0)
        , _HeightScale(ECkDebug_FrameStripHeightScale::RelativeToMax)
        , _SelectedIndexFromEnd(0)
        , _ShowStatusLabel(true)
        , _LiveLabel(FText::FromString(TEXT("LIVE")))
        , _FrozenLabel(FText::FromString(TEXT("FROZEN")))
        , _ValueSuffix(FString{TEXT(" ms")})
        , _AllowZoom(true)
    {}
        SLATE_ARGUMENT(float, DesiredHeight)

        // Base column geometry. Wheel zoom scales BarWidth/BarGap together.
        SLATE_ARGUMENT(float, BarWidth)
        SLATE_ARGUMENT(float, BarGap)

        // Right inset is the gutter the LIVE / Frame #N label lives in — keep it wide enough
        // for the label or the columns will run under it.
        SLATE_ARGUMENT(FMargin, BarAreaPadding)

        SLATE_ATTRIBUTE(double, BudgetMs)
        SLATE_ARGUMENT(ECkDebug_FrameStripHeightScale, HeightScale)

        // 0 = newest/live. Owner-held so the strip and the rest of the debugger cannot disagree.
        SLATE_ATTRIBUTE(int32, SelectedIndexFromEnd)

        SLATE_ARGUMENT(bool, ShowStatusLabel)
        SLATE_ARGUMENT(FText, LiveLabel)
        SLATE_ARGUMENT(FText, FrozenLabel)

        // Appended to the hover tooltip's value, e.g. " ms".
        SLATE_ARGUMENT(FString, ValueSuffix)

        // Named in the tooltip when a hovered sample has HasMarker set (e.g. "pumped").
        SLATE_ARGUMENT(FString, MarkerMeaning)

        // Non-empty enables right-click → Copy on the strip (right-DRAG still pans).
        SLATE_ATTRIBUTE(FString, CopyText)

        SLATE_ARGUMENT(bool, AllowZoom)

        SLATE_EVENT(FOnCkDebug_FrameStripScrubbed, OnScrubbed)
        SLATE_EVENT(FOnCkDebug_FrameStripReturnedToLive, OnReturnToLive)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** Replace the rendered history. Cheap — one array move; paint is immediate-mode. */
    auto Set_Samples(TArray<FCkDebug_FrameSample> InSamples) -> void;

    auto Get_SampleCount() const -> int32;

    /** Drop zoom back to 1x and re-anchor the view on the newest sample. */
    auto Reset_View() -> void;

    // ---- SWidget ----------------------------------------------------------------------------------------------------

    auto OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const -> int32 override;

    auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

    auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseLeave(const FPointerEvent& InMouseEvent) -> void override;
    auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;

    auto IsInteractable() const -> bool override { return true; }
    auto SupportsKeyboardFocus() const -> bool override { return true; }

private:
    auto Get_BarStride() const -> float;
    auto Get_ScaledBarWidth() const -> float;

    /** Largest sample value in the strip; never zero, so it is always a safe divisor. */
    auto Compute_MaxValue() const -> double;

    /** Normalized [0,1] heat input for one sample. */
    auto Compute_HeatNormalized(double InValue, double InMaxValue, double InBudget) const -> float;

    auto Compute_IndexFromEndAt(const FGeometry& InGeometry, float InLocalX) const -> int32;

    /** Array index (0 = oldest) for a from-end selection index. */
    auto Compute_ArrayIndex(int32 InIndexFromEnd) const -> int32;

    auto Do_Scrub(int32 InIndexFromEnd) -> void;

private:
    TArray<FCkDebug_FrameSample> _Samples;

    float _DesiredHeight = 44.0f;
    float _BarWidth = 5.0f;
    float _BarGap = 1.0f;
    FMargin _BarAreaPadding = FMargin{10.0f, 4.0f, 80.0f, 4.0f};

    TAttribute<double> _BudgetMs;
    ECkDebug_FrameStripHeightScale _HeightScale = ECkDebug_FrameStripHeightScale::RelativeToMax;
    TAttribute<int32> _SelectedIndexFromEnd;

    bool _ShowStatusLabel = true;
    FText _LiveLabel;
    FText _FrozenLabel;
    FString _ValueSuffix;
    FString _MarkerMeaning;
    TAttribute<FString> _CopyText;
    bool _AllowZoom = true;

    FOnCkDebug_FrameStripScrubbed _OnScrubbed;
    FOnCkDebug_FrameStripReturnedToLive _OnReturnToLive;

    // ---- View state. Mutable because clamping needs the paint-time width, exactly as the
    // Scheduler strip did — there is no layout pass that could do it earlier.
    mutable float _ScrollOffsetX = 0.0f;
    float _ZoomScale = 1.0f;

    // ---- Interaction state
    bool _IsScrubbing = false;
    bool _IsPanning = false;
    bool _PanWasDragged = false;
    float _PanStartX = 0.0f;
    float _PanStartScrollX = 0.0f;
    int32 _HoveredIndexFromEnd = INDEX_NONE;
};

// ====================================================================================================================
