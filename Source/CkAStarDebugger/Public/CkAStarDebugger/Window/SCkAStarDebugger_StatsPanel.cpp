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

// ====================================================================================================================
// Helper: stat row (label + value)
// ====================================================================================================================

static auto
    MakeStatRow(
        const FString& InLabel,
        TSharedPtr<SCkDebug_SelectableLabel>& OutValueText,
        const FString& InDefaultValue = TEXT("—"))
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(InLabel))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Secondary)
            ]
        + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SAssignNew(OutValueText, SCkDebug_SelectableLabel)
                    .Text(FText::FromString(InDefaultValue))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                    .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Primary)
            ];
}

// ====================================================================================================================
// Helper: mini stat card
// ====================================================================================================================

static auto
    MakeMiniStat(
        TSharedPtr<SCkDebug_SelectableLabel>& OutValueText,
        const FString& InLabel,
        const FString& InDefaultValue = TEXT("0"))
    -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .Padding(FMargin(8.0f))
        [
            SNew(SVerticalBox)
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Center)
                    [
                        SAssignNew(OutValueText, SCkDebug_SelectableLabel)
                            .Text(FText::FromString(InDefaultValue))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                            .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Primary)
                    ]
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Center)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(InLabel))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                            .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Secondary)
                    ]
        ];
}

// ====================================================================================================================
// Helper: section header
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

                        // Mini stats grid (2x2)
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().FillWidth(1.0f) [ MakeMiniStat(_IterationsText, TEXT("ITERATIONS")) ]
                                    + SHorizontalBox::Slot().FillWidth(1.0f) [ MakeMiniStat(_OpenText, TEXT("OPEN")) ]
                                    + SHorizontalBox::Slot().FillWidth(1.0f) [ MakeMiniStat(_ClosedText, TEXT("CLOSED")) ]
                                    + SHorizontalBox::Slot().FillWidth(1.0f) [ MakeMiniStat(_PathText, TEXT("PATH"), TEXT("\u2014")) ]
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

                        // Details section
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                            [
                                SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f) [ MakeSectionHeader(TEXT("Details")) ]
                                    + SVerticalBox::Slot().AutoHeight() [ MakeStatRow(TEXT("Grid"), _GridSizeText) ]
                                    + SVerticalBox::Slot().AutoHeight() [ MakeStatRow(TEXT("Blocked"), _BlockedText) ]
                                    + SVerticalBox::Slot().AutoHeight() [ MakeStatRow(TEXT("Cost"), _CostText) ]
                                    + SVerticalBox::Slot().AutoHeight() [ MakeStatRow(TEXT("Time"), _TimeText) ]
                                    + SVerticalBox::Slot().AutoHeight() [ MakeStatRow(TEXT("Threshold"), _ThresholdText, TEXT("disabled")) ]
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
                : FString(TEXT("\u2014"));

            auto FStr = FString(TEXT("\u2014"));
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
            auto Font = FCoreStyle::GetDefaultFontStyle("Regular", 9);
            auto BoldFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);

            _CellDetailBox->AddSlot().AutoHeight()
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(FString::Printf(TEXT("Cell (%d, %d) \u2014 #%d"), CellX, CellY, SelectedCell)))
                        .Font(BoldFont)
                        .ColorAndOpacity(TitleColor)
                ];

            auto AddDetailRow = [&](const FString& InLabel, const FString& InValue)
            {
                TSharedPtr<SCkDebug_SelectableLabel> Dummy;
                _CellDetailBox->AddSlot().AutoHeight() [ MakeStatRow(InLabel, Dummy, InValue) ];
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
    _IterationsText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo.TotalIterations)));
    _OpenText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo.OpenSetSize)));
    _ClosedText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo.ClosedSetSize)));

    if (InInfo.Path.Num() > 0)
    {
        _PathText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo.Path.Num())));
    }
    else
    {
        _PathText->SetText(FText::FromString(TEXT("\u2014")));
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
        _GridSizeText->SetText(FText::FromString(FString::Printf(TEXT("%d x %d"), InInfo.GridWidth, InInfo.GridHeight)));
    }
    else
    {
        _GridSizeText->SetText(FText::FromString(TEXT("\u2014")));
    }

    _BlockedText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo.BlockedCells.Num())));

    if (InInfo.TotalCost > 0.0f)
    {
        _CostText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), InInfo.TotalCost)));
    }
    else
    {
        _CostText->SetText(FText::FromString(TEXT("\u2014")));
    }

    _TimeText->SetText(FText::FromString(FString::Printf(TEXT("%lld us"), InInfo.TotalTimeMicroseconds)));

    if (InInfo.CostThreshold > 0.0f)
    {
        _ThresholdText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), InInfo.CostThreshold)));
    }
    else
    {
        _ThresholdText->SetText(FText::FromString(TEXT("disabled")));
    }

    _IterationsText->SetColorAndOpacity(CkAStarDebugger::GetStatusColor(InInfo.SearchStatus));
}

// ====================================================================================================================
