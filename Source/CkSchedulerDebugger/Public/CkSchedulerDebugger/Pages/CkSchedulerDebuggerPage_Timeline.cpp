#include "CkSchedulerDebuggerPage_Timeline.h"

#include "CkSchedulerDebugger/Data/CkSchedulerDebugger_Types.h"
#include "CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"
#include "CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_Timeline.h"
#include "CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_Inspector.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Timeline::
    Get_PageName() const
    -> FText
{
    return FText::FromString(TEXT("Timeline"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Timeline::
    Build_Content(
        TSharedPtr<FCkSchedulerDebugger_ViewModel> InViewModel)
    -> TSharedRef<SWidget>
{
    _ViewModel = InViewModel;

    _PumpBreakdownContent = SNew(SVerticalBox);

    auto Content = SNew(SSplitter)
        .Orientation(Orient_Vertical)

        // ---- Top: Timeline widget (65%)
        + SSplitter::Slot()
        .Value(0.65f)
        [
            SAssignNew(_Timeline, SCkSchedulerDebugger_Timeline)
            .ViewModel(_ViewModel)
        ]

        // ---- Bottom: Pump breakdown + Inspector (35%)
        + SSplitter::Slot()
        .Value(0.35f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Horizontal)

            // ---- Left: Pump breakdown (60%)
            + SSplitter::Slot()
            .Value(0.6f)
            [
                DoBuildPumpBreakdownPanel()
            ]

            // ---- Right: Inspector (40%)
            + SSplitter::Slot()
            .Value(0.4f)
            [
                SNew(SCkSchedulerDebugger_Inspector)
                .ViewModel(_ViewModel)
            ]
        ];

    if (_ViewModel.IsValid())
    {
        _DataRefreshedHandle = _ViewModel->OnDataRefreshed.AddRaw(
            this, &FCkSchedulerDebuggerPage_Timeline::DoRebuildPumpBreakdown);
    }

    DoRebuildPumpBreakdown();

    return Content;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Timeline::
    Tick(
        float InDeltaTime)
    -> void
{
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Timeline::
    OnSelectionChanged(
        int32 InProcessorIndex)
    -> void
{
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Timeline::
    DoBuildPumpBreakdownPanel()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
        .Padding(FCkSchedulerDebuggerStyle::Padding_Small)
        [
            SAssignNew(_PumpBreakdownScrollBox, SScrollBox)
            + SScrollBox::Slot()
            [
                _PumpBreakdownContent.ToSharedRef()
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSchedulerDebuggerPage_Timeline::
    DoRebuildPumpBreakdown()
    -> void
{
    if (NOT _PumpBreakdownContent.IsValid() || NOT _ViewModel.IsValid()) { return; }

    _PumpBreakdownContent->ClearChildren();

    const auto& DataCollector = _ViewModel->Get_DataCollector();
    const auto& Processors = DataCollector.Get_Processors();
    const auto PumpCount = DataCollector.Get_PumpCount();

    const auto HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
    const auto SubHeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
    const auto BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);

    // ---- PUMP BREAKDOWN header
    _PumpBreakdownContent->AddSlot()
    .AutoHeight()
    .Padding(0.0f, 0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Medium)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("PUMP BREAKDOWN")))
        .Font(HeaderFont)
        .ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Secondary)
    ];

    // ---- Main pass section
    {
        _PumpBreakdownContent->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Pass 0 (Main)")))
            .Font(SubHeaderFont)
            .ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Primary)
        ];

        auto MainPassTotalMs = 0.0;
        auto MainPassProcessorCount = 0;

        for (const auto& Processor : Processors)
        {
            if (Processor.MainPassTimeMs > 0.0)
            {
                MainPassTotalMs += Processor.MainPassTimeMs;
                ++MainPassProcessorCount;
            }
        }

        // ---- Aggregate bar
        const auto MaxBarWidth = 200.0f;
        const auto NormalizedWidth = FMath::Clamp(
            static_cast<float>(MainPassTotalMs / FMath::Max(DataCollector.Get_TotalFrameTimeMs(), 0.001)), 0.0f, 1.0f);

        _PumpBreakdownContent->AddSlot()
        .AutoHeight()
        .Padding(FCkSchedulerDebuggerStyle::Padding_Small, 2.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(MaxBarWidth * NormalizedWidth)
                .HeightOverride(8.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(FCkSchedulerDebuggerStyle::Color_Active)
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.3fms (%d processors)"), MainPassTotalMs, MainPassProcessorCount)))
                .Font(BodyFont)
                .ColorAndOpacity(FCkSchedulerDebuggerStyle::Get_TimingColor(MainPassTotalMs))
            ]
        ];
    }

    // ---- Pump pass sections
    for (auto PumpIndex = 0; PumpIndex < PumpCount; ++PumpIndex)
    {
        _PumpBreakdownContent->AddSlot()
        .AutoHeight()
        .Padding(0.0f, FCkSchedulerDebuggerStyle::Padding_Medium, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small)
        [
            SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("Pump %d"), PumpIndex + 1)))
            .Font(SubHeaderFont)
            .ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Pumped)
        ];

        auto PumpTotalMs = 0.0;
        auto PumpProcessorNames = TArray<FString>{};

        for (const auto& Processor : Processors)
        {
            if (Processor.PumpPassTimesMs.IsValidIndex(PumpIndex) && Processor.PumpPassTimesMs[PumpIndex] > 0.0)
            {
                PumpTotalMs += Processor.PumpPassTimesMs[PumpIndex];
                PumpProcessorNames.Add(FString::Printf(TEXT("  %s  %.3fms"),
                    *Processor.DisplayName,
                    Processor.PumpPassTimesMs[PumpIndex]));
            }
        }

        // ---- Pump aggregate bar
        const auto MaxBarWidth = 200.0f;
        const auto NormalizedWidth = FMath::Clamp(
            static_cast<float>(PumpTotalMs / FMath::Max(DataCollector.Get_TotalFrameTimeMs(), 0.001)), 0.0f, 1.0f);

        _PumpBreakdownContent->AddSlot()
        .AutoHeight()
        .Padding(FCkSchedulerDebuggerStyle::Padding_Small, 2.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(FMath::Max(MaxBarWidth * NormalizedWidth, 4.0f))
                .HeightOverride(8.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(FCkSchedulerDebuggerStyle::Color_Pumped)
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FCkSchedulerDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.3fms"), PumpTotalMs)))
                .Font(BodyFont)
                .ColorAndOpacity(FCkSchedulerDebuggerStyle::Get_TimingColor(PumpTotalMs))
            ]
        ];

        // ---- Processor list for this pump
        for (const auto& ProcessorLine : PumpProcessorNames)
        {
            _PumpBreakdownContent->AddSlot()
            .AutoHeight()
            .Padding(FCkSchedulerDebuggerStyle::Padding_Medium, 1.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(ProcessorLine))
                .Font(BodyFont)
                .ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Secondary)
            ];
        }
    }

    // ---- DIRTY PROCESSORS section
    _PumpBreakdownContent->AddSlot()
    .AutoHeight()
    .Padding(0.0f, FCkSchedulerDebuggerStyle::Padding_Large, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("DIRTY PROCESSORS")))
        .Font(HeaderFont)
        .ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_DirtyMarker)
    ];

    auto HasDirty = false;

    for (const auto& Processor : Processors)
    {
        if (NOT Processor.WasDirtyThisFrame) { continue; }

        HasDirty = true;

        _PumpBreakdownContent->AddSlot()
        .AutoHeight()
        .Padding(FCkSchedulerDebuggerStyle::Padding_Small, 1.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, FCkSchedulerDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("\u25CF")))
                .Font(BodyFont)
                .ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_DirtyMarker)
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%s  (pumped %d times)"),
                    *Processor.DisplayName, Processor.PumpCountThisFrame)))
                .Font(BodyFont)
                .ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Primary)
            ]
        ];
    }

    if (NOT HasDirty)
    {
        _PumpBreakdownContent->AddSlot()
        .AutoHeight()
        .Padding(FCkSchedulerDebuggerStyle::Padding_Small, 1.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("None")))
            .Font(BodyFont)
            .ColorAndOpacity(FCkSchedulerDebuggerStyle::Color_Text_Muted)
        ];
    }
}

// --------------------------------------------------------------------------------------------------------------------
