#include "CkInsightsDebugger/Widgets/SCkFrameBarChart.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"

// --------------------------------------------------------------------------------------------------------------------

namespace FrameBarChart
{
    constexpr float MinBarWidth = 1.0f;
    constexpr float MaxBarWidth = 20.0f;
    constexpr float DesiredHeight = 200.0f;
    constexpr float TopPadding = 18.0f;     // room for frame number labels
    constexpr float BottomPadding = 4.0f;
    constexpr float ScreenshotRailTopPadding = 4.0f;
    constexpr float ScreenshotThumbnailWidth = 40.0f;
    constexpr float ScreenshotThumbnailHeight = 22.0f;
    constexpr float ScreenshotThumbnailGap = 2.0f;
    constexpr float ScreenshotMarkerWidth = 2.0f;
    constexpr float ScreenshotMarkerCapWidth = 8.0f;
    constexpr float ScreenshotMarkerCapHeight = 5.0f;
    constexpr float LabelRightMargin = 120.0f;  // room for "100 ms (10 fps)" labels
    constexpr double MinFramesPerPixel = 0.05;   // max zoom in
    constexpr double MaxFramesPerPixel = 500.0;  // max zoom out (allows fitting thousands of frames)

    // Fixed vertical ceiling — ensures threshold lines are always visible
    constexpr double MinDisplayMaxMs = 120.0;

    // Style-token-backed colors, resolved lazily at paint time — CkStyle reads
    // a settings CDO, so these must never be evaluated at static init.
    auto ColorGreen()  -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Ok(), 0.9f); }
    auto ColorYellow() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Warn(), 0.9f); }
    auto ColorRed()    -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Err(), 0.9f); }
    auto ColorHover()  -> FLinearColor { return CkStyle::OverlayOf(CkStyle::TextStrong(), 0.3f); }
    auto ColorSelect() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Selection(), 0.25f); }
    auto ColorThresholdGreen()  -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Ok(), 0.4f); }
    auto ColorThresholdYellow() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Warn(), 0.4f); }
    auto ColorThresholdOrange() -> FLinearColor { return CkStyle::OverlayOf(FMath::Lerp(CkStyle::Warn(), CkStyle::Err(), 0.5f), 0.4f); }
    auto ColorThresholdRed()    -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Err(), 0.4f); }
    auto ColorLabel()      -> FLinearColor { return CkStyle::OverlayOf(CkStyle::TextDim(), 0.7f); }
    auto ColorBackground() -> FLinearColor { return CkStyle::BgRoot(); }
    auto ColorGridLine()   -> FLinearColor { return CkStyle::OverlayOf(CkStyle::BorderStrong(), 0.25f); }
    auto ColorScreenshotMarker() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Selection(), 0.95f); }
    auto ColorScreenshotBorder() -> FLinearColor { return CkStyle::OverlayOf(CkStyle::Selection(), 0.7f); }

    auto Get_DisplayMaxMs(const TArray<double>& InDurationsMs) -> double
    {
        if (InDurationsMs.Num() == 0)
        { return MinDisplayMaxMs; }

        TArray<double> Sorted = InDurationsMs;
        Sorted.Sort();
        const int32 P95Index = FMath::Min(
            FMath::FloorToInt32(static_cast<float>(Sorted.Num()) * 0.95f),
            Sorted.Num() - 1);

        constexpr double HeadroomAboveP95 = 1.3;
        return FMath::Max(Sorted[P95Index] * HeadroomAboveP95, MinDisplayMaxMs);
    }

    auto Get_FramesPerPixel_FitAll(int32 InFrameCount) -> double
    {
        // The real geometry isn't available when frame data arrives; assume a typical tab width.
        constexpr double AssumedWidgetWidthPx = 1000.0;

        const double NumFrames = FMath::Max(1.0, static_cast<double>(InFrameCount));
        const double DesiredBarStep = AssumedWidgetWidthPx / NumFrames;
        const double DesiredBarWidth = FMath::Clamp(
            DesiredBarStep - ((DesiredBarStep >= 3.0) ? 1.0 : 0.0),
            static_cast<double>(MinBarWidth),
            static_cast<double>(MaxBarWidth));

        return FMath::Clamp(1.0 / DesiredBarWidth, MinFramesPerPixel, MaxFramesPerPixel);
    }

    /** Format an integer with comma separators (e.g. 4160 → "4,160"). */
    auto FormatWithCommas(int64 Value) -> FString
    {
        FString Raw = FString::Printf(TEXT("%lld"), Value);
        FString Result;
        const int32 Len = Raw.Len();
        for (int32 i = 0; i < Len; ++i)
        {
            if (i > 0 && (Len - i) % 3 == 0)
            {
                Result.AppendChar(TEXT(','));
            }
            Result.AppendChar(Raw[i]);
        }
        return Result;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkFrameBarChart::
    Construct(const FArguments& InArgs)
    -> void
{
    _TargetFrameMs = InArgs._TargetFrameMs;
    _OnFrameSelectionChanged = InArgs._OnFrameSelectionChanged;
    _OnScreenshotMarkerClicked = InArgs._OnScreenshotMarkerClicked;

    _FrameTooltip = SNew(SToolTip)
        .TextMargin(FMargin(4.0f))
        [
            SAssignNew(_TooltipTextBlock, STextBlock)
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
        ];

    SetToolTip(_FrameTooltip);

    // Both must be Visible for the whole chart area to be hit-testable, not just over
    // children — SBox defaults to SelfHitTestInvisible.
    SetVisibility(EVisibility::Visible);

    ChildSlot
    [
        SNew(SBox)
        .Visibility(EVisibility::Visible)
    ];
}

// --------------------------------------------------------------------------------------------------------------------
// Data
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkFrameBarChart::
    SetFrameData(TArray<double>&& InFrameDurationsMs)
    -> void
{
    _FrameDurationsMs = MoveTemp(InFrameDurationsMs);
    _ViewOffset = 0.0;

    _DisplayMaxMs = FrameBarChart::Get_DisplayMaxMs(_FrameDurationsMs);
    _FramesPerPixel = FrameBarChart::Get_FramesPerPixel_FitAll(_FrameDurationsMs.Num());

    ClearSelection();
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkFrameBarChart::
    AppendFrameData(const TArray<double>& NewDurationsMs)
    -> void
{
    if (NewDurationsMs.Num() == 0) return;

    _FrameDurationsMs.Append(NewDurationsMs);

    // _DisplayMaxMs deliberately stays at its floor while loading — startup/shutdown outliers
    // would flatten every normal bar; DoFinishLoading calls RecalculateDisplayMax once all
    // frames are in. Re-fitting the zoom each chunk grows the bars left-to-right, like Insights.
    _ViewOffset = 0.0;
    _FramesPerPixel = FrameBarChart::Get_FramesPerPixel_FitAll(_FrameDurationsMs.Num());

    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkFrameBarChart::
    ClearFrameData()
    -> void
{
    _FrameDurationsMs.Empty();
    ClearScreenshotMarkers();
    _ViewOffset = 0.0;
    _FramesPerPixel = 1.0;
    _DisplayMaxMs = FrameBarChart::MinDisplayMaxMs;
    ClearSelection();
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkFrameBarChart::
    RecalculateDisplayMax()
    -> void
{
    _DisplayMaxMs = FrameBarChart::Get_DisplayMaxMs(_FrameDurationsMs);
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkFrameBarChart::
    SetTargetFrameMs(double InTargetMs)
    -> void
{
    _TargetFrameMs = FMath::Max(1.0, InTargetMs);
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkFrameBarChart::
    SetScreenshotMarkers(TArray<FCk_FrameScreenshotMarker>&& InMarkers)
    -> void
{
    // Stable ordering makes overlap stacking and hit precedence deterministic across reloads.
    InMarkers.Sort([](const FCk_FrameScreenshotMarker& Left, const FCk_FrameScreenshotMarker& Right)
    {
        if (Left.FrameIndex != Right.FrameIndex)
        {
            return Left.FrameIndex < Right.FrameIndex;
        }
        if (Left.ScreenshotId != Right.ScreenshotId)
        {
            return Left.ScreenshotId < Right.ScreenshotId;
        }
        return Left.Label < Right.Label;
    });

    _ScreenshotMarkers = MoveTemp(InMarkers);
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

auto
    SCkFrameBarChart::
    ClearScreenshotMarkers()
    -> void
{
    if (_ScreenshotMarkers.IsEmpty()) return;

    _ScreenshotMarkers.Empty();
    Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

// --------------------------------------------------------------------------------------------------------------------
// Selection
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkFrameBarChart::
    SetSelection(uint64 StartFrame, uint64 EndFrame)
    -> void
{
    _bHasSelection = true;
    _SelectionStart = FMath::Min(StartFrame, EndFrame);
    _SelectionEnd = FMath::Max(StartFrame, EndFrame);
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkFrameBarChart::
    ClearSelection()
    -> void
{
    _bHasSelection = false;
    _SelectionStart = 0;
    _SelectionEnd = 0;
    Invalidate(EInvalidateWidgetReason::Paint);
}

auto
    SCkFrameBarChart::
    EnsureFrameVisible(uint64 FrameIndex)
    -> void
{
    if (_FrameDurationsMs.IsEmpty()) return;

    const uint64 LastFrame = static_cast<uint64>(_FrameDurationsMs.Num() - 1);
    const uint64 ClampedFrame = FMath::Min(FrameIndex, LastFrame);
    const FGeometry& CachedGeometry = GetCachedGeometry();
    const float Width = CachedGeometry.GetLocalSize().X;
    if (Width <= 0.0f) return;

    const float BarStep = GetBarStep(CachedGeometry);
    const double VisibleFrameCount = FMath::Max(1.0, static_cast<double>(Width) / static_cast<double>(BarStep));
    const double FirstVisibleFrame = _ViewOffset;
    const double LastVisibleFrame = FirstVisibleFrame + VisibleFrameCount - 1.0;
    if (static_cast<double>(ClampedFrame) >= FirstVisibleFrame && static_cast<double>(ClampedFrame) <= LastVisibleFrame)
    {
        return;
    }

    const double MaxOffset = FMath::Max(0.0, static_cast<double>(_FrameDurationsMs.Num()) - VisibleFrameCount);
    _ViewOffset = FMath::Clamp(static_cast<double>(ClampedFrame) - VisibleFrameCount * 0.5, 0.0, MaxOffset);
    Invalidate(EInvalidateWidgetReason::Paint);
}

// --------------------------------------------------------------------------------------------------------------------
// SWidget Interface
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkFrameBarChart::
    ComputeDesiredSize(float) const
    -> FVector2D
{
    return FVector2D(400.0f, FrameBarChart::DesiredHeight);
}

auto
    SCkFrameBarChart::
    OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
            const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
            int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
    -> int32
{
    const float Width = AllottedGeometry.GetLocalSize().X;
    const float Height = AllottedGeometry.GetLocalSize().Y;
    const float ChartBottom = GetChartBottom(AllottedGeometry);
    const float DrawHeight = ChartBottom - FrameBarChart::TopPadding;

    if (_FrameDurationsMs.Num() == 0 || Width <= 0.0f || DrawHeight <= 0.0f)
    {
        return LayerId;
    }

    FSlateDrawElement::MakeBox(
        OutDrawElements, LayerId,
        AllottedGeometry.ToPaintGeometry(),
        FAppStyle::GetBrush(TEXT("WhiteBrush")),
        ESlateDrawEffect::None,
        FrameBarChart::ColorBackground());

    const double MaxMs = _DisplayMaxMs;

    const float BarW = GetBarWidth(AllottedGeometry);
    const float BarStep = GetBarStep(AllottedGeometry);

    const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);
    const float BarAreaRight = Width - FrameBarChart::LabelRightMargin;

    auto DrawThresholdLine = [&](double ThresholdMs, const FLinearColor& LineColor, const FString& Label)
    {
        const float Y = FrameBarChart::TopPadding + DrawHeight * (1.0f - static_cast<float>(ThresholdMs / MaxMs));
        if (Y >= FrameBarChart::TopPadding && Y <= Height - FrameBarChart::BottomPadding)
        {
            TArray<FVector2D> LinePoints;
            LinePoints.Add(FVector2D(0.0f, Y));
            LinePoints.Add(FVector2D(BarAreaRight, Y));
            FSlateDrawElement::MakeLines(
                OutDrawElements, LayerId + 1,
                AllottedGeometry.ToPaintGeometry(),
                LinePoints, ESlateDrawEffect::None, LineColor, true, 1.0f);

            FSlateDrawElement::MakeText(
                OutDrawElements, LayerId + 1,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(FrameBarChart::LabelRightMargin, 14.0f),
                    FSlateLayoutTransform(FVector2D(BarAreaRight + 4.0f, Y - 7.0f))),
                Label, LabelFont,
                ESlateDrawEffect::None, FrameBarChart::ColorLabel());
        }
    };

    DrawThresholdLine(16.67,  FrameBarChart::ColorThresholdGreen(),  TEXT("16.7 ms (60 fps)"));
    DrawThresholdLine(33.33,  FrameBarChart::ColorThresholdYellow(), TEXT("33.3 ms (30 fps)"));
    DrawThresholdLine(50.0,   FrameBarChart::ColorThresholdOrange(), TEXT("50 ms (20 fps)"));
    DrawThresholdLine(66.67,  FrameBarChart::ColorThresholdRed(),    TEXT("66.7 ms (15 fps)"));
    DrawThresholdLine(100.0,  FrameBarChart::ColorThresholdRed(),    TEXT("100 ms (10 fps)"));

    {
        const FSlateFontInfo AxisFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);
        const FLinearColor GridLineColor = FrameBarChart::ColorGridLine();

        constexpr float MinLabelSpacing = 80.0f;
        const int32 FramesPerLabel = FMath::Max(1, FMath::CeilToInt32(MinLabelSpacing / BarStep));

        int32 NiceInterval = 1;
        const int32 NiceSteps[] = { 1, 2, 5 };
        int32 Magnitude = 1;
        while (NiceInterval < FramesPerLabel)
        {
            for (int32 Step : NiceSteps)
            {
                NiceInterval = Step * Magnitude;
                if (NiceInterval >= FramesPerLabel) break;
            }
            if (NiceInterval < FramesPerLabel) Magnitude *= 10;
        }

        const int32 LabelFirstFrame = FMath::Max(0,
            (FMath::FloorToInt32(static_cast<float>(_ViewOffset)) / NiceInterval) * NiceInterval);

        for (int32 i = LabelFirstFrame; i < _FrameDurationsMs.Num(); i += NiceInterval)
        {
            const float LabelX = (static_cast<float>(i) - static_cast<float>(_ViewOffset)) * BarStep;
            if (LabelX < -30.0f) continue;
            if (LabelX > BarAreaRight) break;

            TArray<FVector2D> GridPoints;
            GridPoints.Add(FVector2D(LabelX, FrameBarChart::TopPadding));
            GridPoints.Add(FVector2D(LabelX, ChartBottom));
            FSlateDrawElement::MakeLines(
                OutDrawElements, LayerId + 1,
                AllottedGeometry.ToPaintGeometry(),
                GridPoints, ESlateDrawEffect::None, GridLineColor, true, 1.0f);

            FSlateDrawElement::MakeText(
                OutDrawElements, LayerId + 1,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(60.0f, 14.0f),
                    FSlateLayoutTransform(FVector2D(LabelX + 2.0f, 1.0f))),
                FrameBarChart::FormatWithCommas(i),
                AxisFont,
                ESlateDrawEffect::None,
                FrameBarChart::ColorLabel());
        }
    }

    const int32 FirstFrame = FMath::Max(0, FMath::FloorToInt32(_ViewOffset));
    const int32 VisibleBars = FMath::CeilToInt32(Width / BarStep) + 2;
    const int32 LastFrame = FMath::Min(FirstFrame + VisibleBars, _FrameDurationsMs.Num());

    for (int32 i = FirstFrame; i < LastFrame; ++i)
    {
        const float X = (static_cast<float>(i) - static_cast<float>(_ViewOffset)) * BarStep;
        if (X + BarW < 0.0f || X > Width) continue;

        const double Ms = _FrameDurationsMs[i];
        const float BarHeight = FMath::Clamp(
            DrawHeight * static_cast<float>(Ms / MaxMs), 1.0f, DrawHeight);
        const float BarY = FrameBarChart::TopPadding + DrawHeight - BarHeight;

        const FLinearColor Color = GetBarColor(Ms);

        FSlateDrawElement::MakeBox(
            OutDrawElements, LayerId + 2,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(BarW, BarHeight),
                FSlateLayoutTransform(FVector2D(X, BarY))),
            FAppStyle::GetBrush(TEXT("WhiteBrush")),
            ESlateDrawEffect::None, Color);
    }

    // Screenshot data is pre-decoded by the owner. This pass only places existing brushes, so
    // neither normal painting nor hover input can introduce hitches while inspecting a trace.
    if (NOT _ScreenshotMarkers.IsEmpty())
    {
        const float RailTop = ChartBottom + FrameBarChart::ScreenshotRailTopPadding;
        for (int32 MarkerIndex = 0; MarkerIndex < _ScreenshotMarkers.Num(); ++MarkerIndex)
        {
            const FCk_FrameScreenshotMarker& Marker = _ScreenshotMarkers[MarkerIndex];
            if (Marker.FrameIndex >= static_cast<uint64>(_FrameDurationsMs.Num())) continue;

            const float MarkerX = FrameToX(AllottedGeometry, static_cast<int64>(Marker.FrameIndex));
            if (MarkerX >= -FrameBarChart::ScreenshotMarkerCapWidth && MarkerX <= Width + FrameBarChart::ScreenshotMarkerCapWidth)
            {
                FSlateDrawElement::MakeBox(
                    OutDrawElements, LayerId + 3,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2D(FrameBarChart::ScreenshotMarkerWidth, ChartBottom - FrameBarChart::TopPadding),
                        FSlateLayoutTransform(FVector2D(MarkerX - FrameBarChart::ScreenshotMarkerWidth * 0.5f, FrameBarChart::TopPadding))),
                    FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, FrameBarChart::ColorScreenshotMarker());

                FSlateDrawElement::MakeBox(
                    OutDrawElements, LayerId + 4,
                    AllottedGeometry.ToPaintGeometry(
                        FVector2D(FrameBarChart::ScreenshotMarkerCapWidth, FrameBarChart::ScreenshotMarkerCapHeight),
                        FSlateLayoutTransform(FVector2D(MarkerX - FrameBarChart::ScreenshotMarkerCapWidth * 0.5f, FrameBarChart::TopPadding))),
                    FAppStyle::GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, FrameBarChart::ColorScreenshotMarker());
            }

            // Do not pin off-screen images to either rail edge: the rail follows the same pan
            // and zoom mapping as the bars, so an image enters only with its frame marker.
            if (MarkerX < -FrameBarChart::ScreenshotThumbnailWidth ||
                MarkerX > Width + FrameBarChart::ScreenshotThumbnailWidth)
            {
                continue;
            }

            const FSlateRect ThumbnailRect = GetScreenshotThumbnailRect(AllottedGeometry, MarkerIndex);
            if (ThumbnailRect.Right < 0.0f || ThumbnailRect.Left > Width || ThumbnailRect.Bottom < RailTop || ThumbnailRect.Top > Height) continue;

            const FVector2D ThumbnailSize(ThumbnailRect.Right - ThumbnailRect.Left, ThumbnailRect.Bottom - ThumbnailRect.Top);
            FSlateDrawElement::MakeBox(
                OutDrawElements, LayerId + 4,
                AllottedGeometry.ToPaintGeometry(ThumbnailSize, FSlateLayoutTransform(FVector2D(ThumbnailRect.Left, ThumbnailRect.Top))),
                Marker.ThumbnailBrush.IsValid() ? Marker.ThumbnailBrush.Get() : FAppStyle::GetBrush(TEXT("WhiteBrush")),
                ESlateDrawEffect::None, FLinearColor::White);

            TArray<FVector2D> BorderPoints;
            BorderPoints.Add(FVector2D(ThumbnailRect.Left, ThumbnailRect.Top));
            BorderPoints.Add(FVector2D(ThumbnailRect.Right, ThumbnailRect.Top));
            BorderPoints.Add(FVector2D(ThumbnailRect.Right, ThumbnailRect.Bottom));
            BorderPoints.Add(FVector2D(ThumbnailRect.Left, ThumbnailRect.Bottom));
            BorderPoints.Add(FVector2D(ThumbnailRect.Left, ThumbnailRect.Top));
            FSlateDrawElement::MakeLines(
                OutDrawElements, LayerId + 5, AllottedGeometry.ToPaintGeometry(), BorderPoints,
                ESlateDrawEffect::None, FrameBarChart::ColorScreenshotBorder(), true, 1.0f);
        }
    }

    if (_bHasSelection)
    {
        const float SelX1 = (static_cast<float>(_SelectionStart) - static_cast<float>(_ViewOffset)) * BarStep;
        const float SelX2 = (static_cast<float>(_SelectionEnd + 1) - static_cast<float>(_ViewOffset)) * BarStep;
        const float ClampedX1 = FMath::Max(0.0f, SelX1);
        const float ClampedX2 = FMath::Min(Width, SelX2);

        if (ClampedX2 > ClampedX1)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements, LayerId + 3,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(ClampedX2 - ClampedX1, Height),
                    FSlateLayoutTransform(FVector2D(ClampedX1, 0.0f))),
                FAppStyle::GetBrush(TEXT("WhiteBrush")),
                ESlateDrawEffect::None, FrameBarChart::ColorSelect());
        }
    }

    if (_HoveredFrame >= 0 && _HoveredFrame < _FrameDurationsMs.Num())
    {
        const float HoverX = (static_cast<float>(_HoveredFrame) - static_cast<float>(_ViewOffset)) * BarStep;
        if (HoverX >= 0.0f && HoverX + BarW <= Width)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements, LayerId + 4,
                AllottedGeometry.ToPaintGeometry(
                    FVector2D(BarW, Height),
                    FSlateLayoutTransform(FVector2D(HoverX, 0.0f))),
                FAppStyle::GetBrush(TEXT("WhiteBrush")),
                ESlateDrawEffect::None, FrameBarChart::ColorHover());
        }
    }

    return LayerId + 6;
}

// --------------------------------------------------------------------------------------------------------------------
// Mouse Interaction
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkFrameBarChart::
    OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> FReply
{
    if (_FrameDurationsMs.Num() == 0) return FReply::Unhandled();

    auto Self = SharedThis(const_cast<SCkFrameBarChart*>(this));

    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
        if (const FCk_FrameScreenshotMarker* ScreenshotMarker = FindScreenshotAt(MyGeometry, LocalPosition))
        {
            _OnScreenshotMarkerClicked.ExecuteIfBound(ScreenshotMarker->ScreenshotId, ScreenshotMarker->FrameIndex);
            return FReply::Handled().SetUserFocus(Self, EFocusCause::Mouse);
        }

        _bIsDragging = true;
        _DragStartFrame = XToFrame(MyGeometry, LocalPosition.X);
        return FReply::Handled().CaptureMouse(Self).SetUserFocus(Self, EFocusCause::Mouse);
    }

    if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton ||
        MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        _bIsPanning = true;
        _PanStartX = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X;
        _PanStartOffset = _ViewOffset;
        return FReply::Handled().CaptureMouse(Self).SetUserFocus(Self, EFocusCause::Mouse);
    }

    return FReply::Unhandled();
}

auto
    SCkFrameBarChart::
    OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> FReply
{
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && _bIsDragging)
    {
        _bIsDragging = false;

        const int64 EndFrame = XToFrame(MyGeometry, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X);
        const uint64 Start = static_cast<uint64>(FMath::Min(_DragStartFrame, EndFrame));
        const uint64 End = static_cast<uint64>(FMath::Max(_DragStartFrame, EndFrame));

        SetSelection(Start, End);
        _OnFrameSelectionChanged.ExecuteIfBound(Start, End);

        return FReply::Handled().ReleaseMouseCapture();
    }

    if ((MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton ||
         MouseEvent.GetEffectingButton() == EKeys::RightMouseButton) && _bIsPanning)
    {
        _bIsPanning = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    return FReply::Unhandled();
}

auto
    SCkFrameBarChart::
    OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> FReply
{
    const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    if (_bIsDragging)
    {
        const int64 CurrentFrame = XToFrame(MyGeometry, LocalPos.X);
        const uint64 Start = static_cast<uint64>(FMath::Min(_DragStartFrame, CurrentFrame));
        const uint64 End = static_cast<uint64>(FMath::Max(_DragStartFrame, CurrentFrame));
        SetSelection(Start, End);
        return FReply::Handled();
    }

    if (_bIsPanning)
    {
        const float BarStep = GetBarStep(MyGeometry);
        const float DeltaX = LocalPos.X - _PanStartX;
        _ViewOffset = _PanStartOffset - static_cast<double>(DeltaX) / static_cast<double>(BarStep);
        _ViewOffset = FMath::Clamp(_ViewOffset, 0.0,
            FMath::Max(0.0, static_cast<double>(_FrameDurationsMs.Num()) - 10.0));
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    const int64 NewHoveredFrame = XToFrame(MyGeometry, LocalPos.X);
    const_cast<SCkFrameBarChart*>(this)->_HoveredFrame = NewHoveredFrame;

    if (NewHoveredFrame >= 0 && NewHoveredFrame < _FrameDurationsMs.Num() && ck::IsValid(_TooltipTextBlock))
    {
        const double Ms = _FrameDurationsMs[NewHoveredFrame];
        const double BudgetDelta = Ms - _TargetFrameMs;

        FString BudgetText;
        if (BudgetDelta > 0.1)
        {
            BudgetText = FString::Printf(TEXT("  (+%.1fms over %.1fms budget)"), BudgetDelta, _TargetFrameMs);
        }
        else
        {
            BudgetText = TEXT("  (within budget)");
        }

        _TooltipTextBlock->SetText(FText::FromString(
            FString::Printf(TEXT("Frame #%s  —  %.2fms%s"),
                *FrameBarChart::FormatWithCommas(NewHoveredFrame), Ms, *BudgetText)));
    }

    Invalidate(EInvalidateWidgetReason::Paint);

    // Unhandled on hover-only: returning Handled can stop Slate routing OnMouseWheel here.
    return FReply::Unhandled();
}

auto
    SCkFrameBarChart::
    OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> FReply
{
    if (_FrameDurationsMs.Num() == 0) return FReply::Unhandled();

    const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    ApplyZoom(MyGeometry, LocalPos.X, MouseEvent.GetWheelDelta());
    return FReply::Handled();
}

auto
    SCkFrameBarChart::
    OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
    -> FReply
{
    if (_FrameDurationsMs.Num() == 0) return FReply::Unhandled();

    const FKey Key = InKeyEvent.GetKey();

    if (Key == EKeys::Add || Key == EKeys::Equals)
    {
        ApplyZoom(MyGeometry, MyGeometry.GetLocalSize().X * 0.5f, 1.0f);
        return FReply::Handled();
    }
    if (Key == EKeys::Subtract || Key == EKeys::Hyphen)
    {
        ApplyZoom(MyGeometry, MyGeometry.GetLocalSize().X * 0.5f, -1.0f);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

auto
    SCkFrameBarChart::
    OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    -> void
{
    SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);

    // Take focus on hover so scroll wheel zoom works without clicking first
    if (NOT HasKeyboardFocus())
    {
        FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::Mouse);
    }
}

auto
    SCkFrameBarChart::
    OnMouseLeave(const FPointerEvent& MouseEvent)
    -> void
{
    _HoveredFrame = -1;
    Invalidate(EInvalidateWidgetReason::Paint);
    SCompoundWidget::OnMouseLeave(MouseEvent);
}

// --------------------------------------------------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkFrameBarChart::
    XToFrame(const FGeometry& Geometry, float LocalX) const
    -> int64
{
    const float BarStep = GetBarStep(Geometry);
    const int64 Frame = static_cast<int64>(_ViewOffset + static_cast<double>(LocalX) / static_cast<double>(BarStep));
    return FMath::Clamp(Frame, static_cast<int64>(0), static_cast<int64>(_FrameDurationsMs.Num() - 1));
}

auto
    SCkFrameBarChart::
    FrameToX(const FGeometry& Geometry, int64 FrameIndex) const
    -> float
{
    const float BarStep = GetBarStep(Geometry);
    return (static_cast<float>(FrameIndex) - static_cast<float>(_ViewOffset)) * BarStep;
}

auto
    SCkFrameBarChart::
    GetScreenshotRailHeight(const FGeometry& Geometry) const
    -> float
{
    if (_ScreenshotMarkers.IsEmpty()) return 0.0f;

    int32 LaneCount = 0;
    for (int32 MarkerIndex = 0; MarkerIndex < _ScreenshotMarkers.Num(); ++MarkerIndex)
    {
        const int32 Lane = GetScreenshotThumbnailLane(Geometry, MarkerIndex);
        if (Lane != INDEX_NONE)
        {
            LaneCount = FMath::Max(LaneCount, Lane + 1);
        }
    }
    if (LaneCount == 0)
    {
        return 0.0f;
    }

    return FrameBarChart::ScreenshotRailTopPadding +
        static_cast<float>(LaneCount) *
            (FrameBarChart::ScreenshotThumbnailHeight + FrameBarChart::ScreenshotThumbnailGap) +
        FrameBarChart::BottomPadding;
}

auto
    SCkFrameBarChart::
    GetChartBottom(const FGeometry& Geometry) const
    -> float
{
    return Geometry.GetLocalSize().Y - FrameBarChart::BottomPadding - GetScreenshotRailHeight(Geometry);
}

auto
    SCkFrameBarChart::
    GetScreenshotThumbnailLane(const FGeometry& Geometry, int32 MarkerIndex) const
    -> int32
{
    check(MarkerIndex >= 0 && MarkerIndex < _ScreenshotMarkers.Num());

    const float Width = Geometry.GetLocalSize().X;
    auto LaneRightEdges = TArray<float, TInlineAllocator<8>>{};
    for (int32 CurrentIndex = 0; CurrentIndex <= MarkerIndex; ++CurrentIndex)
    {
        const float MarkerX = FrameToX(
            Geometry,
            static_cast<int64>(_ScreenshotMarkers[CurrentIndex].FrameIndex));
        if (MarkerX < -FrameBarChart::ScreenshotThumbnailWidth
            || MarkerX > Width + FrameBarChart::ScreenshotThumbnailWidth)
        {
            if (CurrentIndex == MarkerIndex)
            {
                return INDEX_NONE;
            }
            continue;
        }

        const float Left = FMath::Clamp(
            MarkerX - FrameBarChart::ScreenshotThumbnailWidth * 0.5f,
            0.0f,
            FMath::Max(0.0f, Width - FrameBarChart::ScreenshotThumbnailWidth));
        const float Right = Left + FrameBarChart::ScreenshotThumbnailWidth;

        int32 AssignedLane = INDEX_NONE;
        for (int32 CandidateLane = 0; CandidateLane < LaneRightEdges.Num(); ++CandidateLane)
        {
            if (Left >= LaneRightEdges[CandidateLane])
            {
                AssignedLane = CandidateLane;
                break;
            }
        }

        if (AssignedLane == INDEX_NONE)
        {
            AssignedLane = LaneRightEdges.Add(Right);
        }
        else
        {
            LaneRightEdges[AssignedLane] = Right;
        }

        if (CurrentIndex == MarkerIndex)
        {
            return AssignedLane;
        }
    }

    checkNoEntry();
    return 0;
}

auto
    SCkFrameBarChart::
    GetScreenshotThumbnailRect(const FGeometry& Geometry, int32 MarkerIndex) const
    -> FSlateRect
{
    check(MarkerIndex >= 0 && MarkerIndex < _ScreenshotMarkers.Num());

    const float Width = Geometry.GetLocalSize().X;
    const float ChartBottom = GetChartBottom(Geometry);
    const FCk_FrameScreenshotMarker& Marker = _ScreenshotMarkers[MarkerIndex];
    const float MarkerX = FrameToX(Geometry, static_cast<int64>(Marker.FrameIndex));
    const float Left = FMath::Clamp(
        MarkerX - FrameBarChart::ScreenshotThumbnailWidth * 0.5f,
        0.0f,
        FMath::Max(0.0f, Width - FrameBarChart::ScreenshotThumbnailWidth));
    const int32 Lane = GetScreenshotThumbnailLane(Geometry, MarkerIndex);
    check(Lane != INDEX_NONE);
    const float Top = ChartBottom + FrameBarChart::ScreenshotRailTopPadding +
        static_cast<float>(Lane) *
            (FrameBarChart::ScreenshotThumbnailHeight + FrameBarChart::ScreenshotThumbnailGap);

    return FSlateRect(
        Left,
        Top,
        Left + FrameBarChart::ScreenshotThumbnailWidth,
        Top + FrameBarChart::ScreenshotThumbnailHeight);
}

auto
    SCkFrameBarChart::
    FindScreenshotAt(const FGeometry& Geometry, const FVector2D& LocalPosition) const
    -> const FCk_FrameScreenshotMarker*
{
    if (_ScreenshotMarkers.IsEmpty()) return nullptr;

    const float ChartBottom = GetChartBottom(Geometry);
    const float MarkerHitHalfWidth = FrameBarChart::ScreenshotMarkerCapWidth * 0.5f;

    // Reverse traversal deliberately matches paint precedence: if overlapping frame markers are
    // visually stacked, the top-most stable marker is the one that receives the click.
    for (int32 MarkerIndex = _ScreenshotMarkers.Num() - 1; MarkerIndex >= 0; --MarkerIndex)
    {
        const FCk_FrameScreenshotMarker& Marker = _ScreenshotMarkers[MarkerIndex];
        const float MarkerX = Marker.FrameIndex < static_cast<uint64>(_FrameDurationsMs.Num())
            ? FrameToX(Geometry, static_cast<int64>(Marker.FrameIndex))
            : -FrameBarChart::ScreenshotThumbnailWidth - 1.0f;
        const float Width = Geometry.GetLocalSize().X;
        if (MarkerX >= -FrameBarChart::ScreenshotThumbnailWidth &&
            MarkerX <= Width + FrameBarChart::ScreenshotThumbnailWidth)
        {
            const FSlateRect ThumbnailRect = GetScreenshotThumbnailRect(Geometry, MarkerIndex);
            if (LocalPosition.X >= ThumbnailRect.Left && LocalPosition.X <= ThumbnailRect.Right &&
                LocalPosition.Y >= ThumbnailRect.Top && LocalPosition.Y <= ThumbnailRect.Bottom)
            {
                return &Marker;
            }
        }

        if (Marker.FrameIndex >= static_cast<uint64>(_FrameDurationsMs.Num())) continue;
        if (LocalPosition.X >= MarkerX - MarkerHitHalfWidth && LocalPosition.X <= MarkerX + MarkerHitHalfWidth &&
            LocalPosition.Y >= FrameBarChart::TopPadding && LocalPosition.Y <= ChartBottom)
        {
            return &Marker;
        }
    }

    return nullptr;
}

auto
    SCkFrameBarChart::
    GetBarColor(double DurationMs) const
    -> FLinearColor
{
    if (DurationMs > _TargetFrameMs * 2.0)
    {
        return FrameBarChart::ColorRed();
    }
    if (DurationMs > _TargetFrameMs)
    {
        return FrameBarChart::ColorYellow();
    }
    return FrameBarChart::ColorGreen();
}

auto
    SCkFrameBarChart::
    GetBarWidth(const FGeometry& Geometry) const
    -> float
{
    const float Width = static_cast<float>(1.0 / _FramesPerPixel);
    return FMath::Clamp(Width, FrameBarChart::MinBarWidth, FrameBarChart::MaxBarWidth);
}

auto
    SCkFrameBarChart::
    GetBarStep(const FGeometry& Geometry) const
    -> float
{
    const float BarWidth = GetBarWidth(Geometry);
    const float Spacing = (BarWidth >= 2.0f) ? 1.0f : 0.0f;
    return FMath::Max(BarWidth + Spacing, 0.5f); // minimum step to avoid division by zero
}

auto
    SCkFrameBarChart::
    ApplyZoom(const FGeometry& Geometry, float LocalX, float Delta)
    -> void
{
    const float BarStep = GetBarStep(Geometry);
    const double FrameUnderCursor = _ViewOffset + static_cast<double>(LocalX) / static_cast<double>(BarStep);

    const double ZoomFactor = (Delta > 0) ? 0.8 : 1.25;
    _FramesPerPixel = FMath::Clamp(
        _FramesPerPixel * ZoomFactor,
        FrameBarChart::MinFramesPerPixel,
        FrameBarChart::MaxFramesPerPixel);

    // Recalculate offset to keep the frame under cursor in place
    const float NewBarStep = GetBarStep(Geometry);
    _ViewOffset = FrameUnderCursor - static_cast<double>(LocalX) / static_cast<double>(NewBarStep);
    _ViewOffset = FMath::Max(0.0, _ViewOffset);

    Invalidate(EInvalidateWidgetReason::Paint);
}

// --------------------------------------------------------------------------------------------------------------------
