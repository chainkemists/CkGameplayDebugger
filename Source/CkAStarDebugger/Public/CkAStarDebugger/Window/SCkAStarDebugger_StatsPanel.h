#pragma once

#include "CkAStarDebugger/Data/CkAStarDebugger_Types.h"

#include "Widgets/SCompoundWidget.h"

class SCkDebug_SelectableLabel;

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

private:
    auto BuildMiniStats() -> TSharedRef<SWidget>;
    auto BuildBudgetMeter() -> TSharedRef<SWidget>;
    auto BuildExplorationMeter() -> TSharedRef<SWidget>;
    auto BuildDetailsSection() -> TSharedRef<SWidget>;
    auto BuildCellDetailSection() -> TSharedRef<SWidget>;
    auto BuildLegendSection() -> TSharedRef<SWidget>;

    auto RefreshFromSearchInfo(const FCkAStarDebugger_SearchInfo& InInfo) -> void;

    TSharedPtr<FCkAStarDebugger_ViewModel> _ViewModel;

    TSharedPtr<SCkDebug_SelectableLabel> _IterationsText;
    TSharedPtr<SCkDebug_SelectableLabel> _OpenText;
    TSharedPtr<SCkDebug_SelectableLabel> _ClosedText;
    TSharedPtr<SCkDebug_SelectableLabel> _PathText;

    TSharedPtr<SProgressBar> _BudgetBar;
    TSharedPtr<SCkDebug_SelectableLabel> _BudgetPctText;

    TSharedPtr<SProgressBar> _ExplorationBar;
    TSharedPtr<SCkDebug_SelectableLabel> _ExplorationPctText;

    TSharedPtr<SCkDebug_SelectableLabel> _GridSizeText;
    TSharedPtr<SCkDebug_SelectableLabel> _BlockedText;
    TSharedPtr<SCkDebug_SelectableLabel> _CostText;
    TSharedPtr<SCkDebug_SelectableLabel> _TimeText;
    TSharedPtr<SCkDebug_SelectableLabel> _ThresholdText;

    TSharedPtr<SVerticalBox> _CellDetailBox;
    int32 _LastShownCellIndex = -1;
};

// --------------------------------------------------------------------------------------------------------------------
