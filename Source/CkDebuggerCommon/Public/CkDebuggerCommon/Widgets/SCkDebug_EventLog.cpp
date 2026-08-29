#include "CkDebuggerCommon/Widgets/SCkDebug_EventLog.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SCkDebug_EventLog"

// ====================================================================================================================

namespace ck_debug_event_log
{
    auto Format_Timestamp(double InSeconds) -> FString
    {
        const auto Total = FMath::Max(0.0, InSeconds);
        const auto Minutes = static_cast<int32>(Total) / 60;
        const auto Seconds = static_cast<int32>(Total) % 60;
        const auto Millis = static_cast<int32>((Total - FMath::FloorToDouble(Total)) * 1000.0);

        return FString::Printf(TEXT("%02d:%02d.%03d"), Minutes, Seconds, Millis);
    }

    // Rows keep their TSharedPtr identity across refreshes, so a row built before a RowDensity flip
    // is the row that has to move. STableRow::Padding is a TAttribute — bind it and the axis lands
    // on every existing row with no regeneration and no loss of selection.
    auto Get_RowPadding() -> FMargin
    {
        return ck::debug_axes::Get_RowPadding(UCkDebuggerStyleSettings::Get_Selection());
    }
}

// ====================================================================================================================
// CONSTRUCT
// ====================================================================================================================

auto
    SCkDebug_EventLog::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _MaxEntries           = FMath::Max(1, InArgs._MaxEntries);
    _ShowTimestamps       = InArgs._ShowTimestamps;
    _AutoScroll           = InArgs._AutoScroll;
    _TimestampColumnWidth = InArgs._TimestampColumnWidth;
    _SelectedId           = InArgs._SelectedId;
    _OnEntrySelected      = InArgs._OnEntrySelected;

    ChildSlot
    [
        SNew(SOverlay)

        + SOverlay::Slot()
        [
            SAssignNew(_ListView, SListView<FEntryPtr>)
            .ListItemsSource(&_Entries)
            .SelectionMode(ESelectionMode::Multi)
            .OnGenerateRow(this, &SCkDebug_EventLog::Handle_GenerateRow)
            .OnSelectionChanged(this, &SCkDebug_EventLog::Handle_SelectionChanged)
            .OnContextMenuOpening(this, &SCkDebug_EventLog::Handle_ContextMenuOpening)
        ]

        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(InArgs._EmptyText)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
            .Visibility(this, &SCkDebug_EventLog::Get_EmptyVisibility)
        ]
    ];
}

// ====================================================================================================================
// CONTENT
// ====================================================================================================================

auto
    SCkDebug_EventLog::
    Add_Entry(
        FCkDebug_EventLogEntry InEntry)
    -> void
{
    _Entries.Add(MakeShared<FCkDebug_EventLogEntry>(MoveTemp(InEntry)));

    Do_TrimToCap();
    Do_RefreshList(_AutoScroll);
}

auto
    SCkDebug_EventLog::
    Add_Entries(
        TArray<FCkDebug_EventLogEntry> InEntries)
    -> void
{
    if (InEntries.IsEmpty())
    { return; }

    for (auto& Entry : InEntries)
    { _Entries.Add(MakeShared<FCkDebug_EventLogEntry>(MoveTemp(Entry))); }

    Do_TrimToCap();
    Do_RefreshList(_AutoScroll);
}

auto
    SCkDebug_EventLog::
    Set_Entries(
        TArray<FCkDebug_EventLogEntry> InEntries)
    -> void
{
    _Entries.Reset(InEntries.Num());

    for (auto& Entry : InEntries)
    { _Entries.Add(MakeShared<FCkDebug_EventLogEntry>(MoveTemp(Entry))); }

    Do_TrimToCap();
    Do_RefreshList(_AutoScroll);
}

auto
    SCkDebug_EventLog::
    Clear_Entries()
    -> void
{
    _Entries.Reset();
    Do_RefreshList(false);
}

auto
    SCkDebug_EventLog::
    Get_EntryCount() const
    -> int32
{
    return _Entries.Num();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_EventLog::
    Do_TrimToCap()
    -> void
{
    const auto Excess = _Entries.Num() - _MaxEntries;

    if (Excess <= 0)
    { return; }

    // Evict from the FRONT so the surviving entries keep their TSharedPtr — the list view tracks
    // selection by pointer identity.
    _Entries.RemoveAt(0, Excess, EAllowShrinking::No);
}

auto
    SCkDebug_EventLog::
    Do_RefreshList(
        bool InScrollToEnd)
    -> void
{
    if (NOT _ListView.IsValid())
    { return; }

    _ListView->RequestListRefresh();
    Do_RestoreSelection();

    // ScrollIntoView, NEVER RequestNavigateToItem: navigate scrolls AND moves keyboard focus onto
    // the list, so a log that streams entries while the user plays steals the game's input on
    // every append (the VisualLod debugger's Recent Activity pane, live incident)
    if (InScrollToEnd && NOT _Entries.IsEmpty())
    { _ListView->RequestScrollIntoView(_Entries.Last()); }
}

auto
    SCkDebug_EventLog::
    Do_RestoreSelection()
    -> void
{
    if (NOT _ListView.IsValid() || NOT _SelectedId.IsBound())
    { return; }

    const auto TargetId = _SelectedId.Get(INDEX_NONE);

    if (TargetId == INDEX_NONE)
    { return; }

    const auto* Found = _Entries.FindByPredicate([TargetId](const FEntryPtr& InEntry)
    {
        return InEntry.IsValid() && InEntry->SelectionId == TargetId;
    });

    if (Found == nullptr)
    { return; }

    // Compare before setting so the programmatic restore does not echo back as a user selection.
    const auto Current = _ListView->GetSelectedItems();
    const auto AlreadySelected = Current.Num() == 1 && Current[0] == *Found;

    if (NOT AlreadySelected)
    { _ListView->SetItemSelection(*Found, true, ESelectInfo::Direct); }
}

// ====================================================================================================================
// ROWS
// ====================================================================================================================

auto
    SCkDebug_EventLog::
    Handle_GenerateRow(
        FEntryPtr InEntry,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    const auto& Selection = UCkDebuggerStyleSettings::Get_Selection();

    auto Row = SNew(SHorizontalBox);

    if (_ShowTimestamps)
    {
        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(_TimestampColumnWidth)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(
                        InEntry.IsValid() ? ck_debug_event_log::Format_Timestamp(InEntry->TimeSeconds) : FString{}))
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                ]
            ];
    }

    if (InEntry.IsValid() && NOT InEntry->Category.IsEmpty())
    {
        Row->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [
                // Presentation only — every Make_Chip variant is SBorder/SBox/STextBlock, so the
                // row's selection click still bubbles to STableRow.
                ck::debug_axes::Make_Chip(Selection, FText::FromString(InEntry->Category), InEntry->Tone)
            ];
    }

    Row->AddSlot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(FText::FromString(InEntry.IsValid() ? InEntry->Message : FString{}))
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor{
                InEntry.IsValid() ? CkStyle::GetToneColor(InEntry->Tone) : CkStyle::Text()})
        ];

    return SNew(STableRow<FEntryPtr>, InOwnerTable)
        .Padding(TAttribute<FMargin>::CreateStatic(&ck_debug_event_log::Get_RowPadding))
        .ShowSelection(true)
        [
            Row
        ];
}

auto
    SCkDebug_EventLog::
    Handle_SelectionChanged(
        FEntryPtr InEntry,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    // Ignore the programmatic restore so it cannot echo back into the owner's state.
    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    if (NOT InEntry.IsValid() || InEntry->SelectionId == INDEX_NONE)
    { return; }

    _OnEntrySelected.ExecuteIfBound(InEntry->SelectionId);
}

auto
    SCkDebug_EventLog::
    Handle_ContextMenuOpening()
    -> TSharedPtr<SWidget>
{
    if (NOT _ListView.IsValid())
    { return nullptr; }

    const auto Selected = _ListView->GetSelectedItems();

    if (Selected.IsEmpty())
    { return nullptr; }

    auto Lines = TArray<FString>{};
    Lines.Reserve(Selected.Num());

    for (const auto& Entry : Selected)
    {
        if (Entry.IsValid())
        { Lines.Add(Compose_CopyLine(*Entry)); }
    }

    if (Lines.IsEmpty())
    { return nullptr; }

    auto MenuBuilder = FMenuBuilder{true, nullptr};

    ck::DebugCopyMenu::AddCopyEntry(
        MenuBuilder,
        Selected.Num() > 1
            ? LOCTEXT("CopyEvents", "Copy Events")
            : LOCTEXT("CopyEvent", "Copy Event"),
        LOCTEXT("CopyEventTooltip", "Copy the selected event line(s) to the clipboard"),
        FString::Join(Lines, TEXT("\n")));

    return MenuBuilder.MakeWidget();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_EventLog::
    Get_EmptyVisibility() const
    -> EVisibility
{
    return _Entries.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

auto
    SCkDebug_EventLog::
    Compose_CopyLine(
        const FCkDebug_EventLogEntry& InEntry)
    -> FString
{
    const auto Timestamp = ck_debug_event_log::Format_Timestamp(InEntry.TimeSeconds);

    return InEntry.Category.IsEmpty()
        ? ck::Format_UE(TEXT("[{}] {}"), Timestamp, InEntry.Message)
        : ck::Format_UE(TEXT("[{}] {}  {}"), Timestamp, InEntry.Category, InEntry.Message);
}

// ====================================================================================================================

#undef LOCTEXT_NAMESPACE
