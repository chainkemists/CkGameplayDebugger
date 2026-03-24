#include "CkSmDebugger/Window/SCkSmDebugger_Timeline.h"

#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_Timeline::
    Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkSmDebugger_ViewModel> InViewModel)
    -> void
{
    _ViewModel = InViewModel;
    _DesiredHeight = InArgs._DesiredHeight;
}

auto
    SCkSmDebugger_Timeline::
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const
    -> FVector2D
{
    return FVector2D(100.0f, _DesiredHeight);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_Timeline::
    GetViewRange(
        double& OutStart,
        double& OutDuration) const
    -> void
{
    if (NOT _ViewModel.IsValid())
    {
        OutStart = 0.0;
        OutDuration = 10.0;
        return;
    }

    auto& ScrubState = _ViewModel->Get_ScrubState();
    OutDuration = ScrubState.TimelineViewDuration;

    auto SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (NOT SmInfo)
    {
        OutStart = 0.0;
        return;
    }

    auto& Run = (_ViewModel->Get_ScrubState().SelectedRunIndex < 0)
        ? SmInfo->CurrentRun
        : (ScrubState.SelectedRunIndex < SmInfo->CompletedRuns.Num()
            ? SmInfo->CompletedRuns[ScrubState.SelectedRunIndex]
            : SmInfo->CurrentRun);

    if (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Live)
    {
        // Segments use relative time (0 to Duration), so view range must also be relative
        auto Now = Run.Duration;
        OutStart = Now - OutDuration + ScrubState.TimelineScrollX;
    }
    else
    {
        // ScrubTime is also relative to run start
        OutStart = ScrubState.ScrubTime - OutDuration * 0.5 + ScrubState.TimelineScrollX;
    }
}

auto
    SCkSmDebugger_Timeline::
    TimeToX(
        double InTime,
        double InViewStart,
        double InViewDuration,
        float InWidth) const
    -> float
{
    if (InViewDuration <= 0.0)
    { return 0.0f; }

    return static_cast<float>((InTime - InViewStart) / InViewDuration) * InWidth;
}

auto
    SCkSmDebugger_Timeline::
    XToTime(
        float InX,
        double InViewStart,
        double InViewDuration,
        float InWidth) const
    -> double
{
    if (InWidth <= 0.0f)
    { return InViewStart; }

    return InViewStart + (static_cast<double>(InX) / InWidth) * InViewDuration;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_Timeline::
    OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InMyCullingRect,
        FSlateWindowElementList& InOutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InbParentEnabled) const
    -> int32
{
    // Background
    FSlateDrawElement::MakeBox(
        InOutDrawElements,
        InLayerId,
        InAllottedGeometry.ToPaintGeometry(),
        FAppStyle::GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        FLinearColor(0.01f, 0.01f, 0.01f));

    if (NOT _ViewModel.IsValid())
    { return InLayerId + 1; }

    auto SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (NOT SmInfo)
    { return InLayerId + 1; }

    auto& ScrubState = _ViewModel->Get_ScrubState();
    auto RunIndex = ScrubState.SelectedRunIndex;

    auto& Run = (RunIndex < 0)
        ? SmInfo->CurrentRun
        : (RunIndex < SmInfo->CompletedRuns.Num()
            ? SmInfo->CompletedRuns[RunIndex]
            : SmInfo->CurrentRun);

    auto ViewStart = 0.0;
    auto ViewDuration = 10.0;
    GetViewRange(ViewStart, ViewDuration);

    PaintSegments(InAllottedGeometry, InOutDrawElements, InLayerId + 1, Run, ViewStart, ViewDuration);
    PaintBusyFrames(InAllottedGeometry, InOutDrawElements, InLayerId + 2, Run, ViewStart, ViewDuration);

    if (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Scrub)
    {
        PaintScrubCursor(InAllottedGeometry, InOutDrawElements, InLayerId + 3, ViewStart, ViewDuration);
    }

    // Time labels
    {
        auto Size = InAllottedGeometry.GetLocalSize();
        auto Font = FCoreStyle::GetDefaultFontStyle("Mono", 7);
        auto TextColor = FLinearColor(0.35f, 0.35f, 0.4f);

        constexpr auto LabelCount = 5;
        for (auto i = 0; i <= LabelCount; ++i)
        {
            auto Frac = static_cast<float>(i) / LabelCount;
            auto Time = ViewStart + ViewDuration * Frac;
            auto X = Frac * Size.X;

            auto Label = FString::Printf(TEXT("%.1fs"), Time);
            auto FontService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            auto TextSize = FontService->Measure(Label, Font);

            auto TextPos = FVector2D(X - TextSize.X * 0.5f, Size.Y - TextSize.Y - 2.0f);
            TextPos.X = FMath::Clamp(TextPos.X, 0.0f, Size.X - TextSize.X);

            FSlateDrawElement::MakeText(
                InOutDrawElements,
                InLayerId + 4,
                InAllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPos)),
                Label,
                Font,
                ESlateDrawEffect::None,
                TextColor);
        }
    }

    return InLayerId + 5;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_Timeline::
    PaintSegments(
        const FGeometry& InGeometry,
        FSlateWindowElementList& InOutDrawElements,
        int32 InLayerId,
        const FCkSmDebugger_RunInfo& InRun,
        double InViewStart,
        double InViewDuration) const
    -> void
{
    auto Size = InGeometry.GetLocalSize();
    auto SegmentHeight = Size.Y * 0.6f;

    for (auto& Segment : InRun.Segments)
    {
        auto X0 = TimeToX(Segment.StartTime, InViewStart, InViewDuration, Size.X);
        auto X1 = TimeToX(Segment.EndTime, InViewStart, InViewDuration, Size.X);

        if (X1 < 0.0f || X0 > Size.X)
        { continue; }

        X0 = FMath::Max(X0, 0.0f);
        X1 = FMath::Min(X1, Size.X);

        auto Width = FMath::Max(X1 - X0, 1.0f);

        FSlateDrawElement::MakeBox(
            InOutDrawElements,
            InLayerId,
            InGeometry.ToPaintGeometry(
                FVector2D(Width, SegmentHeight),
                FSlateLayoutTransform(FVector2D(X0, 0.0f))),
            FAppStyle::GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            Segment.Color);

        // State name label if wide enough
        if (Width > 40.0f)
        {
            auto Font = FCoreStyle::GetDefaultFontStyle("Regular", 7);
            auto FontService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            auto TextSize = FontService->Measure(Segment.StateName, Font);

            if (TextSize.X < Width - 4.0f)
            {
                auto TextPos = FVector2D(
                    X0 + (Width - TextSize.X) * 0.5f,
                    (SegmentHeight - TextSize.Y) * 0.5f);

                FSlateDrawElement::MakeText(
                    InOutDrawElements,
                    InLayerId + 1,
                    InGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPos)),
                    Segment.StateName,
                    Font,
                    ESlateDrawEffect::None,
                    FLinearColor(0.85f, 0.85f, 0.85f));
            }
        }
    }
}

auto
    SCkSmDebugger_Timeline::
    PaintBusyFrames(
        const FGeometry& InGeometry,
        FSlateWindowElementList& InOutDrawElements,
        int32 InLayerId,
        const FCkSmDebugger_RunInfo& InRun,
        double InViewStart,
        double InViewDuration) const
    -> void
{
    auto Size = InGeometry.GetLocalSize();

    for (auto& BusyFrame : InRun.BusyFrames)
    {
        auto X = TimeToX(BusyFrame.Time, InViewStart, InViewDuration, Size.X);
        if (X < 0.0f || X > Size.X)
        { continue; }

        TArray<FVector2D> Points;
        Points.Add(FVector2D(X, 0.0f));
        Points.Add(FVector2D(X, Size.Y));

        FSlateDrawElement::MakeLines(
            InOutDrawElements,
            InLayerId,
            InGeometry.ToPaintGeometry(),
            Points,
            ESlateDrawEffect::None,
            FLinearColor(0.9f, 0.7f, 0.1f),
            false,
            1.0f);
    }
}

auto
    SCkSmDebugger_Timeline::
    PaintScrubCursor(
        const FGeometry& InGeometry,
        FSlateWindowElementList& InOutDrawElements,
        int32 InLayerId,
        double InViewStart,
        double InViewDuration) const
    -> void
{
    auto Size = InGeometry.GetLocalSize();
    auto ScrubTime = _ViewModel->Get_ScrubState().ScrubTime;
    auto X = TimeToX(ScrubTime, InViewStart, InViewDuration, Size.X);

    if (X < 0.0f || X > Size.X)
    { return; }

    TArray<FVector2D> Points;
    Points.Add(FVector2D(X, 0.0f));
    Points.Add(FVector2D(X, Size.Y));

    FSlateDrawElement::MakeLines(
        InOutDrawElements,
        InLayerId,
        InGeometry.ToPaintGeometry(),
        Points,
        ESlateDrawEffect::None,
        FLinearColor::White,
        false,
        2.0f);

    // Small triangle indicator at top
    auto TriSize = 5.0f;
    FSlateDrawElement::MakeBox(
        InOutDrawElements,
        InLayerId,
        InGeometry.ToPaintGeometry(
            FVector2D(TriSize * 2.0f, TriSize),
            FSlateLayoutTransform(FVector2D(X - TriSize, 0.0f))),
        FAppStyle::GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        FLinearColor::White);
}

// --------------------------------------------------------------------------------------------------------------------
// Input
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_Timeline::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && _ViewModel.IsValid())
    {
        _ViewModel->Set_ViewMode(ECkSmDebugger_ViewMode::Scrub);

        auto Size = InGeometry.GetLocalSize();
        auto LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

        auto ViewStart = 0.0;
        auto ViewDuration = 10.0;
        GetViewRange(ViewStart, ViewDuration);

        auto ScrubTime = XToTime(LocalPos.X, ViewStart, ViewDuration, Size.X);

        auto NewScrubState = _ViewModel->Get_ScrubState();
        NewScrubState.ViewMode = ECkSmDebugger_ViewMode::Scrub;
        NewScrubState.ScrubTime = ScrubTime;
        _ViewModel->Set_ScrubState(NewScrubState);

        _IsScrubbing = true;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    return FReply::Unhandled();
}

auto
    SCkSmDebugger_Timeline::
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

    return FReply::Unhandled();
}

auto
    SCkSmDebugger_Timeline::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (_IsScrubbing && _ViewModel.IsValid())
    {
        auto Size = InGeometry.GetLocalSize();
        auto LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

        auto ViewStart = 0.0;
        auto ViewDuration = 10.0;
        GetViewRange(ViewStart, ViewDuration);

        auto ScrubTime = XToTime(LocalPos.X, ViewStart, ViewDuration, Size.X);

        auto NewScrubState = _ViewModel->Get_ScrubState();
        NewScrubState.ScrubTime = ScrubTime;
        _ViewModel->Set_ScrubState(NewScrubState);

        return FReply::Handled();
    }

    return FReply::Unhandled();
}

auto
    SCkSmDebugger_Timeline::
    OnMouseWheel(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (NOT _ViewModel.IsValid())
    { return FReply::Unhandled(); }

    auto NewScrubState = _ViewModel->Get_ScrubState();

    if (InMouseEvent.IsControlDown())
    {
        // Zoom timeline
        constexpr auto ZoomFactor = 0.15;
        auto Multiplier = (InMouseEvent.GetWheelDelta() > 0) ? (1.0 - ZoomFactor) : (1.0 + ZoomFactor);
        NewScrubState.TimelineViewDuration = FMath::Clamp(
            NewScrubState.TimelineViewDuration * Multiplier, 1.0, 120.0);
    }
    else
    {
        // Scroll timeline
        constexpr auto ScrollStep = 0.5f;
        NewScrubState.TimelineScrollX -= InMouseEvent.GetWheelDelta() * ScrollStep;
    }

    _ViewModel->Set_ScrubState(NewScrubState);
    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------
