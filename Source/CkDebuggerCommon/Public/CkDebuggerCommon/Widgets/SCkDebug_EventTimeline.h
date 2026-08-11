#pragma once

#include "Widgets/SLeafWidget.h"

// ====================================================================================================================
// Event timeline — the mockup's session-timeline dock: horizontal lanes with
// time-positioned event markers, a tick axis, and activation spans.
//
//   Square markers   world-state changes (informational, tooltip only)
//   Diamond markers  replans — selectable; the owner scrubs to the selected one
//   Spans            activation bars (e.g. "sub-planner active from t1..t2")
//
// Pure data-in widget: the owner supplies lanes/events/spans and re-supplies
// them via Set_Content when the underlying history changes (cheap — one
// array copy; paint is immediate-mode). Selection and hover render live.
//
// Marker tooltips: SLeafWidget can't host per-element tooltip widgets, so the
// widget tracks the hovered marker in OnMouseMove and exposes its tooltip
// through the normal SWidget tooltip (bound internally).
// ====================================================================================================================

enum class ECkDebug_TimelineMarker : uint8
{
    Square,     // informational (WS change)
    Diamond,    // selectable event (replan)
};

struct FCkDebug_TimelineEvent
{
    int32  LaneIndex = 0;
    double TimeSeconds = 0.0;
    ECkDebug_TimelineMarker Shape = ECkDebug_TimelineMarker::Square;
    FLinearColor Color = FLinearColor::White;
    FString Tooltip;

    // >= 0 makes the marker selectable; reported through OnEventSelected.
    int32  SelectionId = INDEX_NONE;

    // Rendered just right of the marker ("×2" coalesce note).
    FString SideLabel;
};

struct FCkDebug_TimelineSpan
{
    int32  LaneIndex = 0;
    double StartSeconds = 0.0;
    double EndSeconds = 0.0;
    FLinearColor Color = FLinearColor::White;
    FString Tooltip;
};

DECLARE_DELEGATE_OneParam(FOnCkDebug_TimelineEventSelected, int32 /* SelectionId */);
DECLARE_DELEGATE_OneParam(FOnCkDebug_TimelineScrubbed, double /* Time */);
DECLARE_DELEGATE_RetVal_OneParam(FString, FOnCkDebug_TimelineFormatTick, double /* Time */);

class CKDEBUGGERCOMMON_API SCkDebug_EventTimeline : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_EventTimeline)
        : _DesiredHeight(96.0f)
        , _AllowPanZoom(false)
        , _InitialViewDuration(0.0)
    {}
        SLATE_ARGUMENT(TArray<FString>, LaneLabels)
        SLATE_ATTRIBUTE(int32, SelectedId)
        SLATE_ARGUMENT(float, DesiredHeight)
        SLATE_EVENT(FOnCkDebug_TimelineEventSelected, OnEventSelected)

        // Opt-in view-window interaction (the ScrubTimeline contract): wheel zooms about the cursor,
        // RMB/MMB/Ctrl+LMB drag pans, F re-attaches the live-following full view. The window is owned by the
        // WIDGET; when following, Set_Content keeps its right edge glued to the newest data.
        SLATE_ARGUMENT(bool, AllowPanZoom)

        // > 0 opens with a live-following window of this many axis units instead of the full data range.
        // A full-range view makes pan a structural no-op (there is nothing outside it to pan to) — an initial
        // window is what makes right-drag immediately useful, exactly like the SM scrub track's 10s default.
        SLATE_ARGUMENT(double, InitialViewDuration)

        // When bound, LMB press/drag on empty track reports the cursor's axis value (marker clicks still select).
        SLATE_EVENT(FOnCkDebug_TimelineScrubbed, OnScrubbed)

        // When set, a full-height cursor line is drawn at this axis value — the visible half of scrubbing.
        SLATE_ATTRIBUTE(TOptional<double>, CursorTime)

        // Ruler label text. Default is "%.*fs" — frame-indexed consumers return "f<frame>" instead.
        SLATE_EVENT(FOnCkDebug_TimelineFormatTick, OnFormatTick)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Replace the rendered content. TimeMin/TimeMax define the DATA range;
    // with no custom view the axis window is that whole range.
    auto Set_Content(
        double InTimeMin,
        double InTimeMax,
        TArray<FCkDebug_TimelineEvent> InEvents,
        TArray<FCkDebug_TimelineSpan> InSpans) -> void;

    // View-window access, for carrying pan/zoom across a lane-set rebuild. Duration <= 0 = full-range live view.
    auto Get_ViewStart() const -> double { return _ViewStart; }
    auto Get_ViewDuration() const -> double { return _ViewDuration; }
    auto Set_View(double InViewStart, double InViewDuration) -> void;

    // True while a scrub or pan drag holds mouse capture — the owner must not rebuild this widget mid-drag
    // (destroying the captor kills the drag under the user's finger).
    auto Get_IsInteracting() const -> bool { return _IsScrubbing || _IsPanning; }

    // Follow state is OWNED, never inferred: a parked window that happens to touch the live edge must stay
    // parked across rebuilds — the owner carries this flag explicitly alongside the view window.
    auto Get_IsFollowingLive() const -> bool { return _FollowLive; }
    auto Set_FollowLive(bool InFollow) -> void { _FollowLive = InFollow; }

    // Re-attach the window's right edge to the newest data without changing zoom — the owner's "go live".
    auto Refollow_Live() -> void { Set_FollowLive(true); }

protected:
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
    auto OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseLeave(const FPointerEvent& InMouseEvent) -> void override;
    auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;

    auto SupportsKeyboardFocus() const -> bool override { return _AllowPanZoom; }

private:
    // The effective axis window — the custom view when one is set, the full data range otherwise.
    auto Get_ViewMin() const -> double;
    auto Get_ViewMax() const -> double;

    // X pixel for a time value inside the given paint width.
    auto Compute_X(double InTime, float InWidth) const -> float;
    auto Compute_Time(float InLocalX, float InWidth) const -> double;
    auto Compute_LaneY(int32 InLane, float InHeight) const -> float;

    // Nearest selectable event within the pick radius; INDEX_NONE otherwise.
    auto Find_EventAt(const FVector2D& InLocal, const FVector2D& InSize) const -> int32;

    auto Do_ApplyView(double InViewStart, double InViewDuration) -> void;

private:
    TArray<FString> _LaneLabels;
    TAttribute<int32> _SelectedId;
    float _DesiredHeight = 96.0f;
    bool _AllowPanZoom = false;
    FOnCkDebug_TimelineEventSelected _OnEventSelected;
    FOnCkDebug_TimelineScrubbed _OnScrubbed;
    FOnCkDebug_TimelineFormatTick _OnFormatTick;
    TAttribute<TOptional<double>> _CursorTime;

    double _TimeMin = 0.0;
    double _TimeMax = 1.0;
    TArray<FCkDebug_TimelineEvent> _Events;
    TArray<FCkDebug_TimelineSpan> _Spans;

    // Lane-label gutter, measured from the labels at construction so long lane names never sit under the plot.
    float _PadLeft = 56.0f;

    // Custom view window; Duration <= 0 = none (full data range). When following, Set_Content re-anchors the
    // window's right edge on the newest data.
    double _ViewStart = 0.0;
    double _ViewDuration = 0.0;
    bool _FollowLive = true;

    // Interaction state
    bool _IsScrubbing = false;
    bool _IsPanning = false;
    float _PanStartX = 0.0f;
    double _PanStartViewStart = 0.0;

    // Hover state — feeds the widget-level tooltip + hover ring.
    int32 _HoveredEventIndex = INDEX_NONE;
};

// ====================================================================================================================
