#include "CkDebuggerCommon/Widgets/SCkDebug_EventLog.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/SNullWidget.h"
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

    auto Tokens(float InTimestampWidth) -> FCkUiView::FTokens
    {
        return {{TEXT("--event-log-muted"), TEXT("#") + CkStyle::TextMute().ToFColorSRGB().ToHex()},
            {TEXT("--event-log-small"), FString::FromInt(CkStyle::FontSizeSmall()) + TEXT("px")},
            {TEXT("--event-log-micro"), FString::FromInt(CkStyle::FontSizeMicro()) + TEXT("px")},
            {TEXT("--event-log-timestamp-width"), FString::SanitizeFloat(InTimestampWidth) + TEXT("px")},
            {TEXT("--event-log-gap"), FString::SanitizeFloat(CkStyle::SpaceS) + TEXT("px")}};
    }
}

struct SCkDebug_EventLog::FAuthoredState
{
    struct FRow
    {
        TSharedPtr<FCkUiView> View;
        TWeakPtr<SBox> Host;
    };
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    TSharedPtr<FCkUiView> Shell;
    TSharedPtr<FCkUiView> RowPrototype;
    TMap<FEntryPtr, FRow> Rows;
    TArray<FString> AcceptedSources;
    TArray<FString> AttemptedSources;
    FCkUiView::FTokens AttemptedTokens;
    FString Directory;
    FString Failure;
    double NextPollSeconds = 0.0;
    bool Accepted = false;
    bool SourceUnavailable = false;
};

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
    _EmptyText            = InArgs._EmptyText;

    SAssignNew(_ListView, SListView<FEntryPtr>)
        .ListItemsSource(&_Entries)
        .SelectionMode(ESelectionMode::Multi)
        .OnGenerateRow(this, &SCkDebug_EventLog::Handle_GenerateRow)
        .OnSelectionChanged(this, &SCkDebug_EventLog::Handle_SelectionChanged)
        .OnContextMenuOpening(this, &SCkDebug_EventLog::Handle_ContextMenuOpening);

    ChildSlot[DoCreate_NativePresentation()];
    if (InArgs._UseAuthoredPresentation)
    {
        const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
        if (NOT Plugin.IsValid()) { return; }
        _Authored = MakeShared<FAuthoredState>();
        if (NOT FCkDebug_UiRegistry::TryCreate(_Authored->Registry).Succeeded)
        { _Authored.Reset(); return; }
        _Authored->Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
        auto Data = FCkUiView::FDataBindings{};
        const TWeakPtr<SCkDebug_EventLog> WeakLog = SharedThis(this);
        Data.Text.Add(TEXT("event-log-empty"), TAttribute<FText>::CreateLambda([WeakLog]()
        { const auto Log = WeakLog.Pin(); return Log.IsValid() ? Log->_EmptyText : FText::GetEmpty(); }));
        Data.Visibility.Add(TEXT("event-log-empty"), TAttribute<bool>::CreateLambda([WeakLog]()
        { const auto Log = WeakLog.Pin(); return Log.IsValid() && Log->_Entries.IsEmpty(); }));
        Data.CanDispatchEvents = TAttribute<bool>(false);
        _Authored->Shell = FCkUiView::Create({{TEXT("event-log-list"), _ListView}}, {},
            ck_debug_event_log::Tokens(_TimestampColumnWidth), CkStyle::RegularFont(CkStyle::FontSizeSmall()), MoveTemp(Data));
        _Authored->Shell->GetRegion(TEXT("main"));
        _Authored->RowPrototype = DoCreate_AuthoredRowView({});
        Poll_AuthoredPresentation();
    }
}

SCkDebug_EventLog::~SCkDebug_EventLog()
{
    // External row widgets may outlive the log; their attributes only pin a weak owner.
    ChildSlot[SNullWidget::NullWidget];
    if (_ListView.IsValid()) { _ListView->ClearItemsSource(); }
    _Authored.Reset();
}

auto SCkDebug_EventLog::DoCreate_NativePresentation() -> TSharedRef<SWidget>
{
    return SNew(SOverlay)
        + SOverlay::Slot()[_ListView.ToSharedRef()]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(_EmptyText)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                .Visibility(this, &SCkDebug_EventLog::Get_EmptyVisibility)
        ];
}

auto SCkDebug_EventLog::Tick(const FGeometry&, double InCurrentTime, float) -> void
{
    if (NOT _Authored.IsValid() || InCurrentTime < _Authored->NextPollSeconds) { return; }
    _Authored->NextPollSeconds = InCurrentTime + 0.5;
    Poll_AuthoredPresentation();
}

auto SCkDebug_EventLog::Get_UsesAuthoredPresentation() const -> bool
{ return _Authored.IsValid() && _Authored->Accepted; }

auto SCkDebug_EventLog::Get_AuthoredView() const -> TSharedPtr<FCkUiView>
{ return _Authored.IsValid() ? _Authored->Shell : nullptr; }

auto SCkDebug_EventLog::Get_AuthoredFailure() const -> FString
{ return _Authored.IsValid() ? _Authored->Failure : FString{}; }

auto SCkDebug_EventLog::Get_ContainsEntry(const FEntryPtr& InEntry) const -> bool
{ return InEntry.IsValid() && _Entries.Contains(InEntry); }

auto SCkDebug_EventLog::DoCreate_AuthoredRowView(FEntryPtr InEntry) -> TSharedRef<FCkUiView>
{
    const TWeakPtr<SCkDebug_EventLog> WeakLog = SharedThis(this);
    const TWeakPtr<FCkDebug_EventLogEntry> WeakEntry = InEntry;
    const auto Current = [WeakLog, WeakEntry]() -> FEntryPtr
    {
        const auto Log = WeakLog.Pin(); const auto Entry = WeakEntry.Pin();
        return Log.IsValid() && Log->Get_ContainsEntry(Entry) ? Entry : nullptr;
    };
    const auto Category = ck::debug_axes::Make_Chip(UCkDebuggerStyleSettings::Get_Selection(),
        FText::FromString(InEntry.IsValid() ? InEntry->Category : FString{}),
        InEntry.IsValid() ? InEntry->Tone : ECk_Tone::Neutral);
    auto Data = FCkUiView::FDataBindings{};
    Data.CanDispatchEvents = TAttribute<bool>(false); // Row selection belongs exclusively to STableRow.
    Data.Text.Add(TEXT("event-time"), TAttribute<FText>::CreateLambda([Current]()
    { const auto Entry = Current(); return Entry.IsValid() ? FText::FromString(ck_debug_event_log::Format_Timestamp(Entry->TimeSeconds)) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("event-message"), TAttribute<FText>::CreateLambda([Current]()
    { const auto Entry = Current(); return Entry.IsValid() ? FText::FromString(Entry->Message) : FText::GetEmpty(); }));
    Data.Visibility.Add(TEXT("event-timestamp"), TAttribute<bool>::CreateLambda([WeakLog, Current]()
    { const auto Log = WeakLog.Pin(); return Log.IsValid() && Log->_ShowTimestamps && Current().IsValid(); }));
    Data.Visibility.Add(TEXT("event-category"), TAttribute<bool>::CreateLambda([Current]()
    { const auto Entry = Current(); return Entry.IsValid() && NOT Entry->Category.IsEmpty(); }));
    Data.Color.Add(TEXT("event-foreground"), TAttribute<FLinearColor>::CreateLambda([Current]()
    { const auto Entry = Current(); return Entry.IsValid() ? CkStyle::GetToneColor(Entry->Tone) : CkStyle::Text(); }));
    const auto View = FCkUiView::Create({{TEXT("event-category-chip"), Category}}, {}, ck_debug_event_log::Tokens(_TimestampColumnWidth),
        CkStyle::RegularFont(CkStyle::FontSizeSmall()), MoveTemp(Data), _Authored->Registry);
    View->GetRegion(TEXT("main"));
    return View;
}

auto SCkDebug_EventLog::TryApply_AuthoredSources(const TArray<FString>& InSources) -> bool
{
    if (NOT _Authored.IsValid() || InSources.Num() != 4) { return false; }
    const auto Tokens = ck_debug_event_log::Tokens(_TimestampColumnWidth);
    auto RowDocument = FCkUiDocument{};
    const auto Parsed = FCkUiDocumentParser::TryParse(InSources[2], InSources[3], Tokens, RowDocument,
        TEXT("DebuggerEventLogRow"), _Authored->Registry);
    if (NOT Parsed.Succeeded)
    { _Authored->Failure = FString::Join(Parsed.Errors, TEXT("\n")); return false; }
    const auto Passive = [](const FCkUiNode& Node, auto&& Self) -> bool
    {
        if (Node.Kind != ECkUiNodeKind::Row && Node.Kind != ECkUiNodeKind::Column
            && Node.Kind != ECkUiNodeKind::Text && Node.Kind != ECkUiNodeKind::Overlay
            && NOT (Node.Kind == ECkUiNodeKind::Native && Node.Binding == TEXT("event-category-chip"))) { return false; }
        for (const auto& Child : Node.Children) { if (NOT Self(Child, Self)) { return false; } }
        return true;
    };
    for (const auto& Region : RowDocument.Regions)
    {
        if (NOT RowDocument.Menus.IsEmpty() || NOT Passive(Region.Value, Passive))
        { _Authored->Failure = TEXT("Event log rows require passive authored content so STableRow owns selection."); return false; }
    }
    auto Requests = TArray<FCkUiView::FReloadRequest>{};
    Requests.Add({_Authored->Shell, InSources[0], InSources[1], TEXT("DebuggerEventLog"), Tokens});
    Requests.Add({_Authored->RowPrototype, InSources[2], InSources[3], TEXT("DebuggerEventLogRow"), Tokens});
    for (auto It = _Authored->Rows.CreateIterator(); It; ++It)
    {
        if (NOT Get_ContainsEntry(It.Key()) || NOT It.Value().Host.IsValid()) { It.RemoveCurrent(); continue; }
        Requests.Add({It.Value().View, InSources[2], InSources[3], TEXT("DebuggerEventLogRow"), Tokens});
    }
    struct FDetachedRow
    {
        TSharedPtr<SBox> Host;
        TSharedRef<SWidget> Content;
    };
    auto DetachedRows = TArray<FDetachedRow>{};
    DetachedRows.Reserve(_Authored->Rows.Num());
    for (const auto& Pair : _Authored->Rows)
    {
        if (const auto Host = Pair.Value.Host.Pin())
        {
            DetachedRows.Add({Host, ConstCastSharedRef<SWidget>(Host->GetChildren()->GetChildAt(0))});
            Host->SetContent(SNullWidget::NullWidget);
        }
    }
    // The shell owns the native list, whose live row widgets otherwise recursively overlap the
    // independently reloaded row views. Detachment keeps participant ownership disjoint.
    // The fallback owns the list. Release that parent before staging its retained authored port.
    if (NOT _Authored->Accepted) { ChildSlot[SNullWidget::NullWidget]; }
    const auto Result = FCkUiView::TryReloadBatch(Requests);
    if (NOT Result.Succeeded)
    {
        _Authored->Failure = FString::Join(Result.Errors, TEXT("\n"));
        for (const auto& Row : DetachedRows) { Row.Host->SetContent(Row.Content); }
        if (NOT _Authored->Accepted) { ChildSlot[DoCreate_NativePresentation()]; }
        return false;
    }
    _Authored->Accepted = true;
    _Authored->Failure.Reset();
    _Authored->AcceptedSources = InSources;
    ChildSlot[_Authored->Shell->GetRegion(TEXT("main"))];
    for (const auto& Pair : _Authored->Rows)
    { if (const auto Host = Pair.Value.Host.Pin()) { Host->SetContent(Pair.Value.View->GetRegion(TEXT("main"))); } }
    return true;
}

auto SCkDebug_EventLog::Poll_AuthoredPresentation() -> void
{
    if (NOT _Authored.IsValid()) { return; }
    auto Sources = TArray<FString>{};
    for (const auto* File : {TEXT("DebuggerEventLog.ui.html"), TEXT("DebuggerEventLog.ui.css"),
        TEXT("DebuggerEventLogRow.ui.html"), TEXT("DebuggerEventLogRow.ui.css")})
    {
        auto Source = FString{};
        if (NOT FFileHelper::LoadFileToString(Source, *FPaths::Combine(_Authored->Directory, File)))
        {
            _Authored->SourceUnavailable = true;
            _Authored->Failure = FString::Printf(TEXT("Unable to read event log resource: %s"), File);
            return;
        }
        Sources.Add(MoveTemp(Source));
    }
    const auto Tokens = ck_debug_event_log::Tokens(_TimestampColumnWidth);
    const bool RecoveredSource = _Authored->SourceUnavailable;
    _Authored->SourceUnavailable = false;
    auto SameTokens = Tokens.Num() == _Authored->AttemptedTokens.Num();
    for (const auto& Pair : Tokens)
    {
        const auto* Previous = _Authored->AttemptedTokens.Find(Pair.Key);
        if (Previous == nullptr || *Previous != Pair.Value) { SameTokens = false; break; }
    }
    if (NOT RecoveredSource && Sources == _Authored->AttemptedSources && SameTokens) { return; }
    _Authored->AttemptedSources = Sources;
    _Authored->AttemptedTokens = Tokens;
    TryApply_AuthoredSources(Sources);
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
    if (_Authored.IsValid())
    {
        for (auto It = _Authored->Rows.CreateIterator(); It; ++It)
        { if (NOT Get_ContainsEntry(It.Key()) || NOT It.Value().Host.IsValid()) { It.RemoveCurrent(); } }
    }
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
    TSharedRef<SWidget> Content = SNullWidget::NullWidget;
    if (_Authored.IsValid())
    {
        const auto View = DoCreate_AuthoredRowView(InEntry);
        const bool Loaded = _Authored->Accepted && View->TryReload(
            _Authored->AcceptedSources[2], _Authored->AcceptedSources[3], TEXT("DebuggerEventLogRow")).Succeeded;
        const auto Host = SNew(SBox)[Loaded ? View->GetRegion(TEXT("main")) : DoCreate_NativeRow(InEntry)];
        _Authored->Rows.Add(InEntry, FAuthoredState::FRow{View, Host});
        Content = Host;
    }
    else { Content = DoCreate_NativeRow(InEntry); }
    return SNew(STableRow<FEntryPtr>, InOwnerTable)
        .Padding(TAttribute<FMargin>::CreateStatic(&ck_debug_event_log::Get_RowPadding))
        .ShowSelection(true)[Content];
}

auto SCkDebug_EventLog::DoCreate_NativeRow(FEntryPtr InEntry) -> TSharedRef<SWidget>
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

    return Row;
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

    if (NOT Get_ContainsEntry(InEntry) || InEntry->SelectionId == INDEX_NONE)
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
        if (Get_ContainsEntry(Entry))
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
