#pragma once

#include "CkEditorTools/Style/CkStyle.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class STableViewBase;
class STextBlock;

// ====================================================================================================================
// Event log — a capped, auto-scrolling list of "what just happened" lines with severity tones.
//
// Promoted out of CkCrowdDebugger's SCkCrowdDebugger_EventLogPanel (which was still a placeholder)
// and generalized so any debugger with an event ring can render it without writing list plumbing.
//
// ROW SAFETY: rows are built from STextBlock / SBox / SHorizontalBox and the axis chip helper only
// — no SButton, no SEditableText — so STableRow keeps its selection click. See the
// "List / tree rows" section of CkDebuggerCommon/CLAUDE.md.
//
// COPY: right-click anywhere in the list opens "Copy Event(s)", which is multi-select aware and
// joins the selected lines with newlines. Nothing extra to wire at the call site.
//
// POINTER IDENTITY: Add_Entry appends and evicts from the front, so surviving rows keep their
// TSharedPtr and the user's selection survives a refresh. Set_Entries replaces wholesale and is
// the right call only when the whole log is regenerated.
// ====================================================================================================================

struct FCkDebug_EventLogEntry
{
    /** The line itself. Shorten class/tag names with SCkDebug_NameLabel::Get_ShortName first. */
    FString Message;

    /** Optional short bucket rendered as a leading chip ("PATH", "REPLAN", …). May be empty. */
    FString Category;

    /** Drives both the chip and the message color. */
    ECk_Tone Tone = ECk_Tone::Neutral;

    double TimeSeconds = 0.0;

    /** >= 0 makes the row report through OnEntrySelected (e.g. click-to-scrub). */
    int32 SelectionId = INDEX_NONE;
};

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DELEGATE_OneParam(FOnCkDebug_EventLogEntrySelected, int32 /* SelectionId */);

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_EventLog : public SCompoundWidget
{
public:
    using FEntryPtr = TSharedPtr<FCkDebug_EventLogEntry>;

    SLATE_BEGIN_ARGS(SCkDebug_EventLog)
        : _MaxEntries(200)
        , _ShowTimestamps(true)
        , _AutoScroll(true)
        , _EmptyText(FText::FromString(TEXT("No events yet.")))
        , _SelectedId(INDEX_NONE)
        , _TimestampColumnWidth(58.0f)
    {}
        /** Oldest entries are evicted past this count. Clamped to at least 1. */
        SLATE_ARGUMENT(int32, MaxEntries)

        SLATE_ARGUMENT(bool, ShowTimestamps)

        /** Scroll to the newest line whenever entries are appended. */
        SLATE_ARGUMENT(bool, AutoScroll)

        SLATE_ARGUMENT(FText, EmptyText)

        /** Programmatic selection restore, applied on every refresh. */
        SLATE_ATTRIBUTE(int32, SelectedId)

        SLATE_ARGUMENT(float, TimestampColumnWidth)

        SLATE_EVENT(FOnCkDebug_EventLogEntrySelected, OnEntrySelected)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** Append one line, evicting the oldest if the cap is exceeded. */
    auto Add_Entry(FCkDebug_EventLogEntry InEntry) -> void;

    /** Append several lines in one refresh. */
    auto Add_Entries(TArray<FCkDebug_EventLogEntry> InEntries) -> void;

    /** Replace the whole log. Drops pointer identity — prefer Add_Entry for a live ring. */
    auto Set_Entries(TArray<FCkDebug_EventLogEntry> InEntries) -> void;

    auto Clear_Entries() -> void;

    auto Get_EntryCount() const -> int32;

private:
    auto Do_TrimToCap() -> void;
    auto Do_RefreshList(bool InScrollToEnd) -> void;
    auto Do_RestoreSelection() -> void;

    auto Handle_GenerateRow(FEntryPtr InEntry, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto Handle_SelectionChanged(FEntryPtr InEntry, ESelectInfo::Type InSelectInfo) -> void;
    auto Handle_ContextMenuOpening() -> TSharedPtr<SWidget>;

    auto Get_EmptyVisibility() const -> EVisibility;

    /** One clipboard line for an entry — timestamp, category, message. */
    static auto Compose_CopyLine(const FCkDebug_EventLogEntry& InEntry) -> FString;

private:
    TArray<FEntryPtr> _Entries;

    int32 _MaxEntries = 200;
    bool _ShowTimestamps = true;
    bool _AutoScroll = true;
    float _TimestampColumnWidth = 58.0f;

    TAttribute<int32> _SelectedId;
    FOnCkDebug_EventLogEntrySelected _OnEntrySelected;

    TSharedPtr<SListView<FEntryPtr>> _ListView;
};

// ====================================================================================================================
