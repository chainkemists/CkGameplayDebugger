#include "CkSmDebugger/Window/SCkSmDebugger_Timeline.h"

#include "CkCore/Macros/CkMacros.h"

#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

// ====================================================================================================================
// Construction
// ====================================================================================================================

auto
    SCkSmDebugger_Timeline::
    Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkSmDebugger_ViewModel> InViewModel,
        UCkSmDebugGraph* InGraph)
    -> void
{
    _ViewModel = InViewModel;
    _Graph = InGraph;
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

// ====================================================================================================================
// View range + coordinate conversion
// ====================================================================================================================

auto
    SCkSmDebugger_Timeline::
    GetCurrentRunDuration() const
    -> double
{
    if (NOT _ViewModel.IsValid()) { return 0.0; }
    auto SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (NOT SmInfo) { return 0.0; }

    auto& ScrubState = _ViewModel->Get_ScrubState();
    auto& Run = (ScrubState.SelectedRunIndex < 0)
        ? SmInfo->CurrentRun
        : (ScrubState.SelectedRunIndex < SmInfo->CompletedRuns.Num()
            ? SmInfo->CompletedRuns[ScrubState.SelectedRunIndex]
            : SmInfo->CurrentRun);

    return Run.Duration;
}

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

    // TimelineScrollX is the absolute view-start offset.
    // In Live mode with ScrollX == 0, the view's right edge tracks "now".
    auto RunDuration = GetCurrentRunDuration();

    if (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Live)
    {
        OutStart = RunDuration - OutDuration + ScrubState.TimelineScrollX;
    }
    else
    {
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
    if (InViewDuration <= 0.0) { return 0.0f; }
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
    if (InWidth <= 0.0f) { return InViewStart; }
    return InViewStart + (static_cast<double>(InX) / InWidth) * InViewDuration;
}

// ====================================================================================================================
// Painting
// ====================================================================================================================

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
    FSlateDrawElement::MakeBox(
        InOutDrawElements, InLayerId,
        InAllottedGeometry.ToPaintGeometry(),
        FAppStyle::GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        FLinearColor(0.04f, 0.04f, 0.06f));

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
    PaintBusyFrames(InAllottedGeometry, InOutDrawElements, InLayerId + 3, Run, ViewStart, ViewDuration);
    PaintScrubCursor(InAllottedGeometry, InOutDrawElements, InLayerId + 4, ViewStart, ViewDuration);

    // Time labels — relative seconds
    {
        auto Size = InAllottedGeometry.GetLocalSize();
        auto Font = FCoreStyle::GetDefaultFontStyle("Mono", 7);
        auto TextColor = FLinearColor(0.4f, 0.4f, 0.45f);

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
            TextPos.X = FMath::Clamp(TextPos.X, 0.0, Size.X - TextSize.X);

            FSlateDrawElement::MakeText(
                InOutDrawElements, InLayerId + 5,
                InAllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPos)),
                Label, Font, ESlateDrawEffect::None, TextColor);
        }
    }

    return InLayerId + 6;
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
    auto NameDepth = _Graph ? _Graph->LayoutParams.NameDepth : 1;

    for (auto& Segment : InRun.Segments)
    {
        auto X0 = TimeToX(Segment.StartTime, InViewStart, InViewDuration, Size.X);
        auto X1 = TimeToX(Segment.EndTime, InViewStart, InViewDuration, Size.X);

        if (X1 < 0.0f || X0 > Size.X) { continue; }

        X0 = FMath::Max(X0, 0.0f);
        X1 = FMath::Min(X1, static_cast<float>(Size.X));

        auto Width = FMath::Max(X1 - X0, 1.0f);
        constexpr auto Gap = 1.0f;
        auto BodyWidth = FMath::Max(Width - Gap, 1.0f);

        FSlateDrawElement::MakeBox(
            InOutDrawElements, InLayerId,
            InGeometry.ToPaintGeometry(
                FVector2D(BodyWidth, SegmentHeight),
                FSlateLayoutTransform(FVector2D(X0, 0.0f))),
            FAppStyle::GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            Segment.Color);

        if (Width > 30.0f)
        {
            auto DisplayName = FCkSmLayoutParams::ComputeDisplayName(Segment.StateName, NameDepth);
            auto Font = FCoreStyle::GetDefaultFontStyle("Bold", 8);
            auto FontService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
            auto TextSize = FontService->Measure(DisplayName, Font);

            if (TextSize.X < BodyWidth - 4.0f)
            {
                auto TextPos = FVector2D(
                    X0 + (BodyWidth - TextSize.X) * 0.5f,
                    (SegmentHeight - TextSize.Y) * 0.5f);

                FSlateDrawElement::MakeText(
                    InOutDrawElements, InLayerId + 1,
                    InGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPos)),
                    DisplayName, Font, ESlateDrawEffect::None,
                    FLinearColor(0.95f, 0.95f, 0.95f));
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

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
        if (X < 0.0f || X > Size.X) { continue; }

        FSlateDrawElement::MakeLines(
            InOutDrawElements, InLayerId,
            InGeometry.ToPaintGeometry(),
            TArray<FVector2D>{ FVector2D(X, 0.0f), FVector2D(X, Size.Y) },
            ESlateDrawEffect::None,
            FLinearColor(0.9f, 0.7f, 0.1f, 0.6f),
            false, 1.0f);
    }
}

// --------------------------------------------------------------------------------------------------------------------

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
    auto CursorTime = 0.0;
    auto CursorColor = FLinearColor::White;

    if (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Live)
    {
        CursorTime = GetCurrentRunDuration();
        CursorColor = FLinearColor(0.2f, 0.8f, 0.2f);
    }
    else
    {
        CursorTime = _ViewModel->Get_ScrubState().ScrubTime;
        CursorColor = FLinearColor::White;
    }

    auto X = TimeToX(CursorTime, InViewStart, InViewDuration, Size.X);
    if (X < -5.0f || X > Size.X + 5.0f) { return; }
    X = FMath::Clamp(X, 0.0f, static_cast<float>(Size.X));

    FSlateDrawElement::MakeLines(
        InOutDrawElements, InLayerId,
        InGeometry.ToPaintGeometry(),
        TArray<FVector2D>{ FVector2D(X, 0.0f), FVector2D(X, Size.Y) },
        ESlateDrawEffect::None,
        CursorColor, false, 2.0f);

    constexpr auto TriSize = 5.0f;
    FSlateDrawElement::MakeBox(
        InOutDrawElements, InLayerId,
        InGeometry.ToPaintGeometry(
            FVector2D(TriSize * 2.0f, TriSize),
            FSlateLayoutTransform(FVector2D(X - TriSize, 0.0f))),
        FAppStyle::GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        CursorColor);
}

// ====================================================================================================================
// Input
// ====================================================================================================================

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

        // Clamp to valid run range
        auto RunDuration = GetCurrentRunDuration();
        ScrubTime = FMath::Clamp(ScrubTime, 0.0, RunDuration);

        auto NewScrubState = _ViewModel->Get_ScrubState();
        NewScrubState.ViewMode = ECkSmDebugger_ViewMode::Scrub;
        NewScrubState.ScrubTime = ScrubTime;
        // Reset scroll so view centers on the new scrub position
        NewScrubState.TimelineScrollX = 0.0f;
        _ViewModel->Set_ScrubState(NewScrubState);

        _IsScrubbing = true;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        auto LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        _PanStartX = LocalPos.X;
        _IsPanning = true;
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

    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && _IsPanning)
    {
        _IsPanning = false;
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

        // Clamp to valid run range
        auto RunDuration = GetCurrentRunDuration();
        ScrubTime = FMath::Clamp(ScrubTime, 0.0, RunDuration);

        auto NewScrubState = _ViewModel->Get_ScrubState();
        NewScrubState.ScrubTime = ScrubTime;
        _ViewModel->Set_ScrubState(NewScrubState);

        return FReply::Handled();
    }

    if (_IsPanning && _ViewModel.IsValid())
    {
        auto LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        auto DeltaX = LocalPos.X - _PanStartX;
        _PanStartX = LocalPos.X;

        auto Size = InGeometry.GetLocalSize();
        auto ViewDuration = _ViewModel->Get_ScrubState().TimelineViewDuration;
        auto TimeDelta = -(static_cast<double>(DeltaX) / Size.X) * ViewDuration;

        auto NewScrubState = _ViewModel->Get_ScrubState();
        NewScrubState.TimelineScrollX += static_cast<float>(TimeDelta);
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

    // Zoom centered on the mouse cursor position.
    // We compute the time under the cursor, change the duration, then adjust
    // ScrollX so that same time stays under the cursor in the new view.

    auto Size = InGeometry.GetLocalSize();
    auto LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    auto CursorFrac = static_cast<double>(LocalPos.X) / Size.X;

    auto ViewStart = 0.0;
    auto ViewDuration = 0.0;
    GetViewRange(ViewStart, ViewDuration);

    auto CursorTime = ViewStart + ViewDuration * CursorFrac;

    constexpr auto ZoomFactor = 0.15;
    auto Multiplier = (InMouseEvent.GetWheelDelta() > 0) ? (1.0 - ZoomFactor) : (1.0 + ZoomFactor);
    auto NewDuration = FMath::Clamp(
        _ViewModel->Get_ScrubState().TimelineViewDuration * Multiplier, 0.5, 300.0);

    // Derive the required ScrollX so CursorTime stays at CursorFrac.
    //
    // For Live mode:  ViewStart = RunDuration - Duration + ScrollX
    //   => want: RunDuration - NewDuration + NewScrollX = CursorTime - NewDuration * CursorFrac
    //   => NewScrollX = CursorTime - RunDuration + NewDuration * (1.0 - CursorFrac)
    //
    // For Scrub mode: ViewStart = ScrubTime - Duration * 0.5 + ScrollX
    //   => want: ScrubTime - NewDuration * 0.5 + NewScrollX = CursorTime - NewDuration * CursorFrac
    //   => NewScrollX = CursorTime - ScrubTime + NewDuration * (0.5 - CursorFrac)

    auto NewScrubState = _ViewModel->Get_ScrubState();
    NewScrubState.TimelineViewDuration = NewDuration;

    if (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Live)
    {
        auto RunDuration = GetCurrentRunDuration();
        NewScrubState.TimelineScrollX = static_cast<float>(
            CursorTime - RunDuration + NewDuration * (1.0 - CursorFrac));
    }
    else
    {
        NewScrubState.TimelineScrollX = static_cast<float>(
            CursorTime - NewScrubState.ScrubTime + NewDuration * (0.5 - CursorFrac));
    }

    _ViewModel->Set_ScrubState(NewScrubState);
    return FReply::Handled();
}

auto
    SCkSmDebugger_Timeline::
    OnKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent)
    -> FReply
{
    if (InKeyEvent.GetKey() == EKeys::F && _ViewModel.IsValid())
    {
        // F = focus: reset scroll so the view centers on the scrub cursor / "now"
        auto NewScrubState = _ViewModel->Get_ScrubState();
        NewScrubState.TimelineScrollX = 0.0f;
        _ViewModel->Set_ScrubState(NewScrubState);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------
