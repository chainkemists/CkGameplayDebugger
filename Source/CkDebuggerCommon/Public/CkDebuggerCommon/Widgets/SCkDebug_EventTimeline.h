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

class CKDEBUGGERCOMMON_API SCkDebug_EventTimeline : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_EventTimeline)
        : _DesiredHeight(96.0f)
    {}
        SLATE_ARGUMENT(TArray<FString>, LaneLabels)
        SLATE_ATTRIBUTE(int32, SelectedId)
        SLATE_ARGUMENT(float, DesiredHeight)
        SLATE_EVENT(FOnCkDebug_TimelineEventSelected, OnEventSelected)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    // Replace the rendered content. TimeMin/TimeMax define the axis window;
    // events/spans outside it clamp to the edges.
    auto Set_Content(
        double InTimeMin,
        double InTimeMax,
        TArray<FCkDebug_TimelineEvent> InEvents,
        TArray<FCkDebug_TimelineSpan> InSpans) -> void;

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
    auto OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseLeave(const FPointerEvent& InMouseEvent) -> void override;

private:
    // X pixel for a time value inside the given paint width.
    auto Compute_X(double InTime, float InWidth) const -> float;
    auto Compute_LaneY(int32 InLane, float InHeight) const -> float;

    // Nearest selectable event within the pick radius; INDEX_NONE otherwise.
    auto Find_EventAt(const FVector2D& InLocal, const FVector2D& InSize) const -> int32;

private:
    TArray<FString> _LaneLabels;
    TAttribute<int32> _SelectedId;
    float _DesiredHeight = 96.0f;
    FOnCkDebug_TimelineEventSelected _OnEventSelected;

    double _TimeMin = 0.0;
    double _TimeMax = 1.0;
    TArray<FCkDebug_TimelineEvent> _Events;
    TArray<FCkDebug_TimelineSpan> _Spans;

    // Hover state — feeds the widget-level tooltip + hover ring.
    int32 _HoveredEventIndex = INDEX_NONE;
};

// ====================================================================================================================
