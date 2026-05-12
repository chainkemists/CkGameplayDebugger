#include "CkEqsDebugger/Window/SCkEqsDebugger_QueryList.h"

#include "CkEqsDebugger/CkEqsDebuggerStyle.h"
#include "CkEqsDebugger/ViewModel/CkEqsDebugger_ViewModel.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCkEqsDebugger_QueryList"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    // Map our query status enum onto the common widget's tone palette so SCkDebug_HistoryRow / StatusPill render
    // a recognisable color per status.
    auto Tone_FromStatus(ECkEqsDebugger_QueryStatus InStatus) -> ECkDebug_Tone
    {
        switch (InStatus)
        {
        case ECkEqsDebugger_QueryStatus::Pending:    return ECkDebug_Tone::Neutral;
        case ECkEqsDebugger_QueryStatus::InProgress: return ECkDebug_Tone::Info;
        case ECkEqsDebugger_QueryStatus::Complete:   return ECkDebug_Tone::Ok;
        case ECkEqsDebugger_QueryStatus::Failed:     return ECkDebug_Tone::Err;
        case ECkEqsDebugger_QueryStatus::Cancelled:  return ECkDebug_Tone::Warn;
        case ECkEqsDebugger_QueryStatus::Unknown:    return ECkDebug_Tone::Neutral;
        }
        return ECkDebug_Tone::Neutral;
    }

    auto BuildTestSummary(const TArray<ECk_Eqs_TestType>& InTestTypes) -> FString
    {
        if (InTestTypes.IsEmpty())
        { return TEXT("(no tests)"); }

        auto Out = FString{};
        for (auto i = 0; i < InTestTypes.Num(); ++i)
        {
            if (i > 0) { Out += TEXT(", "); }
            Out += CkEqsDebugger::GetTestTypeString(InTestTypes[i]);
        }
        return Out;
    }

    // Case-insensitive substring match. Empty needle = match everything.
    auto MatchesQuery(const FCkEqsDebugger_QueryInfo& InInfo, const FString& InNeedle) -> bool
    {
        if (InNeedle.IsEmpty())
        { return true; }

        if (InInfo.DebugName.Contains(InNeedle, ESearchCase::IgnoreCase))
        { return true; }
        if (CkEqsDebugger::GetGeneratorTypeString(InInfo.GeneratorType).Contains(InNeedle, ESearchCase::IgnoreCase))
        { return true; }
        if (CkEqsDebugger::GetStatusString(InInfo.Status).Contains(InNeedle, ESearchCase::IgnoreCase))
        { return true; }
        if (BuildTestSummary(InInfo.TestTypes).Contains(InNeedle, ESearchCase::IgnoreCase))
        { return true; }
        return false;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_QueryList::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    if (_ViewModel.IsValid())
    {
        _OnQueryListChangedHandle = _ViewModel->OnQueryListChanged.AddSP(this, &SCkEqsDebugger_QueryList::OnQueryListChanged);
    }

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 0.0f, 0.0f, 4.0f})
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Queries")))
            .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Secondary})
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
        ]

        + SVerticalBox::Slot().AutoHeight().Padding(FMargin{0.0f, 0.0f, 0.0f, 4.0f})
        [
            SAssignNew(_SearchBar, SCkDebug_DualSearchBar)
            .FilterHintText(FText::FromString(TEXT("Filter queries…")))
            .HighlightHintText(FText::FromString(TEXT("Highlight…")))
            .OnFilterTextChanged_Lambda([this](const FString& InText)
            {
                if (_FilterString == InText) { return; }
                _FilterString = InText;
                ApplyFilterPipeline(_LastQueries);
            })
            .OnHighlightTextChanged_Lambda([this](const FString& InText)
            {
                if (_HighlightString == InText) { return; }
                _HighlightString = InText;
                ApplyFilterPipeline(_LastQueries);
            })
        ]

        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SAssignNew(_ListView, SListView<TSharedPtr<FRowItem>>)
            .ListItemsSource(&_Items)
            .OnGenerateRow(this, &SCkEqsDebugger_QueryList::OnGenerateRow)
            .OnSelectionChanged(this, &SCkEqsDebugger_QueryList::OnSelectionChanged)
            .OnContextMenuOpening(this, &SCkEqsDebugger_QueryList::OnContextMenuOpening)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];
}

SCkEqsDebugger_QueryList::~SCkEqsDebugger_QueryList()
{
    if (_ViewModel.IsValid() && _OnQueryListChangedHandle.IsValid())
    { _ViewModel->OnQueryListChanged.Remove(_OnQueryListChangedHandle); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_QueryList::
    OnQueryListChanged(
        const TArray<FCkEqsDebugger_QueryInfo>& InQueries)
    -> void
{
    _LastQueries = InQueries;
    ApplyFilterPipeline(InQueries);
}

auto
    SCkEqsDebugger_QueryList::
    ApplyFilterPipeline(
        const TArray<FCkEqsDebugger_QueryInfo>& InQueries)
    -> void
{
    const auto SelectedHandle = _ViewModel.IsValid() ? _ViewModel->Get_SelectedQueryHandle() : FCk_Handle_EqsQuery{};

    // Reuse existing FRowItem TSharedPtr instances when the query handle is the same as last refresh —
    // SListView tracks selection by pointer identity, so allocating fresh items every Tick (~30Hz) destroys
    // the user's selection before the click can register AND visibly flickers as rows re-generate. Mirrors
    // the SCkSchedulerDebugger_ProcessorTree pattern of updating existing nodes in place across refreshes.
    auto Existing = TMap<FCk_Handle_EqsQuery, TSharedPtr<FRowItem>>{};
    Existing.Reserve(_Items.Num());
    for (const auto& I : _Items)
    {
        if (I.IsValid())
        { Existing.Add(I->Info.QueryHandle, I); }
    }

    auto NewItems = TArray<TSharedPtr<FRowItem>>{};
    NewItems.Reserve(InQueries.Num());
    auto NewSelection = TSharedPtr<FRowItem>{};
    auto SetChanged = false;

    for (const auto& Q : InQueries)
    {
        // Filter-pass: hide non-matches entirely.
        if (NOT MatchesQuery(Q, _FilterString))
        { continue; }

        auto Item = TSharedPtr<FRowItem>{};
        if (auto* Found = Existing.Find(Q.QueryHandle))
        {
            Item = *Found;       // stable pointer across refreshes
            Existing.Remove(Q.QueryHandle);
        }
        else
        {
            Item = MakeShared<FRowItem>();
            SetChanged = true;
        }

        // Update fields in-place. Row widget displays via FText::FromString snapshot at row-build time;
        // for in-progress queries that need live updates, this still works because the row is rebuilt
        // on RequestListRefresh below when the entity set changes (new-cycle queries are new entities).
        Item->Info             = Q;
        Item->IsHighlightMatch = MatchesQuery(Q, _HighlightString);

        if (ck::IsValid(SelectedHandle) && Item->Info.QueryHandle == SelectedHandle)
        { NewSelection = Item; }

        NewItems.Add(MoveTemp(Item));
    }

    // Anything left in Existing was dropped from the source list (entity destroyed) → set changed.
    if (Existing.Num() > 0)
    { SetChanged = true; }

    _Items = MoveTemp(NewItems);

    if (_ListView.IsValid())
    {
        // Only request a full refresh if the SET of items actually changed (added / removed). In-place
        // updates of existing items don't need a full refresh — the existing row widgets stay valid.
        if (SetChanged)
        { _ListView->RequestListRefresh(); }

        // Restore selection if the user's previously-selected query is still in the list. ESelectInfo::Direct
        // is filtered in OnSelectionChanged so this doesn't echo back into the ViewModel.
        if (NewSelection.IsValid())
        {
            const auto Cur = _ListView->GetSelectedItems();
            const auto AlreadySelected = (Cur.Num() == 1 && Cur[0] == NewSelection);
            if (NOT AlreadySelected)
            { _ListView->SetItemSelection(NewSelection, true, ESelectInfo::Direct); }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_QueryList::
    OnGenerateRow(
        TSharedPtr<FRowItem>              InItem,
        const TSharedRef<STableViewBase>& InOwner)
    -> TSharedRef<ITableRow>
{
    // Tone is muted to Neutral when not highlight-matched so matches stand out among visible rows.
    const auto& Info = InItem->Info;
    const auto Tone  = InItem->IsHighlightMatch
        ? Tone_FromStatus(Info.Status)
        : ECkDebug_Tone::Neutral;
    const auto ToneColor = CkDebugStyle::GetToneColor(Tone);

    const auto NameText = NOT Info.DebugName.IsEmpty() ? Info.DebugName : FString{TEXT("(unnamed)")};
    const auto MetaText = ck::Format_UE(TEXT("{}  -  {}"),
        CkEqsDebugger::GetGeneratorTypeString(Info.GeneratorType),
        BuildTestSummary(Info.TestTypes));

    const auto RightText = [&]() -> FString
    {
        if (Info.Status == ECkEqsDebugger_QueryStatus::InProgress && Info.NextTestIndex > 0)
        { return ck::Format_UE(TEXT("test {}/{}"), Info.NextTestIndex, Info.TestCount); }
        if (Info.Status == ECkEqsDebugger_QueryStatus::Complete)
        { return ck::Format_UE(TEXT("{} cand"), Info.CandidateCount); }
        return CkEqsDebugger::GetStatusString(Info.Status);
    }();

    // IMPORTANT: build the row body from widgets that DO NOT consume click events. SCkDebug_HistoryRow
    // wraps its content in an SButton that returns FReply::Handled() on every click — that traps the
    // selection click before STableRow can see it. Mirror the CkSchedulerDebugger_ProcessorTree pattern
    // (custom STextBlock/SBox/SImage tree under STableRow). Selection styling is handled by STableRow
    // itself via .ShowSelection(true); right-click context menu is wired via the SListView's
    // OnContextMenuOpening below.
    return SNew(STableRow<TSharedPtr<FRowItem>>, InOwner)
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)

            // Status dot
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin{6.0f, 0.0f, 8.0f, 0.0f})
            [
                SNew(SBox)
                .WidthOverride(8.0f)
                .HeightOverride(8.0f)
                [
                    SNew(SImage)
                    .Image(CkDebugStyle::GetFilledBrush())
                    .ColorAndOpacity(FSlateColor{ToneColor})
                ]
            ]

            // Title + meta (vertical stack)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(FMargin{0.0f, 2.0f})
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(NameText))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Primary})
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(MetaText))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Secondary})
                ]
            ]

            // Right text
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin{8.0f, 0.0f, 6.0f, 0.0f})
            [
                SNew(STextBlock)
                .Text(FText::FromString(RightText))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                .ColorAndOpacity(FSlateColor{FCkEqsDebuggerStyle::Color_Text_Secondary})
            ]
        ];
}

auto
    SCkEqsDebugger_QueryList::
    OnSelectionChanged(
        TSharedPtr<FRowItem> InItem,
        ESelectInfo::Type    InSelectInfo)
    -> void
{
    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    if (NOT _ViewModel.IsValid())
    { return; }

    if (InItem.IsValid())
    { _ViewModel->Set_SelectedQueryHandle(InItem->Info.QueryHandle); }
    else
    { _ViewModel->Set_SelectedQueryHandle(FCk_Handle_EqsQuery{}); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_QueryList::
    OnContextMenuOpening()
    -> TSharedPtr<SWidget>
{
    if (NOT _ListView.IsValid())
    { return nullptr; }

    auto Selected = _ListView->GetSelectedItems();
    if (Selected.IsEmpty() || NOT Selected[0].IsValid())
    { return nullptr; }

    const auto& Info = Selected[0]->Info;

    auto MenuBuilder = FMenuBuilder(true, nullptr);

    ck::DebugCopyMenu::AddCopyEntry(
        MenuBuilder,
        FText::FromString(TEXT("Copy Name")),
        FText::FromString(TEXT("Copy this query's debug name")),
        Info.DebugName);

    ck::DebugCopyMenu::AddCopyEntry(
        MenuBuilder,
        FText::FromString(TEXT("Copy Generator Type")),
        FText::FromString(TEXT("Copy the generator type")),
        CkEqsDebugger::GetGeneratorTypeString(Info.GeneratorType));

    ck::DebugCopyMenu::AddCopyEntry(
        MenuBuilder,
        FText::FromString(TEXT("Copy All")),
        FText::FromString(TEXT("Copy a multi-line summary of this query")),
        BuildCopyTextForItem(Info));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkEqsDebugger_QueryList::
    BuildCopyTextForItem(
        const FCkEqsDebugger_QueryInfo& InInfo) const
    -> FString
{
    return ck::Format_UE(
        TEXT("EqsQuery [{}]\n")
        TEXT("  status:    {}\n")
        TEXT("  generator: {}\n")
        TEXT("  tests:     {}\n")
        TEXT("  run mode:  {}\n")
        TEXT("  cursor:    test {}/{}\n")
        TEXT("  results:   {} candidate(s) (HasResults={})\n")
        TEXT("  best loc:  ({:.0f}, {:.0f}, {:.0f})"),
        InInfo.DebugName,
        CkEqsDebugger::GetStatusString(InInfo.Status),
        CkEqsDebugger::GetGeneratorTypeString(InInfo.GeneratorType),
        BuildTestSummary(InInfo.TestTypes),
        CkEqsDebugger::GetRunModeString(InInfo.RunMode),
        InInfo.NextTestIndex, InInfo.TestCount,
        InInfo.CandidateCount, InInfo.HasResults ? TEXT("true") : TEXT("false"),
        InInfo.BestLocation.X, InInfo.BestLocation.Y, InInfo.BestLocation.Z);
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------
