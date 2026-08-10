#pragma once

#include "CkAStarDebugger/Data/CkAStarDebugger_Types.h"

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkAStarDebugger_ViewModel;
class SCkDebug_RailContainer;

// --------------------------------------------------------------------------------------------------------------------
// Search history — scrollable rail of past search completion events, built from the common
// SCkDebug_RailContainer + SCkDebug_HistoryRow primitives.
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
    TSharedPtr<SCkDebug_RailContainer> _Rail;
    int32 _LastEntryCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------
