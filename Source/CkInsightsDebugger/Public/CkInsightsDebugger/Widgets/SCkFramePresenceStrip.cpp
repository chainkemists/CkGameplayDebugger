#include "CkInsightsDebugger/Widgets/SCkFramePresenceStrip.h"

#include "CkInsightsAnalyzer/Core/CkTimerCategorizer.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_frame_presence_strip
{
    constexpr float StripWidth = 290.0f;
    constexpr float StripHeight = 14.0f;
    constexpr float MinSlotHeight = 2.0f;
    constexpr float MinPresentOpacity = 0.45f;

    // Style-token-backed colors, resolved lazily at paint time — CkStyle reads a settings CDO, so
    // these must never be evaluated at static init. Bar and spike deliberately match
    // SCkFrameBarChart's ColorYellow / ColorRed so a spike reads the same in both surfaces.
    auto ColorBar()   -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Warn(), 0.9f); }
    auto ColorSpike() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Err(), 0.9f); }
    auto ColorTrack() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Bg3(), 0.7f); }
    auto ColorHover() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::TextStrong(), 0.25f); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkFramePresenceStrip::
    Construct(const FArguments& InArgs)
    -> void
{
    _Node = InArgs._Node;
    _AnalysedFrameIndices = InArgs._AnalysedFrameIndices;
    _OnFrameClicked = InArgs._OnFrameClicked;
    _OnRefineToRuns = InArgs._OnRefineToRuns;

    _SlotTooltip = SNew(SToolTip)
        .TextMargin(FMargin(4.0f))
        [
            SAssignNew(_TooltipTextBlock, STextBlock)
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
        ];

    SetToolTip(_SlotTooltip);

    SetVisibility(EVisibility::Visible);
}

auto
    SCkFramePresenceStrip::
    ComputeDesiredSize(float) const
    -> FVector2D
{
    return FVector2D(ck_frame_presence_strip::StripWidth, ck_frame_presence_strip::StripHeight);
}

auto
    SCkFramePresenceStrip::
    OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
            const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
            int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
    -> int32
{
    using namespace ck_frame_presence_strip;

    const float Width = AllottedGeometry.GetLocalSize().X;
    const float Height = AllottedGeometry.GetLocalSize().Y;

    if (Width <= 0.0f || Height <= 0.0f)
    {
        return LayerId;
    }

    const auto* FilledBrush = FAppStyle::GetBrush(TEXT("WhiteBrush"));

    FSlateDrawElement::MakeBox(
        OutDrawElements, LayerId,
        AllottedGeometry.ToPaintGeometry(),
        FilledBrush,
        ESlateDrawEffect::None,
        ColorTrack());

    const int32 OrdinalCount = Get_OrdinalCount();
    if (OrdinalCount == 0)
    {
        return LayerId + 1;
    }

    const int32 SlotCount = Get_SlotCount(Width);
    const float SlotWidth = Width / static_cast<float>(SlotCount);

    const double MaxMs = _Node->MaxInclusiveMs;
    const double P95Ms = _Node->P95InclusiveMs;
    const bool HasSpikeBand = MaxMs > P95Ms;

    const FLinearColor BarColor = ColorBar();
    const FLinearColor SpikeColor = ColorSpike();

    for (int32 Slot = 0; Slot < SlotCount; ++Slot)
    {
        const auto Ordinals = Get_SlotOrdinals(Slot, SlotCount, OrdinalCount);

        float SlotMaxMs = -1.0f;
        bool SlotHasSpike = false;

        for (int32 Ordinal = Ordinals.Key; Ordinal < Ordinals.Value; ++Ordinal)
        {
            const float SampleMs = _Node->PerFrameInclusiveMs[Ordinal];
            if (SampleMs < 0.0f)
            {
                continue;
            }

            SlotMaxMs = FMath::Max(SlotMaxMs, SampleMs);
            SlotHasSpike = SlotHasSpike || (HasSpikeBand && static_cast<double>(SampleMs) >= P95Ms);
        }

        if (SlotMaxMs < 0.0f)
        {
            continue;
        }

        const float Magnitude = (MaxMs > 0.0)
            ? FMath::Clamp(SlotMaxMs / static_cast<float>(MaxMs), 0.0f, 1.0f)
            : 1.0f;

        const float SlotHeight = FMath::Max(MinSlotHeight, Height * Magnitude);

        FLinearColor SlotColor = SlotHasSpike ? SpikeColor : BarColor;
        SlotColor.A *= FMath::Lerp(MinPresentOpacity, 1.0f, Magnitude);

        FSlateDrawElement::MakeBox(
            OutDrawElements, LayerId + 1,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(SlotWidth, SlotHeight),
                FSlateLayoutTransform(FVector2D(static_cast<float>(Slot) * SlotWidth, Height - SlotHeight))),
            FilledBrush,
            ESlateDrawEffect::None,
            SlotColor);
    }

    if (_HoveredSlot >= 0 && _HoveredSlot < SlotCount)
    {
        FSlateDrawElement::MakeBox(
            OutDrawElements, LayerId + 2,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(SlotWidth, Height),
                FSlateLayoutTransform(FVector2D(static_cast<float>(_HoveredSlot) * SlotWidth, 0.0f))),
            FilledBrush,
            ESlateDrawEffect::None,
            ColorHover());
    }

    return LayerId + 3;
}

// --------------------------------------------------------------------------------------------------------------------
// Mouse Interaction
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkFramePresenceStrip::
    OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> FReply
{
    if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || Get_OrdinalCount() == 0)
    {
        return FReply::Unhandled();
    }

    // A click that would do nothing stays Unhandled so the owning table row still gets its selection.
    if (NOT MouseEvent.IsControlDown())
    {
        const int32 SlotCount = Get_SlotCount(MyGeometry.GetLocalSize().X);
        const int32 Slot = Get_SlotAt(MyGeometry, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));

        if (Get_PresentOrdinalInSlot(Slot, SlotCount) == INDEX_NONE)
        {
            return FReply::Unhandled();
        }
    }

    return FReply::Handled().CaptureMouse(SharedThis(this));
}

auto
    SCkFramePresenceStrip::
    OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> FReply
{
    if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || NOT HasMouseCapture())
    {
        return FReply::Unhandled();
    }

    const auto Reply = FReply::Handled().ReleaseMouseCapture();

    if (MouseEvent.IsControlDown())
    {
        DoRefineToPresentFrames();
        return Reply;
    }

    const int32 SlotCount = Get_SlotCount(MyGeometry.GetLocalSize().X);
    const int32 Slot = Get_SlotAt(MyGeometry, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
    const int32 Ordinal = Get_PresentOrdinalInSlot(Slot, SlotCount);

    if (Ordinal != INDEX_NONE)
    {
        _OnFrameClicked.ExecuteIfBound((*_AnalysedFrameIndices)[Ordinal]);
    }

    return Reply;
}

auto
    SCkFramePresenceStrip::
    OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> FReply
{
    const int32 SlotCount = Get_SlotCount(MyGeometry.GetLocalSize().X);
    const int32 Slot = Get_SlotAt(MyGeometry, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));

    if (Slot != _HoveredSlot)
    {
        _HoveredSlot = Slot;
        Invalidate(EInvalidateWidgetReason::Paint);
    }

    DoUpdateTooltip(Slot, SlotCount);

    // Unhandled on hover-only: a Handled hover would take the row's own pointer routing with it.
    return FReply::Unhandled();
}

auto
    SCkFramePresenceStrip::
    OnMouseLeave(const FPointerEvent& MouseEvent)
    -> void
{
    SLeafWidget::OnMouseLeave(MouseEvent);

    _HoveredSlot = INDEX_NONE;
    Invalidate(EInvalidateWidgetReason::Paint);
}

// --------------------------------------------------------------------------------------------------------------------
// Slot mapping
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkFramePresenceStrip::
    Get_OrdinalCount() const
    -> int32
{
    if (ck::Is_NOT_Valid(_Node) || ck::Is_NOT_Valid(_AnalysedFrameIndices))
    {
        return 0;
    }

    return FMath::Min(_Node->PerFrameInclusiveMs.Num(), _AnalysedFrameIndices->Num());
}

auto
    SCkFramePresenceStrip::
    Get_SlotCount(float InWidth) const
    -> int32
{
    const int32 OrdinalCount = Get_OrdinalCount();
    if (OrdinalCount == 0)
    {
        return 0;
    }

    const int32 MaxWholePixelSlots = FMath::Max(1, FMath::FloorToInt32(InWidth));
    return FMath::Min(OrdinalCount, MaxWholePixelSlots);
}

auto
    SCkFramePresenceStrip::
    Get_SlotOrdinals(int32 InSlot, int32 InSlotCount, int32 InOrdinalCount) const
    -> TPair<int32, int32>
{
    const int64 Begin = (static_cast<int64>(InSlot) * InOrdinalCount) / InSlotCount;
    const int64 End = (static_cast<int64>(InSlot + 1) * InOrdinalCount) / InSlotCount;

    return TPair<int32, int32>{static_cast<int32>(Begin), static_cast<int32>(FMath::Max(End, Begin + 1))};
}

auto
    SCkFramePresenceStrip::
    Get_SlotAt(const FGeometry& InGeometry, const FVector2D& InLocalPosition) const
    -> int32
{
    const float Width = InGeometry.GetLocalSize().X;
    const int32 SlotCount = Get_SlotCount(Width);

    if (SlotCount == 0 || Width <= 0.0f)
    {
        return INDEX_NONE;
    }

    const float SlotWidth = Width / static_cast<float>(SlotCount);
    return FMath::Clamp(FMath::FloorToInt32(static_cast<float>(InLocalPosition.X) / SlotWidth), 0, SlotCount - 1);
}

auto
    SCkFramePresenceStrip::
    Get_PresentOrdinalInSlot(int32 InSlot, int32 InSlotCount) const
    -> int32
{
    const int32 OrdinalCount = Get_OrdinalCount();
    if (InSlot < 0 || InSlot >= InSlotCount || OrdinalCount == 0)
    {
        return INDEX_NONE;
    }

    const auto Ordinals = Get_SlotOrdinals(InSlot, InSlotCount, OrdinalCount);

    int32 HeaviestOrdinal = INDEX_NONE;
    float HeaviestMs = -1.0f;

    for (int32 Ordinal = Ordinals.Key; Ordinal < Ordinals.Value; ++Ordinal)
    {
        const float SampleMs = _Node->PerFrameInclusiveMs[Ordinal];
        if (SampleMs >= 0.0f && SampleMs > HeaviestMs)
        {
            HeaviestMs = SampleMs;
            HeaviestOrdinal = Ordinal;
        }
    }

    return HeaviestOrdinal;
}

auto
    SCkFramePresenceStrip::
    Get_FirstOrdinalInSlot(int32 InSlot, int32 InSlotCount) const
    -> int32
{
    const int32 OrdinalCount = Get_OrdinalCount();
    if (InSlot < 0 || InSlot >= InSlotCount || OrdinalCount == 0)
    {
        return INDEX_NONE;
    }

    return Get_SlotOrdinals(InSlot, InSlotCount, OrdinalCount).Key;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkFramePresenceStrip::
    DoUpdateTooltip(int32 InSlot, int32 InSlotCount)
    -> void
{
    if (ck::Is_NOT_Valid(_TooltipTextBlock) || Get_OrdinalCount() == 0)
    {
        return;
    }

    const int32 PresentOrdinal = Get_PresentOrdinalInSlot(InSlot, InSlotCount);
    const int32 Ordinal = (PresentOrdinal != INDEX_NONE) ? PresentOrdinal : Get_FirstOrdinalInSlot(InSlot, InSlotCount);

    if (Ordinal == INDEX_NONE)
    {
        return;
    }

    const uint64 FrameIndex = (*_AnalysedFrameIndices)[Ordinal];

    if (PresentOrdinal != INDEX_NONE)
    {
        _TooltipTextBlock->SetText(FText::FromString(FString::Printf(
            TEXT("Frame %llu — %s · present %llu/%d"),
            FrameIndex,
            *FCk_TimerCategorizer::FormatMs(_Node->PerFrameInclusiveMs[Ordinal]),
            _Node->FramesPresent,
            _AnalysedFrameIndices->Num())));
        return;
    }

    _TooltipTextBlock->SetText(FText::FromString(FString::Printf(
        TEXT("Frame %llu — not in hot paths this frame · %s when hit"),
        FrameIndex,
        *FCk_TimerCategorizer::FormatMs(_Node->HitAvgInclusiveMs))));
}

auto
    SCkFramePresenceStrip::
    DoRefineToPresentFrames()
    -> void
{
    const int32 OrdinalCount = Get_OrdinalCount();

    auto Runs = TArray<FCk_FrameRun>{};

    for (int32 Ordinal = 0; Ordinal < OrdinalCount; ++Ordinal)
    {
        if (_Node->PerFrameInclusiveMs[Ordinal] < 0.0f)
        {
            continue;
        }

        const uint64 FrameIndex = (*_AnalysedFrameIndices)[Ordinal];

        if (NOT Runs.IsEmpty() && FrameIndex == Runs.Last().LastFrame + 1)
        {
            Runs.Last().LastFrame = FrameIndex;
            continue;
        }

        Runs.Add(FCk_FrameRun{FrameIndex, FrameIndex});
    }

    // A rendered node always has at least one present frame; refusing the empty case keeps a
    // malformed node from clearing the user's selection.
    if (Runs.IsEmpty())
    {
        return;
    }

    _OnRefineToRuns.ExecuteIfBound(Runs);
}

// --------------------------------------------------------------------------------------------------------------------
