#include "CkDebuggerCommon/Widgets/SCkDebug_ScrubTimeline.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

// ====================================================================================================================

namespace ck_debug_scrub_timeline
{
    constexpr auto SegmentGap = 1.0f;
    constexpr auto MinLabelWidth = 30.0f;
    constexpr auto MinSubdivisionPx = 2.0f;

    constexpr auto CursorThickness = 2.0f;
    constexpr auto CursorHeadHalf = 5.0f;

    constexpr auto MarkThickness = 2.0f;
    constexpr auto CutOffset = 1.5f;
    constexpr auto FlagWidth = 8.0f;
    constexpr auto FlagHeight = 6.0f;
    constexpr auto DotRadius = 4.0f;
    constexpr auto DotSelectedRadius = 6.0f;
    constexpr auto DotSelectionRing = 1.5f;
    constexpr auto PickRadius = 10.0f;

    constexpr auto RulerGap = 5.0f;
    constexpr auto MajorTickHeight = 7.0f;
    constexpr auto MinorTickHeight = 4.0f;
    constexpr auto TickWidth = 1.0f;
    constexpr auto MinorPerMajor = 5;

    // Edge margin (as a fraction of the window) at which a drag starts auto-panning.
    constexpr auto EdgeMarginFrac = 0.05;

    constexpr auto PanDeadZonePx = 3.0f;

    constexpr auto ZoomStep = 0.15;

    // Hash-derived segment colors land anywhere on the wheel — white is unreadable on a bright
    // yellow. Pick the label color from the fill's perceived brightness instead of assuming dark.
    auto Pick_ReadableTextColor(const FLinearColor& InFill) -> FLinearColor
    {
        const auto Luminance = 0.299f * InFill.R + 0.587f * InFill.G + 0.114f * InFill.B;
        return Luminance > 0.55f ? CkStyle::BgRoot() : CkStyle::TextStrong();
    }
}

// ====================================================================================================================
// CONSTRUCT / CONTENT
// ====================================================================================================================

auto
    SCkDebug_ScrubTimeline::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _DesiredHeight          = InArgs._DesiredHeight;
    _Mode                   = InArgs._Mode;
    _ScrubTime              = InArgs._ScrubTime;
    _LiveTime               = InArgs._LiveTime;
    _SelectedMarkId         = InArgs._SelectedMarkId;
    _CopyText               = InArgs._CopyText;
    _MinViewDuration        = FMath::Max(KINDA_SMALL_NUMBER, InArgs._MinViewDuration);
    _MaxViewDuration        = FMath::Max(_MinViewDuration, InArgs._MaxViewDuration);
    _SegmentHeightFraction  = FMath::Clamp(InArgs._SegmentHeightFraction, 0.1f, 1.0f);
    _ShowRuler              = InArgs._ShowRuler;
    _RulerLabelCount        = FMath::Max(1, InArgs._RulerLabelCount);
    _AllowZoom              = InArgs._AllowZoom;
    _OnFormatTime           = InArgs._OnFormatTime;
    _OnScrubbed             = InArgs._OnScrubbed;
    _OnMarkSelected         = InArgs._OnMarkSelected;
    _OnViewChanged          = InArgs._OnViewChanged;

    _ViewDuration = FMath::Clamp(InArgs._InitialViewDuration, _MinViewDuration, _MaxViewDuration);
    _ViewStart = 0.0;

    // SLeafWidget has no children to hang a per-element tooltip on — the hovered mark (or, failing
    // that, the hovered segment) speaks through the widget-level tooltip channel.
    SetToolTipText(TAttribute<FText>::CreateLambda([this]() -> FText
    {
        if (_Marks.IsValidIndex(_HoveredMarkIndex))
        { return FText::FromString(_Marks[_HoveredMarkIndex].Tooltip); }

        if (_Segments.IsValidIndex(_HoveredSegmentIndex))
        { return FText::FromString(_Segments[_HoveredSegmentIndex].Tooltip); }

        return FText::GetEmpty();
    }));
}

auto
    SCkDebug_ScrubTimeline::
    Set_Content(
        TArray<FCkDebug_ScrubSegment> InSegments,
        TArray<FCkDebug_ScrubMark> InMarks)
    -> void
{
    _Segments = MoveTemp(InSegments);
    _Marks = MoveTemp(InMarks);
    _HoveredMarkIndex = INDEX_NONE;
    _HoveredSegmentIndex = INDEX_NONE;

    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkDebug_ScrubTimeline::
    Set_View(
        double InViewStart,
        double InViewDuration)
    -> void
{
    // A window whose right edge already sits at "now" is a live window — re-attach the follow so
    // the caller does not have to know about the flag.
    const auto Live = _LiveTime.Get(0.0);
    _FollowLive = FMath::IsNearlyEqual(InViewStart + InViewDuration, Live, KINDA_SMALL_NUMBER);

    Do_ApplyView(InViewStart, InViewDuration);
}

auto
    SCkDebug_ScrubTimeline::
    Focus_OnCursor()
    -> void
{
    _FollowLive = true;
    Do_ApplyView(Compute_CursorTime() - _ViewDuration * 0.5, _ViewDuration);
}

auto
    SCkDebug_ScrubTimeline::
    ComputeDesiredSize(float) const
    -> FVector2D
{
    return FVector2D{240.0f, _DesiredHeight};
}

// ====================================================================================================================
// VIEW / COORDINATE HELPERS
// ====================================================================================================================

auto
    SCkDebug_ScrubTimeline::
    Do_ApplyView(
        double InViewStart,
        double InViewDuration)
    -> void
{
    _ViewDuration = FMath::Clamp(InViewDuration, _MinViewDuration, _MaxViewDuration);
    _ViewStart = InViewStart;

    _OnViewChanged.ExecuteIfBound(_ViewStart, _ViewDuration);
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkDebug_ScrubTimeline::
    Do_UpdateLiveFollow() const
    -> void
{
    if (NOT _FollowLive || _Mode.Get(ECkDebug_ScrubMode::Live) != ECkDebug_ScrubMode::Live)
    { return; }

    _ViewStart = _LiveTime.Get(0.0) - _ViewDuration;
}

auto
    SCkDebug_ScrubTimeline::
    Compute_X(
        double InTime,
        float InWidth) const
    -> float
{
    if (_ViewDuration <= 0.0)
    { return 0.0f; }

    return static_cast<float>((InTime - _ViewStart) / _ViewDuration) * InWidth;
}

auto
    SCkDebug_ScrubTimeline::
    Compute_Time(
        float InLocalX,
        float InWidth) const
    -> double
{
    if (InWidth <= 0.0f)
    { return _ViewStart; }

    return _ViewStart + (static_cast<double>(InLocalX) / static_cast<double>(InWidth)) * _ViewDuration;
}

auto
    SCkDebug_ScrubTimeline::
    Compute_CursorTime() const
    -> double
{
    return _Mode.Get(ECkDebug_ScrubMode::Live) == ECkDebug_ScrubMode::Live
        ? _LiveTime.Get(0.0)
        : _ScrubTime.Get(0.0);
}

auto
    SCkDebug_ScrubTimeline::
    Find_MarkAt(
        const FVector2D& InLocal,
        const FVector2D& InSize) const
    -> int32
{
    using namespace ck_debug_scrub_timeline;

    auto BestIndex = int32{INDEX_NONE};
    auto BestDistance = PickRadius;

    for (auto Index = 0; Index < _Marks.Num(); ++Index)
    {
        const auto X = Compute_X(_Marks[Index].TimeSeconds, static_cast<float>(InSize.X));
        const auto Distance = FMath::Abs(static_cast<float>(InLocal.X) - X);

        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            BestIndex = Index;
        }
    }

    return BestIndex;
}

auto
    SCkDebug_ScrubTimeline::
    Find_SegmentAt(
        double InTime) const
    -> int32
{
    for (auto Index = 0; Index < _Segments.Num(); ++Index)
    {
        if (InTime >= _Segments[Index].StartSeconds && InTime <= _Segments[Index].EndSeconds)
        { return Index; }
    }

    return INDEX_NONE;
}

// ====================================================================================================================
// PAINT
// ====================================================================================================================

auto
    SCkDebug_ScrubTimeline::
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
    const auto Size = InAllottedGeometry.GetLocalSize();
    const auto Width = static_cast<float>(Size.X);
    const auto Height = static_cast<float>(Size.Y);
    const auto SegmentHeight = Height * _SegmentHeightFraction;

    FSlateDrawElement::MakeBox(
        OutDrawElements, InLayerId,
        InAllottedGeometry.ToPaintGeometry(FVector2f{Width, Height}, FSlateLayoutTransform{}),
        CkStyle::GetFilledBrush(), ESlateDrawEffect::None, CkStyle::BgRoot());

    Do_UpdateLiveFollow();

    Paint_Segments(InAllottedGeometry, OutDrawElements, InLayerId + 1, SegmentHeight);
    Paint_Marks(InAllottedGeometry, OutDrawElements, InLayerId + 3, SegmentHeight);

    if (_ShowRuler)
    { Paint_Ruler(InAllottedGeometry, OutDrawElements, InLayerId + 4, SegmentHeight); }

    Paint_Cursor(InAllottedGeometry, OutDrawElements, InLayerId + 6);

    return InLayerId + 7;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_ScrubTimeline::
    Paint_Segments(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        float InSegmentHeight) const
    -> void
{
    using namespace ck_debug_scrub_timeline;

    const auto Width = static_cast<float>(InGeometry.GetLocalSize().X);
    const auto* FilledBrush = CkStyle::GetFilledBrush();
    const auto LabelFont = CkStyle::BoldFont(CkStyle::FontSizeMicro());

    for (const auto& Segment : _Segments)
    {
        const auto FullX0 = Compute_X(Segment.StartSeconds, Width);
        const auto FullX1 = Compute_X(Segment.EndSeconds, Width);

        if (FullX1 < 0.0f || FullX0 > Width)
        { continue; }

        const auto X0 = FMath::Max(FullX0, 0.0f);
        const auto X1 = FMath::Min(FullX1, Width);
        const auto VisibleWidth = FMath::Max(X1 - X0, 1.0f);
        const auto BodyWidth = FMath::Max(VisibleWidth - SegmentGap, 1.0f);

        FSlateDrawElement::MakeBox(
            OutDrawElements, InLayerId,
            InGeometry.ToPaintGeometry(
                FVector2f{BodyWidth, InSegmentHeight},
                FSlateLayoutTransform{FVector2f{X0, 0.0f}}),
            FilledBrush, ESlateDrawEffect::None, Segment.Color);

        // ---- Subdivision boundaries ------------------------------------------------------------------------------
        const auto CellCount = Segment.SubdivisionCount;
        const auto FullWidth = FMath::Max(FullX1 - FullX0, 1.0f);
        const auto PxPerCell = FullWidth / static_cast<float>(FMath::Max(1, CellCount));

        if (CellCount > 1 && PxPerCell >= MinSubdivisionPx)
        {
            auto LineColor = Segment.Color * 0.55f;
            LineColor.A = 1.0f;

            for (auto Cell = 1; Cell < CellCount; ++Cell)
            {
                const auto Alpha = static_cast<double>(Cell) / static_cast<double>(CellCount);
                const auto BoundaryTime =
                    Segment.StartSeconds + (Segment.EndSeconds - Segment.StartSeconds) * Alpha;

                // Snap to whole pixels: sub-pixel boundaries alpha-blend across two columns and
                // the eye reads that as the cells drifting as the window scrolls.
                const auto LineX = FMath::FloorToFloat(Compute_X(BoundaryTime, Width));

                if (LineX < 0.0f || LineX > Width)
                { continue; }

                FSlateDrawElement::MakeBox(
                    OutDrawElements, InLayerId,
                    InGeometry.ToPaintGeometry(
                        FVector2f{1.0f, InSegmentHeight},
                        FSlateLayoutTransform{FVector2f{LineX, 0.0f}}),
                    FilledBrush, ESlateDrawEffect::None, LineColor);
            }
        }

        // ---- Centred label ----------------------------------------------------------------------------------------
        if (Segment.Label.IsEmpty() || VisibleWidth <= MinLabelWidth)
        { continue; }

        const auto FontService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        const auto TextSize = FontService->Measure(Segment.Label, LabelFont);

        if (static_cast<float>(TextSize.X) >= BodyWidth - 4.0f)
        { continue; }

        const auto TextPosition = FVector2f{
            X0 + (BodyWidth - static_cast<float>(TextSize.X)) * 0.5f,
            (InSegmentHeight - static_cast<float>(TextSize.Y)) * 0.5f};

        FSlateDrawElement::MakeText(
            OutDrawElements, InLayerId + 1,
            InGeometry.ToPaintGeometry(
                FVector2f{static_cast<float>(TextSize.X), static_cast<float>(TextSize.Y)},
                FSlateLayoutTransform{TextPosition}),
            Segment.Label, LabelFont, ESlateDrawEffect::None,
            Pick_ReadableTextColor(Segment.Color));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_ScrubTimeline::
    Paint_Marks(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        float InSegmentHeight) const
    -> void
{
    using namespace ck_debug_scrub_timeline;

    const auto Width = static_cast<float>(InGeometry.GetLocalSize().X);
    const auto* FilledBrush = CkStyle::GetFilledBrush();
    const auto SelectedId = _SelectedMarkId.Get(INDEX_NONE);
    const auto Midline = InSegmentHeight * 0.5f;

    for (auto Index = 0; Index < _Marks.Num(); ++Index)
    {
        const auto& Mark = _Marks[Index];
        const auto X = Compute_X(Mark.TimeSeconds, Width);

        if (X < -FlagWidth || X > Width + FlagWidth)
        { continue; }

        const auto IsSelected = Mark.SelectionId != INDEX_NONE && Mark.SelectionId == SelectedId;
        const auto IsHovered = Index == _HoveredMarkIndex;

        switch (Mark.Kind)
        {
            case ECkDebug_ScrubMarkKind::Tick:
            {
                FSlateDrawElement::MakeBox(
                    OutDrawElements, InLayerId,
                    InGeometry.ToPaintGeometry(
                        FVector2f{MarkThickness, InSegmentHeight},
                        FSlateLayoutTransform{FVector2f{X - MarkThickness * 0.5f, 0.0f}}),
                    FilledBrush, ESlateDrawEffect::None, Mark.Color);
                break;
            }

            case ECkDebug_ScrubMarkKind::Cut:
            {
                // Two half-bars offset in opposite directions — reads as a snip across the track.
                FSlateDrawElement::MakeBox(
                    OutDrawElements, InLayerId,
                    InGeometry.ToPaintGeometry(
                        FVector2f{MarkThickness, Midline},
                        FSlateLayoutTransform{FVector2f{X - CutOffset - MarkThickness * 0.5f, 0.0f}}),
                    FilledBrush, ESlateDrawEffect::None, Mark.Color);

                FSlateDrawElement::MakeBox(
                    OutDrawElements, InLayerId,
                    InGeometry.ToPaintGeometry(
                        FVector2f{MarkThickness, InSegmentHeight - Midline},
                        FSlateLayoutTransform{FVector2f{X + CutOffset - MarkThickness * 0.5f, Midline}}),
                    FilledBrush, ESlateDrawEffect::None, Mark.Color);
                break;
            }

            case ECkDebug_ScrubMarkKind::Flag:
            {
                FSlateDrawElement::MakeBox(
                    OutDrawElements, InLayerId,
                    InGeometry.ToPaintGeometry(
                        FVector2f{FlagWidth, FlagHeight},
                        FSlateLayoutTransform{FVector2f{X - FlagWidth * 0.5f, 0.0f}}),
                    FilledBrush, ESlateDrawEffect::None, Mark.Color);

                FSlateDrawElement::MakeBox(
                    OutDrawElements, InLayerId,
                    InGeometry.ToPaintGeometry(
                        FVector2f{1.0f, InSegmentHeight},
                        FSlateLayoutTransform{FVector2f{X - 0.5f, 0.0f}}),
                    FilledBrush, ESlateDrawEffect::None, Mark.Color.CopyWithNewOpacity(0.45f));
                break;
            }

            case ECkDebug_ScrubMarkKind::Dot:
            {
                const auto Radius = IsSelected ? DotSelectedRadius : DotRadius;

                if (IsSelected || IsHovered)
                {
                    const auto Ring = Radius + DotSelectionRing;
                    FSlateDrawElement::MakeBox(
                        OutDrawElements, InLayerId,
                        InGeometry.ToPaintGeometry(
                            FVector2f{Ring * 2.0f, Ring * 2.0f},
                            FSlateLayoutTransform{FVector2f{X - Ring, Midline - Ring}}),
                        CkStyle::GetRoundedBrush_Pill(), ESlateDrawEffect::None,
                        IsSelected ? CkStyle::TextStrong() : CkStyle::TextDim());
                }

                FSlateDrawElement::MakeBox(
                    OutDrawElements, InLayerId + 1,
                    InGeometry.ToPaintGeometry(
                        FVector2f{Radius * 2.0f, Radius * 2.0f},
                        FSlateLayoutTransform{FVector2f{X - Radius, Midline - Radius}}),
                    CkStyle::GetRoundedBrush_Pill(), ESlateDrawEffect::None, Mark.Color);
                break;
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_ScrubTimeline::
    Paint_Cursor(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId) const
    -> void
{
    using namespace ck_debug_scrub_timeline;

    const auto Size = InGeometry.GetLocalSize();
    const auto Width = static_cast<float>(Size.X);
    const auto Height = static_cast<float>(Size.Y);

    const auto IsLive = _Mode.Get(ECkDebug_ScrubMode::Live) == ECkDebug_ScrubMode::Live;
    const auto CursorColor = IsLive ? CkStyle::Ok() : CkStyle::TextStrong();

    auto X = Compute_X(Compute_CursorTime(), Width);

    if (X < -CursorHeadHalf || X > Width + CursorHeadHalf)
    { return; }

    X = FMath::Clamp(X, 0.0f, Width);

    FSlateDrawElement::MakeBox(
        OutDrawElements, InLayerId,
        InGeometry.ToPaintGeometry(
            FVector2f{CursorThickness, Height},
            FSlateLayoutTransform{FVector2f{X - CursorThickness * 0.5f, 0.0f}}),
        CkStyle::GetFilledBrush(), ESlateDrawEffect::None, CursorColor);

    FSlateDrawElement::MakeBox(
        OutDrawElements, InLayerId,
        InGeometry.ToPaintGeometry(
            FVector2f{CursorHeadHalf * 2.0f, CursorHeadHalf},
            FSlateLayoutTransform{FVector2f{X - CursorHeadHalf, 0.0f}}),
        CkStyle::GetFilledBrush(), ESlateDrawEffect::None, CursorColor);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_ScrubTimeline::
    Paint_Ruler(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        float InSegmentHeight) const
    -> void
{
    using namespace ck_debug_scrub_timeline;

    const auto Size = InGeometry.GetLocalSize();
    const auto Width = static_cast<float>(Size.X);
    const auto Height = static_cast<float>(Size.Y);
    const auto* FilledBrush = CkStyle::GetFilledBrush();
    const auto Font = CkStyle::MonoFont(CkStyle::FontSizeMicro());
    const auto TickBaseY = InSegmentHeight + RulerGap;
    const auto FontService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

    // Minor ticks first so the majors draw over them at shared positions.
    for (auto Index = 0; Index < _RulerLabelCount * MinorPerMajor; ++Index)
    {
        if (Index % MinorPerMajor == 0)
        { continue; }

        const auto X = static_cast<float>(Index) / static_cast<float>(_RulerLabelCount * MinorPerMajor) * Width;

        FSlateDrawElement::MakeBox(
            OutDrawElements, InLayerId,
            InGeometry.ToPaintGeometry(
                FVector2f{TickWidth, MinorTickHeight},
                FSlateLayoutTransform{FVector2f{X - TickWidth * 0.5f, TickBaseY}}),
            FilledBrush, ESlateDrawEffect::None, CkStyle::Border());
    }

    for (auto Index = 0; Index <= _RulerLabelCount; ++Index)
    {
        const auto Alpha = static_cast<float>(Index) / static_cast<float>(_RulerLabelCount);
        const auto X = Alpha * Width;
        const auto Time = _ViewStart + _ViewDuration * static_cast<double>(Alpha);

        FSlateDrawElement::MakeBox(
            OutDrawElements, InLayerId,
            InGeometry.ToPaintGeometry(
                FVector2f{TickWidth, MajorTickHeight},
                FSlateLayoutTransform{FVector2f{X - TickWidth * 0.5f, TickBaseY}}),
            FilledBrush, ESlateDrawEffect::None, CkStyle::BorderStrong());

        const auto Label = _OnFormatTime.IsBound()
            ? _OnFormatTime.Execute(Time)
            : FString::Printf(TEXT("%.1fs"), Time);

        const auto TextSize = FontService->Measure(Label, Font);
        const auto TextX = FMath::Clamp(
            X - static_cast<float>(TextSize.X) * 0.5f, 0.0f, FMath::Max(0.0f, Width - static_cast<float>(TextSize.X)));

        FSlateDrawElement::MakeText(
            OutDrawElements, InLayerId + 1,
            InGeometry.ToPaintGeometry(
                FVector2f{static_cast<float>(TextSize.X), static_cast<float>(TextSize.Y)},
                FSlateLayoutTransform{FVector2f{TextX, Height - static_cast<float>(TextSize.Y) - 2.0f}}),
            Label, Font, ESlateDrawEffect::None, CkStyle::TextMute());
    }
}

// ====================================================================================================================
// INPUT
// ====================================================================================================================

auto
    SCkDebug_ScrubTimeline::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    using namespace ck_debug_scrub_timeline;

    const auto Size = InGeometry.GetLocalSize();
    const auto Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto Self = SharedThis(this);

    // Pan first: Ctrl+LMB is the one-handed alternative to RMB, so it must win over scrubbing.
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton ||
        InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton ||
        (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && InMouseEvent.IsControlDown()))
    {
        _IsPanning = true;
        _PanWasDragged = false;
        _PanStartX = static_cast<float>(Local.X);

        return FReply::Handled().CaptureMouse(Self).SetUserFocus(Self, EFocusCause::Mouse);
    }

    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return FReply::Unhandled(); }

    // A click on a selectable mark selects it rather than scrubbing.
    const auto MarkIndex = Find_MarkAt(Local, Size);
    if (_Marks.IsValidIndex(MarkIndex) && _Marks[MarkIndex].SelectionId != INDEX_NONE)
    {
        _OnMarkSelected.ExecuteIfBound(_Marks[MarkIndex].SelectionId);
        return FReply::Handled().SetUserFocus(Self, EFocusCause::Mouse);
    }

    // Freeze the window the moment scrubbing starts. Without it, a Live-follow (or a
    // recenter-on-scrub-time) window slides opposite the drag and the track reads as inverted.
    _FollowLive = false;
    _IsScrubbing = true;

    // LiveTime is the ceiling only when the owner actually supplies one — an unbound (0) ceiling
    // would otherwise pin every scrub to the origin.
    const auto Ceiling = _LiveTime.Get(0.0);
    auto Time = Compute_Time(static_cast<float>(Local.X), static_cast<float>(Size.X));
    if (Ceiling > 0.0)
    { Time = FMath::Min(Time, Ceiling); }

    _OnScrubbed.ExecuteIfBound(Time);

    return FReply::Handled().CaptureMouse(Self).SetUserFocus(Self, EFocusCause::Mouse);
}

auto
    SCkDebug_ScrubTimeline::
    OnMouseButtonUp(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (_IsScrubbing && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        _IsScrubbing = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    if (NOT _IsPanning)
    { return FReply::Unhandled(); }

    const auto WasRightButton = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
    _IsPanning = false;

    const auto CopyText = _CopyText.Get(FString{});
    if (WasRightButton && NOT _PanWasDragged && NOT CopyText.IsEmpty())
    {
        return ck::DebugCopyMenu::Push_CopyTextMenu(
            SharedThis(this), InMouseEvent.GetScreenSpacePosition(), CopyText)
            .ReleaseMouseCapture();
    }

    return FReply::Handled().ReleaseMouseCapture();
}

auto
    SCkDebug_ScrubTimeline::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    using namespace ck_debug_scrub_timeline;

    const auto Size = InGeometry.GetLocalSize();
    const auto Width = static_cast<float>(Size.X);
    const auto Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

    if (_IsScrubbing)
    {
        auto Time = Compute_Time(static_cast<float>(Local.X), Width);

        const auto Ceiling = _LiveTime.Get(0.0);
        if (Ceiling > 0.0)
        { Time = FMath::Min(Time, Ceiling); }

        // Auto-pan: dragging the cursor to (or past) an edge slides the window so it stays visible.
        const auto Margin = _ViewDuration * EdgeMarginFrac;
        if (Time < _ViewStart + Margin)
        { Do_ApplyView(Time - Margin, _ViewDuration); }
        else if (Time > _ViewStart + _ViewDuration - Margin)
        { Do_ApplyView(Time - _ViewDuration + Margin, _ViewDuration); }

        _OnScrubbed.ExecuteIfBound(Time);
        return FReply::Handled();
    }

    if (_IsPanning)
    {
        const auto DeltaX = static_cast<float>(Local.X) - _PanStartX;
        _PanStartX = static_cast<float>(Local.X);

        if (FMath::Abs(DeltaX) > PanDeadZonePx)
        {
            _PanWasDragged = true;
            _FollowLive = false;
        }

        const auto TimeDelta = -(static_cast<double>(DeltaX) / static_cast<double>(FMath::Max(Width, 1.0f))) * _ViewDuration;
        Do_ApplyView(_ViewStart + TimeDelta, _ViewDuration);

        return FReply::Handled();
    }

    _HoveredMarkIndex = Find_MarkAt(Local, Size);
    _HoveredSegmentIndex = Find_SegmentAt(Compute_Time(static_cast<float>(Local.X), Width));
    Invalidate(EInvalidateWidgetReason::Paint);

    // Unhandled on hover-only: returning Handled can stop Slate routing OnMouseWheel here.
    return FReply::Unhandled();
}

auto
    SCkDebug_ScrubTimeline::
    OnMouseWheel(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    using namespace ck_debug_scrub_timeline;

    if (NOT _AllowZoom)
    { return FReply::Unhandled(); }

    const auto Width = static_cast<float>(InGeometry.GetLocalSize().X);
    const auto Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto CursorFraction = FMath::Clamp(static_cast<double>(Local.X) / static_cast<double>(FMath::Max(Width, 1.0f)), 0.0, 1.0);
    const auto TimeUnderCursor = _ViewStart + _ViewDuration * CursorFraction;

    const auto Multiplier = InMouseEvent.GetWheelDelta() > 0.0f ? (1.0 - ZoomStep) : (1.0 + ZoomStep);
    const auto NewDuration = FMath::Clamp(_ViewDuration * Multiplier, _MinViewDuration, _MaxViewDuration);

    // Keep the time under the cursor pinned to the same pixel.
    _FollowLive = false;
    Do_ApplyView(TimeUnderCursor - NewDuration * CursorFraction, NewDuration);

    return FReply::Handled();
}

auto
    SCkDebug_ScrubTimeline::
    OnMouseLeave(
        const FPointerEvent& InMouseEvent)
    -> void
{
    _HoveredMarkIndex = INDEX_NONE;
    _HoveredSegmentIndex = INDEX_NONE;
    Invalidate(EInvalidateWidgetReason::Paint);
    SLeafWidget::OnMouseLeave(InMouseEvent);
}

auto
    SCkDebug_ScrubTimeline::
    OnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent)
    -> FReply
{
    if (InKeyEvent.GetKey() == EKeys::F)
    {
        Focus_OnCursor();
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

// ====================================================================================================================
