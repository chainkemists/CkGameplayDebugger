#include "CkAStarDebugger/Window/SCkAStarDebugger_StatsPanel.h"
#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"

#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"

#include "CkEditorTools/Style/CkStyle.h"

// ====================================================================================================================
// Helpers
// ====================================================================================================================

namespace ck_astar_debugger_stats_panel
{
    auto MakeSectionHeader(const FString& InTitle) -> TSharedRef<SWidget>
    {
        return ck::debug_axes::Make_SectionHeader(
            UCkDebuggerStyleSettings::Get_Selection(),
            FText::FromString(InTitle),
            ECk_Tone::Neutral);
    }

    // Height for the two meter rows. SCkDebug_MeterBar has no intrinsic height, so the panel names
    // one — chunky enough to read next to the percent label without dominating the section.
    constexpr auto MeterHeight = 8.0f;

    // RowDensity on the 2x2 stat grid. Value form bakes the margin at construct time and the panel
    // never rebuilds, so the axis has to ride the slot's attribute instead.
    auto Get_StatCardPadding() -> FMargin
    { return ck::debug_axes::Apply_RowDensity(FMargin(8.0f)); }

    // Fonts as attributes so TextScale lands without a rebuild.
    auto Get_PercentFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); }

    auto Get_EmptyStateFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Italic", CkStyle::FontSizeBody()); }

    auto Get_CellTitleFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()); }
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
                                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(TAttribute<FMargin>::CreateStatic(&ck_astar_debugger_stats_panel::Get_StatCardPadding))
                                        [
                                            SAssignNew(_IterationsStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                                                .Value(FText::FromString(TEXT("0")))
                                                .Label(FText::FromString(TEXT("ITERATIONS")))
                                        ]
                                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(TAttribute<FMargin>::CreateStatic(&ck_astar_debugger_stats_panel::Get_StatCardPadding))
                                        [
                                            SAssignNew(_OpenStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                                                .Value(FText::FromString(TEXT("0")))
                                                .Label(FText::FromString(TEXT("OPEN")))
                                        ]
                                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(TAttribute<FMargin>::CreateStatic(&ck_astar_debugger_stats_panel::Get_StatCardPadding))
                                        [
                                            SAssignNew(_ClosedStat, SCkDebug_StatPair)
                                                .Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop)
                                                .Value(FText::FromString(TEXT("0")))
                                                .Label(FText::FromString(TEXT("CLOSED")))
                                        ]
                                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(TAttribute<FMargin>::CreateStatic(&ck_astar_debugger_stats_panel::Get_StatCardPadding))
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
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f) [ ck_astar_debugger_stats_panel::MakeSectionHeader(TEXT("Budget Usage")) ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SNew(SHorizontalBox)
                                                + SHorizontalBox::Slot()
                                                    .FillWidth(1.0f)
                                                    .VAlign(VAlign_Center)
                                                    [
                                                        // Budget usage rides the shared cold→hot ramp: Ok at
                                                        // idle, Warn at half the budget, Err at the cap.
                                                        SNew(SCkDebug_MeterBar)
                                                            .DesiredSize(FVector2D(110.0f, ck_astar_debugger_stats_panel::MeterHeight))
                                                            .Fraction_Lambda([this]() { return _BudgetFraction; })
                                                            .FillColor_Lambda([this]()
                                                            {
                                                                return ck::debug_axes::Get_HeatColor(_BudgetFraction);
                                                            })
                                                    ]
                                                + SHorizontalBox::Slot()
                                                    .AutoWidth()
                                                    .Padding(8.0f, 0.0f, 0.0f, 0.0f)
                                                    .VAlign(VAlign_Center)
                                                    [
                                                        SAssignNew(_BudgetPctText, SCkDebug_SelectableLabel)
                                                            .Text(FText::FromString(TEXT("0%")))
                                                            .Font_Static(&ck_astar_debugger_stats_panel::Get_PercentFont)
                                                            .ColorAndOpacity(CkStyle::TextDim())
                                                    ]
                                        ]
                            ]

                        // Exploration section
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f) [ ck_astar_debugger_stats_panel::MakeSectionHeader(TEXT("Exploration")) ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SNew(SHorizontalBox)
                                                + SHorizontalBox::Slot()
                                                    .FillWidth(1.0f)
                                                    .VAlign(VAlign_Center)
                                                    [
                                                        // Exploration is coverage, not pressure — a flat accent
                                                        // keeps it visually distinct from the budget heat bar.
                                                        SNew(SCkDebug_MeterBar)
                                                            .DesiredSize(FVector2D(110.0f, ck_astar_debugger_stats_panel::MeterHeight))
                                                            .Fraction_Lambda([this]() { return _ExplorationFraction; })
                                                            .FillColor(CkStyle::Accent())
                                                    ]
                                                + SHorizontalBox::Slot()
                                                    .AutoWidth()
                                                    .Padding(8.0f, 0.0f, 0.0f, 0.0f)
                                                    .VAlign(VAlign_Center)
                                                    [
                                                        SAssignNew(_ExplorationPctText, SCkDebug_SelectableLabel)
                                                            .Text(FText::FromString(TEXT("0%")))
                                                            .Font_Static(&ck_astar_debugger_stats_panel::Get_PercentFont)
                                                            .ColorAndOpacity(CkStyle::TextDim())
                                                    ]
                                        ]
                            ]

                        // Details section — label-first inline stat rows.
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f) [ ck_astar_debugger_stats_panel::MakeSectionHeader(TEXT("Details")) ]
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
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f) [ ck_astar_debugger_stats_panel::MakeSectionHeader(TEXT("Selected Cell")) ]
                                    + SVerticalBox::Slot().AutoHeight()
                                        [
                                            SAssignNew(_CellDetailBox, SVerticalBox)
                                                + SVerticalBox::Slot()
                                                    .AutoHeight()
                                                    [
                                                        SNew(SCkDebug_SelectableLabel)
                                                            .Text(FText::FromString(TEXT("Click a cell on the grid to inspect it")))
                                                            .Font_Static(&ck_astar_debugger_stats_panel::Get_EmptyStateFont)
                                                            .ColorAndOpacity(CkStyle::TextMute())
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
                        .Font_Static(&ck_astar_debugger_stats_panel::Get_EmptyStateFont)
                        .ColorAndOpacity(CkStyle::TextMute())
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

            auto TitleColor = CkStyle::Accent();
            auto BoldFont = TAttribute<FSlateFontInfo>::CreateStatic(&ck_astar_debugger_stats_panel::Get_CellTitleFont);

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

auto
    SCkAStarDebugger_StatsPanel::
    Rebuild_ForStyleChange()
    -> void
{
    // The cell-detail rows are the one sub-tree here that is emitted imperatively, gated on the
    // selected-cell index changing. Poisoning the cached index makes the next Tick re-emit them with
    // whatever the new selection composes to.
    _LastShownCellIndex = MIN_int32;
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
    _BudgetFraction = BudgetPct / 100.0f;
    _BudgetPctText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(BudgetPct))));

    auto TotalCells = InInfo.GridWidth * InInfo.GridHeight;
    auto Reachable = TotalCells - InInfo.BlockedCells.Num();

    if (Reachable > 0)
    {
        auto ExplPct = static_cast<float>(InInfo.ClosedSetSize) / static_cast<float>(Reachable);
        _ExplorationFraction = FMath::Clamp(ExplPct, 0.0f, 1.0f);
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
