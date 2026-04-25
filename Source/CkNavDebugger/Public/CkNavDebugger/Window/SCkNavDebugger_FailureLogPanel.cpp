#include "CkNavDebugger/Window/SCkNavDebugger_FailureLogPanel.h"

#include "CkNavDebugger/ViewModel/CkNavDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_HistoryRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Widgets/Views/STableRow.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkNavDebugger_FailureLogPanel::
    Construct(
        const FArguments& InArgs,
        TSharedRef<FCkNavDebugger_ViewModel> InViewModel)
    -> void
{
    _ViewModel = InViewModel;

    ChildSlot
    [
        // Selection driven from HistoryRow OnClicked (its inner SButton consumes clicks
        // before they reach STableRow). See AgentListPanel for the same pattern.
        SAssignNew(_ListView, SListView<TSharedPtr<FCkNavDebugger_FailureLogEntry>>)
            .ListItemsSource(&_Items)
            .OnGenerateRow(this, &SCkNavDebugger_FailureLogPanel::OnGenerateRow)
            .SelectionMode(ESelectionMode::None)
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkNavDebugger_FailureLogPanel::
    Refresh()
    -> void
{
    auto VM = _ViewModel.Pin();
    if (NOT VM.IsValid()) { return; }

    const auto& Log = VM->Get_FailureLog();

    // Skip the rebuild unless count changed — avoids per-tick churn in the SListView.
    if (Log.Num() == _LastSeenLogCount && _Items.Num() == Log.Num())
    { return; }

    _LastSeenLogCount = Log.Num();

    _Items.Reset();
    _Items.Reserve(Log.Num());
    // Newest first.
    for (auto i = Log.Num() - 1; i >= 0; --i)
    {
        _Items.Add(MakeShared<FCkNavDebugger_FailureLogEntry>(Log[i]));
    }

    if (_ListView.IsValid()) { _ListView->RequestListRefresh(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkNavDebugger_FailureLogPanel::
    OnGenerateRow(
        TSharedPtr<FCkNavDebugger_FailureLogEntry> InItem,
        const TSharedRef<STableViewBase>& InOwner)
    -> TSharedRef<ITableRow>
{
    const auto& E = *InItem;
    const auto ReasonStr = CkNavDebugger::GetFailReasonString(E.FailReason);

    const auto Title = FText::FromString(FString::Printf(TEXT("%s — %s"),
        *E.DebugName, *ReasonStr));
    const auto Subtitle = FText::FromString(FString::Printf(
        TEXT("agent=%s   target=%s   raw_pts=%d   %.2f ms"),
        *E.AgentLocation.ToString(), *E.TargetLocation.ToString(),
        E.RawPathPointCount, E.DurationMs));
    const auto Right = FText::FromString(FString::Printf(TEXT("F%llu"), E.FrameNumber));

    const auto CopyText = FString::Printf(TEXT(
        "Frame %llu  agent=%s  reason=%s\n"
        "  target=%s  agent_loc=%s\n"
        "  start_proj=%s @ %s   end_proj=%s @ %s\n"
        "  raw_pts=%d  duration_ms=%.2f"),
        E.FrameNumber, *E.DebugName, *ReasonStr,
        *E.TargetLocation.ToString(), *E.AgentLocation.ToString(),
        E.StartProjected ? TEXT("true") : TEXT("false"), *E.ProjectedStart.ToString(),
        E.EndProjected   ? TEXT("true") : TEXT("false"), *E.ProjectedEnd.ToString(),
        E.RawPathPointCount, E.DurationMs);

    auto VMWeak = _ViewModel;
    const auto Hash = GetTypeHash(InItem->EntityHandle);

    return SNew(STableRow<TSharedPtr<FCkNavDebugger_FailureLogEntry>>, InOwner)
    [
        SNew(SCkDebug_HistoryRow)
            .Tone(ECkDebug_Tone::Err)
            .TitleText(Title)
            .RightText(Right)
            .SubtitleText(Subtitle)
            .CopyText(CopyText)
            .OnClicked_Lambda([VMWeak, Hash]()
            {
                const auto VM = VMWeak.Pin();
                if (NOT VM.IsValid()) { return; }
                for (const auto& Agent : VM->Get_AllAgents())
                {
                    if (GetTypeHash(Agent.EntityHandle) == Hash)
                    { VM->Set_SelectedEntityHandle(Agent.EntityHandle); return; }
                }
            })
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkNavDebugger_FailureLogPanel::
    OnSelectionChanged(
        TSharedPtr<FCkNavDebugger_FailureLogEntry> InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    auto VM = _ViewModel.Pin();
    if (NOT VM.IsValid() || NOT InItem.IsValid()) { return; }

    const auto Hash = GetTypeHash(InItem->EntityHandle);
    for (const auto& Agent : VM->Get_AllAgents())
    {
        if (GetTypeHash(Agent.EntityHandle) == Hash)
        { VM->Set_SelectedEntityHandle(Agent.EntityHandle); return; }
    }
}

// --------------------------------------------------------------------------------------------------------------------
