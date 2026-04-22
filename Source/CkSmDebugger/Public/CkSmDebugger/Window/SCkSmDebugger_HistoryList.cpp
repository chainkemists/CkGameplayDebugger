#include "CkSmDebugger/Window/SCkSmDebugger_HistoryList.h"

#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CopyableContainer.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_HistoryRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

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

        // On world change the selected SM goes away and OnSmDataRefreshed stops firing,
        // so we also listen to the list-level change to drop stale history rows.
        _SmListChangedHandle = _ViewModel->OnSmListChanged.AddLambda(
            [this](const TArray<FCkSmDebugger_SmInfo>& InList)
            {
                if (InList.Num() == 0 && _Items.Num() > 0)
                {
                    _Items.Empty();
                    _LastHistoryCount = 0;
                    if (_ListView.IsValid())
                    { _ListView->RequestListRefresh(); }
                }
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
    // Single-line layout — maximises rows-per-screen for transition history:
    //
    //   #Frame  +Delta  ToState ◀── FromState  via cond1, cond2       [chips]
    //   └dim ──┘└accent┘└────── bold ────────┘└──── dim ────┘
    //
    // The SCkDebug_HistoryRow primitive doesn't fit this exact layout (it's a
    // title + right-text + subtitle-below shape), so the row is assembled
    // manually from the shared style tokens instead.

    const auto Depth   = _Graph ? _Graph->LayoutParams.NameDepth : 1;
    const auto Index   = _Items.IndexOfByKey(InItem);
    const auto FromName = FCkSmLayoutParams::ComputeDisplayName(InItem->FromStateName, Depth);
    const auto ToName   = FCkSmLayoutParams::ComputeDisplayName(InItem->ToStateName,   Depth);

    const auto FrameStr = FString::Printf(TEXT("#%llu"), InItem->FrameNumber);
    auto DeltaStr = FString{};
    if (Index + 1 < _Items.Num())
    {
        const auto Delta = InItem->FrameNumber - _Items[Index + 1]->FrameNumber;
        DeltaStr = FString::Printf(TEXT("+%llu"), Delta);
    }

    auto CondStr = FString{};
    {
        auto Parts = TArray<FString>{};
        for (const auto& Name : InItem->ConditionNames)
        { Parts.Add(FCkSmLayoutParams::ComputeDisplayName(Name, Depth)); }
        CondStr = Parts.Num() > 0
            ? FString::Printf(TEXT("via %s"), *FString::Join(Parts, TEXT(", ")))
            : FString(TEXT("(unconditional)"));
    }

    const auto TitleStr = FString::Printf(TEXT("%s  \u25C0\u2500  %s"), *ToName, *FromName);

    const auto MonoSmall = FCoreStyle::GetDefaultFontStyle("Mono", CkDebugStyle::FontSizeSmall());
    const auto BoldBody  = FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeBody());
    const auto RegSmall  = FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall());

    auto Line = SNew(SHorizontalBox)

        // Frame number — mono, dim.
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkDebugStyle::SpaceS, 0.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(FrameStr))
            .Font(MonoSmall)
            .ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()))
        ]

        // Delta from previous transition — mono, accent colour (the important
        // signal when scanning the list for spikes / long gaps).
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(DeltaStr))
            .Font(MonoSmall)
            .ColorAndOpacity(FSlateColor(CkDebugStyle::Accent()))
            .Visibility(DeltaStr.IsEmpty() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible)
        ]

        // Transition title — bold.
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TitleStr))
            .Font(BoldBody)
            .ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
        ]

        // Conditions — dim, regular.
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(FText::FromString(CondStr))
            .Font(RegSmall)
            .ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
        ]

        // Spacer so chips go to the right edge.
        + SHorizontalBox::Slot().FillWidth(1.0f)
        [ SNew(SSpacer) ]

        // Task chips — on the same line, right-aligned.
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            .Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
        [ BuildTaskChips(InItem->TaskSnapshots, true) ];

    // Compact one-line clipboard payload mirroring the visible row.
    const auto CopyStr = DeltaStr.IsEmpty()
        ? FString::Printf(TEXT("%s  %s  %s"), *FrameStr, *TitleStr, *CondStr)
        : FString::Printf(TEXT("%s  %s  %s  %s"), *FrameStr, *DeltaStr, *TitleStr, *CondStr);

    return SNew(STableRow<FHistoryItemPtr>, InOwnerTable)
        .Style(&FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
        [
            SNew(SCkDebug_CopyableContainer)
            .CopyText(CopyStr)
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
                .Padding(FMargin(CkDebugStyle::SpaceS, 2.0f))
                [ Line ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------
// Task chips
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

    const auto Depth = _Graph ? _Graph->LayoutParams.NameDepth : 1;
    auto Box = SNew(SHorizontalBox);

    for (const auto& Snap : InSnapshots)
    {
        const auto ResultColor = CkSmDebugger::GetTaskResultColor(Snap.Result);
        auto Icon = FString{};
        switch (Snap.Result)
        {
        case ECk_SmTaskResult::Running:   Icon = TEXT("\x25CF"); break;
        case ECk_SmTaskResult::Succeeded: Icon = TEXT("\x2713"); break;
        case ECk_SmTaskResult::Failed:    Icon = TEXT("\x2717"); break;
        }

        const auto DisplayName = InShortNames
            ? FCkSmLayoutParams::ComputeDisplayName(Snap.TaskName, Depth)
            : Snap.TaskName;

        Box->AddSlot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_CountBadge)
                    .ValueText(FText::FromString(DisplayName))
                    .SuffixText(FText::FromString(Icon))
                    .ValueColor(ResultColor)
                    .SuffixColor(ResultColor)
                    .BorderColor(FLinearColor(ResultColor.R, ResultColor.G, ResultColor.B, 0.4f))
            ];
    }

    return Box;
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
