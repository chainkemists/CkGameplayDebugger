#pragma once

#include "CkAStarDebugger/Data/CkAStarDebugger_Types.h"

#include "Widgets/SCompoundWidget.h"

class SCkDebug_SelectableLabel;
class SCkDebug_StatPair;

// --------------------------------------------------------------------------------------------------------------------

class FCkAStarDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Stats panel — displays search metrics, budget usage, and selected cell detail.
// --------------------------------------------------------------------------------------------------------------------

class SCkAStarDebugger_StatsPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkAStarDebugger_StatsPanel) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkAStarDebugger_ViewModel> InViewModel) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    /**
     * Re-emit the selected-cell detail rows after a Layer-B style revision. Everything else on this
     * panel is attribute-bound and has already moved by the time this runs.
     */
    auto Rebuild_ForStyleChange() -> void;

private:
    auto RefreshFromSearchInfo(const FCkAStarDebugger_SearchInfo& InInfo) -> void;

    TSharedPtr<FCkAStarDebugger_ViewModel> _ViewModel;

    // Stacked stat-cards (top 2x2 grid).
    TSharedPtr<SCkDebug_StatPair> _IterationsStat;
    TSharedPtr<SCkDebug_StatPair> _OpenStat;
    TSharedPtr<SCkDebug_StatPair> _ClosedStat;
    TSharedPtr<SCkDebug_StatPair> _PathStat;

    // Inline label-first stat rows (Details section).
    TSharedPtr<SCkDebug_StatPair> _GridSizeStat;
    TSharedPtr<SCkDebug_StatPair> _BlockedStat;
    TSharedPtr<SCkDebug_StatPair> _CostStat;
    TSharedPtr<SCkDebug_StatPair> _TimeStat;
    TSharedPtr<SCkDebug_StatPair> _ThresholdStat;

    // Right-aligned percent labels next to the two SCkDebug_MeterBars. The meters themselves are
    // attribute-driven: the refresh pass writes the fractions below and each bar reads its own on
    // the next paint, so no widget handle is needed for them.
    TSharedPtr<SCkDebug_SelectableLabel> _BudgetPctText;
    TSharedPtr<SCkDebug_SelectableLabel> _ExplorationPctText;

    float _BudgetFraction      = 0.0f;
    float _ExplorationFraction = 0.0f;

    TSharedPtr<SVerticalBox> _CellDetailBox;
    int32 _LastShownCellIndex = -1;
};

// --------------------------------------------------------------------------------------------------------------------
