#include "CkDebuggerCommon/Widgets/SCkDebug_EventTimeline.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

// ====================================================================================================================

namespace ck_debug_event_timeline
{
    constexpr auto PadLeftMin = 56.0f;   // gutter floor; the real gutter is measured from the lane labels
    constexpr auto PadLeftMax = 180.0f;
    constexpr auto PadRight  = 12.0f;
    constexpr auto PadTop    = 8.0f;
    constexpr auto AxisBand  = 18.0f;   // tick labels under the axis
    constexpr auto MarkerHalf = 4.0f;
    constexpr auto PickRadius = 12.0f;
    constexpr auto ZoomStepFactor = 1.25;

    // Edge margin (as a fraction of the window) at which a scrub drag starts auto-panning.
    constexpr auto ScrubEdgeMarginFraction = 0.05;

    // Nice tick step so ~6-8 labels span the window.
    auto Compute_TickStep(double InRange) -> double
    {
        if (InRange <= 0.0) { return 1.0; }
        const auto Raw = InRange / 7.0;
        const auto Mag = FMath::Pow(10.0, FMath::FloorToDouble(FMath::LogX(10.0, Raw)));
        const auto Norm = Raw / Mag;
        if (Norm < 1.5) { return Mag; }
        if (Norm < 3.5) { return 2.0 * Mag; }
        if (Norm < 7.5) { return 5.0 * Mag; }
        return 10.0 * Mag;
    }

    // Diamond outline points (closed) around a center.
    auto MakeDiamond(const FVector2D& InCenter, float InHalf) -> TArray<FVector2D>
    {
        return {
            InCenter + FVector2D(0.0f, -InHalf),
            InCenter + FVector2D(InHalf, 0.0f),
            InCenter + FVector2D(0.0f, InHalf),
            InCenter + FVector2D(-InHalf, 0.0f),
            InCenter + FVector2D(0.0f, -InHalf),
        };
    }
}

// ====================================================================================================================
// CONSTRUCT / CONTENT
// ====================================================================================================================

auto
    SCkDebug_EventTimeline::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _LaneLabels      = InArgs._LaneLabels;
    _SelectedId      = InArgs._SelectedId;
    _DesiredHeight   = InArgs._DesiredHeight;
    _AllowPanZoom    = InArgs._AllowPanZoom;
    _OnEventSelected = InArgs._OnEventSelected;
    _OnScrubbed      = InArgs._OnScrubbed;
    _OnFormatTick    = InArgs._OnFormatTick;
    _CursorTime      = InArgs._CursorTime;

    if (InArgs._InitialViewDuration > 0.0)
    {
        _ViewDuration = InArgs._InitialViewDuration;
        _FollowLive = true;
    }

    // The gutter is sized by the widest lane label — a marker at the window's left edge lands at the gutter
    // boundary, never under the text.
    {
        using namespace ck_debug_event_timeline;

        const auto LaneFont = CkStyle::BoldFont(CkStyle::FontSizeMicro());
        const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

        auto MaxLabelWidth = 0.0f;
        for (const auto& Label : _LaneLabels)
        { MaxLabelWidth = FMath::Max(MaxLabelWidth, static_cast<float>(FontMeasure->Measure(Label, LaneFont).X)); }

        _PadLeft = FMath::Clamp(MaxLabelWidth + 12.0f, PadLeftMin, PadLeftMax);
    }

    // Hover-tracked tooltip — the hovered marker's text through the normal
    // widget tooltip channel.
    SetToolTipText(TAttribute<FText>::CreateLambda([this]() -> FText
    {
        if (NOT _Events.IsValidIndex(_HoveredEventIndex)) { return FText::GetEmpty(); }
        return FText::FromString(_Events[_HoveredEventIndex].Tooltip);
    }));
}

auto
    SCkDebug_EventTimeline::
    Set_Content(
        double InTimeMin,
        double InTimeMax,
        TArray<FCkDebug_TimelineEvent> InEvents,
        TArray<FCkDebug_TimelineSpan> InSpans)
    -> void
{
    _TimeMin = InTimeMin;
    _TimeMax = FMath::Max(InTimeMax, InTimeMin + KINDA_SMALL_NUMBER);
    _Events  = MoveTemp(InEvents);
    _Spans   = MoveTemp(InSpans);
    _HoveredEventIndex = INDEX_NONE;

    if (_ViewDuration > 0.0)
    {
        // A zoomed window rides the data: following glues its right edge to the newest value; a parked one
        // only gets clamped back inside the range. The DURATION is never shrunk to fit a still-small data
        // range — early on, the window simply shows everything and grows into its size (Get_ViewMax caps).
        _ViewStart = _FollowLive
            ? FMath::Max(_TimeMin, _TimeMax - _ViewDuration)
            : FMath::Clamp(_ViewStart, _TimeMin, FMath::Max(_TimeMin, _TimeMax - _ViewDuration));
    }
}

auto
    SCkDebug_EventTimeline::
    Set_View(
        double InViewStart,
        double InViewDuration)
    -> void
{
    if (InViewDuration <= 0.0)
    {
        _ViewDuration = 0.0;
        _FollowLive = true;
        return;
    }

    // The one place follow IS derived — a caller-supplied window whose right edge sits at the newest data is
    // a live window (the ScrubTimeline's Set_View contract). Owners carrying explicit state override after.
    Do_ApplyView(InViewStart, InViewDuration);
    _FollowLive = _ViewStart + _ViewDuration >= _TimeMax - KINDA_SMALL_NUMBER;
}

auto
    SCkDebug_EventTimeline::
    Do_ApplyView(
        double InViewStart,
        double InViewDuration)
    -> void
{
    // The duration is the REQUESTED window — wider-than-data just shows everything and grows into its size
    // (the effective view caps at the data edges). Collapsing that case into the duration-0 "full view" was a
    // TRAP: pan and park are structural no-ops without a window, and one full zoom-out (or one rebuild while
    // the data range was still younger than the window) killed both permanently.
    //
    // Follow is NEVER derived here — the ScrubTimeline contract: every interaction site sets the flag
    // deliberately (scrub/pan/zoom detach, F re-attaches). Inferring it from "right edge at live" silently
    // un-parked freshly parked windows.
    _ViewDuration = InViewDuration;
    _ViewStart = FMath::Clamp(InViewStart, _TimeMin, FMath::Max(_TimeMin, _TimeMax - InViewDuration));
}

auto
    SCkDebug_EventTimeline::
    Get_ViewMin() const
    -> double
{
    return _ViewDuration > 0.0 ? _ViewStart : _TimeMin;
}

auto
    SCkDebug_EventTimeline::
    Get_ViewMax() const
    -> double
{
    return _ViewDuration > 0.0 ? FMath::Min(_ViewStart + _ViewDuration, _TimeMax) : _TimeMax;
}

auto
    SCkDebug_EventTimeline::
    ComputeDesiredSize(float) const
    -> FVector2D
{
    return FVector2D(320.0f, _DesiredHeight);
}

// ====================================================================================================================
// GEOMETRY HELPERS
// ====================================================================================================================

auto
    SCkDebug_EventTimeline::
    Compute_X(double InTime, float InWidth) const
    -> float
{
    using namespace ck_debug_event_timeline;

    const auto ViewMin = Get_ViewMin();
    const auto ViewMax = Get_ViewMax();

    const auto Usable = FMath::Max(1.0f, InWidth - _PadLeft - PadRight);
    const auto Alpha  = FMath::Clamp((InTime - ViewMin) / (ViewMax - ViewMin), 0.0, 1.0);
    return _PadLeft + static_cast<float>(Alpha) * Usable;
}

auto
    SCkDebug_EventTimeline::
    Compute_Time(float InLocalX, float InWidth) const
    -> double
{
    using namespace ck_debug_event_timeline;

    const auto ViewMin = Get_ViewMin();
    const auto ViewMax = Get_ViewMax();

    const auto Usable = FMath::Max(1.0f, InWidth - _PadLeft - PadRight);
    const auto Alpha  = FMath::Clamp((InLocalX - _PadLeft) / Usable, 0.0f, 1.0f);
    return ViewMin + static_cast<double>(Alpha) * (ViewMax - ViewMin);
}

auto
    SCkDebug_EventTimeline::
    Compute_LaneY(int32 InLane, float InHeight) const
    -> float
{
    using namespace ck_debug_event_timeline;

    const auto LaneCount = FMath::Max(1, _LaneLabels.Num());
    const auto Band = FMath::Max(1.0f, InHeight - PadTop - AxisBand);
    const auto Step = Band / static_cast<float>(LaneCount);
    return PadTop + Step * (static_cast<float>(InLane) + 0.5f);
}

auto
    SCkDebug_EventTimeline::
    Find_EventAt(const FVector2D& InLocal, const FVector2D& InSize) const
    -> int32
{
    using namespace ck_debug_event_timeline;

    auto BestIndex = int32{INDEX_NONE};
    auto BestDistSq = PickRadius * PickRadius;

    const auto ViewMin = Get_ViewMin();
    const auto ViewMax = Get_ViewMax();

    for (auto Index = 0; Index < _Events.Num(); ++Index)
    {
        const auto& Event = _Events[Index];

        // Culled markers must not be pickable either — an edge-clamped X would pick an invisible one.
        if (Event.TimeSeconds < ViewMin || Event.TimeSeconds > ViewMax)
        { continue; }

        const auto Center = FVector2D(
            Compute_X(Event.TimeSeconds, static_cast<float>(InSize.X)),
            Compute_LaneY(Event.LaneIndex, static_cast<float>(InSize.Y)));
        const auto DistSq = static_cast<float>(FVector2D::DistSquared(InLocal, Center));
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestIndex = Index;
        }
    }
    return BestIndex;
}

// ====================================================================================================================
// PAINT
// ====================================================================================================================

auto
    SCkDebug_EventTimeline::
    OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const
    -> int32
{
    using namespace ck_debug_event_timeline;

    const auto Size = InAllottedGeometry.GetLocalSize();
    const auto Width = static_cast<float>(Size.X);
    const auto Height = static_cast<float>(Size.Y);
    const auto AxisY = Height - AxisBand;

    const auto* FilledBrush = CkStyle::GetFilledBrush();
    const auto LaneFont = CkStyle::BoldFont(CkStyle::FontSizeMicro());
    const auto TickFont = CkStyle::MonoFont(CkStyle::FontSizeMicro());

    auto Layer = InLayerId;

    const auto ViewMin = Get_ViewMin();
    const auto ViewMax = Get_ViewMax();

    // ---- Lane labels -----------------------------------------------------------
    for (auto Lane = 0; Lane < _LaneLabels.Num(); ++Lane)
    {
        FSlateDrawElement::MakeText(
            OutDrawElements,
            Layer,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f(_PadLeft - 8.0f, 12.0f),
                FSlateLayoutTransform(FVector2f(4.0f, Compute_LaneY(Lane, Height) - 6.0f))),
            _LaneLabels[Lane],
            LaneFont,
            ESlateDrawEffect::None,
            CkStyle::TextMute());
    }

    // ---- Gutter boundary — the visual wall between labels and plot -------------
    {
        auto GutterPoints = TArray<FVector2D>{
            FVector2D(_PadLeft - 4.0f, PadTop),
            FVector2D(_PadLeft - 4.0f, AxisY)};
        FSlateDrawElement::MakeLines(
            OutDrawElements, Layer,
            InAllottedGeometry.ToPaintGeometry(),
            GutterPoints, ESlateDrawEffect::None, CkStyle::Border(), true, 1.0f);
    }

    // ---- Axis + ticks ----------------------------------------------------------
    {
        auto AxisPoints = TArray<FVector2D>{
            FVector2D(_PadLeft, AxisY),
            FVector2D(Width - PadRight, AxisY)};
        FSlateDrawElement::MakeLines(
            OutDrawElements, Layer,
            InAllottedGeometry.ToPaintGeometry(),
            AxisPoints, ESlateDrawEffect::None, CkStyle::Border(), true, 1.0f);

        const auto Step = Compute_TickStep(ViewMax - ViewMin);
        // Label precision follows the step — a sub-second step with "%.0fs"
        // renders the same rounded second 4-5 times in a row.
        const auto Decimals = Step >= 1.0 ? 0 : Step >= 0.1 ? 1 : 2;
        for (auto Tick = FMath::CeilToDouble(ViewMin / Step) * Step; Tick <= ViewMax + KINDA_SMALL_NUMBER; Tick += Step)
        {
            const auto X = Compute_X(Tick, Width);
            auto TickPoints = TArray<FVector2D>{
                FVector2D(X, AxisY - 3.0f),
                FVector2D(X, AxisY + 3.0f)};
            FSlateDrawElement::MakeLines(
                OutDrawElements, Layer,
                InAllottedGeometry.ToPaintGeometry(),
                TickPoints, ESlateDrawEffect::None, CkStyle::Border(), true, 1.0f);

            // Snap the epsilon-negative tick so it can't label as "-0s".
            const auto LabelTick = FMath::IsNearlyZero(Tick, Step * 0.001) ? 0.0 : Tick;
            const auto TickLabel = _OnFormatTick.IsBound()
                ? _OnFormatTick.Execute(LabelTick)
                : FString::Printf(TEXT("%.*fs"), Decimals, LabelTick);
            FSlateDrawElement::MakeText(
                OutDrawElements, Layer,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f(40.0f, 10.0f),
                    FSlateLayoutTransform(FVector2f(X - 10.0f, AxisY + 5.0f))),
                TickLabel,
                TickFont,
                ESlateDrawEffect::None,
                CkStyle::TextMute());
        }
    }
    ++Layer;

    // ---- Spans (activation bars) ----------------------------------------------
    for (const auto& Span : _Spans)
    {
        if (Span.EndSeconds < ViewMin || Span.StartSeconds > ViewMax)
        { continue; }

        const auto X0 = Compute_X(Span.StartSeconds, Width);
        const auto X1 = Compute_X(Span.EndSeconds, Width);
        const auto Y  = Compute_LaneY(Span.LaneIndex, Height);

        FSlateDrawElement::MakeBox(
            OutDrawElements, Layer,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f(FMath::Max(2.0f, X1 - X0), 8.0f),
                FSlateLayoutTransform(FVector2f(X0, Y - 4.0f))),
            FilledBrush,
            ESlateDrawEffect::None,
            Span.Color);
    }
    ++Layer;

    // ---- Events ----------------------------------------------------------------
    const auto SelectedId = _SelectedId.Get(INDEX_NONE);

    for (auto Index = 0; Index < _Events.Num(); ++Index)
    {
        const auto& Event = _Events[Index];

        // A zoomed window skips outside markers outright — clamped, they would pile up on the edges.
        if (Event.TimeSeconds < ViewMin || Event.TimeSeconds > ViewMax)
        { continue; }

        const auto Center = FVector2D(
            Compute_X(Event.TimeSeconds, Width),
            Compute_LaneY(Event.LaneIndex, Height));

        const auto IsSelected = Event.SelectionId != INDEX_NONE && Event.SelectionId == SelectedId;
        const auto IsHovered  = Index == _HoveredEventIndex;

        switch (Event.Shape)
        {
            case ECkDebug_TimelineMarker::Diamond:
            {
                const auto Half = MarkerHalf + (IsSelected ? 2.0f : 0.0f);
                const auto Points = MakeDiamond(Center, Half);
                FSlateDrawElement::MakeLines(
                    OutDrawElements, Layer,
                    InAllottedGeometry.ToPaintGeometry(),
                    Points, ESlateDrawEffect::None,
                    IsSelected ? CkStyle::Accent() : Event.Color,
                    true,
                    IsSelected || IsHovered ? 2.5f : 1.5f);
                break;
            }
            default:
            {
                FSlateDrawElement::MakeBox(
                    OutDrawElements, Layer,
                    InAllottedGeometry.ToPaintGeometry(
                        FVector2f(MarkerHalf * 2.0f, MarkerHalf * 2.0f),
                        FSlateLayoutTransform(FVector2f(
                            static_cast<float>(Center.X) - MarkerHalf,
                            static_cast<float>(Center.Y) - MarkerHalf))),
                    FilledBrush,
                    ESlateDrawEffect::None,
                    IsHovered ? Event.Color : Event.Color.CopyWithNewOpacity(0.85f));
                break;
            }
        }

        if (NOT Event.SideLabel.IsEmpty())
        {
            FSlateDrawElement::MakeText(
                OutDrawElements, Layer,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f(28.0f, 10.0f),
                    FSlateLayoutTransform(FVector2f(
                        static_cast<float>(Center.X) + MarkerHalf + 2.0f,
                        static_cast<float>(Center.Y) - 5.0f))),
                Event.SideLabel,
                TickFont,
                ESlateDrawEffect::None,
                CkStyle::Warn());
        }
    }
    ++Layer;

    // ---- Scrub cursor — the visible half of scrubbing ---------------------------
    if (const auto CursorTime = _CursorTime.Get(TOptional<double>{});
        CursorTime.IsSet() && CursorTime.GetValue() >= ViewMin && CursorTime.GetValue() <= ViewMax)
    {
        const auto X = Compute_X(CursorTime.GetValue(), Width);

        auto CursorPoints = TArray<FVector2D>{
            FVector2D(X, PadTop),
            FVector2D(X, AxisY)};
        FSlateDrawElement::MakeLines(
            OutDrawElements, Layer,
            InAllottedGeometry.ToPaintGeometry(),
            CursorPoints, ESlateDrawEffect::None, CkStyle::Warn(), true, 2.0f);

        FSlateDrawElement::MakeBox(
            OutDrawElements, Layer,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f(7.0f, 5.0f),
                FSlateLayoutTransform(FVector2f(X - 3.5f, PadTop - 4.0f))),
            FilledBrush,
            ESlateDrawEffect::None,
            CkStyle::Warn());
    }
    ++Layer;

    return Layer;
}

// ====================================================================================================================
// INPUT
// ====================================================================================================================

auto
    SCkDebug_EventTimeline::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    const auto Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto Width = static_cast<float>(InGeometry.GetLocalSize().X);
    const auto Button = InMouseEvent.GetEffectingButton();

    const auto IsPanButton =
        Button == EKeys::RightMouseButton ||
        Button == EKeys::MiddleMouseButton ||
        (Button == EKeys::LeftMouseButton && InMouseEvent.IsControlDown());

    // PreventThrottling on every drag capture: a held mouse button on an editor widget otherwise engages
    // Slate's responsive-UI throttle, which pauses PIE itself — the timeline freezes mid-drag, and worse, an
    // INPUT debugger stops the very thing it is measuring.
    if (_AllowPanZoom && IsPanButton)
    {
        _IsPanning = true;
        _PanStartX = static_cast<float>(Local.X);
        _PanStartViewStart = Get_ViewMin();
        return FReply::Handled()
            .CaptureMouse(SharedThis(this))
            .SetUserFocus(SharedThis(this), EFocusCause::Mouse)
            .PreventThrottling();
    }

    if (Button != EKeys::LeftMouseButton)
    { return FReply::Unhandled(); }

    const auto Index = Find_EventAt(Local, InGeometry.GetLocalSize());
    const auto HitMarker = _Events.IsValidIndex(Index) &&
        _Events[Index].SelectionId != INDEX_NONE &&
        _OnEventSelected.IsBound();

    if (HitMarker)
    { _OnEventSelected.Execute(_Events[Index].SelectionId); }
    else if (_OnScrubbed.IsBound())
    { _OnScrubbed.Execute(Compute_Time(static_cast<float>(Local.X), Width)); }
    else
    { return FReply::Unhandled(); }

    if (_OnScrubbed.IsBound())
    {
        // Either entry becomes a drag: a marker click lands on the marker's exact value first, then the drag
        // scrubs from there. Without this, any press within the pick radius of a marker could never START a
        // drag — fatal on a marker-dense track.
        _IsScrubbing = true;

        // Scrubbing PARKS the window: a live-following view slides out from under the drag, re-mapping the
        // cursor to a different value every frame. The data keeps accumulating; only the window stands still.
        if (_ViewDuration > 0.0)
        { _FollowLive = false; }
        return FReply::Handled()
            .CaptureMouse(SharedThis(this))
            .SetUserFocus(SharedThis(this), EFocusCause::Mouse)
            .PreventThrottling();
    }

    return FReply::Handled();
}

auto
    SCkDebug_EventTimeline::
    OnMouseButtonUp(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (NOT _IsScrubbing && NOT _IsPanning)
    { return FReply::Unhandled(); }

    _IsScrubbing = false;
    _IsPanning = false;
    return FReply::Handled().ReleaseMouseCapture();
}

auto
    SCkDebug_EventTimeline::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    using namespace ck_debug_event_timeline;

    const auto Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto Width = static_cast<float>(InGeometry.GetLocalSize().X);

    if (_IsPanning)
    {
        const auto Usable = FMath::Max(1.0f, Width - _PadLeft - PadRight);
        const auto UnitsPerPx = (Get_ViewMax() - Get_ViewMin()) / static_cast<double>(Usable);
        const auto Delta = static_cast<double>(static_cast<float>(Local.X) - _PanStartX) * UnitsPerPx;

        if (_ViewDuration > 0.0)
        {
            _FollowLive = false;
            Do_ApplyView(_PanStartViewStart - Delta, _ViewDuration);
        }

        return FReply::Handled();
    }

    if (_IsScrubbing)
    {
        const auto Time = Compute_Time(static_cast<float>(Local.X), Width);

        // Auto-pan: dragging the cursor to a window edge slides the window along, so a scrub can walk into
        // history past the visible span without a separate pan (the ScrubTimeline's edge-margin behavior).
        if (_ViewDuration > 0.0)
        {
            const auto Margin = (Get_ViewMax() - Get_ViewMin()) * ScrubEdgeMarginFraction;

            if (Time < Get_ViewMin() + Margin)
            { Do_ApplyView(Time - Margin, _ViewDuration); }
            else if (Time > Get_ViewMax() - Margin)
            { Do_ApplyView(Time - _ViewDuration + Margin, _ViewDuration); }
        }

        _OnScrubbed.ExecuteIfBound(Time);
        return FReply::Handled();
    }

    _HoveredEventIndex = Find_EventAt(Local, InGeometry.GetLocalSize());
    return FReply::Unhandled();
}

auto
    SCkDebug_EventTimeline::
    OnMouseWheel(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    using namespace ck_debug_event_timeline;

    if (NOT _AllowPanZoom)
    { return FReply::Unhandled(); }

    const auto Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto Width = static_cast<float>(InGeometry.GetLocalSize().X);

    const auto FullRange = _TimeMax - _TimeMin;
    const auto MinDuration = FMath::Max(5.0, FullRange * 0.005);

    const auto Duration = Get_ViewMax() - Get_ViewMin();
    const auto NewDuration = FMath::Clamp(
        InMouseEvent.GetWheelDelta() > 0.0f ? Duration / ZoomStepFactor : Duration * ZoomStepFactor,
        MinDuration,
        FullRange);

    // Zoom about the cursor: the axis value under the pointer stays under the pointer. Zooming is an act of
    // inspection — it detaches the live follow (the ScrubTimeline contract); F or Go-live re-attach.
    const auto Anchor = Compute_Time(static_cast<float>(Local.X), Width);
    _FollowLive = false;
    Do_ApplyView(Anchor - (Anchor - Get_ViewMin()) * NewDuration / Duration, NewDuration);

    return FReply::Handled();
}

auto
    SCkDebug_EventTimeline::
    OnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent)
    -> FReply
{
    if (_AllowPanZoom && InKeyEvent.GetKey() == EKeys::F)
    {
        // F = "show everything up to now and follow" — a real window spanning the current range, never the
        // duration-0 state (see Do_ApplyView's trap note).
        Do_ApplyView(_TimeMin, FMath::Max(_TimeMax - _TimeMin, 1.0));
        _FollowLive = true;
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

auto
    SCkDebug_EventTimeline::
    OnMouseLeave(
        const FPointerEvent& InMouseEvent)
    -> void
{
    _HoveredEventIndex = INDEX_NONE;
    SLeafWidget::OnMouseLeave(InMouseEvent);
}

// ====================================================================================================================
