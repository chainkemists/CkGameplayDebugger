#include "SCkPerfLabPage.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include <Editor.h>
#include <Engine/World.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SSpinBox.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SSeparator.h>
#include <Widgets/Text/STextBlock.h>

#define LOCTEXT_NAMESPACE "SCkPerfLabPage"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_page
{
    // The measurement is a separate process writing files; polling them a few times a second is plenty and keeps this
    // page off the per-frame path entirely, which is what the window's no-Tick rule requires.
    constexpr auto k_PollIntervalSec = 0.25f;

    auto
        Get_ModeLabel(
            ECk_PerfLab_Mode InMode)
        -> FString
    {
        switch (InMode)
        {
            case ECk_PerfLab_Mode::Quick:    return TEXT("Quick");
            case ECk_PerfLab_Mode::Standard: return TEXT("Standard");
            case ECk_PerfLab_Mode::Deep:     return TEXT("Deep");
        }

        return TEXT("Quick");
    }

    /** A sortable, collision-resistant id. The timestamp leads so descending id is newest first. */
    auto
        Make_SessionId(
            const FString& InMapPath)
        -> FString
    {
        return FString::Printf(TEXT("%s-%08x"),
            *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")),
            GetTypeHash(InMapPath));
    }

    /** The level the user is actually looking at, which is the one they mean by "this level". */
    auto
        Get_CurrentMapPath()
        -> FString
    {
        if (ck::Is_NOT_Valid(GEditor))
        {
            return FString{};
        }

        const auto* World = GEditor->GetEditorWorldContext().World();

        if (ck::Is_NOT_Valid(World))
        {
            return FString{};
        }

        return World->GetOutermost()->GetPathName();
    }

    auto
        Get_SeverityTone(
            ECk_PerfLab_Severity InSeverity)
        -> ECk_Tone
    {
        switch (InSeverity)
        {
            case ECk_PerfLab_Severity::Critical: return ECk_Tone::Err;
            case ECk_PerfLab_Severity::Major:    return ECk_Tone::Warn;
            case ECk_PerfLab_Severity::Minor:    return ECk_Tone::Info;
        }

        return ECk_Tone::Neutral;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkPerfLabPage::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _MapPath = ck_perf_lab_page::Get_CurrentMapPath();

    DoRefresh_Sessions();

    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
        [
            SNew(SCkDebug_SectionHeader)
            .Label(FText::FromString(TEXT("Performance")))
            .SubText(FText::FromString(TEXT(
                "measured in a separate game instance on this machine — a relative benchmark, not a packaged-build number")))
            .Underline(true)
            .RightContent()
            [
                SNew(SCkDebug_StatusPill)
                .ShowDot(true)
                .Text_Lambda([this]() -> FText { return Get_StatusText(); })
                .Tone_Lambda([this]() -> ECk_Tone { return Get_StatusTone(); })
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM, 0.0f)
        [
            DoBuild_RunControls()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM, 0.0f)
        [
            DoBuild_SessionList()
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM)
        [
            SNew(SScrollBox)

            + SScrollBox::Slot()
            [
                SAssignNew(_ResultsBox, SVerticalBox)
            ]
        ]
    ];

    DoBuild_Results();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkPerfLabPage::
    Get_IsRunning() const
    -> bool
{
    return _Child.IsValid() && _Child->Get_Outcome() == ck::perf_lab::ECk_ChildOutcome::Running;
}

auto
    SCkPerfLabPage::
    Get_StatusText() const
    -> FText
{
    if (Get_IsRunning())
    {
        const auto& Heartbeat = _Child->Get_Heartbeat();

        return FText::FromString(FString::Printf(TEXT("%s  %d/%d"),
            *ck::Format_UE(TEXT("{}"), Heartbeat.Get_State()),
            Heartbeat.Get_PositionsDone(),
            Heartbeat.Get_PositionsTotal()));
    }

    if (NOT _LastFailure.IsEmpty())
    {
        return FText::FromString(_LastFailure);
    }

    return FText::FromString(_SessionRows.IsEmpty()
        ? TEXT("no sessions yet")
        : FString::Printf(TEXT("%d session(s)"), _SessionRows.Num()));
}

auto
    SCkPerfLabPage::
    Get_StatusTone() const
    -> ECk_Tone
{
    if (Get_IsRunning())
    {
        return ECk_Tone::Info;
    }

    return _LastFailure.IsEmpty() ? ECk_Tone::Neutral : ECk_Tone::Err;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkPerfLabPage::
    DoBuild_RunControls()
    -> TSharedRef<SWidget>
{
    auto ModeButtons = SNew(SHorizontalBox);

    for (const auto Mode : {ECk_PerfLab_Mode::Quick, ECk_PerfLab_Mode::Standard, ECk_PerfLab_Mode::Deep})
    {
        ModeButtons->AddSlot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SButton)
            .Text(FText::FromString(ck_perf_lab_page::Get_ModeLabel(Mode)))
            .ToolTipText(FText::FromString(TEXT(
                "Modes are parameter presets over one measurement path: how many positions, how many directions at "
                "each, and how hard it tries to settle before sampling.")))
            .IsEnabled_Lambda([this]() -> bool { return NOT Get_IsRunning(); })
            .OnClicked_Lambda([this, Mode]() -> FReply
            {
                _Mode = Mode;
                return FReply::Handled();
            })
        ];
    }

    return SNew(SBorder)
        .Padding(CkStyle::SpaceM)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText
                {
                    return FText::FromString(_MapPath.IsEmpty()
                        ? TEXT("No level open — open one to measure it.")
                        : FString::Printf(TEXT("Level:  %s"), *_MapPath));
                })
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("Frame budget (ms)")))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(CkStyle::SpaceS, 0.0f, CkStyle::SpaceL, 0.0f)
                [
                    SNew(SSpinBox<float>)
                    .MinValue(0.5f)
                    .MaxValue(100.0f)
                    .MinDesiredWidth(80.0f)
                    .Value_Lambda([this]() -> float { return _BudgetMs; })
                    .IsEnabled_Lambda([this]() -> bool { return NOT Get_IsRunning(); })
                    .OnValueChanged_Lambda([this](float InValue) { _BudgetMs = InValue; })
                    .ToolTipText(FText::FromString(TEXT(
                        "Every score component and every finding is measured against this. 16.67 is 60fps, 33.3 is 30.")))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    ModeButtons
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SButton)
                    .Text_Lambda([this]() -> FText
                    {
                        return FText::FromString(_HeatmapEnabled ? TEXT("Heatmap: on") : TEXT("Heatmap: off"));
                    })
                    .ToolTipText(FText::FromString(TEXT(
                        "Draws each measured position over the level viewport. Spawns nothing and modifies nothing; "
                        "it will not draw at all if the selected session was measured against a different level.")))
                    .IsEnabled_Lambda([this]() -> bool { return NOT _SelectedSessionId.IsEmpty(); })
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        DoToggle_Heatmap();
                        return FReply::Handled();
                    })
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Export")))
                    .ToolTipText(FText::FromString(TEXT(
                        "Writes report.html, report.csv and report.json beside the session, plus compare.html when a "
                        "baseline is selected. Every file carries the limitation paragraph.")))
                    .IsEnabled_Lambda([this]() -> bool { return NOT _SelectedSessionId.IsEmpty(); })
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        DoExport_Session();
                        return FReply::Handled();
                    })
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text_Lambda([this]() -> FText
                    {
                        return FText::FromString(Get_IsRunning() ? TEXT("Cancel") : TEXT("Run"));
                    })
                    .IsEnabled_Lambda([this]() -> bool { return NOT _MapPath.IsEmpty(); })
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        if (Get_IsRunning())
                        {
                            DoCancel_Run();
                        }
                        else
                        {
                            DoStart_Run();
                        }

                        return FReply::Handled();
                    })
                ]
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkPerfLabPage::
    DoBuild_SessionList()
    -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .MaxDesiredHeight(140.0f)
        [
            SAssignNew(_SessionListView, SListView<TSharedPtr<FCk_PerfLab_SessionRow>>)
            .ListItemsSource(&_SessionRows)
            .SelectionMode(ESelectionMode::Single)
            .OnSelectionChanged_Lambda([this](TSharedPtr<FCk_PerfLab_SessionRow> InRow, ESelectInfo::Type)
            {
                if (InRow.IsValid())
                {
                    DoSelect_Session(InRow->Get_SessionId());
                }
            })
            .OnGenerateRow_Lambda([this](TSharedPtr<FCk_PerfLab_SessionRow> InRow,
                                         const TSharedRef<STableViewBase>& InOwner)
            {
                // An unreadable session still gets a row. Dropping it would leave the reader looking at a folder
                // this page claims does not exist.
                const auto Label = InRow->Get_IsReadable()
                    ? FString::Printf(TEXT("%s    %s    %s"),
                        *InRow->Get_SessionId(),
                        *ck::Format_UE(TEXT("{}"), InRow->Get_State()),
                        *FPaths::GetCleanFilename(InRow->Get_MapPath()))
                    : FString::Printf(TEXT("%s    (unreadable)"), *InRow->Get_SessionId());

                const auto SessionId = InRow->Get_SessionId();

                return SNew(STableRow<TSharedPtr<FCk_PerfLab_SessionRow>>, InOwner)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Text(FText::FromString(Label))
                    ]

                    // An explicit button rather than a modifier-click: the baseline of a comparison is
                    // a deliberate choice, and a gesture nobody can see is a feature nobody finds.
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .IsEnabled(InRow->Get_IsReadable())
                        .Text_Lambda([this, SessionId]
                        {
                            return FText::FromString(_BaselineSessionId == SessionId
                                ? TEXT("Baseline ✓")
                                : TEXT("Baseline"));
                        })
                        .OnClicked_Lambda([this, SessionId]
                        {
                            // Clicking the current baseline clears it, so there is always a way back to
                            // reading one session on its own.
                            DoSelect_Baseline(_BaselineSessionId == SessionId ? FString{} : SessionId);
                            return FReply::Handled();
                        })
                    ]
                ];
            })
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkPerfLabPage::
    DoBuild_ScoreCard()
    -> TSharedRef<SWidget>
{
    const auto& Score = _SelectedAnalysis.Get_Score();

    auto Components = SNew(SVerticalBox);

    // The whole component table, including the parts that were excluded. A score nobody can decompose is a rating to
    // be trusted rather than a measurement to be read, and the excluded rows are exactly where that trust is earned.
    for (const auto& Component : Score.Get_Components())
    {
        Components->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(Component.Get_Included()
                ? FString::Printf(TEXT("   %-26s  weight %5.3f   score %3d"),
                    *ck::Format_UE(TEXT("{}"), Component.Get_Component()),
                    Component.Get_Weight(),
                    Component.Get_Value())
                : FString::Printf(TEXT("   %-26s  excluded — %s"),
                    *ck::Format_UE(TEXT("{}"), Component.Get_Component()),
                    *Component.Get_ExcludedReason())))
        ];
    }

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("Score  %d / 100"), Score.Get_Value())))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text(FText::FromString(Score.Get_Formula()))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            Components
        ];
}

auto
    SCkPerfLabPage::
    DoBuild_Results()
    -> TSharedRef<SWidget>
{
    if (NOT _ResultsBox.IsValid())
    {
        return SNullWidget::NullWidget;
    }

    _ResultsBox->ClearChildren();

    if (_SelectedSessionId.IsEmpty())
    {
        _ResultsBox->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("Select a session, or press Run to measure this level.")))
        ];

        return _ResultsBox.ToSharedRef();
    }

    _ResultsBox->AddSlot()
    .AutoHeight()
    [
        DoBuild_ScoreCard()
    ];

    if (NOT _ClickedPositionId.IsEmpty())
    {
        _ResultsBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("Selected in viewport: %s"), *_ClickedPositionId)))
        ];
    }

    // "Measured clean" is a statement, not an absence. A level nobody scanned and a level with nothing wrong look
    // identical if the page only ever shows findings.
    if (_SelectedAnalysis.Get_MeasuredClean())
    {
        _ResultsBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            SNew(SCkDebug_StatusPill)
            .ShowDot(true)
            .Tone(ECk_Tone::Ok)
            .Text(FText::FromString(TEXT("Measured clean — every position was inside budget")))
        ];
    }

    _ResultsBox->AddSlot()
    .AutoHeight()
    .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(FString::Printf(TEXT("Positions measured: %d      Findings: %d"),
            _SelectedSession.Get_Positions().Num(),
            _SelectedAnalysis.Get_Findings().Num())))
    ];

    for (const auto& Finding : _SelectedAnalysis.Get_Findings())
    {
        const auto& Evidence = Finding.Get_Evidence();

        auto FindingBox = SNew(SVerticalBox);

        FindingBox->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_StatusPill)
            .ShowDot(true)
            .Tone(ck_perf_lab_page::Get_SeverityTone(Finding.Get_Severity()))
            .Text(FText::FromString(FString::Printf(TEXT("%s   at %s"),
                *Finding.Get_CheckId(), *Finding.Get_PositionId())))
        ];

        // The measurement that justified the finding, shown with it. A finding that cannot say what it measured is
        // exactly what this tool exists not to produce, so the evidence is never a hover or a detail pane.
        FindingBox->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("   %s measured %.2f ms against a %.2f ms budget (%.2fx) — %s = %.0f"),
                *ck::Format_UE(TEXT("{}"), Evidence.Get_Metric()),
                Evidence.Get_MeasuredMs(),
                Evidence.Get_BudgetMs(),
                Evidence.Get_OverBudgetRatio(),
                *Evidence.Get_Signal(),
                Evidence.Get_SignalValue())))
        ];

        for (const auto& Contributor : Finding.Get_Contributors())
        {
            FindingBox->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("      %d. %s  (%.0f cm) — %s"),
                    Contributor.Get_Rank(),
                    *FPaths::GetCleanFilename(Contributor.Get_ObjectPath()),
                    Contributor.Get_DistanceCm(),
                    *Contributor.Get_Note())))
            ];
        }

        for (const auto& Recommendation : Finding.Get_Recommendations())
        {
            FindingBox->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .Text(FText::FromString(FString::Printf(TEXT("      gain %s / effort %s — %s"),
                    *ck::Format_UE(TEXT("{}"), Recommendation.Get_GainBand()),
                    *ck::Format_UE(TEXT("{}"), Recommendation.Get_EffortBand()),
                    *Recommendation.Get_Text())))
            ];
        }

        _ResultsBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            FindingBox
        ];
    }

    _ResultsBox->AddSlot()
    .AutoHeight()
    .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
    [
        DoBuild_Compare()
    ];

    if (NOT _ExportMessage.IsEmpty())
    {
        _ResultsBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            SNew(STextBlock).Text(FText::FromString(_ExportMessage))
        ];
    }

    return _ResultsBox.ToSharedRef();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkPerfLabPage::
    DoBuild_Compare()
    -> TSharedRef<SWidget>
{
    if (_BaselineSessionId.IsEmpty())
    {
        return SNew(STextBlock)
            .Text(FText::FromString(TEXT("Press Baseline on another session to compare this one against it.")));
    }

    auto Box = SNew(SVerticalBox);

    Box->AddSlot()
    .AutoHeight()
    [
        SNew(SCkDebug_SectionHeader)
        .Label(FText::FromString(FString::Printf(TEXT("Compared against %s"), *_BaselineSessionId)))
    ];

    // A refusal replaces the table rather than sitting above an empty one, because the reason IS the
    // whole result: there is nothing underneath it to read.
    if (NOT _Compare.Get_IsComparable())
    {
        Box->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_StatusPill)
            .ShowDot(true)
            .Tone(ECk_Tone::Warn)
            .Text(FText::FromString(ck::perf_lab::Get_RefusalText(_Compare.Get_Refusal())))
        ];

        return Box;
    }

    // Warnings sit ABOVE the numbers they qualify. Below them, a reader has already drawn a
    // conclusion from a table that was never comparable in the way they assumed.
    for (const auto Warning : _Compare.Get_Warnings())
    {
        Box->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_StatusPill)
            .ShowDot(true)
            .Tone(ECk_Tone::Warn)
            .Text(FText::FromString(ck::perf_lab::Get_WarningText(Warning)))
        ];
    }

    // Always set by this point: the refusal returned above, and a comparison that was not refused
    // always has a delta because both scores were computed against the same budget.
    const auto ScoreDelta = _Compare.Get_ScoreDelta().Get(0.0f);

    Box->AddSlot()
    .AutoHeight()
    .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(FString::Printf(
            TEXT("Score %.1f → %.1f  (%+.1f)      %d regressed, %d improved"),
            _Compare.Get_BaselineScore(), _Compare.Get_CurrentScore(), ScoreDelta,
            _Compare.Get_RegressedCount(), _Compare.Get_ImprovedCount())))
    ];

    for (const auto& Position : _Compare.Get_Positions())
    {
        // Unchanged positions are the bulk of any comparison and carry no information a reader is
        // scanning for. They stay in the export; the page shows what moved.
        if (Position.Get_Verdict() == ECk_PerfLab_CompareVerdict::Unchanged ||
            Position.Get_Verdict() == ECk_PerfLab_CompareVerdict::Incomparable)
        {
            continue;
        }

        const auto Frame = Position.Get_FrameDelta();

        Box->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_StatusPill)
            .ShowDot(true)
            .Tone(Position.Get_Verdict() == ECk_PerfLab_CompareVerdict::Regressed
                ? ECk_Tone::Err
                : ECk_Tone::Ok)
            .Text(FText::FromString(Frame.IsSet()
                ? FString::Printf(TEXT("%s   %s   %.2f → %.2f ms  (%+.2f, band ±%.2f)"),
                      *Position.Get_PositionId(),
                      *ck::Format_UE(TEXT("{}"), Position.Get_Verdict()),
                      Frame->Get_BaselineMs(), Frame->Get_CurrentMs(),
                      Frame->Get_DeltaMs(), Frame->Get_NoiseBandMs())
                : FString::Printf(TEXT("%s   %s"),
                      *Position.Get_PositionId(),
                      *ck::Format_UE(TEXT("{}"), Position.Get_Verdict()))))
        ];
    }

    // Positions on one side only are the finding, not noise to drop: a map that changed shape
    // between two runs is what a reader chasing a regression needs to know first.
    const auto AppendIdList = [&Box](const TCHAR* InLabel, const TArray<FString>& InIds)
    {
        if (InIds.IsEmpty())
        { return; }

        Box->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text(FText::FromString(FString::Printf(TEXT("%s: %s"), InLabel, *FString::Join(InIds, TEXT(", ")))))
        ];
    };

    AppendIdList(TEXT("Only in the baseline"), _Compare.Get_OnlyInBaseline());
    AppendIdList(TEXT("Only in this session"), _Compare.Get_OnlyInCurrent());

    return Box;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkPerfLabPage::
    DoRefresh_Sessions()
    -> void
{
    _SessionRows.Reset();

    for (const auto& Row : ck::perf_lab::Get_SessionRows())
    {
        _SessionRows.Add(MakeShared<FCk_PerfLab_SessionRow>(Row));
    }

    if (_SessionListView.IsValid())
    {
        _SessionListView->RequestListRefresh();
    }
}

auto
    SCkPerfLabPage::
    DoSelect_Session(
        const FString& InSessionId)
    -> void
{
    _SelectedSessionId = InSessionId;
    _SelectedSession   = FCk_PerfLab_Session{};
    _SelectedAnalysis  = FCk_PerfLab_Analysis{};

    if (ck::perf_lab::TryLoad_Session(InSessionId, _SelectedSession))
    {
        // Analysed against the budget shown on this page rather than the one the run was requested with, so a
        // captured session can be re-read against a different target without measuring again.
        _SelectedAnalysis = ck::perf_lab::Analyse_Session(_SelectedSession, _BudgetMs);
    }

    ck::perf_lab::heatmap::Set_SelectedPositionId(FString{});

    DoRecompute_Compare();
    DoPublish_Heatmap();
    DoBuild_Results();
}

auto
    SCkPerfLabPage::
    DoSelect_Baseline(
        const FString& InSessionId)
    -> void
{
    _BaselineSessionId = InSessionId;
    _BaselineSession   = FCk_PerfLab_Session{};

    if (NOT InSessionId.IsEmpty() && NOT ck::perf_lab::TryLoad_Session(InSessionId, _BaselineSession))
    {
        // A baseline that will not load is dropped rather than silently compared as an empty
        // session — comparing against nothing would report every position as newly appeared.
        _BaselineSessionId.Reset();
    }

    DoRecompute_Compare();
    DoBuild_Results();
}

auto
    SCkPerfLabPage::
    DoRecompute_Compare()
    -> void
{
    _Compare = FCk_PerfLab_Compare{};

    if (_BaselineSessionId.IsEmpty() || _SelectedSessionId.IsEmpty())
    {
        return;
    }

    // Both sides are analysed against the budget on this page, so the comparison answers "against
    // the target I care about now" rather than against two targets set at two different times.
    _Compare = ck::perf_lab::Compare_Sessions(_BaselineSession, _SelectedSession, _BudgetMs);
}

auto
    SCkPerfLabPage::
    DoExport_Session()
    -> void
{
    if (_SelectedSessionId.IsEmpty())
    {
        return;
    }

    const auto Directory = ck::perf_lab::Get_SessionDirFor(_SelectedSessionId);

    // Written beside the session it describes rather than through a save dialog: the reports belong
    // with the measurement, and a reader who finds the folder finds everything about that run.
    const auto GeneratedAt = FDateTime::UtcNow();

    const auto Write = [&Directory](const TCHAR* InFileName, const FString& InContent)
    {
        return FFileHelper::SaveStringToFile(InContent, *FPaths::Combine(Directory, InFileName),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    };

    const auto Wrote =
        Write(TEXT("report.html"),
            ck::perf_lab::exporter::Build_SessionHtml(_SelectedSession, _SelectedAnalysis, GeneratedAt)) &&
        Write(TEXT("report.csv"),
            ck::perf_lab::exporter::Build_SessionCsvBundle(_SelectedSession, _SelectedAnalysis)) &&
        Write(TEXT("report.json"),
            ck::perf_lab::exporter::Build_SessionJson(_SelectedSession, _SelectedAnalysis, GeneratedAt));

    if (Wrote && NOT _BaselineSessionId.IsEmpty())
    {
        Write(TEXT("compare.html"), ck::perf_lab::exporter::Build_CompareHtml(_Compare, GeneratedAt));
    }

    _ExportMessage = Wrote
        ? FString::Printf(TEXT("Exported to %s"), *Directory)
        : FString::Printf(TEXT("Could not write the reports to %s"), *Directory);

    DoBuild_Results();
}

auto
    SCkPerfLabPage::
    DoPublish_Heatmap()
    -> void
{
    if (NOT _HeatmapEnabled || _SelectedSessionId.IsEmpty())
    {
        ck::perf_lab::heatmap::Clear();
        return;
    }

    ck::perf_lab::heatmap::Publish(
        ck::perf_lab::heatmap::Build_Snapshot(_SelectedSession, _SelectedAnalysis));
}

auto
    SCkPerfLabPage::
    DoToggle_Heatmap()
    -> void
{
    _HeatmapEnabled = NOT _HeatmapEnabled;

    DoPublish_Heatmap();

    if (_HeatmapEnabled)
    {
        if (NOT _HeatmapTimer.IsValid())
        {
            _HeatmapTimer = RegisterActiveTimer(ck_perf_lab_page::k_PollIntervalSec,
                FWidgetActiveTimerDelegate::CreateSP(this, &SCkPerfLabPage::DoPoll_Heatmap));
        }
    }
    else if (_HeatmapTimer.IsValid())
    {
        UnRegisterActiveTimer(_HeatmapTimer.ToSharedRef());
        _HeatmapTimer.Reset();
    }
}

auto
    SCkPerfLabPage::
    DoPoll_Heatmap(
        double InCurrentTime,
        float InDeltaTime)
    -> EActiveTimerReturnType
{
    if (NOT _HeatmapEnabled)
    {
        _HeatmapTimer.Reset();
        return EActiveTimerReturnType::Stop;
    }

    // The EdMode pushes a clicked marker's id back through the slot; picking it up here is what makes a viewport
    // click select the row, without either side holding a reference to the other.
    const auto Clicked = ck::perf_lab::heatmap::Get_SelectedPositionId();

    if (NOT Clicked.IsEmpty() && Clicked != _ClickedPositionId)
    {
        _ClickedPositionId = Clicked;
        DoBuild_Results();
    }

    return EActiveTimerReturnType::Continue;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkPerfLabPage::
    DoStart_Run()
    -> void
{
    if (Get_IsRunning() || _MapPath.IsEmpty())
    {
        return;
    }

    _LastFailure.Reset();

    auto ModeParams = FCk_PerfLab_ModeParams::Get_ForMode(_Mode);

    const auto Request = FCk_PerfLab_Request{}
        .Set_SessionId(ck_perf_lab_page::Make_SessionId(_MapPath))
        .Set_MapPath(_MapPath)
        .Set_BudgetMs(_BudgetMs)
        .Set_Seed(1337)
        .Set_RequestingHostPid(static_cast<int32>(FPlatformProcess::GetCurrentProcessId()))
        .Set_CreatedUtc(FDateTime::UtcNow().ToIso8601())
        .Set_Mode(_Mode)
        .Set_ModeParams(ModeParams);

    _Child = MakeUnique<ck::perf_lab::FCk_MeasurementChild>();

    if (NOT _Child->Try_Launch(Request))
    {
        _LastFailure = _Child->Get_FailureReason();
        _Child.Reset();
        return;
    }

    // Polling starts only once a child exists, and stops itself when one does not — this page costs nothing at all
    // while it is merely open.
    if (NOT _PollTimer.IsValid())
    {
        _PollTimer = RegisterActiveTimer(ck_perf_lab_page::k_PollIntervalSec,
            FWidgetActiveTimerDelegate::CreateSP(this, &SCkPerfLabPage::DoPoll));
    }
}

auto
    SCkPerfLabPage::
    DoCancel_Run()
    -> void
{
    if (_Child.IsValid())
    {
        _Child->Request_Cancel();
    }
}

auto
    SCkPerfLabPage::
    DoPoll(
        double InCurrentTime,
        float InDeltaTime)
    -> EActiveTimerReturnType
{
    if (NOT _Child.IsValid())
    {
        _PollTimer.Reset();
        return EActiveTimerReturnType::Stop;
    }

    const auto Outcome = _Child->Poll(InDeltaTime);

    if (Outcome == ck::perf_lab::ECk_ChildOutcome::Running)
    {
        return EActiveTimerReturnType::Continue;
    }

    if (Outcome != ck::perf_lab::ECk_ChildOutcome::ExitedCleanly)
    {
        _LastFailure = _Child->Get_FailureReason();
    }

    const auto SessionId = _Child->Get_Heartbeat().Get_SessionId();

    _Child.Reset();
    _PollTimer.Reset();

    DoRefresh_Sessions();

    if (NOT SessionId.IsEmpty())
    {
        DoSelect_Session(SessionId);
    }

    return EActiveTimerReturnType::Stop;
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE
