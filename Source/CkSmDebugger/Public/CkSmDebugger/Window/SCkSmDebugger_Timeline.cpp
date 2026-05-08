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

    // While the user is actively dragging the scrub needle, freeze the view origin.
    // Otherwise the auto-recenter on ScrubTime makes segments slide opposite to the drag,
    // which feels like the timeline is inverted.
    if (_ScrubAnchorViewStart.IsSet())
    {
        OutStart = _ScrubAnchorViewStart.GetValue();
        return;
    }

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

auto
    SCkSmDebugger_Timeline::
    TimeToFrame(
        const FCkSmDebugger_RunInfo& InRun,
        double InRunRelativeTime) const
    -> int64
{
    if (InRun.FrameSegments.IsEmpty())
    { return 0; }

    auto& First = InRun.FrameSegments[0];
    auto& Last = InRun.FrameSegments.Last();

    if (InRunRelativeTime <= First.StartTime)
    { return static_cast<int64>(First.StartFrame); }

    if (InRunRelativeTime >= Last.EndTime)
    { return static_cast<int64>(Last.EndFrame); }

    for (auto& Seg : InRun.FrameSegments)
    {
        if (InRunRelativeTime >= Seg.StartTime && InRunRelativeTime <= Seg.EndTime)
        {
            auto Span = Seg.EndTime - Seg.StartTime;
            if (Span <= 0.0)
            { return static_cast<int64>(Seg.StartFrame); }

            auto Frac = (InRunRelativeTime - Seg.StartTime) / Span;
            auto FrameSpan = static_cast<int64>(Seg.EndFrame) - static_cast<int64>(Seg.StartFrame);
            // Floor (not round) so the reported frame matches the cell visually under the
            // needle: cell N covers [N/total, (N+1)/total), and Floor maps any time in that
            // range to N. Round would tick to N+1 at the cell midpoint, ahead of the needle.
            return static_cast<int64>(Seg.StartFrame) + FMath::FloorToInt64(Frac * FrameSpan);
        }
    }

    return static_cast<int64>(Last.EndFrame);
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

    auto& SourceRun = (RunIndex < 0)
        ? SmInfo->CurrentRun
        : (RunIndex < SmInfo->CompletedRuns.Num()
            ? SmInfo->CompletedRuns[RunIndex]
            : SmInfo->CurrentRun);

    // While in Scrub mode on the live (current) run, override the last segment's End
    // with the values captured when scrubbing began. This stops per-frame cells from
    // shrinking as real time advances during scrub. The override only applies if the
    // last segment's state matches the captured state — if the SM has transitioned
    // since scrub start, the new live segment renders normally.
    auto FrozenRun = FCkSmDebugger_RunInfo{};
    auto UseFrozenRun = false;
    if (RunIndex < 0 && _ViewModel->Get_LiveSegmentFreezeActive())
    {
        auto& Segs = SourceRun.Segments;
        if (Segs.Num() > 0 && Segs.Last().StateName == _ViewModel->Get_LiveSegmentFreezeStateName())
        {
            FrozenRun = SourceRun;
            auto& LastSeg = FrozenRun.Segments.Last();
            LastSeg.EndTime = _ViewModel->Get_LiveSegmentFreezeEndTime();
            LastSeg.EndFrame = _ViewModel->Get_LiveSegmentFreezeEndFrame();
            UseFrozenRun = true;
        }
    }
    auto& Run = UseFrozenRun ? FrozenRun : SourceRun;

    auto ViewStart = 0.0;
    auto ViewDuration = 10.0;
    GetViewRange(ViewStart, ViewDuration);

    PaintSegments(InAllottedGeometry, InOutDrawElements, InLayerId + 1, Run, ViewStart, ViewDuration);
    PaintScrubFrameHighlight(InAllottedGeometry, InOutDrawElements, InLayerId + 2, Run, ViewStart, ViewDuration);
    PaintBusyFrames(InAllottedGeometry, InOutDrawElements, InLayerId + 3, Run, ViewStart, ViewDuration);
    PaintPauseMarkers(InAllottedGeometry, InOutDrawElements, InLayerId + 4, Run, ViewStart, ViewDuration);
    PaintScrubCursor(InAllottedGeometry, InOutDrawElements, InLayerId + 5, ViewStart, ViewDuration);

    // Time / frame labels along the bottom edge with ruler ticks above them.
    // Major ticks at each label, minor ticks (4 between each major) for finer
    // visual subdivisions — same convention as a hardware ruler.
    {
        auto Size = InAllottedGeometry.GetLocalSize();
        auto Font = FCoreStyle::GetDefaultFontStyle("Mono", 7);
        auto TextColor = FLinearColor(0.4f, 0.4f, 0.45f);
        auto MajorTickColor = FLinearColor(0.55f, 0.55f, 0.6f);
        auto MinorTickColor = FLinearColor(0.32f, 0.32f, 0.36f);
        auto SegmentHeight = Size.Y * 0.5f;
        auto ShowFrames = ScrubState.ShowFramesOnTimeline;
        auto Brush = FAppStyle::GetBrush("WhiteBrush");

        // Layout (DaVinci-style — ruler band between cells and labels):
        // [cells] [busy underline] [ruler band] [label text]
        constexpr auto LabelCount = 5;
        constexpr auto MinorPerMajor = 5;        // 5 minor segments per major (4 minor ticks between)
        constexpr auto MajorTickHeight = 7.0f;
        constexpr auto MinorTickHeight = 4.0f;
        constexpr auto TickWidth = 1.0f;
        const auto TickBaseY = SegmentHeight + 5.0f;

        // Minor ticks: between every consecutive pair of major positions.
        for (auto i = 0; i < LabelCount * MinorPerMajor; ++i)
        {
            if (i % MinorPerMajor == 0) { continue; } // skip positions where major ticks live
            auto Frac = static_cast<float>(i) / (LabelCount * MinorPerMajor);
            auto X = Frac * Size.X;
            FSlateDrawElement::MakeBox(
                InOutDrawElements, InLayerId + 4,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2D(TickWidth, MinorTickHeight),
                    FSlateLayoutTransform(FVector2D(X - TickWidth * 0.5f, TickBaseY))),
                Brush, ESlateDrawEffect::None, MinorTickColor);
        }

        // Major ticks + their labels.
        for (auto i = 0; i <= LabelCount; ++i)
        {
            auto Frac = static_cast<float>(i) / LabelCount;
            auto Time = ViewStart + ViewDuration * Frac;
            auto X = Frac * Size.X;

            // Major tick mark above the label
            FSlateDrawElement::MakeBox(
                InOutDrawElements, InLayerId + 4,
                InAllottedGeometry.ToPaintGeometry(
                    FVector2D(TickWidth, MajorTickHeight),
                    FSlateLayoutTransform(FVector2D(X - TickWidth * 0.5f, TickBaseY))),
                Brush, ESlateDrawEffect::None, MajorTickColor);

            // Show absolute frame plus a [mod 1000] tail so the eye can easily compare
            // nearby labels even when the absolute number runs into the millions. The
            // raw frame is kept so the value is still useful to copy/grep against logs.
            auto Frame = TimeToFrame(Run, Time);
            auto Label = ShowFrames
                ? FString::Printf(TEXT("f%lld [%03lld]"), Frame, Frame % 1000)
                : FString::Printf(TEXT("%.1fs"), Time);
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
    auto SegmentHeight = Size.Y * 0.5f;
    auto NameDepth = _Graph ? _Graph->LayoutParams.NameDepth : 1;
    auto Brush = FAppStyle::GetBrush("WhiteBrush");

    for (auto& Segment : InRun.Segments)
    {
        auto X0Full = TimeToX(Segment.StartTime, InViewStart, InViewDuration, Size.X);
        auto X1Full = TimeToX(Segment.EndTime, InViewStart, InViewDuration, Size.X);

        if (X1Full < 0.0f || X0Full > Size.X) { continue; }

        auto X0 = FMath::Max(X0Full, 0.0f);
        auto X1 = FMath::Min(X1Full, static_cast<float>(Size.X));

        auto Width = FMath::Max(X1 - X0, 1.0f);
        constexpr auto Gap = 1.0f;
        auto BodyWidth = FMath::Max(Width - Gap, 1.0f);

        // Always draw the segment as a solid colored block. Per-frame structure is then
        // overlaid as thin darker-shade separator lines at each frame boundary — this
        // keeps the segment a coherent block of color so labels read cleanly, while
        // still showing where individual frames begin and end.
        FSlateDrawElement::MakeBox(
            InOutDrawElements, InLayerId,
            InGeometry.ToPaintGeometry(
                FVector2D(BodyWidth, SegmentHeight),
                FSlateLayoutTransform(FVector2D(X0, 0.0f))),
            Brush, ESlateDrawEffect::None, Segment.Color);

        auto FrameCount = static_cast<int64>(FMath::Max<int64>(
            static_cast<int64>(Segment.EndFrame) - static_cast<int64>(Segment.StartFrame), 1));
        auto FullWidth = FMath::Max(X1Full - X0Full, 1.0f);
        auto PxPerFrame = FullWidth / static_cast<float>(FrameCount);
        auto DrawFrameLines = FrameCount > 1 && PxPerFrame >= 2.0f;

        if (DrawFrameLines)
        {
            // Darker variant of the segment color — visible against the segment but soft
            // enough not to compete with the centered label text.
            auto LineColor = Segment.Color * 0.55f;
            LineColor.A = 1.0f;

            for (auto F = int64{1}; F < FrameCount; ++F)
            {
                auto BoundaryTime = Segment.StartTime + (Segment.EndTime - Segment.StartTime) * (static_cast<double>(F) / static_cast<double>(FrameCount));
                auto LineX = TimeToX(BoundaryTime, InViewStart, InViewDuration, Size.X);
                if (LineX < 0.0f || LineX > Size.X) { continue; }

                FSlateDrawElement::MakeBox(
                    InOutDrawElements, InLayerId,
                    InGeometry.ToPaintGeometry(
                        FVector2D(1.0f, SegmentHeight),
                        FSlateLayoutTransform(FVector2D(LineX, 0.0f))),
                    Brush, ESlateDrawEffect::None, LineColor);
            }
        }

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

                // Luminance-based label color: hash-derived segment colors land on bright
                // yellows/cyans where white is unreadable. Pick black or white based on the
                // segment color's perceived brightness.
                auto Lum = 0.299f * Segment.Color.R + 0.587f * Segment.Color.G + 0.114f * Segment.Color.B;
                auto LabelColor = (Lum > 0.55f)
                    ? FLinearColor(0.05f, 0.05f, 0.05f)
                    : FLinearColor(0.95f, 0.95f, 0.95f);

                FSlateDrawElement::MakeText(
                    InOutDrawElements, InLayerId + 1,
                    InGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPos)),
                    DisplayName, Font, ESlateDrawEffect::None,
                    LabelColor);
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
    auto SegmentHeight = Size.Y * 0.5f;

    // Render the busy-frame indicator as a small underline that covers exactly the
    // cells living in the same engine frame. Each busy frame produces:
    //   • N-1 zero-frame-wide cells (the intermediate state slivers) stacked at X
    //   • the first cell of the post-busy state (StartFrame == busy frame number)
    // To size the underline correctly we need "one engine frame's pixel width" —
    // derived from the segment whose StartFrame matches the busy frame, since
    // BusyFrame.EndTime measures the gap to the *next non-busy* transition (which
    // could be many frames away).
    constexpr auto MarkThicknessPx = 2.0f;
    constexpr auto MinMarkWidthPx = 4.0f;
    auto Brush = FAppStyle::GetBrush("WhiteBrush");
    for (auto& BusyFrame : InRun.BusyFrames)
    {
        auto X = TimeToX(BusyFrame.Time, InViewStart, InViewDuration, Size.X);

        // Find the segment with StartFrame == busy frame's FrameNumber — that's the
        // post-busy state (or the last sliver of a zero-time state). Its PxPerFrame
        // is "what 1 engine frame looks like" right here.
        auto PxPerFrame = MinMarkWidthPx;
        for (auto& Seg : InRun.Segments)
        {
            if (Seg.StartFrame == BusyFrame.FrameNumber && Seg.EndFrame > Seg.StartFrame)
            {
                auto SegX0 = TimeToX(Seg.StartTime, InViewStart, InViewDuration, Size.X);
                auto SegX1 = TimeToX(Seg.EndTime, InViewStart, InViewDuration, Size.X);
                auto FrameCount = static_cast<float>(Seg.EndFrame - Seg.StartFrame);
                if (FrameCount > 0.0f)
                { PxPerFrame = FMath::Max((SegX1 - SegX0) / FrameCount, MinMarkWidthPx); }
                break;
            }
        }

        // Cluster width = stacked thin cells (~1.5px each) + 1 frame of new state.
        auto ThinCellsWidth = static_cast<float>(BusyFrame.TransitionCount - 1) * 1.5f;
        auto Width = FMath::Max(PxPerFrame + ThinCellsWidth, MinMarkWidthPx);

        if (X + Width < 0.0f || X > Size.X) { continue; }
        auto LeftEdge = FMath::Max(X, 0.0f);
        auto Right = FMath::Min(X + Width, static_cast<float>(Size.X));
        auto Visible = FMath::Max(Right - LeftEdge, 1.0f);

        FSlateDrawElement::MakeBox(
            InOutDrawElements, InLayerId,
            InGeometry.ToPaintGeometry(
                FVector2D(Visible, MarkThicknessPx),
                FSlateLayoutTransform(FVector2D(LeftEdge, SegmentHeight + 1.0f))),
            Brush, ESlateDrawEffect::None,
            FLinearColor(0.95f, 0.55f, 0.1f, 0.95f));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_Timeline::
    PaintScrubFrameHighlight(
        const FGeometry& InGeometry,
        FSlateWindowElementList& InOutDrawElements,
        int32 InLayerId,
        const FCkSmDebugger_RunInfo& InRun,
        double InViewStart,
        double InViewDuration) const
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }
    if (_ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Live) { return; }

    auto Size = InGeometry.GetLocalSize();
    auto SegmentHeight = Size.Y * 0.5f;
    auto ScrubTime = _ViewModel->Get_ScrubState().ScrubTime;

    // Find the containing state segment.
    auto Containing = static_cast<const FCkSmDebugger_TimelineSegment*>(nullptr);
    for (auto& Seg : InRun.Segments)
    {
        if (ScrubTime >= Seg.StartTime && ScrubTime <= Seg.EndTime)
        {
            Containing = &Seg;
            break;
        }
    }
    if (NOT Containing) { return; }

    auto FrameCount = static_cast<int64>(FMath::Max<int64>(
        static_cast<int64>(Containing->EndFrame) - static_cast<int64>(Containing->StartFrame), 1));

    auto SegFullX0 = TimeToX(Containing->StartTime, InViewStart, InViewDuration, Size.X);
    auto SegFullX1 = TimeToX(Containing->EndTime, InViewStart, InViewDuration, Size.X);
    auto SegFullWidth = FMath::Max(SegFullX1 - SegFullX0, 1.0f);
    auto PxPerFrame = SegFullWidth / static_cast<float>(FrameCount);

    // Only highlight when slicing is visible — otherwise the cursor itself is enough.
    if (FrameCount <= 1 || PxPerFrame < 2.0f) { return; }

    // Use floor (not round) so the highlighted cell is the one VISUALLY CONTAINING the
    // needle position. Round would flip to the next cell at half-cell, which feels like
    // the highlight is leading the needle.
    auto SegSpan = Containing->EndTime - Containing->StartTime;
    auto Frac = (SegSpan > 0.0) ? (ScrubTime - Containing->StartTime) / SegSpan : 0.0;
    auto LocalIndex = FMath::Clamp(static_cast<int64>(FMath::FloorToInt64(Frac * FrameCount)), int64{0}, FrameCount - 1);

    auto CellStartTime = Containing->StartTime + (Containing->EndTime - Containing->StartTime) * (static_cast<double>(LocalIndex) / static_cast<double>(FrameCount));
    auto CellEndTime = Containing->StartTime + (Containing->EndTime - Containing->StartTime) * (static_cast<double>(LocalIndex + 1) / static_cast<double>(FrameCount));

    auto X0 = TimeToX(CellStartTime, InViewStart, InViewDuration, Size.X);
    auto X1 = TimeToX(CellEndTime, InViewStart, InViewDuration, Size.X);
    if (X1 < 0.0f || X0 > Size.X) { return; }
    X0 = FMath::Max(X0, 0.0f);
    X1 = FMath::Min(X1, static_cast<float>(Size.X));

    constexpr auto Gap = 1.0f;
    auto CellWidth = FMath::Max(X1 - X0 - Gap, 1.0f);

    // Brightened version of the segment color.
    auto Hi = Containing->Color * 1.6f;
    Hi.A = 1.0f;
    Hi.R = FMath::Min(Hi.R, 1.0f);
    Hi.G = FMath::Min(Hi.G, 1.0f);
    Hi.B = FMath::Min(Hi.B, 1.0f);

    FSlateDrawElement::MakeBox(
        InOutDrawElements, InLayerId,
        InGeometry.ToPaintGeometry(
            FVector2D(CellWidth, SegmentHeight),
            FSlateLayoutTransform(FVector2D(X0, 0.0f))),
        FAppStyle::GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        Hi);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_Timeline::
    PaintPauseMarkers(
        const FGeometry& InGeometry,
        FSlateWindowElementList& InOutDrawElements,
        int32 InLayerId,
        const FCkSmDebugger_RunInfo& InRun,
        double InViewStart,
        double InViewDuration) const
    -> void
{
    if (InRun.PauseMarkers.IsEmpty()) { return; }

    auto Size = InGeometry.GetLocalSize();
    auto SegmentHeight = Size.Y * 0.5f;
    auto Brush = FAppStyle::GetBrush("WhiteBrush");

    // Cut effect: two thin vertical segments slightly offset horizontally — top half
    // shifted left, bottom half shifted right — meeting at the segment's vertical
    // midpoint. Reads as a scissor snip across the timeline at this point. Breakpoint
    // pauses get an amber tint, manual pauses a softer cyan.
    constexpr auto Offset = 1.5f;
    constexpr auto Thickness = 2.0f;
    for (auto& Marker : InRun.PauseMarkers)
    {
        auto X = TimeToX(Marker.Time, InViewStart, InViewDuration, Size.X);
        if (X < -Offset - Thickness || X > Size.X + Offset + Thickness) { continue; }

        auto MarkerColor = Marker.IsBreakpoint
            ? FLinearColor(1.0f, 0.6f, 0.0f)        // amber for breakpoint
            : FLinearColor(0.4f, 0.85f, 1.0f);      // cyan for manual pause

        auto Mid = SegmentHeight * 0.5f;

        // Top half — shifted left of center
        FSlateDrawElement::MakeBox(
            InOutDrawElements, InLayerId,
            InGeometry.ToPaintGeometry(
                FVector2D(Thickness, Mid),
                FSlateLayoutTransform(FVector2D(X - Offset - Thickness * 0.5f, 0.0f))),
            Brush, ESlateDrawEffect::None, MarkerColor);

        // Bottom half — shifted right of center
        FSlateDrawElement::MakeBox(
            InOutDrawElements, InLayerId,
            InGeometry.ToPaintGeometry(
                FVector2D(Thickness, SegmentHeight - Mid),
                FSlateLayoutTransform(FVector2D(X + Offset - Thickness * 0.5f, Mid))),
            Brush, ESlateDrawEffect::None, MarkerColor);
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
        auto Size = InGeometry.GetLocalSize();
        auto LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

        // Snapshot the view origin BEFORE switching modes — this is what we'll anchor to.
        // Without an anchor, switching to Scrub mode recenters the view on ScrubTime every
        // paint, which makes segments slide opposite to the drag direction.
        auto ViewStart = 0.0;
        auto ViewDuration = 10.0;
        GetViewRange(ViewStart, ViewDuration);

        auto ScrubTime = XToTime(LocalPos.X, ViewStart, ViewDuration, Size.X);

        auto RunDuration = GetCurrentRunDuration();
        ScrubTime = FMath::Clamp(ScrubTime, 0.0, RunDuration);

        _ViewModel->Set_ViewMode(ECkSmDebugger_ViewMode::Scrub);

        auto NewScrubState = _ViewModel->Get_ScrubState();
        NewScrubState.ViewMode = ECkSmDebugger_ViewMode::Scrub;
        NewScrubState.ScrubTime = ScrubTime;
        _ViewModel->Set_ScrubState(NewScrubState);

        _ScrubAnchorViewStart = ViewStart;
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
        // Transfer the frozen view origin into TimelineScrollX so the view stays where the
        // user left it once the anchor is cleared. In Scrub mode the natural origin is
        // ScrubTime - Duration*0.5; the difference becomes the persisted scroll offset.
        if (_ScrubAnchorViewStart.IsSet() && _ViewModel.IsValid())
        {
            auto& ScrubState = _ViewModel->Get_ScrubState();
            auto NewScrubState = ScrubState;
            NewScrubState.TimelineScrollX = static_cast<float>(
                _ScrubAnchorViewStart.GetValue() - ScrubState.ScrubTime + ScrubState.TimelineViewDuration * 0.5);
            _ViewModel->Set_ScrubState(NewScrubState);
        }
        _ScrubAnchorViewStart.Reset();
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

        // Auto-pan: if the user drags the needle to / past either edge of the viewport,
        // slide the frozen anchor so the needle stays visible. This makes "drag past the
        // edge to see more of that side" work without needing a separate scroll gesture.
        if (_ScrubAnchorViewStart.IsSet())
        {
            constexpr auto EdgeMarginFrac = 0.05;
            auto MarginTime = ViewDuration * EdgeMarginFrac;
            auto MinVisible = ViewStart + MarginTime;
            auto MaxVisible = ViewStart + ViewDuration - MarginTime;

            if (ScrubTime < MinVisible)
            {
                _ScrubAnchorViewStart = ScrubTime - MarginTime;
            }
            else if (ScrubTime > MaxVisible)
            {
                _ScrubAnchorViewStart = ScrubTime - ViewDuration + MarginTime;
            }
        }

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
