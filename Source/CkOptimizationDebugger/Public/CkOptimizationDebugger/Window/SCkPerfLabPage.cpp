#include "SCkPerfLabPage.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

#include <Engine/World.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <Styling/AppStyle.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SCheckBox.h>
#include <Widgets/Input/SSpinBox.h>
#include <Widgets/Layout/SBorder.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SWrapBox.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Views/STableRow.h>

#if WITH_EDITOR
// GEditor, GLevelEditorModeTools and FEditorDelegates. This module is a DeveloperTool — it ships in packaged
// Development and DebugGame, where none of these exist — so every use of them is guarded, and each guard below
// states what the page does instead.
#include <Editor.h>
#include <EditorModeManager.h>
#include <LevelEditor.h>
#endif

#define LOCTEXT_NAMESPACE "SCkPerfLabPage"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_page
{
    // The measurement is a separate process writing files; polling them a few times a second is plenty and keeps this
    // page off the per-frame path entirely, which is what the window's no-Tick rule requires.
    constexpr auto k_PollIntervalSec = 0.25f;

    // Fixed rather than drawn from a clock: the seed feeds position planning, so holding it constant is what makes
    // two runs of one map produce the same positions and therefore compare at all.
    constexpr auto k_PlannerSeed = 1337;

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

    /**
     * What each mode actually costs and buys. Modes are parameter presets over ONE measurement path — position
     * count, directions per position, and how hard the runner tries to settle before sampling — so the difference
     * between them is coverage and runtime, never method.
     */
    auto
        Get_ModeTooltip(
            ECk_PerfLab_Mode InMode)
        -> FString
    {
        switch (InMode)
        {
            case ECk_PerfLab_Mode::Quick:
                return TEXT("Quick — the coarsest. Answers whether this level has a problem at all, not precisely "
                            "where. Fewest positions, fewest camera directions, shortest dwell.");

            case ECk_PerfLab_Mode::Standard:
                return TEXT("Standard — the default. Denser sampling at a runtime most levels can absorb, and "
                            "positions that miss the budget are measured again.");

            case ECk_PerfLab_Mode::Deep:
                return TEXT("Deep — the densest sampling, extra camera pitches and a second full pass. Much the "
                            "longest of the three; use it once Standard has told you roughly where to look.");
        }

        return FString{};
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

    /**
     * The level the user is actually looking at, which is the one they mean by "this level".
     *
     * In a packaged Development build there is no editor world — the running one is the only world there is, and
     * it is equally the level the user is looking at. The measurement itself does not care which produced the
     * path: the child is launched with it either way.
     */
    auto
        Get_CurrentMapPath()
        -> FString
    {
#if WITH_EDITOR
        if (ck::IsValid(GEditor))
        {
            const auto* EditorWorld = GEditor->GetEditorWorldContext().World();

            if (ck::IsValid(EditorWorld))
            {
                return EditorWorld->GetOutermost()->GetPathName();
            }
        }
#endif

        // GWorld is a UWorldProxy, not a pointer — converted explicitly so the validity check is on the thing
        // being dereferenced rather than on the proxy.
        const UWorld* World = GWorld;

        if (ck::Is_NOT_Valid(World))
        {
            return FString{};
        }

        return World->GetOutermost()->GetPathName();
    }

    /** Where a 0-100 score sits, in words. A bare number does not tell a reader whether to act on it. */
    auto
        Get_ScoreBand(
            int32 InScore)
        -> FString
    {
        if (InScore >= 90) { return TEXT("Comfortable"); }
        if (InScore >= 75) { return TEXT("Good"); }
        if (InScore >= 50) { return TEXT("Marginal"); }

        return TEXT("Over budget");
    }

    auto
        Get_ScoreTone(
            int32 InScore)
        -> ECk_Tone
    {
        if (InScore >= 90) { return ECk_Tone::Ok; }
        if (InScore >= 75) { return ECk_Tone::Info; }
        if (InScore >= 50) { return ECk_Tone::Warn; }

        return ECk_Tone::Err;
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

#if WITH_EDITOR
    // Everything on this page is scoped to one level — the Run target, the heatmap's coordinates, which sessions are
    // worth looking at. Polling for a map change in a paint lambda would keep the LABEL honest and leave the rest
    // stale, so the change is handled as the event it is.
    //
    // Outside the editor there is no "open a different level" to react to: the running world is fixed for the
    // lifetime of this page, so the path read at construction stays correct.
    _MapOpenedHandle = FEditorDelegates::OnMapOpened.AddSP(this, &SCkPerfLabPage::DoHandle_MapOpened);
#endif

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

SCkPerfLabPage::~SCkPerfLabPage()
{
#if WITH_EDITOR
    // FEditorDelegates is a global that outlives every window bound to it.
    if (_MapOpenedHandle.IsValid())
    {
        FEditorDelegates::OnMapOpened.Remove(_MapOpenedHandle);
    }
#endif

    // The slot is a process-global and the EdMode is owned by the level editor, so neither dies with this page. Left
    // published, the overlay would keep drawing over the viewport with no UI in existence to turn it off — and a
    // reopened page would come back with its toggle reading "off" while markers were still on screen.
    ck::perf_lab::heatmap::Clear();
    ck::perf_lab::heatmap::Set_SelectedPositionId(FString{});

    DoSet_HeatmapModeActive(false);

    // Detach the item source before _SessionRows is destroyed. This body runs BEFORE any member does, whereas the
    // list widget itself is released by the base class's ChildSlot AFTER all of them — so without this the widget
    // briefly holds the address of a freed array. Declaration order cannot close that window, because the base
    // class always outlives the derived members.
    //
    // ClearItemsSource, NOT SetItemsSource(nullptr): the latter ensures on a null argument and only reaches the
    // clear path after firing, so it turns this teardown into an ensure on every tab close. Detaching is the
    // supported operation and has its own verb.
    if (ck::IsValid(_SessionListView))
    {
        _SessionListView->ClearItemsSource();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkPerfLabPage::
    Get_IsRunning() const
    -> bool
{
    return ck::IsValid(_Child) && _Child->Get_Outcome() == ck::perf_lab::ECk_ChildOutcome::Running;
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
        // Check boxes styled as a button row, not plain buttons: these are mutually exclusive and exactly one is
        // always in effect, so the control has to SHOW which. Three identical buttons that visibly do nothing when
        // pressed read as broken rather than as a choice already made.
        ModeButtons->AddSlot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SCheckBox)
            .Style(FAppStyle::Get(), "RadioButton")
            .ToolTipText(FText::FromString(ck_perf_lab_page::Get_ModeTooltip(Mode)))
            .IsEnabled_Lambda([this]() -> bool { return NOT Get_IsRunning(); })
            .IsChecked_Lambda([this, Mode]() -> ECheckBoxState
            {
                return _Mode == Mode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([this, Mode](ECheckBoxState InState)
            {
                // Radio semantics: a click always SELECTS. Unchecking the active mode would leave no mode at all.
                if (InState == ECheckBoxState::Checked)
                {
                    _Mode = Mode;
                }
            })
            [
                SNew(STextBlock)
                .Margin(FMargin{CkStyle::SpaceS, 0.0f, CkStyle::SpaceS, 0.0f})
                .Text(FText::FromString(ck_perf_lab_page::Get_ModeLabel(Mode)))
            ]
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
                .Text_Lambda([]() -> FText
                {
                    // Read live rather than from the cached path: this label is the user's only confirmation of what
                    // Run will measure, so it must never name a level they have since closed.
                    const auto MapPath = ck_perf_lab_page::Get_CurrentMapPath();

                    return FText::FromString(MapPath.IsEmpty()
                        ? TEXT("No level open — open one to measure it.")
                        : FString::Printf(TEXT("Level:  %s"), *MapPath));
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
                    SNew(STextBlock).Text(FText::FromString(TEXT("Target")))
                ]

                // FPS presets, because a target is something people hold in frames per second and then have to
                // convert. The millisecond box stays beside them — it is what everything is actually measured
                // against, and hiding it would make the score's own formula unreadable.
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    DoBuild_TargetPresets()
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

                    // Committed, not per-tick: analysis is the budget's only consumer, and re-running it on every
                    // frame of a drag would burn work nobody sees. Without this the score, the findings and the
                    // whole compare table keep describing the OLD budget while the spinner beside them reads the new
                    // one.
                    .OnValueCommitted_Lambda([this](float InValue, ETextCommit::Type)
                    {
                        _BudgetMs = InValue;
                        DoSelect_Session(_SelectedSessionId);
                    })
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
                    .IsEnabled_Lambda([]() -> bool { return NOT ck_perf_lab_page::Get_CurrentMapPath().IsEmpty(); })
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
    DoBuild_TargetPresets()
    -> TSharedRef<SWidget>
{
    auto Row = SNew(SHorizontalBox);

    // The three targets worth one click. Anything else is typed into the millisecond box beside them, which is why
    // there is no "Custom" preset — the custom case is the box, already visible.
    const auto Presets = TArray<int32>{30, 60, 120};

    for (const auto Fps : Presets)
    {
        const auto BudgetMs = 1000.0f / static_cast<float>(Fps);

        Row->AddSlot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
        [
            SNew(SCheckBox)
            .Style(FAppStyle::Get(), "RadioButton")
            .IsEnabled_Lambda([this]() -> bool { return NOT Get_IsRunning(); })
            .ToolTipText(FText::FromString(ck::Format_UE(
                TEXT("{} FPS — a {:.2f} ms frame budget. Every score component and every finding is judged against it."),
                Fps, BudgetMs)))
            .IsChecked_Lambda([this, BudgetMs]() -> ECheckBoxState
            {
                // Nearly-equal, not exact: the stored budget is whatever the spin box last committed, and a preset
                // should still read as selected when the user typed the same number by hand.
                return FMath::IsNearlyEqual(_BudgetMs, BudgetMs, 0.01f)
                    ? ECheckBoxState::Checked
                    : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([this, BudgetMs](ECheckBoxState InState)
            {
                if (InState != ECheckBoxState::Checked)
                { return; }

                _BudgetMs = BudgetMs;

                // Re-analyse immediately: the whole point of a preset is seeing the score move.
                DoSelect_Session(_SelectedSessionId);
            })
            [
                SNew(STextBlock)
                .Margin(FMargin{CkStyle::SpaceS, 0.0f, CkStyle::SpaceS, 0.0f})
                .Text(FText::FromString(ck::Format_UE(TEXT("{} FPS"), Fps)))
            ]
        ];
    }

    return Row;
}

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
                if (ck::IsValid(InRow))
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

                const auto SessionId  = InRow->Get_SessionId();
                const auto IsReadable = InRow->Get_IsReadable();

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
                        // Never against itself. A session compared to itself can only report that nothing changed,
                        // which looks exactly like a comparison that silently failed.
                        .IsEnabled_Lambda([this, SessionId, IsReadable]
                        {
                            return IsReadable && SessionId != _SelectedSessionId;
                        })
                        .ToolTipText_Lambda([this, SessionId]
                        {
                            return FText::FromString(SessionId == _SelectedSessionId
                                ? TEXT("This is the session being shown. Pick a DIFFERENT run as the baseline — the "
                                       "one you want to know whether things got better or worse since.")
                                : TEXT("Compare the shown session against this one. Same map only."));
                        })
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

    // Tiles rather than a printf-aligned list. Eight weighted components is exactly the amount of information a
    // column of monospaced text turns into a wall — and the weights are the part that makes the score auditable, so
    // they have to stay legible rather than merely present.
    auto Tiles = SNew(SWrapBox).UseAllottedSize(true);

    for (const auto& Component : Score.Get_Components())
    {
        const auto Included = Component.Get_Included();
        const auto Value    = Component.Get_Value();

        Tiles->AddSlot()
        .Padding(0.0f, 0.0f, CkStyle::SpaceS, CkStyle::SpaceS)
        [
            SNew(SBorder)
            .Padding(CkStyle::SpaceS)
            .ToolTipText(FText::FromString(Included
                ? ck::Format_UE(TEXT("Contributes {:.1f}% of the score."), Component.Get_Weight() * 100.0f)
                : ck::Format_UE(TEXT("Excluded — {}. Its weight is redistributed across the components that "
                                     "were measured, rather than scoring this one as zero."),
                                Component.Get_ExcludedReason())))
            [
                SNew(SBox)
                .MinDesiredWidth(190.0f)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(ck::Format_UE(TEXT("{}"), Component.Get_Component())))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SCkDebug_StatusPill)
                            .ShowDot(true)
                            .Tone(Included
                                ? ck_perf_lab_page::Get_ScoreTone(Value)
                                : ECk_Tone::Neutral)
                            .Text(FText::FromString(Included
                                ? ck::Format_UE(TEXT("{}"), Value)
                                : FString{TEXT("not measured")}))
                        ]

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Included
                                ? ck::Format_UE(TEXT("weight {:.0f}%"), Component.Get_Weight() * 100.0f)
                                : FString{TEXT("weight redistributed")}))
                        ]
                    ]
                ]
            ]
        ];
    }

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)

            // The number, then what it means in words. A bare 63/100 does not tell a reader whether to act.
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_StatusPill)
                .ShowDot(true)
                .Tone(ck_perf_lab_page::Get_ScoreTone(Score.Get_Value()))
                .Text(FText::FromString(ck::Format_UE(TEXT("{} / 100   {}"),
                    Score.Get_Value(), ck_perf_lab_page::Get_ScoreBand(Score.Get_Value()))))
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(ck::Format_UE(
                    TEXT("{} position(s) measured   ·   {} finding(s)   ·   against {:.2f} ms"),
                    _SelectedSession.Get_Positions().Num(),
                    _SelectedAnalysis.Get_Findings().Num(),
                    _SelectedAnalysis.Get_BudgetMs())))
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            Tiles
        ]

        // The formula last and small: it has to be present for the score to be auditable, but a reader reaches for
        // it only once, and putting it above the numbers buries them.
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text(FText::FromString(Score.Get_Formula()))
        ];
}

auto
    SCkPerfLabPage::
    DoBuild_Results()
    -> TSharedRef<SWidget>
{
    // Assigned in Construct's ChildSlot before the first call, so a null box is a broken invariant rather than a
    // state this page ever legitimately reaches.
    CK_ENSURE_IF_NOT(ck::IsValid(_ResultsBox), TEXT("The PerfLab page's results box was never built"))
    { return SNullWidget::NullWidget; }

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

    // Grouped by rule, not listed per position. One rule firing at twelve positions is one thing wrong with the
    // level; twelve near-identical blocks make the reader diff them by eye to discover they say the same thing.
    for (const auto& Group : ck::perf_lab::Group_Findings(_SelectedAnalysis))
    {
        const auto& Evidence  = Group.Get_WorstEvidence();
        const auto  Positions = Group.Get_PositionIds().Num();

        auto GroupBox = SNew(SVerticalBox);

        GroupBox->AddSlot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SCkDebug_StatusPill)
                .ShowDot(true)
                .Tone(ck_perf_lab_page::Get_SeverityTone(Group.Get_Severity()))
                .Text(FText::FromString(Group.Get_CheckId()))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Positions == 1
                    ? ck::Format_UE(TEXT("at 1 position"))
                    : ck::Format_UE(TEXT("at {} positions"), Positions)))
            ]
        ];

        // The measurement that justified the finding, shown with it. A finding that cannot say what it measured is
        // exactly what this tool exists not to produce, so the evidence is never a hover or a detail pane. The WORST
        // position leads, because that is the one worth walking to.
        if (Evidence.Get_MeasuredMs() > 0.0f)
        {
            GroupBox->AddSlot()
            .AutoHeight()
            .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .Text(FText::FromString(ck::Format_UE(
                    TEXT("Worst at {}: {} measured {:.2f} ms against {:.2f} ms ({:.2f}x)"),
                    Group.Get_WorstPositionId(),
                    ck::Format_UE(TEXT("{}"), Evidence.Get_Metric()),
                    Evidence.Get_MeasuredMs(),
                    Evidence.Get_BudgetMs(),
                    Evidence.Get_OverBudgetRatio())))
            ];
        }

        // A signal of "none" is the fallback rule saying it matched nothing — printing it as a measurement would
        // dress an absence up as evidence.
        if (Evidence.Get_Signal() != TEXT("none"))
        {
            GroupBox->AddSlot()
            .AutoHeight()
            .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(ck::Format_UE(TEXT("Signal: {} = {:.0f}"),
                    Evidence.Get_Signal(), Evidence.Get_SignalValue())))
            ];
        }

        for (const auto& Contributor : Group.Get_Contributors())
        {
            GroupBox->AddSlot()
            .AutoHeight()
            .Padding(CkStyle::SpaceL, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .ToolTipText(FText::FromString(Contributor.Get_ObjectPath()))
                .Text(FText::FromString(ck::Format_UE(TEXT("{}. {}  ({:.0f} cm)  — {}"),
                    Contributor.Get_Rank(),
                    FPaths::GetCleanFilename(Contributor.Get_ObjectPath()),
                    Contributor.Get_DistanceCm(),
                    Contributor.Get_Note())))
            ];
        }

        for (const auto& Recommendation : Group.Get_Recommendations())
        {
            GroupBox->AddSlot()
            .AutoHeight()
            .Padding(CkStyle::SpaceL, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .Text(FText::FromString(ck::Format_UE(TEXT("gain {} / effort {} — {}"),
                    Recommendation.Get_GainBand(),
                    Recommendation.Get_EffortBand(),
                    Recommendation.Get_Text())))
            ];
        }

        // Every position the rule fired at, so the group never hides which places it is talking about.
        if (Positions > 1)
        {
            GroupBox->AddSlot()
            .AutoHeight()
            .Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .Text(FText::FromString(ck::Format_UE(TEXT("Positions: {}"),
                    FString::Join(Group.Get_PositionIds(), TEXT(", ")))))
            ];
        }

        _ResultsBox->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
        [
            GroupBox
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
            .Text(FText::FromString(ck::IsValid(Frame)
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
    DoHandle_MapOpened(
        const FString& InFilename,
        bool InAsTemplate)
    -> void
{
    _MapPath = ck_perf_lab_page::Get_CurrentMapPath();

    // The published snapshot carries the OLD map's coordinates. The EdMode already refuses to draw it against a
    // different level, but leaving it published would show the toggle reading "on" over an empty viewport with no
    // explanation — so the overlay is dropped rather than left silently inert.
    if (_HeatmapEnabled)
    {
        _HeatmapEnabled = false;

        ck::perf_lab::heatmap::Clear();
        DoSet_HeatmapModeActive(false);
    }

    // The selected session belongs to the level that was open when it was picked.
    DoSelect_Session(FString{});
    DoSelect_Baseline(FString{});

    DoRefresh_Sessions();
    DoBuild_Results();
}

auto
    SCkPerfLabPage::
    DoRefresh_Sessions()
    -> void
{
    // Assigned rather than cleared-and-filled: SListView holds the ADDRESS of this member, which
    // assignment does not change.
    _SessionRows = ck::algo::Transform<TArray<TSharedPtr<FCk_PerfLab_SessionRow>>>(
        ck::perf_lab::Get_SessionRows(),
        [](const FCk_PerfLab_SessionRow& InRow) { return MakeShared<FCk_PerfLab_SessionRow>(InRow); });

    if (ck::IsValid(_SessionListView))
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

    // The page's own mirror of the viewport selection, cleared alongside the slot's. Left set, it would keep naming
    // a position id belonging to the session the reader just navigated away from.
    _ClickedPositionId.Reset();

    // An empty id is how this page says "nothing selected" — deselecting on a map change, for one. That is not a
    // session which failed to load, and it must never reach the store as a path.
    if (NOT InSessionId.IsEmpty() && ck::perf_lab::TryLoad_Session(InSessionId, _SelectedSession))
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
    DoSet_HeatmapModeActive(
        bool InEnabled)
    -> bool
{
#if WITH_EDITOR
    // Publishing a snapshot makes nothing appear on its own. The module's StaticClass reference only makes the mode
    // DISCOVERABLE; until something activates it on the level editor, Render is never called and the overlay is a
    // feature that exists and cannot be seen.
    if (ck::Is_NOT_Valid(GEditor) || ck::IsValid(GEditor->PlayWorld))
    {
        return false;
    }

    auto& ModeTools = GLevelEditorModeTools();

    if (InEnabled)
    {
        ModeTools.ActivateMode(ck::perf_lab::heatmap::k_EdModeId);
    }
    else
    {
        ModeTools.DeactivateMode(ck::perf_lab::heatmap::k_EdModeId);
    }

    GEditor->RedrawLevelEditingViewports();

    // Ask whether it actually took, rather than assuming. Activation can be refused — another exclusive mode holding
    // the editor, or the mode never registering because its module did not load — and an unchecked call turns that
    // into a toggle that reads "on" while nothing draws.
    return ModeTools.IsModeActive(ck::perf_lab::heatmap::k_EdModeId) == InEnabled;
#else
    // No level editor, so no viewport to draw an overlay over. Returning false is what makes the toggle refuse and
    // say so, rather than latching "on" over a viewport that will never render a marker.
    return false;
#endif
}

auto
    SCkPerfLabPage::
    DoToggle_Heatmap()
    -> void
{
    _HeatmapEnabled = NOT _HeatmapEnabled;

    DoPublish_Heatmap();

    if (NOT DoSet_HeatmapModeActive(_HeatmapEnabled) && _HeatmapEnabled)
    {
        // Refuse to show an "on" toggle over a viewport that will not draw. Saying why is the whole point: the
        // failure is otherwise indistinguishable from a heatmap with nothing to show.
        _HeatmapEnabled = false;
        _ExportMessage  = TEXT("The viewport overlay could not be switched on. The editor refused the mode — this "
                               "usually means another exclusive editor mode is active, or the editor is in play "
                               "mode. Leave PIE and try again.");

        ck::perf_lab::heatmap::Clear();
        DoBuild_Results();

        return;
    }

    if (_HeatmapEnabled)
    {
        if (ck::Is_NOT_Valid(_HeatmapTimer))
        {
            _HeatmapTimer = RegisterActiveTimer(ck_perf_lab_page::k_PollIntervalSec,
                FWidgetActiveTimerDelegate::CreateSP(this, &SCkPerfLabPage::DoPoll_Heatmap));
        }
    }
    else if (ck::IsValid(_HeatmapTimer))
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
    // Re-read the open level HERE, not once at construction. The page outlives any number of File > Open Level, and a
    // stale path would launch the child against a map the user is no longer looking at — producing a session whose
    // heatmap then correctly refuses to draw, which reads to them as the tool being broken.
    _MapPath = ck_perf_lab_page::Get_CurrentMapPath();

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
        .Set_Seed(ck_perf_lab_page::k_PlannerSeed)
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
    if (ck::Is_NOT_Valid(_PollTimer))
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
    if (ck::IsValid(_Child))
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
    if (ck::Is_NOT_Valid(_Child))
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
