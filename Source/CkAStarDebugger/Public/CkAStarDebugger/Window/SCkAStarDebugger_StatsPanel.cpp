#include "CkAStarDebugger/Window/SCkAStarDebugger_StatsPanel.h"
#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"

#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SNullWidget.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/Paths.h"

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

    auto Tokens() -> FCkUiView::FTokens
    {
        return {{TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS)},
                {TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM)},
                {TEXT("--space-l"), FString::SanitizeFloat(CkStyle::SpaceL)},
                {TEXT("--astar-stats-body-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeBody()))},
                {TEXT("--astar-stats-small-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeSmall()))},
                {TEXT("--astar-stats-text"), TEXT("#") + CkStyle::Text().ToFColorSRGB().ToHex()},
                {TEXT("--astar-stats-text-dim"), TEXT("#") + CkStyle::TextDim().ToFColorSRGB().ToHex()},
                {TEXT("--astar-stats-text-mute"), TEXT("#") + CkStyle::TextMute().ToFColorSRGB().ToHex()},
                {TEXT("--astar-stats-accent"), TEXT("#") + CkStyle::Accent().ToFColorSRGB().ToHex()},
                {TEXT("--astar-stats-panel"), TEXT("#") + CkStyle::Bg1().ToFColorSRGB().ToHex()}};
    }
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

    _NativeContent =
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
    ;

    ChildSlot[SAssignNew(_ContentHost, SBox)[_NativeContent.ToSharedRef()]];
    UpdateAuthoredProjection(nullptr);
    TryActivateAuthoredView();
}

SCkAStarDebugger_StatsPanel::~SCkAStarDebugger_StatsPanel()
{
    Release_AuthoredView();
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

    if (_Released || NOT _ViewModel.IsValid())
    { return; }

    if (_AuthoredView.IsValid() && InCurrentTime >= _NextAuthoredPollSeconds)
    {
        _NextAuthoredPollSeconds = InCurrentTime + 0.5;
        _AuthoredView->PollFiles(ck_astar_debugger_stats_panel::Tokens());
        if (NOT _AuthoredView->GetLastResult().Succeeded)
        { _AuthoredLoadFailure = FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n")); }
        else
        {
            _AuthoredLoadFailure.Reset();
            if (NOT _AuthoredMounted)
            {
                _ContentHost->SetContent(_AuthoredView->GetRegion(TEXT("main")));
                _AuthoredMounted = true;
            }
        }
    }

    auto* Info = _ViewModel->Get_CurrentSearchInfo();
    UpdateAuthoredProjection(Info);

    auto EmptyInfo = FCkAStarDebugger_SearchInfo{};
    if (NOT Info) { Info = &EmptyInfo; }

    RefreshFromSearchInfo(*Info);

    auto SelectedCell = _ViewModel->Get_SelectedCellIndex();

    auto CellDetailSignature = GetTypeHash(SelectedCell);
    CellDetailSignature = HashCombine(CellDetailSignature, GetTypeHash(Info->GridWidth));
    CellDetailSignature = HashCombine(CellDetailSignature, GetTypeHash(Info->GoalNode));
    CellDetailSignature = HashCombine(CellDetailSignature, GetTypeHash(Info->BlockedCells.Contains(SelectedCell)));
    CellDetailSignature = HashCombine(CellDetailSignature, GetTypeHash(Info->Path.Contains(SelectedCell)));
    CellDetailSignature = HashCombine(CellDetailSignature, GetTypeHash(Info->ClosedSetCells.Contains(SelectedCell)));
    CellDetailSignature = HashCombine(CellDetailSignature, GetTypeHash(Info->OpenSetCells.Contains(SelectedCell)));
    CellDetailSignature = HashCombine(CellDetailSignature,
        GetTypeHash(Info->GScores.Contains(SelectedCell)));
    CellDetailSignature = HashCombine(CellDetailSignature,
        Info->GScores.Contains(SelectedCell) ? GetTypeHash(Info->GScores[SelectedCell]) : 0u);
    CellDetailSignature = HashCombine(CellDetailSignature,
        GetTypeHash(Info->CameFrom.Contains(SelectedCell)));
    CellDetailSignature = HashCombine(CellDetailSignature,
        Info->CameFrom.Contains(SelectedCell) ? GetTypeHash(Info->CameFrom[SelectedCell]) : 0u);

    if (CellDetailSignature != _LastCellDetailSignature)
    {
        _LastCellDetailSignature = CellDetailSignature;
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
    _LastCellDetailSignature = MAX_uint32;
    if (_AuthoredView.IsValid())
    { _AuthoredView->PollFiles(ck_astar_debugger_stats_panel::Tokens()); }
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
    else
    {
        _ExplorationFraction = 0.0f;
        _ExplorationPctText->SetText(FText::FromString(TEXT("0%")));
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

auto SCkAStarDebugger_StatsPanel::TryActivateAuthoredView() -> void
{
    if (_Released || _AuthoredView.IsValid() || NOT _ContentHost.IsValid()) { return; }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _AuthoredLoadFailure = FString::Join(Errors, TEXT("\n"));
        ActivateNativeFallback();
        return;
    }

    const TWeakPtr<SCkAStarDebugger_StatsPanel> WeakPanel{SharedThis(this)};
    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("astar-stats-copy"), FSimpleDelegate::CreateLambda([WeakPanel]()
    {
        const TSharedPtr<SCkAStarDebugger_StatsPanel> Panel = WeakPanel.Pin();
        if (Panel.IsValid()) { Panel->CopyAuthoredStats(); }
    }));
    auto Data = FCkUiView::FDataBindings{};
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakPanel]()
    {
        const TSharedPtr<SCkAStarDebugger_StatsPanel> Panel = WeakPanel.Pin();
        return Panel.IsValid() && NOT Panel->_Released;
    });
    for (const auto& Pair : _AuthoredText)
    {
        const FString Key = Pair.Key;
        Data.Text.Add(Key, TAttribute<FText>::CreateLambda([WeakPanel, Key]()
        {
            const TSharedPtr<SCkAStarDebugger_StatsPanel> Panel = WeakPanel.Pin();
            return Panel.IsValid() ? Panel->_AuthoredText.FindRef(Key) : FText::GetEmpty();
        }));
    }
    for (const auto& Pair : _AuthoredNumber)
    {
        const FString Key = Pair.Key;
        Data.Number.Add(Key, TAttribute<float>::CreateLambda([WeakPanel, Key]()
        {
            const TSharedPtr<SCkAStarDebugger_StatsPanel> Panel = WeakPanel.Pin();
            return Panel.IsValid() ? Panel->_AuthoredNumber.FindRef(Key) : 0.0f;
        }));
    }
    for (const auto& Pair : _AuthoredColor)
    {
        const FString Key = Pair.Key;
        Data.Color.Add(Key, TAttribute<FLinearColor>::CreateLambda([WeakPanel, Key]()
        {
            const TSharedPtr<SCkAStarDebugger_StatsPanel> Panel = WeakPanel.Pin();
            return Panel.IsValid() ? Panel->_AuthoredColor.FindRef(Key) : FLinearColor::Transparent;
        }));
    }
    for (const auto& Pair : _AuthoredVisibility)
    {
        const FString Key = Pair.Key;
        Data.Visibility.Add(Key, TAttribute<bool>::CreateLambda([WeakPanel, Key]()
        {
            const TSharedPtr<SCkAStarDebugger_StatsPanel> Panel = WeakPanel.Pin();
            return Panel.IsValid() && Panel->_AuthoredVisibility.FindRef(Key);
        }));
    }

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, MoveTemp(Actions), ck_astar_debugger_stats_panel::Tokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Main = Candidate->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Directory, TEXT("AStarDebuggerStats.ui.html")),
        FPaths::Combine(Directory, TEXT("AStarDebuggerStats.ui.css")));
    _AuthoredView = Candidate;
    _AuthoredView->PollFiles();
    if (NOT _AuthoredView->GetLastResult().Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n"));
        return;
    }

    _AuthoredLoadFailure.Reset();
    _ContentHost->SetContent(Main);
    _AuthoredMounted = true;
}

auto SCkAStarDebugger_StatsPanel::ActivateNativeFallback() -> void
{
    _AuthoredView.Reset();
    _AuthoredMounted = false;
    if (_ContentHost.IsValid() && _NativeContent.IsValid())
    { _ContentHost->SetContent(_NativeContent.ToSharedRef()); }
}

auto SCkAStarDebugger_StatsPanel::Release_AuthoredView() -> void
{
    if (_Released) { return; }
    _Released = true;
    _AuthoredView.Reset();
    _AuthoredMounted = false;
    _ViewModel.Reset();
    if (_ContentHost.IsValid()) { _ContentHost->SetContent(SNullWidget::NullWidget); }
}

auto SCkAStarDebugger_StatsPanel::UpdateAuthoredProjection(const FCkAStarDebugger_SearchInfo* InInfo) -> void
{
    auto SetText = [this](const TCHAR* InKey, FString InValue)
    { _AuthoredText.Add(InKey, FText::FromString(MoveTemp(InValue))); };

    SetText(TEXT("astar-stats-iterations"), InInfo ? FString::FromInt(InInfo->TotalIterations) : TEXT("0"));
    SetText(TEXT("astar-stats-open"), InInfo ? FString::FromInt(InInfo->OpenSetSize) : TEXT("0"));
    SetText(TEXT("astar-stats-closed"), InInfo ? FString::FromInt(InInfo->ClosedSetSize) : TEXT("0"));
    SetText(TEXT("astar-stats-path"), InInfo && NOT InInfo->Path.IsEmpty() ? FString::FromInt(InInfo->Path.Num()) : TEXT("—"));
    SetText(TEXT("astar-stats-grid"), InInfo && InInfo->GridWidth > 0 && InInfo->GridHeight > 0
        ? FString::Printf(TEXT("%d x %d"), InInfo->GridWidth, InInfo->GridHeight) : TEXT("—"));
    SetText(TEXT("astar-stats-blocked"), InInfo ? FString::FromInt(InInfo->BlockedCells.Num()) : TEXT("—"));
    SetText(TEXT("astar-stats-cost"), InInfo && InInfo->TotalCost > 0.0f
        ? FString::Printf(TEXT("%.1f"), InInfo->TotalCost) : TEXT("—"));
    SetText(TEXT("astar-stats-time"), InInfo ? FString::Printf(TEXT("%lld us"), InInfo->TotalTimeMicroseconds) : TEXT("—"));
    SetText(TEXT("astar-stats-threshold"), InInfo && InInfo->CostThreshold > 0.0f
        ? FString::Printf(TEXT("%.1f"), InInfo->CostThreshold) : TEXT("disabled"));

    const float BudgetPct = InInfo ? FMath::Clamp(InInfo->BudgetUsagePercent, 0.0f, 100.0f) : 0.0f;
    const int32 TotalCells = InInfo ? InInfo->GridWidth * InInfo->GridHeight : 0;
    const int32 Reachable = InInfo ? TotalCells - InInfo->BlockedCells.Num() : 0;
    const float Exploration = InInfo && Reachable > 0
        ? FMath::Clamp(static_cast<float>(InInfo->ClosedSetSize) / static_cast<float>(Reachable), 0.0f, 1.0f)
        : 0.0f;
    _AuthoredNumber.Add(TEXT("astar-stats-budget-fraction"), BudgetPct / 100.0f);
    _AuthoredNumber.Add(TEXT("astar-stats-exploration-fraction"), Exploration);
    SetText(TEXT("astar-stats-budget-pct"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(BudgetPct)));
    SetText(TEXT("astar-stats-exploration-pct"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Exploration * 100.0f)));
    _AuthoredColor.Add(TEXT("astar-stats-status-color"), InInfo ? CkAStarDebugger::GetStatusColor(InInfo->SearchStatus) : CkStyle::TextDim());
    _AuthoredColor.Add(TEXT("astar-stats-budget-color"), ck::debug_axes::Get_HeatColor(BudgetPct / 100.0f));
    _AuthoredColor.Add(TEXT("astar-stats-exploration-color"), CkStyle::Accent());

    const int32 SelectedCell = _ViewModel.IsValid() ? _ViewModel->Get_SelectedCellIndex() : -1;
    const bool HasCell = InInfo != nullptr && SelectedCell >= 0 && InInfo->GridWidth > 0;
    _AuthoredVisibility.Add(TEXT("astar-stats-cell-selected"), HasCell);
    _AuthoredVisibility.Add(TEXT("astar-stats-cell-empty"), NOT HasCell);
    if (NOT HasCell)
    {
        SetText(TEXT("astar-stats-cell-title"), TEXT(""));
        SetText(TEXT("astar-stats-cell-state"), TEXT("—"));
        SetText(TEXT("astar-stats-cell-g"), TEXT("—"));
        SetText(TEXT("astar-stats-cell-f"), TEXT("—"));
        SetText(TEXT("astar-stats-cell-parent"), TEXT("none"));
        return;
    }

    const int32 CellX = SelectedCell % InInfo->GridWidth;
    const int32 CellY = SelectedCell / InInfo->GridWidth;
    SetText(TEXT("astar-stats-cell-title"), FString::Printf(TEXT("Cell (%d, %d) — #%d"), CellX, CellY, SelectedCell));
    SetText(TEXT("astar-stats-cell-state"), InInfo->BlockedCells.Contains(SelectedCell) ? TEXT("Blocked")
        : InInfo->Path.Contains(SelectedCell) ? TEXT("Path")
        : InInfo->ClosedSetCells.Contains(SelectedCell) ? TEXT("Closed")
        : InInfo->OpenSetCells.Contains(SelectedCell) ? TEXT("Open") : TEXT("Empty"));
    const float* GScore = InInfo->GScores.Find(SelectedCell);
    SetText(TEXT("astar-stats-cell-g"), GScore ? FString::Printf(TEXT("%.1f"), *GScore) : TEXT("—"));
    if (GScore != nullptr)
    {
        const int32 GoalX = InInfo->GoalNode % InInfo->GridWidth;
        const int32 GoalY = InInfo->GoalNode / InInfo->GridWidth;
        const float H = static_cast<float>(FMath::Abs(CellX - GoalX) + FMath::Abs(CellY - GoalY));
        SetText(TEXT("astar-stats-cell-f"), FString::Printf(TEXT("%.1f"), *GScore + H));
    }
    else
    { SetText(TEXT("astar-stats-cell-f"), TEXT("—")); }
    const int32* Parent = InInfo->CameFrom.Find(SelectedCell);
    SetText(TEXT("astar-stats-cell-parent"), Parent ? FString::FromInt(*Parent) : TEXT("none"));
}

auto SCkAStarDebugger_StatsPanel::CopyAuthoredStats() -> void
{
    if (_Released) { return; }
    const auto Text = [this](const TCHAR* InKey) { return _AuthoredText.FindRef(InKey).ToString(); };
    const FString Payload = FString::Printf(
        TEXT("A* search stats\n")
        TEXT("  iterations: %s\n  open:       %s\n  closed:     %s\n  path:       %s\n")
        TEXT("  budget:     %s\n  exploration: %s\n  grid:       %s\n  blocked:    %s\n")
        TEXT("  cost:       %s\n  time:       %s\n  threshold:  %s\n")
        TEXT("  cell:       %s\n  state:      %s\n  g-score:    %s\n  f-score:    %s\n  parent:     %s"),
        *Text(TEXT("astar-stats-iterations")), *Text(TEXT("astar-stats-open")), *Text(TEXT("astar-stats-closed")),
        *Text(TEXT("astar-stats-path")), *Text(TEXT("astar-stats-budget-pct")), *Text(TEXT("astar-stats-exploration-pct")),
        *Text(TEXT("astar-stats-grid")), *Text(TEXT("astar-stats-blocked")), *Text(TEXT("astar-stats-cost")),
        *Text(TEXT("astar-stats-time")), *Text(TEXT("astar-stats-threshold")), *Text(TEXT("astar-stats-cell-title")),
        *Text(TEXT("astar-stats-cell-state")), *Text(TEXT("astar-stats-cell-g")), *Text(TEXT("astar-stats-cell-f")),
        *Text(TEXT("astar-stats-cell-parent")));
    FPlatformApplicationMisc::ClipboardCopy(*Payload);
}

// ====================================================================================================================
