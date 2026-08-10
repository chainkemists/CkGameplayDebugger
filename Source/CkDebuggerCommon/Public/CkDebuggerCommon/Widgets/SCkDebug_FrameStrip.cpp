#include "CkDebuggerCommon/Widgets/SCkDebug_FrameStrip.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Rendering/DrawElements.h"

// ====================================================================================================================

namespace ck_debug_frame_strip
{
    constexpr auto MinZoom = 0.4f;
    constexpr auto MaxZoom = 6.0f;
    constexpr auto ZoomStepIn = 1.25f;
    constexpr auto ZoomStepOut = 0.8f;

    // A sample at 0 still needs a visible sliver, otherwise a quiet stretch reads as "no data".
    constexpr auto MinColumnFraction = 0.02f;

    constexpr auto SelectionBandPad = 2.0f;
    constexpr auto MarkerThickness = 2.0f;
    constexpr auto CursorWidth = 2.0f;
    constexpr auto LabelBoxPadX = 4.0f;
    constexpr auto LabelBoxPadY = 2.0f;

    // A right-click that moved less than this never counted as a pan, so it can open the copy menu.
    constexpr auto PanDeadZonePx = 3.0f;
}

// ====================================================================================================================
// CONSTRUCT / CONTENT
// ====================================================================================================================

auto
    SCkDebug_FrameStrip::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _DesiredHeight        = InArgs._DesiredHeight;
    _BarWidth             = FMath::Max(1.0f, InArgs._BarWidth);
    _BarGap               = FMath::Max(0.0f, InArgs._BarGap);
    _BarAreaPadding       = InArgs._BarAreaPadding;
    _BudgetMs             = InArgs._BudgetMs;
    _HeightScale          = InArgs._HeightScale;
    _SelectedIndexFromEnd = InArgs._SelectedIndexFromEnd;
    _ShowStatusLabel      = InArgs._ShowStatusLabel;
    _LiveLabel            = InArgs._LiveLabel;
    _FrozenLabel          = InArgs._FrozenLabel;
    _ValueSuffix          = InArgs._ValueSuffix;
    _MarkerMeaning        = InArgs._MarkerMeaning;
    _CopyText             = InArgs._CopyText;
    _AllowZoom            = InArgs._AllowZoom;
    _OnScrubbed           = InArgs._OnScrubbed;
    _OnReturnToLive       = InArgs._OnReturnToLive;

    // SLeafWidget has no children to hang a per-column tooltip on, so the hovered column's
    // description goes through the widget-level tooltip channel.
    SetToolTipText(TAttribute<FText>::CreateLambda([this]() -> FText
    {
        const auto ArrayIndex = Compute_ArrayIndex(_HoveredIndexFromEnd);

        if (NOT _Samples.IsValidIndex(ArrayIndex))
        { return FText::GetEmpty(); }

        const auto& Sample = _Samples[ArrayIndex];

        auto Line = _HoveredIndexFromEnd == 0
            ? ck::Format_UE(TEXT("Live — {}{}"), FString::Printf(TEXT("%.2f"), Sample.ValueMs), _ValueSuffix)
            : ck::Format_UE(TEXT("-{} — {}{}"),
                _HoveredIndexFromEnd, FString::Printf(TEXT("%.2f"), Sample.ValueMs), _ValueSuffix);

        if (Sample.HasMarker && NOT _MarkerMeaning.IsEmpty())
        { Line = ck::Format_UE(TEXT("{}  ({})"), Line, _MarkerMeaning); }

        return FText::FromString(Line);
    }));
}

auto
    SCkDebug_FrameStrip::
    Set_Samples(
        TArray<FCkDebug_FrameSample> InSamples)
    -> void
{
    _Samples = MoveTemp(InSamples);
    _HoveredIndexFromEnd = INDEX_NONE;
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkDebug_FrameStrip::
    Get_SampleCount() const
    -> int32
{
    return _Samples.Num();
}

auto
    SCkDebug_FrameStrip::
    Reset_View()
    -> void
{
    _ZoomScale = 1.0f;
    _ScrollOffsetX = TNumericLimits<float>::Max();   // paint clamps this to "scrolled fully right"
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkDebug_FrameStrip::
    ComputeDesiredSize(float) const
    -> FVector2D
{
    return FVector2D{320.0f, _DesiredHeight};
}

// ====================================================================================================================
// GEOMETRY / SCALE HELPERS
// ====================================================================================================================

auto
    SCkDebug_FrameStrip::
    Get_ScaledBarWidth() const
    -> float
{
    return FMath::Max(1.0f, _BarWidth * _ZoomScale);
}

auto
    SCkDebug_FrameStrip::
    Get_BarStride() const
    -> float
{
    return FMath::Max(1.0f, Get_ScaledBarWidth() + _BarGap * _ZoomScale);
}

auto
    SCkDebug_FrameStrip::
    Compute_MaxValue() const
    -> double
{
    auto Max = 0.0;
    for (const auto& Sample : _Samples)
    { Max = FMath::Max(Max, Sample.ValueMs); }

    // Never zero: every normalization below divides by this.
    return Max > 0.0 ? Max : 1.0;
}

auto
    SCkDebug_FrameStrip::
    Compute_HeatNormalized(
        double InValue,
        double InMaxValue,
        double InBudget) const
    -> float
{
    // Budget mode puts Warn exactly at the budget line (Get_HeatColor's 0.5 stop) and saturates
    // to Err at twice the budget — the banding the Insights chart drew as three discrete colors.
    const auto Divisor = InBudget > 0.0 ? InBudget * 2.0 : InMaxValue;
    return static_cast<float>(InValue / Divisor);
}

auto
    SCkDebug_FrameStrip::
    Compute_ArrayIndex(
        int32 InIndexFromEnd) const
    -> int32
{
    return _Samples.Num() - 1 - InIndexFromEnd;
}

auto
    SCkDebug_FrameStrip::
    Compute_IndexFromEndAt(
        const FGeometry& InGeometry,
        float InLocalX) const
    -> int32
{
    if (_Samples.IsEmpty())
    { return 0; }

    const auto Stride = Get_BarStride();
    const auto TotalWidth = static_cast<float>(_Samples.Num()) * Stride;
    const auto VisibleWidth = FMath::Max(
        1.0f, static_cast<float>(InGeometry.GetLocalSize().X) - _BarAreaPadding.Left - _BarAreaPadding.Right);

    // Right-align while the history is still shorter than the strip, so "live" always sits at the
    // right edge instead of leaving a gap there after a world switch.
    const auto RightAlign = FMath::Max(0.0f, VisibleWidth - TotalWidth);

    const auto ContentX = InLocalX - _BarAreaPadding.Left - RightAlign + _ScrollOffsetX;
    const auto Index = FMath::Clamp(FMath::FloorToInt32(ContentX / Stride), 0, _Samples.Num() - 1);

    return _Samples.Num() - 1 - Index;
}

auto
    SCkDebug_FrameStrip::
    Do_Scrub(
        int32 InIndexFromEnd)
    -> void
{
    if (_Samples.IsEmpty())
    { return; }

    const auto Clamped = FMath::Clamp(InIndexFromEnd, 0, _Samples.Num() - 1);
    _OnScrubbed.ExecuteIfBound(Clamped);
    Invalidate(EInvalidateWidgetReason::Paint);
}

// ====================================================================================================================
// PAINT
// ====================================================================================================================

auto
    SCkDebug_FrameStrip::
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
    using namespace ck_debug_frame_strip;

    const auto Size = InAllottedGeometry.GetLocalSize();
    const auto Width = static_cast<float>(Size.X);
    const auto Height = static_cast<float>(Size.Y);
    const auto* FilledBrush = CkStyle::GetFilledBrush();

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        InLayerId,
        InAllottedGeometry.ToPaintGeometry(FVector2f{Width, Height}, FSlateLayoutTransform{}),
        FilledBrush,
        ESlateDrawEffect::None,
        CkStyle::BgRoot());

    if (_Samples.IsEmpty())
    { return InLayerId; }

    const auto Stride = Get_BarStride();
    const auto ColumnWidth = Get_ScaledBarWidth();
    const auto TotalWidth = static_cast<float>(_Samples.Num()) * Stride;
    const auto VisibleWidth = FMath::Max(1.0f, Width - _BarAreaPadding.Left - _BarAreaPadding.Right);
    const auto RightAlign = FMath::Max(0.0f, VisibleWidth - TotalWidth);
    const auto MaxScroll = FMath::Max(0.0f, TotalWidth - VisibleWidth);

    const auto SelectedFromEnd = FMath::Clamp(_SelectedIndexFromEnd.Get(0), 0, _Samples.Num() - 1);

    // Live view follows the newest sample; a frozen view stays where the user left it.
    if (SelectedFromEnd == 0)
    { _ScrollOffsetX = MaxScroll; }

    _ScrollOffsetX = FMath::Clamp(_ScrollOffsetX, 0.0f, MaxScroll);

    const auto ColumnAreaTop = _BarAreaPadding.Top;
    const auto ColumnAreaBottom = Height - _BarAreaPadding.Bottom;
    const auto ColumnAreaHeight = FMath::Max(1.0f, ColumnAreaBottom - ColumnAreaTop);

    const auto MaxValue = Compute_MaxValue();
    const auto Budget = _BudgetMs.Get(0.0);
    const auto UseBudgetHeights = _HeightScale == ECkDebug_FrameStripHeightScale::Budget && Budget > 0.0;

    const auto FirstVisible = FMath::Max(0, FMath::FloorToInt32(_ScrollOffsetX / Stride));
    const auto LastVisible = FMath::Min(
        _Samples.Num() - 1, FMath::CeilToInt32((_ScrollOffsetX + VisibleWidth) / Stride));

    auto Layer = InLayerId;

    for (auto Index = FirstVisible; Index <= LastVisible; ++Index)
    {
        const auto& Sample = _Samples[Index];
        const auto IndexFromEnd = _Samples.Num() - 1 - Index;
        const auto IsSelected = IndexFromEnd == SelectedFromEnd;
        const auto IsHovered = IndexFromEnd == _HoveredIndexFromEnd;

        const auto HeightNormalized = UseBudgetHeights
            ? static_cast<float>(Sample.ValueMs / (Budget * 2.0))
            : static_cast<float>(Sample.ValueMs / MaxValue);

        const auto ColumnHeight =
            FMath::Clamp(HeightNormalized, MinColumnFraction, 1.0f) * ColumnAreaHeight;

        const auto ColumnX = _BarAreaPadding.Left + RightAlign + static_cast<float>(Index) * Stride - _ScrollOffsetX;
        const auto ColumnY = ColumnAreaBottom - ColumnHeight;

        // ---- Highlight-filter band (behind the column) -------------------------------------------------------------
        if (Sample.IsHighlighted)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements, Layer + 1,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f{ColumnWidth + SelectionBandPad * 2.0f, Height},
                    FSlateLayoutTransform{FVector2f{ColumnX - SelectionBandPad, 0.0f}}),
                FilledBrush, ESlateDrawEffect::None,
                CkStyle::Warn().CopyWithNewOpacity(0.35f));
        }

        // ---- Selection band ----------------------------------------------------------------------------------------
        if (IsSelected || IsHovered)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements, Layer + 1,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f{ColumnWidth + SelectionBandPad * 2.0f, Height},
                    FSlateLayoutTransform{FVector2f{ColumnX - SelectionBandPad, 0.0f}}),
                FilledBrush, ESlateDrawEffect::None,
                IsSelected
                    ? CkStyle::Selection().CopyWithNewOpacity(0.30f)
                    : CkStyle::TextStrong().CopyWithNewOpacity(0.12f));
        }

        // ---- The column itself -------------------------------------------------------------------------------------
        auto ColumnColor = ck::debug_axes::Get_HeatColor(
            Compute_HeatNormalized(Sample.ValueMs, MaxValue, Budget));
        ColumnColor.A = IsSelected ? 1.0f : 0.7f;

        FSlateDrawElement::MakeBox(
            OutDrawElements, Layer + 2,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f{ColumnWidth, ColumnHeight},
                FSlateLayoutTransform{FVector2f{ColumnX, ColumnY}}),
            FilledBrush, ESlateDrawEffect::None, ColumnColor);

        // ---- Per-frame marker band ---------------------------------------------------------------------------------
        if (Sample.HasMarker)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements, Layer + 2,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f{ColumnWidth, MarkerThickness},
                    FSlateLayoutTransform{FVector2f{ColumnX, ColumnAreaBottom - MarkerThickness}}),
                FilledBrush, ESlateDrawEffect::None,
                CkStyle::Warn().CopyWithNewOpacity(0.75f));
        }
    }

    Layer += 3;

    // ---- Selection cursor ------------------------------------------------------------------------------------------
    {
        const auto SelectedArrayIndex = _Samples.Num() - 1 - SelectedFromEnd;
        const auto CursorX = _BarAreaPadding.Left + RightAlign
            + static_cast<float>(SelectedArrayIndex) * Stride + ColumnWidth * 0.5f - _ScrollOffsetX;

        if (CursorX >= _BarAreaPadding.Left - CursorWidth &&
            CursorX <= Width - _BarAreaPadding.Right + CursorWidth)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements, Layer,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f{CursorWidth, Height},
                    FSlateLayoutTransform{FVector2f{CursorX - CursorWidth * 0.5f, 0.0f}}),
                FilledBrush, ESlateDrawEffect::None, CkStyle::Selection());
        }
    }
    ++Layer;

    // ---- Status label in the right gutter --------------------------------------------------------------------------
    if (_ShowStatusLabel && _BarAreaPadding.Right > 0.0f)
    {
        const auto IsLive = SelectedFromEnd == 0;
        const auto LabelFont = CkStyle::BoldFont(CkStyle::FontSizeMicro());

        const auto LabelText = IsLive
            ? _LiveLabel.ToString()
            : ck::Format_UE(TEXT("{} -{}"), _FrozenLabel.ToString(), SelectedFromEnd);

        const auto LabelColor = IsLive ? CkStyle::Ok() : CkStyle::Warn();
        const auto LabelX = Width - _BarAreaPadding.Right + LabelBoxPadX;
        const auto LabelWidth = FMath::Max(1.0f, _BarAreaPadding.Right - LabelBoxPadX * 2.0f);

        FSlateDrawElement::MakeBox(
            OutDrawElements, Layer,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f{LabelWidth, static_cast<float>(CkStyle::FontSizeMicro()) + LabelBoxPadY * 2.0f},
                FSlateLayoutTransform{FVector2f{LabelX - LabelBoxPadX, LabelBoxPadY}}),
            FilledBrush, ESlateDrawEffect::None,
            CkStyle::BgRoot().CopyWithNewOpacity(0.85f));

        FSlateDrawElement::MakeText(
            OutDrawElements, Layer + 1,
            InAllottedGeometry.ToPaintGeometry(
                FVector2f{LabelWidth, static_cast<float>(CkStyle::FontSizeMicro()) + LabelBoxPadY * 2.0f},
                FSlateLayoutTransform{FVector2f{LabelX, LabelBoxPadY * 2.0f}}),
            LabelText, LabelFont, ESlateDrawEffect::None, LabelColor);

        Layer += 2;
    }

    // ---- Overflow chevrons -----------------------------------------------------------------------------------------
    {
        const auto ChevronFont = CkStyle::BoldFont(CkStyle::FontSizeSmall());

        if (_ScrollOffsetX > 1.0f)
        {
            FSlateDrawElement::MakeText(
                OutDrawElements, Layer,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f{_BarAreaPadding.Left, 14.0f},
                    FSlateLayoutTransform{FVector2f{0.0f, Height * 0.5f - 7.0f}}),
                FString{TEXT("\u25C0")}, ChevronFont, ESlateDrawEffect::None, CkStyle::TextMute());
        }

        if (_ScrollOffsetX < MaxScroll - 1.0f)
        {
            FSlateDrawElement::MakeText(
                OutDrawElements, Layer,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2f{12.0f, 14.0f},
                    FSlateLayoutTransform{FVector2f{Width - _BarAreaPadding.Right - 12.0f, Height * 0.5f - 7.0f}}),
                FString{TEXT("\u25B6")}, ChevronFont, ESlateDrawEffect::None, CkStyle::TextMute());
        }
    }

    return Layer + 1;
}

// ====================================================================================================================
// INPUT
// ====================================================================================================================

auto
    SCkDebug_FrameStrip::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (_Samples.IsEmpty())
    { return FReply::Unhandled(); }

    const auto Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto Self = SharedThis(this);

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        _IsScrubbing = true;
        Do_Scrub(Compute_IndexFromEndAt(InGeometry, static_cast<float>(Local.X)));

        return FReply::Handled().CaptureMouse(Self).SetUserFocus(Self, EFocusCause::Mouse);
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton ||
        InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
    {
        _IsPanning = true;
        _PanWasDragged = false;
        _PanStartX = static_cast<float>(Local.X);
        _PanStartScrollX = _ScrollOffsetX;

        return FReply::Handled().CaptureMouse(Self).SetUserFocus(Self, EFocusCause::Mouse);
    }

    return FReply::Unhandled();
}

auto
    SCkDebug_FrameStrip::
    OnMouseButtonUp(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && _IsScrubbing)
    {
        _IsScrubbing = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    if (NOT _IsPanning)
    { return FReply::Unhandled(); }

    const auto WasRightButton = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
    const auto WasPanButton = WasRightButton || InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton;

    if (NOT WasPanButton)
    { return FReply::Unhandled(); }

    _IsPanning = false;

    // A right-click that never became a drag is a context click, not a pan — the strip's values
    // are information, so they honour the suite's copy policy.
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
    SCkDebug_FrameStrip::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    using namespace ck_debug_frame_strip;

    const auto Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

    if (_IsScrubbing)
    {
        Do_Scrub(Compute_IndexFromEndAt(InGeometry, static_cast<float>(Local.X)));
        return FReply::Handled();
    }

    if (_IsPanning)
    {
        const auto DeltaX = static_cast<float>(Local.X) - _PanStartX;

        if (FMath::Abs(DeltaX) > PanDeadZonePx)
        { _PanWasDragged = true; }

        // Clamping happens in OnPaint, where the visible width is known.
        _ScrollOffsetX = _PanStartScrollX - DeltaX;
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    _HoveredIndexFromEnd = Compute_IndexFromEndAt(InGeometry, static_cast<float>(Local.X));
    Invalidate(EInvalidateWidgetReason::Paint);

    // Unhandled on hover-only: returning Handled can stop Slate routing OnMouseWheel here.
    return FReply::Unhandled();
}

auto
    SCkDebug_FrameStrip::
    OnMouseButtonDoubleClick(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return FReply::Unhandled(); }

    _OnReturnToLive.ExecuteIfBound();
    Do_Scrub(0);

    return FReply::Handled();
}

auto
    SCkDebug_FrameStrip::
    OnMouseWheel(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    using namespace ck_debug_frame_strip;

    if (NOT _AllowZoom || _Samples.IsEmpty())
    { return FReply::Unhandled(); }

    const auto Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    const auto CursorContentX = static_cast<float>(Local.X) - _BarAreaPadding.Left + _ScrollOffsetX;
    const auto SampleUnderCursor = CursorContentX / Get_BarStride();

    _ZoomScale = FMath::Clamp(
        _ZoomScale * (InMouseEvent.GetWheelDelta() > 0.0f ? ZoomStepIn : ZoomStepOut),
        MinZoom, MaxZoom);

    // Keep the sample under the cursor pinned across the zoom.
    _ScrollOffsetX = SampleUnderCursor * Get_BarStride()
        - (static_cast<float>(Local.X) - _BarAreaPadding.Left);
    _ScrollOffsetX = FMath::Max(0.0f, _ScrollOffsetX);

    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled();
}

auto
    SCkDebug_FrameStrip::
    OnMouseLeave(
        const FPointerEvent& InMouseEvent)
    -> void
{
    _HoveredIndexFromEnd = INDEX_NONE;
    Invalidate(EInvalidateWidgetReason::Paint);
    SLeafWidget::OnMouseLeave(InMouseEvent);
}

auto
    SCkDebug_FrameStrip::
    OnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent)
    -> FReply
{
    if (_Samples.IsEmpty())
    { return FReply::Unhandled(); }

    const auto Key = InKeyEvent.GetKey();
    const auto Current = _SelectedIndexFromEnd.Get(0);

    // Left = older (larger from-end index), Right = newer.
    if (Key == EKeys::Left)  { Do_Scrub(Current + 1); return FReply::Handled(); }
    if (Key == EKeys::Right) { Do_Scrub(Current - 1); return FReply::Handled(); }
    if (Key == EKeys::Home)  { Do_Scrub(_Samples.Num() - 1); return FReply::Handled(); }

    if (Key == EKeys::End)
    {
        _OnReturnToLive.ExecuteIfBound();
        Do_Scrub(0);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

// ====================================================================================================================
