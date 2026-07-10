#include "CkSmDebugger/Window/SCkSmDebugger_HistoryList.h"

#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkEditorTools/Style/CkStyle.h"
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
        SNew(SVerticalBox)

        // Filter / Highlight inputs — same dual-bar widget used by the other
        // CK debuggers. Filter narrows the visible rows, Highlight dims rows
        // that don't match within the filtered set.
        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f, 2.0f, 4.0f, 2.0f)
            [
                SNew(SCkDebug_DualSearchBar)
                    .FilterHintText(NSLOCTEXT("CkSmDebugger", "HistoryFilterHint", "Filter transitions…"))
                    .HighlightHintText(NSLOCTEXT("CkSmDebugger", "HistoryHighlightHint", "Highlight…"))
                    .OnFilterTextChanged_Lambda([this](const FString& InText)
                    {
                        if (_FilterString == InText) { return; }
                        _FilterString = InText;
                        _LastHistoryCount = -1; // force RebuildList to actually rebuild
                        RebuildList();
                    })
                    .OnHighlightTextChanged_Lambda([this](const FString& InText)
                    {
                        if (_HighlightString == InText) { return; }
                        _HighlightString = InText;
                        // Highlight only affects per-row tint via lambdas — no rebuild needed.
                    })
            ]

        + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(_ListView, SListView<FHistoryItemPtr>)
                    .ListItemsSource(&_Items)
                    .OnGenerateRow(this, &SCkSmDebugger_HistoryList::GenerateRow)
                    .OnSelectionChanged(this, &SCkSmDebugger_HistoryList::OnSelectionChanged)
                    .SelectionMode(ESelectionMode::Single)
            ]
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

    // Scroll the row matching the scrub needle into view, but DON'T touch SListView's
    // selection — selection is user-only. The visual highlight for the scrub-active
    // row is applied per-row via a TAttribute background color in GenerateRow, which
    // updates each frame without any feedback into selection state.
    if (NOT _ViewModel.IsValid() || NOT _ListView.IsValid()) { return; }
    if (_ViewModel->Get_ViewMode() != ECkSmDebugger_ViewMode::Scrub) { return; }

    auto SmInfo = _ViewModel->Get_CurrentSmInfo();
    if (NOT SmInfo) { return; }

    auto& ScrubState = _ViewModel->Get_ScrubState();
    auto RunIdx = ScrubState.SelectedRunIndex;
    auto& Run = (RunIdx < 0)
        ? SmInfo->CurrentRun
        : (RunIdx < SmInfo->CompletedRuns.Num() ? SmInfo->CompletedRuns[RunIdx] : SmInfo->CurrentRun);

    if (Run.History.IsEmpty()) { return; }

    auto ScrubAbs = ScrubState.ScrubTime + Run.StartTime;
    auto BestHistIdx = -1;
    for (auto i = 0; i < Run.History.Num(); ++i)
    {
        if (Run.History[i].RealTimeSeconds <= ScrubAbs)
        { BestHistIdx = i; }
        else
        { break; }
    }

    if (BestHistIdx < 0 || BestHistIdx >= Run.History.Num()) { return; }

    auto ListIdx = _Items.Num() - 1 - BestHistIdx;
    if (ListIdx < 0 || ListIdx >= _Items.Num()) { return; }

    // Re-scroll only when the matching row changes — avoids constant scroll thrash.
    if (_LastScrubScrollIdx != ListIdx)
    {
        _ListView->RequestScrollIntoView(_Items[ListIdx]);
        _LastScrubScrollIdx = ListIdx;
    }
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

    const auto MonoSmall = FCoreStyle::GetDefaultFontStyle("Mono", CkStyle::FontSizeSmall());
    const auto BoldBody  = FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeBody());
    const auto RegSmall  = FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeSmall());

    auto Line = SNew(SHorizontalBox)

        // Frame number — mono, dim.
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(FrameStr))
            .Font(MonoSmall)
            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
        ]

        // Delta from previous transition — mono, accent colour (the important
        // signal when scanning the list for spikes / long gaps).
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(DeltaStr))
            .Font(MonoSmall)
            .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
            .Visibility(DeltaStr.IsEmpty() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible)
        ]

        // Transition title — bold.
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TitleStr))
            .Font(BoldBody)
            .ColorAndOpacity(FSlateColor(CkStyle::Text()))
        ]

        // Conditions — dim, regular.
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(FText::FromString(CondStr))
            .Font(RegSmall)
            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
        ]

        // Spacer so chips go to the right edge.
        + SHorizontalBox::Slot().FillWidth(1.0f)
        [ SNew(SSpacer) ]

        // Task chips — on the same line, right-aligned.
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
        [ BuildTaskChips(InItem->TaskSnapshots, true) ];

    // Compact one-line clipboard payload mirroring the visible row.
    const auto CopyStr = DeltaStr.IsEmpty()
        ? FString::Printf(TEXT("%s  %s  %s"), *FrameStr, *TitleStr, *CondStr)
        : FString::Printf(TEXT("%s  %s  %s  %s"), *FrameStr, *DeltaStr, *TitleStr, *CondStr);

    // Scrub-active row tint. A TAttribute checks each frame whether this row's
    // history entry brackets the scrub needle's time; when it does, the SBorder
    // brightens to a soft amber. Independent of SListView selection so user clicks
    // remain authoritative for selection state.
    auto ItemFrame = InItem->FrameNumber;
    auto ItemTime = InItem->RealTimeSeconds;
    auto TintAttr = TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda(
        [ViewModelWeak = TWeakPtr<FCkSmDebugger_ViewModel>(_ViewModel), ItemFrame, ItemTime]() -> FSlateColor
        {
            auto VM = ViewModelWeak.Pin();
            if (NOT VM.IsValid() || VM->Get_ViewMode() != ECkSmDebugger_ViewMode::Scrub)
            { return FLinearColor(1.0f, 1.0f, 1.0f, 0.0f); }

            auto* Info = VM->Get_CurrentSmInfo();
            if (NOT Info) { return FLinearColor(1.0f, 1.0f, 1.0f, 0.0f); }

            auto& SS = VM->Get_ScrubState();
            auto RIdx = SS.SelectedRunIndex;
            auto& R = (RIdx < 0)
                ? Info->CurrentRun
                : (RIdx < Info->CompletedRuns.Num() ? Info->CompletedRuns[RIdx] : Info->CurrentRun);

            // This row matches if its time is the latest history entry whose time is <=
            // the scrubbed absolute time. Walk the run history once to find that entry.
            auto ScrubAbs = SS.ScrubTime + R.StartTime;
            auto BestTime = -DBL_MAX;
            for (auto& Entry : R.History)
            {
                if (Entry.RealTimeSeconds <= ScrubAbs && Entry.RealTimeSeconds > BestTime)
                { BestTime = Entry.RealTimeSeconds; }
            }
            if (BestTime == ItemTime)
            { return FLinearColor(1.0f, 0.6f, 0.1f, 0.35f); }
            return FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);
        }));

    // Highlight dimming — when the dual-search Highlight input has text and this
    // row doesn't match, fade the row to draw the eye toward matching rows. Use
    // SBorder::ColorAndOpacity (bindable, multiplies child content's color); the
    // RenderOpacity slate arg is fixed-value-only.
    auto EntryCopy = *InItem;
    auto ContentTintAttr = TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateLambda(
        [ListWeak = TWeakPtr<SCkSmDebugger_HistoryList>(SharedThis(this)), EntryCopy]() -> FLinearColor
        {
            auto Pinned = ListWeak.Pin();
            if (NOT Pinned.IsValid()) { return FLinearColor::White; }
            if (Pinned->_HighlightString.IsEmpty()) { return FLinearColor::White; }
            return Pinned->MatchesFilter(EntryCopy, Pinned->_HighlightString)
                ? FLinearColor::White
                : FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);
        }));

    return SNew(STableRow<FHistoryItemPtr>, InOwnerTable)
        .Style(&FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
        [
            SNew(SCkDebug_CopyableContainer)
            .CopyText(CopyStr)
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(TintAttr)
                .ColorAndOpacity(ContentTintAttr)
                .Padding(FMargin(CkStyle::SpaceS, 2.0f))
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

    // Skip programmatic selections (Tick auto-mirrors the scrub needle into the list).
    // ESelectInfo::Direct is what SListView::SetItemSelection passes through. User clicks
    // come through as OnMouseClick/OnKeyPress/OnNavigation. Filtering on Direct directly
    // is more robust than a flag — Slate may defer the selection-change broadcast past
    // our flag's reset window, which would otherwise drop user clicks.
    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    _ViewModel->Set_ViewMode(ECkSmDebugger_ViewMode::Scrub);

    auto NewScrubState = _ViewModel->Get_ScrubState();
    NewScrubState.ViewMode = ECkSmDebugger_ViewMode::Scrub;

    // History entries store absolute logical time (ComputeLogicalTime'd FPlatformTime::Seconds).
    // Timeline segments and the scrub cursor live in run-relative space — subtract Run.StartTime
    // so the playhead lands on the correct segment instead of jumping to ~17M seconds off-screen.
    auto SmInfo = _ViewModel->Get_CurrentSmInfo();
    auto RunRelative = InItem->RealTimeSeconds;
    if (SmInfo)
    {
        auto& Run = (NewScrubState.SelectedRunIndex < 0)
            ? SmInfo->CurrentRun
            : (NewScrubState.SelectedRunIndex < SmInfo->CompletedRuns.Num()
                ? SmInfo->CompletedRuns[NewScrubState.SelectedRunIndex]
                : SmInfo->CurrentRun);
        RunRelative = InItem->RealTimeSeconds - Run.StartTime;
    }
    NewScrubState.ScrubTime = RunRelative;
    NewScrubState.TimelineScrollX = 0.0f;

    auto ItemIndex = _Items.IndexOfByKey(InItem);
    NewScrubState.SelectedHistoryIndex = ItemIndex;

    _ViewModel->Set_ScrubState(NewScrubState);

    // Highlight the taken transition on the graph
    for (auto i = 0; i < _Items.Num(); ++i)
    {
        if (_Items[i] == InItem)
        {
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

    // _LastHistoryCount short-circuits when neither history nor filter changed.
    // Filter changes set _LastHistoryCount = -1 to force a rebuild.
    if (History.Num() == _LastHistoryCount)
    { return; }

    _LastHistoryCount = History.Num();
    _Items.Empty(History.Num());

    // Reverse order — most recent transition first. Apply filter as we go.
    for (auto i = History.Num() - 1; i >= 0; --i)
    {
        if (NOT _FilterString.IsEmpty() && NOT MatchesFilter(History[i], _FilterString))
        { continue; }
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

auto
    SCkSmDebugger_HistoryList::
    MatchesFilter(
        const FCkSmDebugger_HistoryEntry& InEntry,
        const FString& InText) const
    -> bool
{
    if (InText.IsEmpty()) { return true; }

    auto Lower = InText.ToLower();
    auto Test = [&Lower](const FString& InCandidate)
    {
        return InCandidate.ToLower().Contains(Lower);
    };

    if (Test(InEntry.ToStateName)) { return true; }
    if (Test(InEntry.FromStateName)) { return true; }
    for (auto& Cond : InEntry.ConditionNames)
    {
        if (Test(Cond)) { return true; }
    }
    if (Test(FString::Printf(TEXT("%llu"), InEntry.FrameNumber))) { return true; }

    return false;
}

// --------------------------------------------------------------------------------------------------------------------
