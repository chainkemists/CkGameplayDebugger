#include "CkSmDebugger/Window/SCkSmDebugger_HistoryList.h"

#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"

#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_HistoryList::
    Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkSmDebugger_ViewModel> InViewModel,
        UCkSmDebugGraph* InGraph)
    -> void
{
    _ViewModel = InViewModel;
    _Graph = InGraph;

    ChildSlot
    [
        SAssignNew(_ListView, SListView<FHistoryItemPtr>)
            .ListItemsSource(&_Items)
            .OnGenerateRow(this, &SCkSmDebugger_HistoryList::GenerateRow)
            .OnSelectionChanged(this, &SCkSmDebugger_HistoryList::OnSelectionChanged)
            .SelectionMode(ESelectionMode::Single)
    ];

    if (_ViewModel.IsValid())
    {
        _SmDataRefreshedHandle = _ViewModel->OnSmDataRefreshed.AddLambda(
            [this](const FCkSmDebugger_SmInfo&)
            {
                RebuildList();
            });
    }
}

auto
    SCkSmDebugger_HistoryList::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
}

// --------------------------------------------------------------------------------------------------------------------
// Row dispatch
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_HistoryList::
    GenerateRow(
        FHistoryItemPtr InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    auto ItemIndex = _Items.IndexOfByKey(InItem);
    auto Style = _Graph
        ? _Graph->LayoutParams.HistoryStyle
        : ECkSmDebugger_HistoryStyle::ClassicArrows;

    switch (Style)
    {
    case ECkSmDebugger_HistoryStyle::ArrowCards:
        return GenerateRow_ArrowCards(InItem, InOwnerTable, ItemIndex);
    case ECkSmDebugger_HistoryStyle::CompactBlocks:
        return GenerateRow_CompactBlocks(InItem, InOwnerTable, ItemIndex);
    case ECkSmDebugger_HistoryStyle::ClassicArrows:
    default:
        return GenerateRow_ClassicArrows(InItem, InOwnerTable, ItemIndex);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Shared: task chips widget
// --------------------------------------------------------------------------------------------------------------------
// Shared helpers
// --------------------------------------------------------------------------------------------------------------------

namespace
{
    auto FormatConditions(
        const TArray<FString>& InNames,
        int32 InNameDepth)
        -> FString
    {
        if (InNames.Num() == 0)
        { return FString{}; }

        auto Parts = TArray<FString>{};
        for (const auto& Name : InNames)
        { Parts.Add(FCkSmLayoutParams::ComputeDisplayName(Name, InNameDepth)); }

        return FString::Join(Parts, TEXT(", "));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_HistoryList::
    BuildTaskChips(
        const TArray<FCkSmDebugger_HistoryTaskSnapshot>& InSnapshots,
        bool InShortNames)
    -> TSharedRef<SWidget>
{
    if (InSnapshots.Num() == 0)
    { return SNullWidget::NullWidget; }

    auto Depth = _Graph ? _Graph->LayoutParams.NameDepth : 1;
    auto Box = SNew(SHorizontalBox);

    for (const auto& Snap : InSnapshots)
    {
        auto ResultColor = CkSmDebugger::GetTaskResultColor(Snap.Result);
        auto Icon = FString{};

        switch (Snap.Result)
        {
        case ECk_SmTaskResult::Running:   Icon = TEXT("\x25CF"); break;
        case ECk_SmTaskResult::Succeeded: Icon = TEXT("\x2713"); break;
        case ECk_SmTaskResult::Failed:    Icon = TEXT("\x2717"); break;
        }

        auto DisplayName = InShortNames
            ? FCkSmLayoutParams::ComputeDisplayName(Snap.TaskName, Depth)
            : Snap.TaskName;

        auto ChipText = FString::Printf(TEXT("%s %s"), *DisplayName, *Icon);

        Box->AddSlot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SBorder)
                    .BorderBackgroundColor(FLinearColor(ResultColor.R, ResultColor.G, ResultColor.B, 0.15f))
                    .Padding(FMargin(4.0f, 1.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(ChipText))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                            .ColorAndOpacity(ResultColor)
                    ]
            ];
    }

    return Box;
}

// --------------------------------------------------------------------------------------------------------------------
// Style: Arrow Cards (two-line)
//
//   [449251 +47]  Chase  ◀──  Idle
//                via CanSee, InRange  [TimedWork ✓] [LogOnly ●]
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_HistoryList::
    GenerateRow_ArrowCards(
        FHistoryItemPtr InItem,
        const TSharedRef<STableViewBase>& InOwnerTable,
        int32 InIndex)
    -> TSharedRef<ITableRow>
{
    auto FromColor = CkSmDebugger::ComputeStateColor(InItem->FromStateName);
    auto ToColor = CkSmDebugger::ComputeStateColor(InItem->ToStateName);
    auto ZebraTint = (InIndex % 2 == 0)
        ? FLinearColor(1.0f, 1.0f, 1.0f, 0.03f)
        : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

    auto Depth = _Graph ? _Graph->LayoutParams.NameDepth : 1;
    auto CondStr = FormatConditions(InItem->ConditionNames, Depth);
    if (NOT CondStr.IsEmpty())
    { CondStr = TEXT("via ") + CondStr; }

    auto FrameStr = FString::Printf(TEXT("[%llu"), InItem->FrameNumber);
    if (InIndex + 1 < _Items.Num())
    {
        auto Delta = InItem->FrameNumber - _Items[InIndex + 1]->FrameNumber;
        FrameStr += FString::Printf(TEXT(" +%llu"), Delta);
    }
    FrameStr += TEXT("]");

    return SNew(STableRow<FHistoryItemPtr>, InOwnerTable)
        .Style(&FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
        [
            SNew(SBorder)
                .BorderBackgroundColor(ZebraTint)
                .Padding(FMargin(4.0f, 3.0f))
                [
                    SNew(SVerticalBox)

                    // Top line: [frame +delta]  To  ◀──  From
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SHorizontalBox)

                            // Frame + delta
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(SBox)
                                        .MinDesiredWidth(90.0f)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(FrameStr))
                                                .ColorAndOpacity(FLinearColor(0.35f, 0.35f, 0.4f))
                                                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                                        ]
                                ]

                            // To (arrived at)
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(SBox)
                                        .MinDesiredWidth(90.0f)
                                        [
                                            SNew(STextBlock)
                                                .Text_Lambda([this, InItem]()
                                                {
                                                    auto D = _Graph ? _Graph->LayoutParams.NameDepth : 1;
                                                    auto Name = FCkSmLayoutParams::ComputeDisplayName(InItem->ToStateName, D);
                                                    if (NOT InItem->SubSmParentStateName.IsEmpty())
                                                    { Name = FCkSmLayoutParams::ComputeDisplayName(InItem->SubSmParentStateName, D) + TEXT(":") + Name; }
                                                    return FText::FromString(Name);
                                                })
                                                .ColorAndOpacity(ToColor)
                                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                        ]
                                ]

                            // Arrow (left-pointing)
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(2.0f, 0.0f)
                                [
                                    SNew(SBox)
                                        .MinDesiredWidth(32.0f)
                                        .HAlign(HAlign_Center)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("\x25C0\x2500\x2500")))
                                                .ColorAndOpacity(FLinearColor(0.45f, 0.45f, 0.5f))
                                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                        ]
                                ]

                            // From (came from)
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(SBox)
                                        .MinDesiredWidth(90.0f)
                                        [
                                            SNew(STextBlock)
                                                .Text_Lambda([this, InItem]()
                                                {
                                                    auto D = _Graph ? _Graph->LayoutParams.NameDepth : 1;
                                                    auto Name = FCkSmLayoutParams::ComputeDisplayName(InItem->FromStateName, D);
                                                    if (NOT InItem->SubSmParentStateName.IsEmpty())
                                                    { Name = FCkSmLayoutParams::ComputeDisplayName(InItem->SubSmParentStateName, D) + TEXT(":") + Name; }
                                                    return FText::FromString(Name);
                                                })
                                                .ColorAndOpacity(FromColor)
                                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                        ]
                                ]

                            + SHorizontalBox::Slot()
                                .FillWidth(1.0f)
                                [ SNullWidget::NullWidget ]
                        ]

                    // Bottom line: conditions + task chips
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(90.0f, 1.0f, 0.0f, 0.0f)
                        [
                            SNew(SHorizontalBox)

                            // Conditions
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(CondStr.IsEmpty() ? TEXT("(unconditional)") : CondStr))
                                        .ColorAndOpacity(FLinearColor(0.45f, 0.45f, 0.5f))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                ]

                            // Task chips (right after conditions)
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(6.0f, 0.0f, 0.0f, 0.0f)
                                [
                                    BuildTaskChips(InItem->TaskSnapshots, true)
                                ]
                        ]
                ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------
// Style: Classic Arrows (single-line)
//
//   [449251 +47]  Chase  ◀─  Idle   via CanSee, InRange  [TW ✓] [LO ●]
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_HistoryList::
    GenerateRow_ClassicArrows(
        FHistoryItemPtr InItem,
        const TSharedRef<STableViewBase>& InOwnerTable,
        int32 InIndex)
    -> TSharedRef<ITableRow>
{
    auto FromColor = CkSmDebugger::ComputeStateColor(InItem->FromStateName);
    auto ToColor = CkSmDebugger::ComputeStateColor(InItem->ToStateName);
    auto ZebraTint = (InIndex % 2 == 0)
        ? FLinearColor(1.0f, 1.0f, 1.0f, 0.03f)
        : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

    auto Depth = _Graph ? _Graph->LayoutParams.NameDepth : 1;
    auto CondStr = FormatConditions(InItem->ConditionNames, Depth);
    if (NOT CondStr.IsEmpty())
    { CondStr = TEXT("via ") + CondStr; }

    // Compute delta frames from the previous (older) transition.
    // _Items is reverse-chronological, so the older entry is at InIndex+1.
    auto FrameStr = FString::Printf(TEXT("[%llu"), InItem->FrameNumber);
    if (InIndex + 1 < _Items.Num())
    {
        auto Delta = InItem->FrameNumber - _Items[InIndex + 1]->FrameNumber;
        FrameStr += FString::Printf(TEXT(" +%llu"), Delta);
    }
    FrameStr += TEXT("]");

    return SNew(STableRow<FHistoryItemPtr>, InOwnerTable)
        .Style(&FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
        [
            SNew(SBorder)
                .BorderBackgroundColor(ZebraTint)
                .Padding(FMargin(4.0f, 2.0f))
                [
                    SNew(SHorizontalBox)

                    // Frame + delta
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                                .MinDesiredWidth(90.0f)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(FrameStr))
                                        .ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.45f))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                                ]
                        ]

                    // To (arrived at)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                                .MinDesiredWidth(90.0f)
                                [
                                    SNew(STextBlock)
                                        .Text_Lambda([this, InItem]()
                                        {
                                            auto D = _Graph ? _Graph->LayoutParams.NameDepth : 1;
                                            auto Name = FCkSmLayoutParams::ComputeDisplayName(InItem->ToStateName, D);
                                            if (NOT InItem->SubSmParentStateName.IsEmpty())
                                            { Name = FCkSmLayoutParams::ComputeDisplayName(InItem->SubSmParentStateName, D) + TEXT(":") + Name; }
                                            return FText::FromString(Name);
                                        })
                                        .ColorAndOpacity(ToColor)
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                ]
                        ]

                    // Arrow (left-pointing)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                                .MinDesiredWidth(28.0f)
                                .HAlign(HAlign_Center)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("\x25C0\x2500")))
                                        .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                ]
                        ]

                    // From (came from)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                                .MinDesiredWidth(90.0f)
                                [
                                    SNew(STextBlock)
                                        .Text_Lambda([this, InItem]()
                                        {
                                            auto D = _Graph ? _Graph->LayoutParams.NameDepth : 1;
                                            auto Name = FCkSmLayoutParams::ComputeDisplayName(InItem->FromStateName, D);
                                            if (NOT InItem->SubSmParentStateName.IsEmpty())
                                            { Name = FCkSmLayoutParams::ComputeDisplayName(InItem->SubSmParentStateName, D) + TEXT(":") + Name; }
                                            return FText::FromString(Name);
                                        })
                                        .ColorAndOpacity(FromColor)
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                ]
                        ]

                    // Conditions
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(CondStr))
                                .ColorAndOpacity(FLinearColor(0.45f, 0.45f, 0.5f))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                        ]

                    // Task chips (right after conditions)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(6.0f, 0.0f, 0.0f, 0.0f)
                        [
                            BuildTaskChips(InItem->TaskSnapshots, true)
                        ]
                ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------
// Style: Compact Blocks (ultra-dense)
//
//   [449251 +47] Chase ◂ Idle  CanSee  [● ✓]
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_HistoryList::
    GenerateRow_CompactBlocks(
        FHistoryItemPtr InItem,
        const TSharedRef<STableViewBase>& InOwnerTable,
        int32 InIndex)
    -> TSharedRef<ITableRow>
{
    auto FromColor = CkSmDebugger::ComputeStateColor(InItem->FromStateName);
    auto ToColor = CkSmDebugger::ComputeStateColor(InItem->ToStateName);
    auto ZebraTint = (InIndex % 2 == 0)
        ? FLinearColor(1.0f, 1.0f, 1.0f, 0.03f)
        : FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

    auto Depth = _Graph ? _Graph->LayoutParams.NameDepth : 1;

    // Compact task result icons: ● ✓ ✗
    auto TaskIcons = FString{};
    for (const auto& Snap : InItem->TaskSnapshots)
    {
        switch (Snap.Result)
        {
        case ECk_SmTaskResult::Running:   TaskIcons += TEXT("\x25CF"); break;
        case ECk_SmTaskResult::Succeeded: TaskIcons += TEXT("\x2713"); break;
        case ECk_SmTaskResult::Failed:    TaskIcons += TEXT("\x2717"); break;
        }
    }

    auto CondBrief = FString{};
    if (InItem->ConditionNames.Num() > 0)
    { CondBrief = FCkSmLayoutParams::ComputeDisplayName(InItem->ConditionNames[0], Depth); }

    auto FrameStr = FString::Printf(TEXT("[%llu"), InItem->FrameNumber);
    if (InIndex + 1 < _Items.Num())
    {
        auto Delta = InItem->FrameNumber - _Items[InIndex + 1]->FrameNumber;
        FrameStr += FString::Printf(TEXT(" +%llu"), Delta);
    }
    FrameStr += TEXT("]");

    return SNew(STableRow<FHistoryItemPtr>, InOwnerTable)
        .Style(&FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
        [
            SNew(SBorder)
                .BorderBackgroundColor(ZebraTint)
                .Padding(FMargin(4.0f, 1.0f))
                [
                    SNew(SHorizontalBox)

                    // Frame + delta
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                                .MinDesiredWidth(80.0f)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(FrameStr))
                                        .ColorAndOpacity(FLinearColor(0.35f, 0.35f, 0.4f))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 7))
                                ]
                        ]

                    // To (arrived at)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                                .MinDesiredWidth(70.0f)
                                [
                                    SNew(STextBlock)
                                        .Text_Lambda([this, InItem]()
                                        {
                                            auto D = _Graph ? _Graph->LayoutParams.NameDepth : 1;
                                            auto Name = FCkSmLayoutParams::ComputeDisplayName(InItem->ToStateName, D);
                                            if (NOT InItem->SubSmParentStateName.IsEmpty())
                                            { Name = FCkSmLayoutParams::ComputeDisplayName(InItem->SubSmParentStateName, D) + TEXT(":") + Name; }
                                            return FText::FromString(Name);
                                        })
                                        .ColorAndOpacity(ToColor)
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                ]
                        ]

                    // ◂
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                                .MinDesiredWidth(16.0f)
                                .HAlign(HAlign_Center)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TEXT("\x25C2")))
                                        .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                ]
                        ]

                    // From (came from)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                                .MinDesiredWidth(70.0f)
                                [
                                    SNew(STextBlock)
                                        .Text_Lambda([this, InItem]()
                                        {
                                            auto D = _Graph ? _Graph->LayoutParams.NameDepth : 1;
                                            auto Name = FCkSmLayoutParams::ComputeDisplayName(InItem->FromStateName, D);
                                            if (NOT InItem->SubSmParentStateName.IsEmpty())
                                            { Name = FCkSmLayoutParams::ComputeDisplayName(InItem->SubSmParentStateName, D) + TEXT(":") + Name; }
                                            return FText::FromString(Name);
                                        })
                                        .ColorAndOpacity(FromColor)
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                ]
                        ]

                    // Brief condition
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                                .MinDesiredWidth(70.0f)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(CondBrief))
                                        .ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.45f))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
                                ]
                        ]

                    // Task icons (right after condition)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TaskIcons))
                                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.65f))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                        ]
                ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------
// Selection
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_HistoryList::
    OnSelectionChanged(
        FHistoryItemPtr InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    if (NOT InItem.IsValid() || NOT _ViewModel.IsValid())
    { return; }

    _ViewModel->Set_ViewMode(ECkSmDebugger_ViewMode::Scrub);

    auto NewScrubState = _ViewModel->Get_ScrubState();
    NewScrubState.ViewMode = ECkSmDebugger_ViewMode::Scrub;
    NewScrubState.ScrubTime = InItem->RealTimeSeconds;

    auto ItemIndex = _Items.IndexOfByKey(InItem);
    NewScrubState.SelectedHistoryIndex = ItemIndex;

    _ViewModel->Set_ScrubState(NewScrubState);

    // Highlight the taken transition on the graph
    for (auto i = 0; i < _Items.Num(); ++i)
    {
        if (_Items[i] == InItem)
        {
            auto SmInfo = _ViewModel->Get_CurrentSmInfo();
            if (SmInfo)
            {
                auto SourceIdx = -1;
                auto TargetIdx = -1;

                for (auto j = 0; j < SmInfo->States.Num(); ++j)
                {
                    if (SmInfo->States[j].StateName == InItem->FromStateName)
                    { SourceIdx = j; }
                    if (SmInfo->States[j].StateName == InItem->ToStateName)
                    { TargetIdx = j; }
                }

                if (SourceIdx >= 0 && TargetIdx >= 0)
                { _ViewModel->SetScrubTransitionHighlight(SourceIdx, TargetIdx); }
            }
            break;
        }
    }

    OnEntrySelected.ExecuteIfBound(InItem);
}

// --------------------------------------------------------------------------------------------------------------------
// List rebuild
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_HistoryList::
    RebuildList()
    -> void
{
    if (NOT _ViewModel.IsValid())
    { return; }

    auto SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (NOT SmInfo)
    {
        _Items.Empty();
        if (_ListView.IsValid())
        { _ListView->RequestListRefresh(); }
        return;
    }

    auto& ScrubState = _ViewModel->Get_ScrubState();
    auto RunIndex = ScrubState.SelectedRunIndex;

    auto& Run = (RunIndex < 0)
        ? SmInfo->CurrentRun
        : (RunIndex < SmInfo->CompletedRuns.Num()
            ? SmInfo->CompletedRuns[RunIndex]
            : SmInfo->CurrentRun);

    auto& History = Run.History;

    if (History.Num() == _LastHistoryCount)
    { return; }

    _LastHistoryCount = History.Num();
    _Items.Empty(History.Num());

    // Reverse order — most recent transition first
    for (auto i = History.Num() - 1; i >= 0; --i)
    {
        _Items.Add(MakeShared<FCkSmDebugger_HistoryEntry>(History[i]));
    }

    if (_ListView.IsValid())
    {
        _ListView->RequestListRefresh();

        if (_Items.Num() > 0 && _ViewModel->Get_ViewMode() == ECkSmDebugger_ViewMode::Live)
        {
            _ListView->RequestScrollIntoView(_Items[0]);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
