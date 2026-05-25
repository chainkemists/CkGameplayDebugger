#include "CkAStarDebugger/Window/SCkAStarDebugger_StatsPanel.h"
#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkAStarDebugger/CkAStarDebuggerStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"

// ====================================================================================================================
// Helpers
// ====================================================================================================================

static auto
    MakeSectionHeader(
        const FString& InTitle)
    -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_SelectableLabel)
        .Text(FText::FromString(InTitle.ToUpper()))
        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
        .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Muted);
}

// ====================================================================================================================
// Construction
// ====================================================================================================================

auto
    SCkAStarDebugger_StatsPanel::
    Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkAStarDebugger_ViewModel> InViewModel)
    -> void
{
    _ViewModel = InViewModel;

    ChildSlot
    [
        SNew(SScrollBox)
            + SScrollBox::Slot()
                .Padding(12.0f)
                [
                    SNew(SVerticalBox)

                        // Mini-stats grid (2x2 — stacked-card layout via SCkDebug_StatPair)
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(8.0f))
                                        [
                                            SAssignNew(_IterationsStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                                                .Value(FText::FromString(TEXT("0")))
                                                .Label(FText::FromString(TEXT("ITERATIONS")))
                                        ]
                                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(8.0f))
                                        [
                                            SAssignNew(_OpenStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                                                .Value(FText::FromString(TEXT("0")))
                                                .Label(FText::FromString(TEXT("OPEN")))
                                        ]
                                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(8.0f))
                                        [
                                            SAssignNew(_ClosedStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                                                .Value(FText::FromString(TEXT("0")))
                                                .Label(FText::FromString(TEXT("CLOSED")))
                                        ]
                                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(8.0f))
                                        [
                                            SAssignNew(_PathStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                                                .Value(FText::FromString(TEXT("—")))
                                                .Label(FText::FromString(TEXT("PATH")))
                                        ]
                            ]

                        // Budget section
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f) [ MakeSectionHeader(TEXT("Budget Usage")) ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SNew(SHorizontalBox)
                                                + SHorizontalBox::Slot()
                                                    .FillWidth(1.0f)
                                                    .VAlign(VAlign_Center)
                                                    [
                                                        SAssignNew(_BudgetBar, SProgressBar)
                                                            .Percent(0.0f)
                                                            .FillColorAndOpacity(FCkAStarDebuggerStyle::Color_Budget_Normal)
                                                    ]
                                                + SHorizontalBox::Slot()
                                                    .AutoWidth()
                                                    .Padding(8.0f, 0.0f, 0.0f, 0.0f)
                                                    .VAlign(VAlign_Center)
                                                    [
                                                        SAssignNew(_BudgetPctText, SCkDebug_SelectableLabel)
                                                            .Text(FText::FromString(TEXT("0%")))
                                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                                            .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Secondary)
                                                    ]
                                        ]
                            ]

                        // Exploration section
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f) [ MakeSectionHeader(TEXT("Exploration")) ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SNew(SHorizontalBox)
                                                + SHorizontalBox::Slot()
                                                    .FillWidth(1.0f)
                                                    .VAlign(VAlign_Center)
                                                    [
                                                        SAssignNew(_ExplorationBar, SProgressBar)
                                                            .Percent(0.0f)
                                                            .FillColorAndOpacity(FCkAStarDebuggerStyle::Color_Exploration)
                                                    ]
                                                + SHorizontalBox::Slot()
                                                    .AutoWidth()
                                                    .Padding(8.0f, 0.0f, 0.0f, 0.0f)
                                                    .VAlign(VAlign_Center)
                                                    [
                                                        SAssignNew(_ExplorationPctText, SCkDebug_SelectableLabel)
                                                            .Text(FText::FromString(TEXT("0%")))
                                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                                            .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Secondary)
                                                    ]
                                        ]
                            ]

                        // Details section — label-first inline stat rows.
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f) [ MakeSectionHeader(TEXT("Details")) ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SAssignNew(_GridSizeStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Inline_LabelFirst)
                                                .Label(FText::FromString(TEXT("Grid")))
                                                .Value(FText::FromString(TEXT("—")))
                                        ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SAssignNew(_BlockedStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Inline_LabelFirst)
                                                .Label(FText::FromString(TEXT("Blocked")))
                                                .Value(FText::FromString(TEXT("—")))
                                        ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SAssignNew(_CostStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Inline_LabelFirst)
                                                .Label(FText::FromString(TEXT("Cost")))
                                                .Value(FText::FromString(TEXT("—")))
                                        ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SAssignNew(_TimeStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Inline_LabelFirst)
                                                .Label(FText::FromString(TEXT("Time")))
                                                .Value(FText::FromString(TEXT("—")))
                                        ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SAssignNew(_ThresholdStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Inline_LabelFirst)
                                                .Label(FText::FromString(TEXT("Threshold")))
                                                .Value(FText::FromString(TEXT("disabled")))
                                        ]
                            ]

                        // Selected cell section
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f) [ MakeSectionHeader(TEXT("Selected Cell")) ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SAssignNew(_CellDetailBox, SVerticalBox)
                                                + SVerticalBox::Slot()
                                                    .AutoHeight()
                                                    [
                                                        SNew(SCkDebug_SelectableLabel)
                                                            .Text(FText::FromString(TEXT("Click a cell on the grid to inspect it")))
                                                            .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                                                            .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Muted)
                                                    ]
                                        ]
                            ]
                ]
    ];
}

// ====================================================================================================================
// Tick — refresh values from ViewModel
// ====================================================================================================================

auto
    SCkAStarDebugger_StatsPanel::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT _ViewModel.IsValid())
    { return; }

    auto* Info = _ViewModel->Get_CurrentSearchInfo();

    if (NOT Info)
    { return; }

    RefreshFromSearchInfo(*Info);

    auto SelectedCell = _ViewModel->Get_SelectedCellIndex();

    if (SelectedCell != _LastShownCellIndex)
    {
        _LastShownCellIndex = SelectedCell;
        _CellDetailBox->ClearChildren();

        if (SelectedCell < 0 || Info->GridWidth <= 0)
        {
            _CellDetailBox->AddSlot()
                .AutoHeight()
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(TEXT("Click a cell on the grid to inspect it")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                        .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Muted)
                ];
        }
        else
        {
            auto CellX = SelectedCell % Info->GridWidth;
            auto CellY = SelectedCell / Info->GridWidth;

            auto StateStr = FString{};
            if (Info->BlockedCells.Contains(SelectedCell)) { StateStr = TEXT("Blocked"); }
            else if (Info->Path.Contains(SelectedCell)) { StateStr = TEXT("Path"); }
            else if (Info->ClosedSetCells.Contains(SelectedCell)) { StateStr = TEXT("Closed"); }
            else if (Info->OpenSetCells.Contains(SelectedCell)) { StateStr = TEXT("Open"); }
            else { StateStr = TEXT("Empty"); }

            auto GStr = Info->GScores.Contains(SelectedCell)
                ? FString::Printf(TEXT("%.1f"), Info->GScores[SelectedCell])
                : FString(TEXT("—"));

            auto FStr = FString(TEXT("—"));
            if (Info->GScores.Contains(SelectedCell) && Info->GridWidth > 0)
            {
                auto GoalX = Info->GoalNode % Info->GridWidth;
                auto GoalY = Info->GoalNode / Info->GridWidth;
                auto H = static_cast<float>(FMath::Abs(CellX - GoalX) + FMath::Abs(CellY - GoalY));
                FStr = FString::Printf(TEXT("%.1f"), Info->GScores[SelectedCell] + H);
            }

            auto ParentStr = Info->CameFrom.Contains(SelectedCell)
                ? FString::Printf(TEXT("%d"), Info->CameFrom[SelectedCell])
                : FString(TEXT("none"));

            auto TitleColor = FCkAStarDebuggerStyle::Color_Cell_Goal;
            auto BoldFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);

            _CellDetailBox->AddSlot().AutoHeight()
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(FString::Printf(TEXT("Cell (%d, %d) — #%d"), CellX, CellY, SelectedCell)))
                        .Font(BoldFont)
                        .ColorAndOpacity(TitleColor)
                ];

            // Static per-cell rows — no live updates needed, so we don't keep
            // handles. Use the shared label-first stat-pair layout for visual
            // consistency with the Details section above.
            auto AddDetailRow = [&](const FString& InLabel, const FString& InValue)
            {
                _CellDetailBox->AddSlot().AutoHeight()
                    [
                        SNew(SCkDebug_StatPair)
                            .Layout(ECkDebug_StatPairLayout::Inline_LabelFirst)
                            .Label(FText::FromString(InLabel))
                            .Value(FText::FromString(InValue))
                    ];
            };

            AddDetailRow(TEXT("State"), StateStr);
            AddDetailRow(TEXT("G-Score"), GStr);
            AddDetailRow(TEXT("F-Score"), FStr);
            AddDetailRow(TEXT("Parent"), ParentStr);
        }
    }
}

// ====================================================================================================================
// Refresh stat values
// ====================================================================================================================

auto
    SCkAStarDebugger_StatsPanel::
    RefreshFromSearchInfo(
        const FCkAStarDebugger_SearchInfo& InInfo)
    -> void
{
    _IterationsStat->SetValue(FText::FromString(FString::Printf(TEXT("%d"), InInfo.TotalIterations)));
    _OpenStat->SetValue(FText::FromString(FString::Printf(TEXT("%d"), InInfo.OpenSetSize)));
    _ClosedStat->SetValue(FText::FromString(FString::Printf(TEXT("%d"), InInfo.ClosedSetSize)));

    if (InInfo.Path.Num() > 0)
    {
        _PathStat->SetValue(FText::FromString(FString::Printf(TEXT("%d"), InInfo.Path.Num())));
    }
    else
    {
        _PathStat->SetValue(FText::FromString(TEXT("—")));
    }

    auto BudgetPct = FMath::Clamp(InInfo.BudgetUsagePercent, 0.0f, 100.0f);
    _BudgetBar->SetPercent(BudgetPct / 100.0f);
    _BudgetPctText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(BudgetPct))));

    auto BudgetColor = BudgetPct > 90.0f ? FCkAStarDebuggerStyle::Color_Budget_Over
        : BudgetPct > 70.0f ? FCkAStarDebuggerStyle::Color_Budget_Warning
        : FCkAStarDebuggerStyle::Color_Budget_Normal;
    _BudgetBar->SetFillColorAndOpacity(BudgetColor);

    auto TotalCells = InInfo.GridWidth * InInfo.GridHeight;
    auto Reachable = TotalCells - InInfo.BlockedCells.Num();

    if (Reachable > 0)
    {
        auto ExplPct = static_cast<float>(InInfo.ClosedSetSize) / static_cast<float>(Reachable);
        _ExplorationBar->SetPercent(FMath::Clamp(ExplPct, 0.0f, 1.0f));
        _ExplorationPctText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(ExplPct * 100.0f))));
    }

    if (InInfo.GridWidth > 0 && InInfo.GridHeight > 0)
    {
        _GridSizeStat->SetValue(FText::FromString(FString::Printf(TEXT("%d x %d"), InInfo.GridWidth, InInfo.GridHeight)));
    }
    else
    {
        _GridSizeStat->SetValue(FText::FromString(TEXT("—")));
    }

    _BlockedStat->SetValue(FText::FromString(FString::Printf(TEXT("%d"), InInfo.BlockedCells.Num())));

    if (InInfo.TotalCost > 0.0f)
    {
        _CostStat->SetValue(FText::FromString(FString::Printf(TEXT("%.1f"), InInfo.TotalCost)));
    }
    else
    {
        _CostStat->SetValue(FText::FromString(TEXT("—")));
    }

    _TimeStat->SetValue(FText::FromString(FString::Printf(TEXT("%lld us"), InInfo.TotalTimeMicroseconds)));

    if (InInfo.CostThreshold > 0.0f)
    {
        _ThresholdStat->SetValue(FText::FromString(FString::Printf(TEXT("%.1f"), InInfo.CostThreshold)));
    }
    else
    {
        _ThresholdStat->SetValue(FText::FromString(TEXT("disabled")));
    }

    _IterationsStat->SetValueColor(CkAStarDebugger::GetStatusColor(InInfo.SearchStatus));
}

// ====================================================================================================================
