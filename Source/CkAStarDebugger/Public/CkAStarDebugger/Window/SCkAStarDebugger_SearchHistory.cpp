#include "CkAStarDebugger/Window/SCkAStarDebugger_SearchHistory.h"
#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_HistoryRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_RailContainer.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Styling/CoreStyle.h"

// ====================================================================================================================
// Construction
// ====================================================================================================================

auto
    SCkAStarDebugger_SearchHistory::
    Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkAStarDebugger_ViewModel> InViewModel)
    -> void
{
    _ViewModel = InViewModel;

    // Standalone fixed-rebuild rail (SVerticalBox inside the container's scroll box), NOT an
    // SListView — which is what makes SCkDebug_HistoryRow legal here: its internal SButton would
    // eat the selection click inside an STableRow.
    ChildSlot
    [
        SAssignNew(_Rail, SCkDebug_RailContainer)
            .Title(FText::FromString(TEXT("Search History")))
            .CountText(FText::FromString(TEXT("0")))
    ];
}

// ====================================================================================================================
// Tick — rebuild list when history changes
// ====================================================================================================================

auto
    SCkAStarDebugger_SearchHistory::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT _ViewModel.IsValid() || NOT _ViewModel->Has_SelectedEntity())
    { return; }

    auto* History = _ViewModel->Get_SearchHistory(_ViewModel->Get_SelectedEntityHandle());
    auto CurrentCount = History ? History->Num() : 0;

    if (CurrentCount != _LastEntryCount)
    {
        _LastEntryCount = CurrentCount;
        RebuildList();
    }
}

auto
    SCkAStarDebugger_SearchHistory::
    Rebuild_ForStyleChange()
    -> void
{
    // SCkDebug_HistoryRow bakes its tone treatment at construction, so the rail is re-emitted rather
    // than left to a paint-time attribute.
    RebuildList();
}

// ====================================================================================================================
// Rebuild list
// ====================================================================================================================

auto
    SCkAStarDebugger_SearchHistory::
    RebuildList()
    -> void
{
    if (NOT _Rail.IsValid())
    { return; }

    _Rail->ClearChildren();

    if (NOT _ViewModel.IsValid() || NOT _ViewModel->Has_SelectedEntity())
    {
        _Rail->Set_CountText(FText::FromString(TEXT("0")));
        return;
    }

    auto* History = _ViewModel->Get_SearchHistory(_ViewModel->Get_SelectedEntityHandle());

    if (NOT History || History->IsEmpty())
    {
        _Rail->Set_CountText(FText::FromString(TEXT("0")));
        _Rail->AddChild(
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(TEXT("No search history yet")))
                .Font(ck::debug_axes::ScaledFont("Italic", CkStyle::FontSizeBody()))
                .ColorAndOpacity(CkStyle::TextMute()));
        return;
    }

    _Rail->Set_CountText(FText::FromString(ck::Format_UE(TEXT("{}"), History->Num())));

    for (auto Idx = History->Num() - 1; Idx >= 0; --Idx)
    {
        _Rail->AddChild(BuildHistoryEntry((*History)[Idx]));
    }
}

// ====================================================================================================================
// Build a single history entry widget
// ====================================================================================================================

auto
    SCkAStarDebugger_SearchHistory::
    BuildHistoryEntry(
        const FCkAStarDebugger_HistoryEntry& InEntry)
    -> TSharedRef<SWidget>
{
    const auto StatusStr = CkAStarDebugger::GetStatusString(InEntry.FinalStatus);
    const auto FrameStr  = ck::Format_UE(TEXT("F#{}"), InEntry.FrameNumber);

    const auto MetaStr = [&]() -> FString
    {
        if (InEntry.FinalStatus == ECk_AStarSearchStatus::Complete)
        {
            return ck::Format_UE(TEXT("{:.1f} cost \u00B7 {} iter \u00B7 {}us"),
                InEntry.TotalCost,
                InEntry.TotalIterations,
                InEntry.TotalTimeMicroseconds);
        }

        if (InEntry.FinalStatus == ECk_AStarSearchStatus::Failed)
        {
            return ck::Format_UE(TEXT("no path \u00B7 {} iter \u00B7 {}us"),
                InEntry.TotalIterations,
                InEntry.TotalTimeMicroseconds);
        }

        return ck::Format_UE(TEXT("{} iter \u00B7 {}us"),
            InEntry.TotalIterations,
            InEntry.TotalTimeMicroseconds);
    }();

    const auto CopyText = ck::Format_UE(
        TEXT("A* search [{}]\n")
        TEXT("  frame:      {}\n")
        TEXT("  iterations: {}\n")
        TEXT("  time:       {}us\n")
        TEXT("  cost:       {}\n")
        TEXT("  path len:   {}"),
        StatusStr,
        InEntry.FrameNumber,
        InEntry.TotalIterations,
        InEntry.TotalTimeMicroseconds,
        InEntry.TotalCost,
        InEntry.PathLength);

    return SNew(SCkDebug_HistoryRow)
        .Tone(CkAStarDebugger::GetStatusTone(InEntry.FinalStatus))
        .TitleText(FText::FromString(StatusStr))
        .RightText(FText::FromString(FrameStr))
        .SubtitleText(FText::FromString(MetaStr))
        .CopyText(CopyText);
}

// ====================================================================================================================
