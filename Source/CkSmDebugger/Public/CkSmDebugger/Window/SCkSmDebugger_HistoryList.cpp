#include "CkSmDebugger/Window/SCkSmDebugger_HistoryList.h"

#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_HistoryList::
    Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkSmDebugger_ViewModel> InViewModel)
    -> void
{
    _ViewModel = InViewModel;

    ChildSlot
    [
        SAssignNew(_ListView, SListView<FHistoryItemPtr>)
            .ListItemsSource(&_Items)
            .OnGenerateRow(this, &SCkSmDebugger_HistoryList::GenerateRow)
            .OnSelectionChanged(this, &SCkSmDebugger_HistoryList::OnSelectionChanged)
            .SelectionMode(ESelectionMode::Single)
            .HeaderRow
            (
                SNew(SHeaderRow)
                + SHeaderRow::Column("Index").DefaultLabel(FText::FromString(TEXT("#"))).FixedWidth(30.0f)
                + SHeaderRow::Column("From").DefaultLabel(FText::FromString(TEXT("From"))).FillWidth(1.0f)
                + SHeaderRow::Column("To").DefaultLabel(FText::FromString(TEXT("To"))).FillWidth(1.0f)
                + SHeaderRow::Column("Frame").DefaultLabel(FText::FromString(TEXT("Frame#"))).FixedWidth(70.0f)
            )
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

auto
    SCkSmDebugger_HistoryList::
    GenerateRow(
        FHistoryItemPtr InItem,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    auto ItemIndex = _Items.IndexOfByKey(InItem);
    auto TimeStr = FString::Printf(TEXT("%llu"), InItem->FrameNumber);

    auto FromColor = CkSmDebugger::ComputeStateColor(InItem->FromStateName);
    auto ToColor = CkSmDebugger::ComputeStateColor(InItem->ToStateName);

    return SNew(STableRow<FHistoryItemPtr>, InOwnerTable)
        .Style(&FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4.0f, 2.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                        .WidthOverride(30.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::AsNumber(ItemIndex))
                                .ColorAndOpacity(FLinearColor(0.35f, 0.35f, 0.4f))
                                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                        ]
                ]
            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(4.0f, 2.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(InItem->FromStateName))
                        .ColorAndOpacity(FromColor)
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                ]
            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(4.0f, 2.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(InItem->ToStateName))
                        .ColorAndOpacity(ToColor)
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                ]
            + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4.0f, 2.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                        .WidthOverride(70.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TimeStr))
                                .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
                                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                        ]
                ]
        ];
}

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
}

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
