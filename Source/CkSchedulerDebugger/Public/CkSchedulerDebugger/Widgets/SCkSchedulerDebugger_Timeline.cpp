#include "SCkSchedulerDebugger_Timeline.h"

#include "CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const
    -> FVector2D
{
    return FVector2D{800.0f, 600.0f};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    TimeToPixelX(
        double InTimeMs) const
    -> float
{
    return _LabelColumnWidth + static_cast<float>(InTimeMs) * _PixelsPerMs - _ScrollOffsetX;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const
    -> int32
{
    _BarHitAreas.Reset();

    const auto GeometrySize = AllottedGeometry.GetLocalSize();

    // ---- Background
    FSlateDrawElement::MakeBox(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(),
        FCoreStyle::Get().GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        FCkSchedulerDebuggerStyle::Color_Background_Dark);

    if (NOT _ViewModel.IsValid()) { return LayerId; }

    const auto& DataCollector = _ViewModel->Get_DataCollector();
    const auto& Processors = DataCollector.Get_Processors();
    const auto& Groups = DataCollector.Get_Groups();

    if (Processors.IsEmpty()) { return LayerId; }

    const auto TotalFrameTimeMs = DataCollector.Get_TotalFrameTimeMs();

    // ---- Time axis at top
    LayerId = DoPaintTimeAxis(AllottedGeometry, OutDrawElements, LayerId, TotalFrameTimeMs);

    auto CurrentY = _TimeAxisHeight;

    // ---- Group processors by tick group for ordered rendering
    auto PrePhysicsGroups = TArray<int32>{};
    auto PostPhysicsGroups = TArray<int32>{};

    for (auto GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
    {
        if (Groups[GroupIndex].TickGroup == TG_PrePhysics)
        {
            PrePhysicsGroups.Add(GroupIndex);
        }
        else
        {
            PostPhysicsGroups.Add(GroupIndex);
        }
    }

    auto PaintTickGroupSection = [&](const TArray<int32>& InGroupIndices, const FString& InTickGroupLabel)
    {
        if (InGroupIndices.IsEmpty()) { return; }

        // ---- Tick group header
        const auto TickGroupHeaderGeometry = AllottedGeometry.MakeChild(
            FVector2D{GeometrySize.X, _GroupHeaderHeight},
            FSlateLayoutTransform{FVector2D{0.0f, CurrentY}});

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            LayerId,
            TickGroupHeaderGeometry.ToPaintGeometry(),
            FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            FCkSchedulerDebuggerStyle::Color_Background_Light);

        const auto TickGroupFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);

        FSlateDrawElement::MakeText(
            OutDrawElements,
            LayerId + 1,
            TickGroupHeaderGeometry.ToPaintGeometry(
                FVector2D{GeometrySize.X, _GroupHeaderHeight},
                FSlateLayoutTransform{FVector2D{FCkSchedulerDebuggerStyle::Padding_Medium, 4.0f}}),
            InTickGroupLabel,
            TickGroupFont,
            ESlateDrawEffect::None,
            FCkSchedulerDebuggerStyle::Color_Text_Secondary);

        CurrentY += _GroupHeaderHeight;

        // ---- Each group
        for (const auto GroupIndex : InGroupIndices)
        {
            const auto& Group = Groups[GroupIndex];
            const auto IsCollapsed = _CollapsedGroups.Contains(Group.GroupName);

            LayerId = DoPaintGroupLabel(AllottedGeometry, OutDrawElements, LayerId, Group, CurrentY, IsCollapsed);

            if (IsCollapsed)
            {
                LayerId = DoPaintCollapsedAggregateBar(AllottedGeometry, OutDrawElements, LayerId, Group, CurrentY);
                CurrentY += _RowHeight;
            }
            else
            {
                // ---- Compute cumulative start times for processors in execution order
                auto CumulativeTimeMs = 0.0;
                auto MainPassMaxTimeMs = 0.0;

                auto SortedMembers = Group.MemberIndices;
                SortedMembers.Sort([&Processors](int32 A, int32 B)
                {
                    return Processors[A].ExecutionOrder < Processors[B].ExecutionOrder;
                });

                for (const auto MemberIndex : SortedMembers)
                {
                    if (NOT Processors.IsValidIndex(MemberIndex)) { continue; }

                    const auto& Processor = Processors[MemberIndex];
                    const auto BarX = TimeToPixelX(CumulativeTimeMs);

                    LayerId = DoPaintProcessorBar(
                        AllottedGeometry, OutDrawElements, LayerId,
                        Processor, Group.GroupName, BarX, CurrentY);

                    auto HitInfo = FBarHitInfo{};
                    HitInfo.ProcessorIndex = MemberIndex;
                    HitInfo.Position = FVector2D{BarX, CurrentY};
                    const auto BarWidth = FMath::Max(static_cast<float>(Processor.MainPassTimeMs * _PixelsPerMs), 4.0f);
                    HitInfo.Size = FVector2D{BarWidth, _RowHeight - 2.0f};
                    _BarHitAreas.Add(HitInfo);

                    CumulativeTimeMs += Processor.MainPassTimeMs;
                    MainPassMaxTimeMs = FMath::Max(MainPassMaxTimeMs, CumulativeTimeMs);

                    // ---- Pump pass bars (offset after main pass region)
                    auto PumpCumulativeMs = MainPassMaxTimeMs;
                    for (auto PumpIndex = 0; PumpIndex < Processor.PumpPassTimesMs.Num(); ++PumpIndex)
                    {
                        const auto PumpTimeMs = Processor.PumpPassTimesMs[PumpIndex];
                        if (PumpTimeMs <= 0.0) { continue; }

                        auto PumpProcessor = Processor;
                        PumpProcessor.MainPassTimeMs = PumpTimeMs;

                        const auto PumpBarX = TimeToPixelX(PumpCumulativeMs);

                        LayerId = DoPaintProcessorBar(
                            AllottedGeometry, OutDrawElements, LayerId,
                            PumpProcessor, Group.GroupName, PumpBarX, CurrentY);

                        PumpCumulativeMs += PumpTimeMs;
                    }

                    CurrentY += _RowHeight;
                }
            }

            // ---- Thin separator after group
            const auto SepGeometry = AllottedGeometry.MakeChild(
                FVector2D{GeometrySize.X, 1.0f},
                FSlateLayoutTransform{FVector2D{0.0f, CurrentY}});

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId,
                SepGeometry.ToPaintGeometry(),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                FCkSchedulerDebuggerStyle::Color_Border * FLinearColor{1.0f, 1.0f, 1.0f, 0.3f});

            CurrentY += 1.0f;
        }
    };

    PaintTickGroupSection(PrePhysicsGroups, TEXT("TG_PrePhysics"));
    PaintTickGroupSection(PostPhysicsGroups, TEXT("TG_PostPhysics"));

    // ---- Pump separators
    const auto MainPassEndX = TimeToPixelX(TotalFrameTimeMs * 0.6);
    LayerId = DoPaintPumpSeparators(AllottedGeometry, OutDrawElements, LayerId, MainPassEndX);

    return LayerId + 1;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    DoPaintTimeAxis(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        float InTotalTimeMs) const
    -> int32
{
    const auto GeometrySize = InGeometry.GetLocalSize();
    const auto AxisFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

    // ---- Axis background
    const auto AxisGeometry = InGeometry.MakeChild(
        FVector2D{GeometrySize.X, _TimeAxisHeight},
        FSlateLayoutTransform{FVector2D::ZeroVector});

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        InLayerId,
        AxisGeometry.ToPaintGeometry(),
        FCoreStyle::Get().GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        FCkSchedulerDebuggerStyle::Color_Background_Medium);

    // ---- Determine tick interval based on zoom level
    auto TickIntervalMs = 0.1;
    const auto PixelsPerTick = TickIntervalMs * _PixelsPerMs;

    if (PixelsPerTick < 30.0)      { TickIntervalMs = 0.5; }
    else if (PixelsPerTick < 15.0) { TickIntervalMs = 1.0; }
    if (PixelsPerTick > 200.0)     { TickIntervalMs = 0.05; }
    if (PixelsPerTick > 500.0)     { TickIntervalMs = 0.01; }

    const auto MaxTimeMs = FMath::Max(InTotalTimeMs, 2.0);
    auto TimeMs = 0.0;

    while (TimeMs <= MaxTimeMs)
    {
        const auto TickX = TimeToPixelX(TimeMs);

        if (TickX >= _LabelColumnWidth && TickX <= GeometrySize.X)
        {
            // ---- Tick mark
            const auto TickGeometry = InGeometry.MakeChild(
                FVector2D{1.0f, 8.0f},
                FSlateLayoutTransform{FVector2D{TickX, _TimeAxisHeight - 10.0f}});

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                InLayerId + 1,
                TickGeometry.ToPaintGeometry(),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                FCkSchedulerDebuggerStyle::Color_Text_Muted);

            // ---- Label
            const auto Label = FString::Printf(TEXT("%.2fms"), TimeMs);

            FSlateDrawElement::MakeText(
                OutDrawElements,
                InLayerId + 2,
                InGeometry.MakeChild(
                    FVector2D{80.0f, 12.0f},
                    FSlateLayoutTransform{FVector2D{TickX + 3.0f, _TimeAxisHeight - 22.0f}}).ToPaintGeometry(),
                Label,
                AxisFont,
                ESlateDrawEffect::None,
                FCkSchedulerDebuggerStyle::Color_Text_Muted);
        }

        TimeMs += TickIntervalMs;
    }

    // ---- Horizontal baseline
    const auto BaselineGeometry = InGeometry.MakeChild(
        FVector2D{GeometrySize.X - _LabelColumnWidth, 1.0f},
        FSlateLayoutTransform{FVector2D{_LabelColumnWidth, _TimeAxisHeight - 1.0f}});

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        InLayerId + 1,
        BaselineGeometry.ToPaintGeometry(),
        FCoreStyle::Get().GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        FCkSchedulerDebuggerStyle::Color_Border);

    return InLayerId + 2;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    DoPaintGroupLabel(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FCkSchedulerDebugger_GroupInfo& InGroup,
        float InY,
        bool InIsCollapsed) const
    -> int32
{
    const auto LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
    const auto CollapseIndicator = InIsCollapsed ? TEXT("\u25B6 ") : TEXT("\u25BC ");
    const auto LabelText = CollapseIndicator + InGroup.DisplayName;

    // ---- Accent bar on left
    const auto AccentGeometry = InGeometry.MakeChild(
        FVector2D{3.0f, _RowHeight - 2.0f},
        FSlateLayoutTransform{FVector2D{0.0f, InY + 1.0f}});

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        InLayerId + 1,
        AccentGeometry.ToPaintGeometry(),
        FCoreStyle::Get().GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        InGroup.AccentColor);

    // ---- Label text
    FSlateDrawElement::MakeText(
        OutDrawElements,
        InLayerId + 1,
        InGeometry.MakeChild(
            FVector2D{_LabelColumnWidth - 20.0f, _RowHeight},
            FSlateLayoutTransform{FVector2D{10.0f, InY + 4.0f}}).ToPaintGeometry(),
        LabelText,
        LabelFont,
        ESlateDrawEffect::None,
        FCkSchedulerDebuggerStyle::Color_Text_Primary);

    // ---- Aggregate time
    const auto TimeText = FString::Printf(TEXT("%.3fms"), InGroup.AggregateTimeMs);
    const auto TimeFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

    FSlateDrawElement::MakeText(
        OutDrawElements,
        InLayerId + 1,
        InGeometry.MakeChild(
            FVector2D{60.0f, _RowHeight},
            FSlateLayoutTransform{FVector2D{_LabelColumnWidth - 70.0f, InY + 6.0f}}).ToPaintGeometry(),
        TimeText,
        TimeFont,
        ESlateDrawEffect::None,
        FCkSchedulerDebuggerStyle::Get_TimingColor(InGroup.AggregateTimeMs));

    return InLayerId + 1;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    DoPaintProcessorBar(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FCkSchedulerDebugger_ProcessorInfo& InProcessor,
        const FName& InGroupName,
        float InX,
        float InY) const
    -> int32
{
    const auto BarWidth = FMath::Max(static_cast<float>(InProcessor.MainPassTimeMs * _PixelsPerMs), 4.0f);
    const auto BarHeight = _RowHeight - 4.0f;

    // ---- Determine bar color with heat tinting
    auto GroupAccent = FCkSchedulerDebuggerStyle::Get_GroupColor(InGroupName);
    auto HeatColor = FCkSchedulerDebuggerStyle::Get_TimingColor(InProcessor.MainPassTimeMs);
    auto BarColor = FLinearColor::LerpUsingHSV(GroupAccent, HeatColor, 0.4f);

    if (InProcessor.IsGhost)
    {
        BarColor.A = 0.4f;
    }

    // ---- Bar background
    const auto BarGeometry = InGeometry.MakeChild(
        FVector2D{BarWidth, BarHeight},
        FSlateLayoutTransform{FVector2D{InX, InY + 2.0f}});

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        InLayerId + 1,
        BarGeometry.ToPaintGeometry(),
        FCoreStyle::Get().GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        BarColor);

    // ---- Ghost hatching overlay (diagonal lines via dimmed tint)
    if (InProcessor.IsGhost)
    {
        auto HatchColor = FCkSchedulerDebuggerStyle::Color_Background_Dark;
        HatchColor.A = 0.3f;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            InLayerId + 2,
            BarGeometry.ToPaintGeometry(),
            FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None,
            HatchColor);
    }

    // ---- Selected processor: blue border
    const auto IsSelected = _ViewModel.IsValid() && _ViewModel->Get_SelectedProcessorIndex() == InProcessor.NodeIndex;
    if (IsSelected)
    {
        constexpr auto BorderThickness = 2.0f;
        const auto BorderColor = FCkSchedulerDebuggerStyle::Color_Selection;

        // Top
        FSlateDrawElement::MakeBox(OutDrawElements, InLayerId + 3,
            InGeometry.MakeChild(FVector2D{BarWidth, BorderThickness}, FSlateLayoutTransform{FVector2D{InX, InY + 2.0f}}).ToPaintGeometry(),
            FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, BorderColor);
        // Bottom
        FSlateDrawElement::MakeBox(OutDrawElements, InLayerId + 3,
            InGeometry.MakeChild(FVector2D{BarWidth, BorderThickness}, FSlateLayoutTransform{FVector2D{InX, InY + 2.0f + BarHeight - BorderThickness}}).ToPaintGeometry(),
            FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, BorderColor);
        // Left
        FSlateDrawElement::MakeBox(OutDrawElements, InLayerId + 3,
            InGeometry.MakeChild(FVector2D{BorderThickness, BarHeight}, FSlateLayoutTransform{FVector2D{InX, InY + 2.0f}}).ToPaintGeometry(),
            FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, BorderColor);
        // Right
        FSlateDrawElement::MakeBox(OutDrawElements, InLayerId + 3,
            InGeometry.MakeChild(FVector2D{BorderThickness, BarHeight}, FSlateLayoutTransform{FVector2D{InX + BarWidth - BorderThickness, InY + 2.0f}}).ToPaintGeometry(),
            FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, BorderColor);
    }

    // ---- Text: processor name + exec order (only if bar is wide enough)
    const auto TextFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);
    constexpr auto MinBarWidthForText = 30.0f;

    if (BarWidth > MinBarWidthForText)
    {
        auto DisplayText = FString::Printf(TEXT("#%d %s"), InProcessor.ExecutionOrder, *InProcessor.DisplayName);

        const auto FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        const auto TextSize = FontMeasureService->Measure(DisplayText, TextFont);

        if (TextSize.X > BarWidth - 20.0f)
        {
            const auto MaxChars = FMath::Max(3, static_cast<int32>((BarWidth - 24.0f) / (TextSize.X / DisplayText.Len())));
            DisplayText = DisplayText.Left(MaxChars) + TEXT("..");
        }

        auto BadgeText = FString{};
        if (InProcessor.HasDirtyMarker) { BadgeText += TEXT("\u25CF"); }
        if (InProcessor.IsParallel)     { BadgeText += TEXT("\u25C6"); }

        const auto FullText = BadgeText.IsEmpty() ? DisplayText : BadgeText + TEXT(" ") + DisplayText;

        FSlateDrawElement::MakeText(
            OutDrawElements,
            InLayerId + 2,
            InGeometry.MakeChild(
                FVector2D{BarWidth - 6.0f, BarHeight},
                FSlateLayoutTransform{FVector2D{InX + 3.0f, InY + 5.0f}}).ToPaintGeometry(),
            FullText,
            TextFont,
            ESlateDrawEffect::None,
            InProcessor.IsGhost ? FCkSchedulerDebuggerStyle::Color_Text_Muted : FCkSchedulerDebuggerStyle::Color_Text_Primary);
    }
    else if (BarWidth > 8.0f)
    {
        // ---- Badge-only for narrow bars
        auto BadgeText = FString{};
        if (InProcessor.HasDirtyMarker) { BadgeText += TEXT("\u25CF"); }
        if (InProcessor.IsParallel)     { BadgeText += TEXT("\u25C6"); }

        if (NOT BadgeText.IsEmpty())
        {
            FSlateDrawElement::MakeText(
                OutDrawElements,
                InLayerId + 2,
                InGeometry.MakeChild(
                    FVector2D{BarWidth, BarHeight},
                    FSlateLayoutTransform{FVector2D{InX + 1.0f, InY + 5.0f}}).ToPaintGeometry(),
                BadgeText,
                TextFont,
                ESlateDrawEffect::None,
                FCkSchedulerDebuggerStyle::Color_Text_Muted);
        }
    }

    return InLayerId + 3;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    DoPaintPumpSeparators(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        float InMainPassEndX) const
    -> int32
{
    if (NOT _ViewModel.IsValid()) { return InLayerId; }

    const auto& DataCollector = _ViewModel->Get_DataCollector();
    const auto PumpCount = DataCollector.Get_PumpCount();
    const auto GeometrySize = InGeometry.GetLocalSize();
    const auto LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

    if (PumpCount <= 0) { return InLayerId; }

    // ---- "Pass 0 (Main)" label above the main pass region
    FSlateDrawElement::MakeText(
        OutDrawElements,
        InLayerId + 1,
        InGeometry.MakeChild(
            FVector2D{100.0f, 12.0f},
            FSlateLayoutTransform{FVector2D{_LabelColumnWidth + 4.0f, 2.0f}}).ToPaintGeometry(),
        TEXT("Pass 0 (Main)"),
        LabelFont,
        ESlateDrawEffect::None,
        FCkSchedulerDebuggerStyle::Color_Text_Muted);

    // ---- Dashed vertical separator lines for each pump
    constexpr auto DashHeight = 6.0f;
    constexpr auto DashGap = 4.0f;
    auto SeparatorColor = FCkSchedulerDebuggerStyle::Color_Pumped;
    SeparatorColor.A = 0.4f;

    for (auto PumpIndex = 0; PumpIndex < PumpCount; ++PumpIndex)
    {
        const auto SepX = InMainPassEndX + static_cast<float>(PumpIndex) * 50.0f;

        if (SepX < _LabelColumnWidth || SepX > GeometrySize.X) { continue; }

        // ---- Dashed vertical line
        auto DashY = _TimeAxisHeight;
        while (DashY < GeometrySize.Y)
        {
            const auto DashGeometry = InGeometry.MakeChild(
                FVector2D{1.0f, DashHeight},
                FSlateLayoutTransform{FVector2D{SepX, DashY}});

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                InLayerId + 1,
                DashGeometry.ToPaintGeometry(),
                FCoreStyle::Get().GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                SeparatorColor);

            DashY += DashHeight + DashGap;
        }

        // ---- Pump label
        const auto PumpLabel = FString::Printf(TEXT("Pump %d"), PumpIndex + 1);

        FSlateDrawElement::MakeText(
            OutDrawElements,
            InLayerId + 2,
            InGeometry.MakeChild(
                FVector2D{60.0f, 12.0f},
                FSlateLayoutTransform{FVector2D{SepX + 4.0f, 2.0f}}).ToPaintGeometry(),
            PumpLabel,
            LabelFont,
            ESlateDrawEffect::None,
            FCkSchedulerDebuggerStyle::Color_Pumped);
    }

    return InLayerId + 2;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    DoPaintCollapsedAggregateBar(
        const FGeometry& InGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FCkSchedulerDebugger_GroupInfo& InGroup,
        float InY) const
    -> int32
{
    const auto BarWidth = FMath::Max(static_cast<float>(InGroup.AggregateTimeMs * _PixelsPerMs), 8.0f);
    constexpr auto BarHeight = 10.0f;
    const auto BarX = TimeToPixelX(0.0);
    const auto BarY = InY + (_RowHeight - BarHeight) * 0.5f;

    auto BarColor = InGroup.AccentColor;
    BarColor.A = 0.6f;

    const auto BarGeometry = InGeometry.MakeChild(
        FVector2D{BarWidth, BarHeight},
        FSlateLayoutTransform{FVector2D{BarX, BarY}});

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        InLayerId + 1,
        BarGeometry.ToPaintGeometry(),
        FCoreStyle::Get().GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        BarColor);

    // ---- Time label on the bar
    const auto TimeText = FString::Printf(TEXT("%.3fms"), InGroup.AggregateTimeMs);

    FSlateDrawElement::MakeText(
        OutDrawElements,
        InLayerId + 2,
        InGeometry.MakeChild(
            FVector2D{60.0f, BarHeight},
            FSlateLayoutTransform{FVector2D{BarX + BarWidth + 4.0f, BarY - 1.0f}}).ToPaintGeometry(),
        TimeText,
        FCoreStyle::GetDefaultFontStyle("Regular", 8),
        ESlateDrawEffect::None,
        FCkSchedulerDebuggerStyle::Get_TimingColor(InGroup.AggregateTimeMs));

    return InLayerId + 2;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    OnMouseButtonDown(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent)
    -> FReply
{
    const auto LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // ---- Hit test group labels for collapse toggle
        if (LocalPosition.X < _LabelColumnWidth && _ViewModel.IsValid())
        {
            const auto& DataCollector = _ViewModel->Get_DataCollector();
            const auto& Groups = DataCollector.Get_Groups();

            auto TestY = _TimeAxisHeight + _GroupHeaderHeight;

            for (const auto& Group : Groups)
            {
                if (LocalPosition.Y >= TestY && LocalPosition.Y < TestY + _RowHeight)
                {
                    if (_CollapsedGroups.Contains(Group.GroupName))
                    {
                        _CollapsedGroups.Remove(Group.GroupName);
                    }
                    else
                    {
                        _CollapsedGroups.Add(Group.GroupName);
                    }
                    return FReply::Handled();
                }

                const auto IsCollapsed = _CollapsedGroups.Contains(Group.GroupName);
                if (IsCollapsed)
                {
                    TestY += _RowHeight + 1.0f;
                }
                else
                {
                    TestY += _RowHeight * Group.MemberIndices.Num() + 1.0f;
                }
            }
        }

        // ---- Hit test processor bars
        for (const auto& HitArea : _BarHitAreas)
        {
            if (LocalPosition.X >= HitArea.Position.X &&
                LocalPosition.X <= HitArea.Position.X + HitArea.Size.X &&
                LocalPosition.Y >= HitArea.Position.Y &&
                LocalPosition.Y <= HitArea.Position.Y + HitArea.Size.Y)
            {
                if (_ViewModel.IsValid())
                {
                    _ViewModel->Set_SelectedProcessorIndex(HitArea.ProcessorIndex);
                }
                return FReply::Handled();
            }
        }

        return FReply::Handled();
    }

    if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
    {
        _IsPanning = true;
        _PanStartX = LocalPosition.X;
        _PanStartOffsetX = _ScrollOffsetX;
        return FReply::Handled().CaptureMouse(SharedThis(const_cast<SCkSchedulerDebugger_Timeline*>(this)));
    }

    return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    OnMouseButtonUp(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent)
    -> FReply
{
    if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && _IsPanning)
    {
        _IsPanning = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    OnMouseWheel(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent)
    -> FReply
{
    const auto Delta = MouseEvent.GetWheelDelta();
    const auto ZoomFactor = (Delta > 0.0f) ? 1.1f : 0.9f;

    const auto LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    const auto TimeUnderCursor = (LocalPosition.X - _LabelColumnWidth + _ScrollOffsetX) / _PixelsPerMs;

    _PixelsPerMs = FMath::Clamp(_PixelsPerMs * ZoomFactor, 50.0f, 5000.0f);

    _ScrollOffsetX = static_cast<float>(TimeUnderCursor) * _PixelsPerMs - (LocalPosition.X - _LabelColumnWidth);

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSchedulerDebugger_Timeline::
    OnMouseMove(
        const FGeometry& MyGeometry,
        const FPointerEvent& MouseEvent)
    -> FReply
{
    if (_IsPanning)
    {
        const auto LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
        _ScrollOffsetX = _PanStartOffsetX - (LocalPosition.X - _PanStartX);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------
