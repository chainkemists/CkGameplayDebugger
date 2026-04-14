#pragma once

#include "CkAStarDebugger/Data/CkAStarDebugger_Types.h"

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkAStarDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Search history — scrollable list of past search completion events.
// --------------------------------------------------------------------------------------------------------------------

class SCkAStarDebugger_SearchHistory : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkAStarDebugger_SearchHistory) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkAStarDebugger_ViewModel> InViewModel) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
    auto RebuildList() -> void;
    auto BuildHistoryEntry(const FCkAStarDebugger_HistoryEntry& InEntry) -> TSharedRef<SWidget>;

    TSharedPtr<FCkAStarDebugger_ViewModel> _ViewModel;
    TSharedPtr<SVerticalBox> _EntryListBox;
    int32 _LastEntryCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------
