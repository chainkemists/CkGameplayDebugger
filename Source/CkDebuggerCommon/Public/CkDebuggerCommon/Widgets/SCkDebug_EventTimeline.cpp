#include "CkDebuggerCommon/Widgets/SCkDebug_EventTimeline.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Rendering/DrawElements.h"

// ====================================================================================================================

namespace ck_debug_event_timeline
{
    constexpr auto PadLeft   = 56.0f;   // room for lane labels
    constexpr auto PadRight  = 12.0f;
    constexpr auto PadTop    = 8.0f;
    constexpr auto AxisBand  = 18.0f;   // tick labels under the axis
    constexpr auto MarkerHalf = 4.0f;
    constexpr auto PickRadius = 12.0f;

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
    _OnEventSelected = InArgs._OnEventSelected;

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

    const auto Usable = FMath::Max(1.0f, InWidth - PadLeft - PadRight);
    const auto Alpha  = FMath::Clamp((InTime - _TimeMin) / (_TimeMax - _TimeMin), 0.0, 1.0);
    return PadLeft + static_cast<float>(Alpha) * Usable;
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

    for (auto Index = 0; Index < _Events.Num(); ++Index)
    {
        const auto& Event = _Events[Index];
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

    // ---- Lane labels -----------------------------------------------------------
    for (auto Lane = 0; Lane < _LaneLabels.Num(); ++Lane)
    {
        FSlateDrawElement::MakeText(
            OutDrawElements,
            Layer,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f(PadLeft - 8.0f, 12.0f),
                FSlateLayoutTransform(FVector2f(4.0f, Compute_LaneY(Lane, Height) - 6.0f))),
            _LaneLabels[Lane],
            LaneFont,
            ESlateDrawEffect::None,
            CkStyle::TextMute());
    }

    // ---- Axis + ticks ----------------------------------------------------------
    {
        auto AxisPoints = TArray<FVector2D>{
            FVector2D(PadLeft, AxisY),
            FVector2D(Width - PadRight, AxisY)};
        FSlateDrawElement::MakeLines(
            OutDrawElements, Layer,
            InAllottedGeometry.ToPaintGeometry(),
            AxisPoints, ESlateDrawEffect::None, CkStyle::Border(), true, 1.0f);

        const auto Step = Compute_TickStep(_TimeMax - _TimeMin);
        // Label precision follows the step — a sub-second step with "%.0fs"
        // renders the same rounded second 4-5 times in a row.
        const auto Decimals = Step >= 1.0 ? 0 : Step >= 0.1 ? 1 : 2;
        for (auto Tick = FMath::CeilToDouble(_TimeMin / Step) * Step; Tick <= _TimeMax + KINDA_SMALL_NUMBER; Tick += Step)
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
            FSlateDrawElement::MakeText(
                OutDrawElements, Layer,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f(40.0f, 10.0f),
                    FSlateLayoutTransform(FVector2f(X - 10.0f, AxisY + 5.0f))),
                FString::Printf(TEXT("%.*fs"), Decimals, LabelTick),
                TickFont,
                ESlateDrawEffect::None,
                CkStyle::TextMute());
        }
    }
    ++Layer;

    // ---- Spans (activation bars) ----------------------------------------------
    for (const auto& Span : _Spans)
    {
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
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return FReply::Unhandled(); }

    const auto Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto Index = Find_EventAt(Local, InGeometry.GetLocalSize());

    if (_Events.IsValidIndex(Index) &&
        _Events[Index].SelectionId != INDEX_NONE &&
        _OnEventSelected.IsBound())
    {
        _OnEventSelected.Execute(_Events[Index].SelectionId);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

auto
    SCkDebug_EventTimeline::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    const auto Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    _HoveredEventIndex = Find_EventAt(Local, InGeometry.GetLocalSize());
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
