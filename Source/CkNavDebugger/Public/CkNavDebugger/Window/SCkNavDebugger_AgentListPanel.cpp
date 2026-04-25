#include "CkNavDebugger/Window/SCkNavDebugger_AgentListPanel.h"

#include "CkNavDebugger/ViewModel/CkNavDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_HistoryRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SBox.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    auto ToTone(ECk_Nav_PathStatus InStatus) -> ECkDebug_Tone
    {
        switch (InStatus)
        {
            case ECk_Nav_PathStatus::Ready:   return ECkDebug_Tone::Ok;
            case ECk_Nav_PathStatus::Partial: return ECkDebug_Tone::Warn;
            case ECk_Nav_PathStatus::Failed:  return ECkDebug_Tone::Err;
            case ECk_Nav_PathStatus::Pending: return ECkDebug_Tone::Info;
            default:                          return ECkDebug_Tone::Neutral;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkNavDebugger_AgentListPanel::
    Construct(
        const FArguments& InArgs,
        TSharedRef<FCkNavDebugger_ViewModel> InViewModel)
    -> void
{
    _ViewModel = InViewModel;

    ChildSlot
    [
        // SListView selection is bypassed because SCkDebug_HistoryRow's internal SButton
        // swallows clicks before they reach the table row. Selection is driven directly
        // from the row's OnClicked callback (see OnGenerateRow).
        SAssignNew(_ListView, SListView<TSharedPtr<FCkNavDebugger_AgentInfo>>)
            .ListItemsSource(&_Items)
            .OnGenerateRow(this, &SCkNavDebugger_AgentListPanel::OnGenerateRow)
            .SelectionMode(ESelectionMode::None)
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkNavDebugger_AgentListPanel::
    Refresh()
    -> void
{
    auto VM = _ViewModel.Pin();
    if (NOT VM.IsValid()) { return; }

    const auto& Agents = VM->Get_AllAgents();

    // Only request a list refresh if the count or per-row identity actually changed —
    // SListView handles per-row rebuild without flicker.
    auto NeedsRebuild = _Items.Num() != Agents.Num();
    if (NOT NeedsRebuild)
    {
        for (auto i = 0; i < Agents.Num(); ++i)
        {
            if (NOT _Items[i].IsValid()
                || GetTypeHash(_Items[i]->EntityHandle) != GetTypeHash(Agents[i].EntityHandle))
            { NeedsRebuild = true; break; }
        }
    }

    if (NeedsRebuild)
    {
        _Items.Reset();
        _Items.Reserve(Agents.Num());
        for (const auto& Agent : Agents)
        {
            _Items.Add(MakeShared<FCkNavDebugger_AgentInfo>(Agent));
        }
        if (_ListView.IsValid()) { _ListView->RequestListRefresh(); }
    }
    else
    {
        // In-place update: copy fresh data into the existing TSharedPtrs so the row
        // widgets' TAttribute bindings see new values without rebuilding the table.
        for (auto i = 0; i < Agents.Num(); ++i)
        {
            *_Items[i] = Agents[i];
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkNavDebugger_AgentListPanel::
    OnGenerateRow(
        TSharedPtr<FCkNavDebugger_AgentInfo> InItem,
        const TSharedRef<STableViewBase>& InOwner)
    -> TSharedRef<ITableRow>
{
    const auto Item    = InItem;   // capture by value so the row keeps a stable ptr
    auto VMWeak        = _ViewModel;
    const auto SelHash = static_cast<int32>(GetTypeHash(Item->EntityHandle));

    const auto IsSelected = [VMWeak, SelHash]()
    {
        const auto VM = VMWeak.Pin();
        if (NOT VM.IsValid()) { return false; }
        const auto Sel = VM->Get_SelectedEntityHandle();
        return ck::IsValid(Sel) && static_cast<int32>(GetTypeHash(Sel)) == SelHash;
    }();

    return SNew(STableRow<TSharedPtr<FCkNavDebugger_AgentInfo>>, InOwner)
    [
        SNew(SCkDebug_HistoryRow)
            .Tone(ToTone(Item->PathStatus))
            .TitleText(FText::FromString(Item->DebugName))
            .MetaText(FText::FromString(CkNavDebugger::GetStatusString(Item->PathStatus)))
            .SubtitleText(FText::FromString(
                Item->PathStatus == ECk_Nav_PathStatus::Failed
                && Item->Diagnostics.Get_LastFailReason() != ECk_Nav_PathFailReason::None
                ? CkNavDebugger::GetFailReasonString(Item->Diagnostics.Get_LastFailReason())
                : FString::Printf(TEXT("%d waypoint(s)"), Item->Waypoints.Num())))
            .IsSelected(IsSelected)
            .CopyText(FString::Printf(TEXT("%s | %s | %s"),
                *Item->DebugName,
                *CkNavDebugger::GetStatusString(Item->PathStatus),
                *CkNavDebugger::GetFailReasonString(Item->Diagnostics.Get_LastFailReason())))
            .OnClicked_Lambda([VMWeak, Item]()
            {
                const auto VM = VMWeak.Pin();
                if (NOT VM.IsValid() || NOT Item.IsValid()) { return; }
                VM->Set_SelectedEntityHandle(Item->EntityHandle);
            })
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkNavDebugger_AgentListPanel::
    OnSelectionChanged(
        TSharedPtr<FCkNavDebugger_AgentInfo> InItem,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    // Unused — see Construct() comment. Selection is driven from the HistoryRow OnClicked.
}

// --------------------------------------------------------------------------------------------------------------------
