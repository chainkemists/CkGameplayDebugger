#include "CkInsightsDebugger/Widgets/SCkFrameTimingGraph.h"

#include "CkInsightsAnalyzer/Core/CkTimerCategorizer.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkCore/Format/CkFormat.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Layout/Clipping.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBox.h"

#include <limits>

namespace ck_frame_timing_graph
{
    constexpr float DesiredWidth = 400.0f;
    constexpr float DesiredHeight = 150.0f;
    constexpr float HeaderHeight = 24.0f;
    constexpr float RowHeight = 18.0f;
    constexpr float RowGap = 2.0f;
    constexpr float HorizontalPadding = 4.0f;
    constexpr float LabelPadding = 4.0f;
    constexpr float MinimumLabelRowHeight = 10.0f;
    constexpr float DragThresholdPx = 4.0f;
    constexpr double DefaultVisibleRowCount =
        static_cast<double>(DesiredHeight - HeaderHeight) / static_cast<double>(RowHeight + RowGap);

    auto ColorBackground() -> FLinearColor { return CkStyle::BgRoot(); }
    auto ColorHeader() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Bg3(), 0.85f); }
    auto ColorGrid() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::BorderStrong(), 0.35f); }
    auto ColorText() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::TextDim(), 0.9f); }
    auto ColorHover() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::TextStrong(), 0.30f); }
    auto ColorSelected() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Selection(), 0.30f); }
    auto GraphFont() -> FSlateFontInfo { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); }

    auto Brighten(const FLinearColor& InColor) -> FLinearColor
    {
        return FLinearColor{
            FMath::Min(InColor.R * 1.25f, 1.0f),
            FMath::Min(InColor.G * 1.25f, 1.0f),
            FMath::Min(InColor.B * 1.25f, 1.0f),
            InColor.A};
    }

    auto CompositeOver(const FLinearColor& InForeground, const FLinearColor& InBackground) -> FLinearColor
    {
        const auto Alpha = FMath::Clamp(InForeground.A, 0.0f, 1.0f);
        return FLinearColor{
            InForeground.R * Alpha + InBackground.R * (1.0f - Alpha),
            InForeground.G * Alpha + InBackground.G * (1.0f - Alpha),
            InForeground.B * Alpha + InBackground.B * (1.0f - Alpha),
            1.0f};
    }

    auto RelativeLuminance(const FLinearColor& InColor) -> float
    {
        return 0.2126f * InColor.R + 0.7152f * InColor.G + 0.0722f * InColor.B;
    }

    auto ContrastRatio(const FLinearColor& InFirst, const FLinearColor& InSecond) -> float
    {
        const auto First = RelativeLuminance(InFirst);
        const auto Second = RelativeLuminance(InSecond);
        return (FMath::Max(First, Second) + 0.05f) / (FMath::Min(First, Second) + 0.05f);
    }

    auto ContrastingText(const FLinearColor& InBackground) -> FLinearColor
    {
        auto Light = CkStyle::TextStrong();
        auto Dark = CkStyle::BgRoot();
        Light.A = 1.0f;
        Dark.A = 1.0f;
        return ContrastRatio(Light, InBackground) >= ContrastRatio(Dark, InBackground)
            ? Light
            : Dark;
    }

    auto FindTimer(const FCkInsightsFrameTimingView& InView, uint32 InTimerIndex)
        -> const FCkInsightsFrameTimingTimer*
    {
        return InView.Timers.Find(InTimerIndex);
    }
}

auto
    SCkFrameTimingGraph::
    Construct(const FArguments& InArgs)
    -> void
{
    _OnTimerSelectionChanged = InArgs._OnTimerSelectionChanged;
    SetClipping(EWidgetClipping::ClipToBoundsAlways);
    _EventTooltip = SNew(SToolTip)
        .TextMargin(FMargin(4.0f))
        [
            SAssignNew(_TooltipTextBlock, STextBlock)
            .Font_Lambda([]() { return ck_frame_timing_graph::GraphFont(); })
        ];
    SetToolTip(_EventTooltip);
    SetVisibility(EVisibility::Visible);

    ChildSlot
    [
        SNew(SBox)
        .Visibility(EVisibility::Visible)
    ];
}

auto
    SCkFrameTimingGraph::
    SetView(TSharedPtr<const FCkInsightsFrameTimingView, ESPMode::ThreadSafe> InView)
    -> void
{
    const bool IsSameView = _View.Get() == InView.Get();
    _View = MoveTemp(InView);
    if (NOT IsSameView)
    {
        _HoveredEvent = INDEX_NONE;
        SetSelectedTimerIndex({}, true);
        _ViewStartMs = 0.0;
        _VisibleDurationMs = GetDurationMs();
        _ViewTopRow = 0.0;
        _VisibleRowCount = HasData() ? ck_frame_timing_graph::DefaultVisibleRowCount : 0.0;
        _bPointerDown = false;
        _bPanning = false;
        UpdateTooltip(INDEX_NONE);
    }
    ClampViewport();
    UpdateTimeScaleLabels();
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

auto
    SCkFrameTimingGraph::
    ClearView()
    -> void
{
    SetView(nullptr);
}

auto
    SCkFrameTimingGraph::
    HasData() const
    -> bool
{
    return _View.IsValid() && _View->IsValid() && NOT _View->Events.IsEmpty();
}

auto
    SCkFrameTimingGraph::
    GetSelectedOccurrenceCount() const
    -> int32
{
    if (NOT HasData() || NOT _SelectedTimerIndex.IsSet())
    {
        return 0;
    }
    int32 OccurrenceCount = 0;
    for (const auto& Event : _View->Events)
    {
        OccurrenceCount += Event.TimerIndex == _SelectedTimerIndex.GetValue() ? 1 : 0;
    }
    return OccurrenceCount;
}

auto
    SCkFrameTimingGraph::
    FormatDuration(double InMilliseconds)
    -> FString
{
    return InMilliseconds >= 1000.0
        ? FString::Printf(TEXT("%.2fs"), InMilliseconds / 1000.0)
        : InMilliseconds >= 0.001
            ? FCk_TimerCategorizer::FormatMs(InMilliseconds)
            : InMilliseconds >= 0.000001
                ? FString::Printf(TEXT("%.3fus"), InMilliseconds * 1000.0)
                : InMilliseconds >= 0.000000001
                    ? FString::Printf(TEXT("%.3fns"), InMilliseconds * 1000000.0)
                    : FString::Printf(TEXT("%.3fps"), InMilliseconds * 1000000000.0);
}

auto
    SCkFrameTimingGraph::
    FormatEventLabel(const FString& InDisplayName, double InMilliseconds)
    -> FString
{
    return FString::Printf(TEXT("%s (%s)"), *InDisplayName, *FormatDuration(InMilliseconds));
}

auto
    SCkFrameTimingGraph::
    ShouldAttemptLabel(float InBarWidth)
    -> bool
{
    return InBarWidth > ck_frame_timing_graph::LabelPadding * 2.0f;
}

auto
    SCkFrameTimingGraph::
    SetTimerSelection(TOptional<uint32> InTimerIndex)
    -> void
{
    SetSelectedTimerIndex(InTimerIndex, false);
}

auto
    SCkFrameTimingGraph::
    ComputeDesiredSize(float) const
    -> FVector2D
{
    using namespace ck_frame_timing_graph;
    const auto ContentHeight = HasData()
        ? HeaderHeight + static_cast<float>(_View->MaxDepth + 1) * (RowHeight + RowGap)
        : DesiredHeight;
    return FVector2D(DesiredWidth, FMath::Max(DesiredHeight, ContentHeight));
}

auto
    SCkFrameTimingGraph::
    OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
            const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
            int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
    -> int32
{
    using namespace ck_frame_timing_graph;

    const FVector2D Size = AllottedGeometry.GetLocalSize();
    if (Size.X <= 0.0f || Size.Y <= 0.0f)
    {
        return LayerId;
    }

    const FSlateBrush* FilledBrush = FAppStyle::GetBrush(TEXT("WhiteBrush"));
    FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
        AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform{}), FilledBrush,
        ESlateDrawEffect::None, ColorBackground());

    if (NOT HasData())
    {
        return LayerId + 1;
    }

    const FCkInsightsFrameTimingView& View = *_View;
    const float GraphWidth = GetGraphWidth(AllottedGeometry);
    const auto HeaderFont = GraphFont();
    const auto EventFont = GraphFont();
    const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const auto LocalCullTop = AllottedGeometry.AbsoluteToLocal(
        FVector2D{MyCullingRect.Left, MyCullingRect.Top}).Y;
    const auto LocalCullBottom = AllottedGeometry.AbsoluteToLocal(
        FVector2D{MyCullingRect.Right, MyCullingRect.Bottom}).Y;

    FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
        AllottedGeometry.ToPaintGeometry(FVector2D(Size.X, HeaderHeight), FSlateLayoutTransform{}), FilledBrush,
        ESlateDrawEffect::None, ColorHeader());

    // A fixed four-part scale makes nested bar placement readable without an axis widget.
    for (int32 Tick = 0; Tick <= 4; ++Tick)
    {
        const double Fraction = static_cast<double>(Tick) / 4.0;
        const float X = HorizontalPadding + GraphWidth * static_cast<float>(Fraction);
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2,
            AllottedGeometry.ToPaintGeometry(
                FVector2D{1.0f, Size.Y - HeaderHeight + 4.0f},
                FSlateLayoutTransform{FVector2D{X, HeaderHeight - 4.0f}}),
            FilledBrush,
            ESlateDrawEffect::None,
            ColorGrid());

        const FString& TickLabel = _TimeScaleLabels[Tick];
        FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(30.0f, 10.0f),
                FSlateLayoutTransform(FVector2D(X + 2.0f, 11.0f))),
            TickLabel, HeaderFont, ESlateDrawEffect::None, ColorText());
    }

    FSlateDrawElement::MakeText(OutDrawElements, LayerId + 3,
        AllottedGeometry.ToPaintGeometry(FVector2D(Size.X, 10.0f), FSlateLayoutTransform(FVector2D(HorizontalPadding, 0.0f))),
        View.HeaderText, HeaderFont, ESlateDrawEffect::None, ColorText());

    const bool HasVisibleSelection = _SelectedTimerIndex.IsSet() && GetSelectedOccurrenceCount() > 0;
    const auto GetBarColor = [this, HasVisibleSelection](int32 InEventIndex, uint32 InTimerIndex)
    {
        auto Result = ck::debug_axes::Get_CategoricalColor(static_cast<int32>(InTimerIndex));
        if (HasVisibleSelection && _SelectedTimerIndex.GetValue() != InTimerIndex)
        {
            Result.A *= 0.28f;
        }
        Result = CompositeOver(Result, ColorBackground());
        if (_HoveredEvent == InEventIndex)
        {
            Result = CompositeOver(ColorHover(), Result);
        }
        if (HasVisibleSelection && _SelectedTimerIndex.GetValue() == InTimerIndex)
        {
            Result = CompositeOver(ColorSelected(), Result);
        }
        return Result;
    };

    const FVector2D BodyClipTopLeft = AllottedGeometry.LocalToAbsolute(FVector2D{0.0f, HeaderHeight});
    const FVector2D BodyClipBottomRight = AllottedGeometry.LocalToAbsolute(Size);
    OutDrawElements.PushClip(FSlateClippingZone(FSlateRect(
        BodyClipTopLeft.X,
        BodyClipTopLeft.Y,
        BodyClipBottomRight.X,
        BodyClipBottomRight.Y)));

    for (int32 EventIndex = 0; EventIndex < View.Events.Num(); ++EventIndex)
    {
        const FCkInsightsFrameTimingEvent& Event = View.Events[EventIndex];
        if (FindTimer(View, Event.TimerIndex) == nullptr)
        {
            continue;
        }

        const auto EventRect = GetEventRect(Event, AllottedGeometry);
        const float BarWidth = EventRect.Right - EventRect.Left;
        const float BarHeight = EventRect.Bottom - EventRect.Top;
        if (EventRect.Top >= Size.Y
            || EventRect.Bottom <= HeaderHeight
            || EventRect.Bottom < LocalCullTop
            || EventRect.Top > LocalCullBottom
            || BarWidth <= 0.0f
            || BarHeight <= 0.0f)
        {
            continue;
        }

        const auto BarColor = GetBarColor(EventIndex, Event.TimerIndex);
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 4,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(BarWidth, BarHeight),
                FSlateLayoutTransform(FVector2D(EventRect.Left, EventRect.Top))),
            FilledBrush, ESlateDrawEffect::None, Brighten(BarColor));
        if (BarWidth > 2.0f && BarHeight > 2.0f)
        {
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 4,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(BarWidth - 2.0f, BarHeight - 2.0f),
                    FSlateLayoutTransform(FVector2D(EventRect.Left + 1.0f, EventRect.Top + 1.0f))),
                FilledBrush, ESlateDrawEffect::None, BarColor);
        }
    }

    for (int32 EventIndex = 0; EventIndex < View.Events.Num(); ++EventIndex)
    {
        const FCkInsightsFrameTimingEvent& Event = View.Events[EventIndex];
        const auto* Timer = FindTimer(View, Event.TimerIndex);
        if (Timer == nullptr)
        {
            continue;
        }

        const auto EventRect = GetEventRect(Event, AllottedGeometry);
        const float BarWidth = EventRect.Right - EventRect.Left;
        const float BarHeight = EventRect.Bottom - EventRect.Top;
        if (EventRect.Top >= Size.Y
            || EventRect.Bottom <= HeaderHeight
            || EventRect.Bottom < LocalCullTop
            || EventRect.Top > LocalCullBottom
            || BarWidth <= 0.0f
            || BarHeight <= 0.0f)
        {
            continue;
        }

        const FString Label = FormatEventLabel(Timer->DisplayName, Event.EndMs - Event.StartMs);
        if (NOT Label.IsEmpty() && BarHeight >= MinimumLabelRowHeight && ShouldAttemptLabel(BarWidth))
        {
            const auto LabelWidth = static_cast<float>(FontMeasure->Measure(Label, EventFont).X);
            const float VisibleBarTop = FMath::Max(EventRect.Top, HeaderHeight);
            const FVector2D ClipTopLeft = AllottedGeometry.LocalToAbsolute(
                FVector2D(EventRect.Left + LabelPadding, VisibleBarTop));
            const FVector2D ClipBottomRight = AllottedGeometry.LocalToAbsolute(
                FVector2D(EventRect.Right - LabelPadding, FMath::Min(EventRect.Bottom, Size.Y)));
            OutDrawElements.PushClip(FSlateClippingZone(FSlateRect(
                ClipTopLeft.X, ClipTopLeft.Y, ClipBottomRight.X, ClipBottomRight.Y)));
            FSlateDrawElement::MakeText(OutDrawElements, LayerId + 5,
                AllottedGeometry.ToPaintGeometry(FVector2D(LabelWidth, BarHeight),
                    FSlateLayoutTransform(FVector2D(EventRect.Left + LabelPadding, VisibleBarTop + 3.0f))),
                Label, EventFont, ESlateDrawEffect::None,
                ContrastingText(GetBarColor(EventIndex, Event.TimerIndex)));
            OutDrawElements.PopClip();
        }
    }

    OutDrawElements.PopClip();

    return LayerId + 6;
}

auto
    SCkFrameTimingGraph::
    OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> FReply
{
    if (NOT HasData() ||
        (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton &&
         MouseEvent.GetEffectingButton() != EKeys::MiddleMouseButton &&
         MouseEvent.GetEffectingButton() != EKeys::RightMouseButton))
    {
        return FReply::Unhandled();
    }

    _bPointerDown = true;
    _bPanning = false;
    UpdateVisibleRowCount(MyGeometry);
    ClampViewport();
    _DragButton = MouseEvent.GetEffectingButton();
    _DragStartPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    _DragStartViewMs = _ViewStartMs;
    _DragStartTopRow = _ViewTopRow;
    return FReply::Handled().CaptureMouse(SharedThis(this));
}

auto
    SCkFrameTimingGraph::
    OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> FReply
{
    if (NOT _bPointerDown || MouseEvent.GetEffectingButton() != _DragButton)
    {
        return FReply::Unhandled();
    }

    const bool WasPanning = _bPanning;
    const FKey ReleasedButton = _DragButton;
    _bPointerDown = false;
    _bPanning = false;
    _DragButton = EKeys::Invalid;

    if (ReleasedButton == EKeys::LeftMouseButton && NOT WasPanning)
    {
        const int32 EventIndex = Get_EventAt(MyGeometry,
            MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
        if (EventIndex != INDEX_NONE)
        {
            SetSelectedTimerIndex(_View->Events[EventIndex].TimerIndex, true);
        }
        else
        {
            SetSelectedTimerIndex({}, false);
            _OnTimerSelectionChanged.ExecuteIfBound({});
        }
    }

    return FReply::Handled().ReleaseMouseCapture();
}

auto
    SCkFrameTimingGraph::
    OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> FReply
{
    const FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    if (_bPointerDown)
    {
        const FVector2D Delta = LocalPosition - _DragStartPosition;
        if (NOT _bPanning && (FMath::Abs(Delta.X) >= ck_frame_timing_graph::DragThresholdPx ||
            (_DragButton == EKeys::RightMouseButton && FMath::Abs(Delta.Y) >= ck_frame_timing_graph::DragThresholdPx)))
        {
            _bPanning = true;
        }
        if (_bPanning)
        {
            UpdateVisibleRowCount(MyGeometry);
            _ViewStartMs = _DragStartViewMs - static_cast<double>(Delta.X) * GetMillisecondsPerPixel(MyGeometry);
            if (_DragButton == EKeys::RightMouseButton)
            {
                _ViewTopRow = _DragStartTopRow - static_cast<double>(Delta.Y) * GetRowsPerPixel(MyGeometry);
            }
            ClampViewport();
            UpdateTimeScaleLabels();
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled();
        }
    }

    const int32 NewHoveredEvent = Get_EventAt(MyGeometry, LocalPosition);
    if (NewHoveredEvent != _HoveredEvent)
    {
        _HoveredEvent = NewHoveredEvent;
        UpdateTooltip(_HoveredEvent);
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    return FReply::Unhandled();
}

auto
    SCkFrameTimingGraph::
    OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> FReply
{
    if (NOT HasData())
    {
        return FReply::Unhandled();
    }
    ApplyZoom(MyGeometry, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()), MouseEvent.GetWheelDelta());
    return FReply::Handled();
}

auto
    SCkFrameTimingGraph::
    OnMouseLeave(const FPointerEvent& MouseEvent)
    -> void
{
    SCompoundWidget::OnMouseLeave(MouseEvent);
    _HoveredEvent = INDEX_NONE;
    UpdateTooltip(INDEX_NONE);
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkFrameTimingGraph::
    Get_EventAt(const FGeometry& InGeometry, const FVector2D& InLocalPosition) const
    -> int32
{
    using namespace ck_frame_timing_graph;

    if (NOT HasData() || InLocalPosition.Y < HeaderHeight
        || InLocalPosition.Y > InGeometry.GetLocalSize().Y
        || InLocalPosition.X < HorizontalPadding
        || InLocalPosition.X > InGeometry.GetLocalSize().X - HorizontalPadding)
    {
        return INDEX_NONE;
    }

    const FCkInsightsFrameTimingView& View = *_View;

    // Reverse traversal matches paint precedence if malformed input contains overlapping siblings.
    for (int32 EventIndex = View.Events.Num() - 1; EventIndex >= 0; --EventIndex)
    {
        const FCkInsightsFrameTimingEvent& Event = View.Events[EventIndex];
        if (FindTimer(View, Event.TimerIndex) == nullptr) continue;

        const auto EventRect = GetEventRect(Event, InGeometry);
        if (InLocalPosition.X >= EventRect.Left && InLocalPosition.X <= EventRect.Right
            && InLocalPosition.Y >= EventRect.Top && InLocalPosition.Y <= EventRect.Bottom)
        {
            return EventIndex;
        }
    }
    return INDEX_NONE;
}

auto
    SCkFrameTimingGraph::
    GetEventRect(const FCkInsightsFrameTimingEvent& InEvent, const FGeometry& InGeometry) const
    -> FSlateRect
{
    using namespace ck_frame_timing_graph;

    const double ViewEndMs = _ViewStartMs + _VisibleDurationMs;
    if (InEvent.EndMs < _ViewStartMs || InEvent.StartMs > ViewEndMs)
    {
        return FSlateRect(0.0f, 0.0f, 0.0f, 0.0f);
    }

    const double MillisecondsPerPixel = GetMillisecondsPerPixel(InGeometry);
    const double VisibleStartMs = FMath::Max(InEvent.StartMs, _ViewStartMs);
    const double VisibleEndMs = FMath::Min(InEvent.EndMs, ViewEndMs);
    float Left = HorizontalPadding + static_cast<float>(FMath::RoundToDouble(
        (VisibleStartMs - _ViewStartMs) / MillisecondsPerPixel));
    float Right = HorizontalPadding + static_cast<float>(FMath::RoundToDouble(
        (VisibleEndMs - _ViewStartMs) / MillisecondsPerPixel));
    if (Right <= Left)
    {
        Right = Left + 1.0f;
    }

    // Insights keeps tiny events visible, but reserves one trailing pixel between representable
    // adjacent events. An event continuing past the viewport edge must still reach that edge.
    if (Right - Left > 1.0f && InEvent.EndMs <= ViewEndMs)
    {
        Right -= 1.0f;
    }

    const float Top = GetRowTop(InEvent.Depth, InGeometry);
    return FSlateRect(Left, Top, Right, Top + GetRowHeight(InGeometry));
}

auto
    SCkFrameTimingGraph::
    FocusTimer(uint32 InTimerIndex)
    -> void
{
    if (NOT HasData())
    {
        return;
    }

    double FirstStartMs = TNumericLimits<double>::Max();
    double LastEndMs = 0.0;
    uint32 FirstDepth = 0;
    bool bFound = false;
    for (const FCkInsightsFrameTimingEvent& Event : _View->Events)
    {
        if (Event.TimerIndex != InTimerIndex) continue;
        if (Event.StartMs < FirstStartMs)
        {
            FirstStartMs = Event.StartMs;
            FirstDepth = Event.Depth;
        }
        LastEndMs = FMath::Max(LastEndMs, Event.EndMs);
        bFound = true;
    }
    if (NOT bFound)
    {
        return;
    }

    const double UnionDuration = FMath::Max(0.0, LastEndMs - FirstStartMs);
    const double FocusDuration = UnionDuration <= _VisibleDurationMs ? UnionDuration : 0.0;
    _ViewStartMs = FirstStartMs - (_VisibleDurationMs - FocusDuration) * 0.5;
    const FGeometry& CachedGeometry = GetCachedGeometry();
    if (CachedGeometry.GetLocalSize().Y > ck_frame_timing_graph::HeaderHeight)
    {
        UpdateVisibleRowCount(CachedGeometry);
    }
    _ViewTopRow = static_cast<double>(FirstDepth) - (_VisibleRowCount - 1.0) * 0.5;
    ClampViewport();
    UpdateTimeScaleLabels();
    SetSelectedTimerIndex(InTimerIndex, false);
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkFrameTimingGraph::
    GetGraphWidth(const FGeometry& InGeometry) const
    -> float
{
    return FMath::Max(1.0f, InGeometry.GetLocalSize().X - ck_frame_timing_graph::HorizontalPadding * 2.0f);
}

auto
    SCkFrameTimingGraph::
    GetGraphHeight(const FGeometry& InGeometry) const
    -> float
{
    return FMath::Max(1.0f, InGeometry.GetLocalSize().Y - ck_frame_timing_graph::HeaderHeight);
}

auto
    SCkFrameTimingGraph::
    GetDurationMs() const
    -> double
{
    return HasData() ? FMath::Max(_View->FrameDurationMs, static_cast<double>(KINDA_SMALL_NUMBER)) : 0.0;
}

auto
    SCkFrameTimingGraph::
    GetMillisecondsPerPixel(const FGeometry& InGeometry) const
    -> double
{
    return _VisibleDurationMs / static_cast<double>(GetGraphWidth(InGeometry));
}

auto
    SCkFrameTimingGraph::
    GetRowsPerPixel(const FGeometry&) const
    -> double
{
    return 1.0 / static_cast<double>(ck_frame_timing_graph::RowHeight + ck_frame_timing_graph::RowGap);
}

auto
    SCkFrameTimingGraph::
    GetRowTop(uint32 InDepth, const FGeometry&) const
    -> float
{
    return ck_frame_timing_graph::HeaderHeight + static_cast<float>(
        (static_cast<double>(InDepth) - _ViewTopRow) *
        static_cast<double>(ck_frame_timing_graph::RowHeight + ck_frame_timing_graph::RowGap));
}

auto
    SCkFrameTimingGraph::
    GetRowHeight(const FGeometry&) const
    -> float
{
    return ck_frame_timing_graph::RowHeight;
}

auto
    SCkFrameTimingGraph::
    UpdateVisibleRowCount(const FGeometry& InGeometry)
    -> void
{
    const double RowStride = static_cast<double>(ck_frame_timing_graph::RowHeight + ck_frame_timing_graph::RowGap);
    _VisibleRowCount = FMath::Max(1.0, static_cast<double>(GetGraphHeight(InGeometry)) / RowStride);
}

auto
    SCkFrameTimingGraph::
    ClampViewport()
    -> void
{
    const double DurationMs = GetDurationMs();
    if (DurationMs <= 0.0)
    {
        _ViewStartMs = 0.0;
        _VisibleDurationMs = 0.0;
        return;
    }

    // Stop only where another cursor-anchored subdivision cannot produce stable, distinct double
    // timestamps. The former frame-duration / 10,000 cap hid short trace events unnecessarily.
    const double MinimumVisibleDuration = DurationMs * 64.0 * std::numeric_limits<double>::epsilon();
    _VisibleDurationMs = FMath::Clamp(_VisibleDurationMs, MinimumVisibleDuration, DurationMs);
    _ViewStartMs = FMath::Clamp(_ViewStartMs, 0.0, DurationMs - _VisibleDurationMs);
    const double TotalRows = static_cast<double>(_View->MaxDepth + 1);
    const double MaxTopRow = FMath::Max(0.0, TotalRows - _VisibleRowCount);
    _ViewTopRow = FMath::Clamp(_ViewTopRow, 0.0, MaxTopRow);
}

auto
    SCkFrameTimingGraph::
    ApplyZoom(const FGeometry& InGeometry, const FVector2D& InLocalPosition, float InDelta)
    -> void
{
    if (InDelta == 0.0f || NOT HasData())
    {
        return;
    }

    UpdateVisibleRowCount(InGeometry);
    ClampViewport();
    const float GraphWidth = GetGraphWidth(InGeometry);
    const double CursorFraction = FMath::Clamp(
        static_cast<double>(InLocalPosition.X - ck_frame_timing_graph::HorizontalPadding) / static_cast<double>(GraphWidth),
        0.0,
        1.0);
    const double TimeUnderCursor = _ViewStartMs + CursorFraction * _VisibleDurationMs;
    const double ZoomFactor = InDelta > 0.0f ? 0.8 : 1.25;

    _VisibleDurationMs *= ZoomFactor;
    ClampViewport();
    _ViewStartMs = TimeUnderCursor - CursorFraction * _VisibleDurationMs;
    ClampViewport();
    UpdateTimeScaleLabels();
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkFrameTimingGraph::
    UpdateTimeScaleLabels()
    -> void
{
    constexpr int32 TickCount = 5;
    _TimeScaleLabels.SetNum(TickCount);
    const double TickStepMs = _VisibleDurationMs / static_cast<double>(TickCount - 1);
    for (int32 Tick = 0; Tick < TickCount; ++Tick)
    {
        const double Fraction = static_cast<double>(Tick) / static_cast<double>(TickCount - 1);
        _TimeScaleLabels[Tick] = FormatTimeScaleLabel(
            _ViewStartMs + Fraction * _VisibleDurationMs,
            TickStepMs);
    }
}

auto
    SCkFrameTimingGraph::
    FormatTimeScaleLabel(double InMilliseconds, double InTickStepMilliseconds)
    -> FString
{
    const bool bUseSeconds = InMilliseconds >= 1000.0;
    const double UnitScale = bUseSeconds ? 0.001 : 1.0;
    const double ScaledValue = InMilliseconds * UnitScale;
    const double ScaledStep = FMath::Max(
        InTickStepMilliseconds * UnitScale,
        std::numeric_limits<double>::min());
    const int32 DecimalPlaces = FMath::Clamp(
        FMath::CeilToInt(-FMath::LogX(10.0, ScaledStep)) + 1,
        0,
        15);
    return FString::Printf(
        TEXT("%.*f%s"),
        DecimalPlaces,
        ScaledValue,
        bUseSeconds ? TEXT("s") : TEXT("ms"));
}

auto
    SCkFrameTimingGraph::
    SetSelectedTimerIndex(TOptional<uint32> InTimerIndex, bool bNotify)
    -> void
{
    const bool IsUnchanged = _SelectedTimerIndex.IsSet() == InTimerIndex.IsSet() &&
        (NOT _SelectedTimerIndex.IsSet() || _SelectedTimerIndex.GetValue() == InTimerIndex.GetValue());
    if (IsUnchanged)
    {
        return;
    }

    _SelectedTimerIndex = InTimerIndex;
    Invalidate(EInvalidateWidgetReason::Paint);
    if (bNotify)
    {
        _OnTimerSelectionChanged.ExecuteIfBound(_SelectedTimerIndex);
    }
}

auto
    SCkFrameTimingGraph::
    UpdateTooltip(int32 InEventIndex)
    -> void
{
    using namespace ck_frame_timing_graph;

    if (NOT _TooltipTextBlock.IsValid() || NOT HasData() || InEventIndex < 0 || InEventIndex >= _View->Events.Num())
    {
        if (_TooltipTextBlock.IsValid()) _TooltipTextBlock->SetText(FText::GetEmpty());
        return;
    }

    const FCkInsightsFrameTimingEvent& Event = _View->Events[InEventIndex];
    const auto* Timer = FindTimer(*_View, Event.TimerIndex);
    if (Timer == nullptr)
    {
        _TooltipTextBlock->SetText(FText::GetEmpty());
        return;
    }

    _TooltipTextBlock->SetText(FText::FromString(ck::Format_UE(
        TEXT("{}\n{:.3f} ms | +{:.3f} ms | depth {}"),
        Timer->RawName,
        Event.EndMs - Event.StartMs,
        Event.StartMs,
        Event.Depth)));
}
