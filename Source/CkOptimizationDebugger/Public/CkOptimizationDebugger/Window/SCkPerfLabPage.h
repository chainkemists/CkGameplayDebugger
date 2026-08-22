#pragma once

#include "CkEditorTools/Style/CkStyle.h"

#include "CkPerfLab/Analysis/CkPerfLab_Analysis.h"
#include "CkPerfLab/Analysis/CkPerfLab_SessionCompare.h"
#include "CkPerfLab/Export/CkPerfLab_Export.h"
#include "CkPerfLab/Heatmap/CkPerfLab_HeatmapSlot.h"
#include "CkPerfLab/Host/CkPerfLab_SessionStore.h"
#include "CkPerfLab/Host/CkPerfLab_Subprocess.h"

#include <Widgets/SCompoundWidget.h>
#include <Widgets/Views/SListView.h>

// --------------------------------------------------------------------------------------------------------------------
// The Performance page: set up a measurement run, watch it, and read what it found.
//
// It owns no measurement logic at all. Running is a separate process, analysing is a pure function, and this page
// only starts the one and displays the other — which is what keeps the debugger window free of the no-Tick and
// no-live-handle problems a sampling loop in here would create.
// --------------------------------------------------------------------------------------------------------------------

class CKOPTIMIZATIONDEBUGGER_API SCkPerfLabPage : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkPerfLabPage) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

public:
    /** Sessions on disk, so the tab badge can show a count without this page being the one asked. */
    auto Get_SessionCount() const -> int32 { return _SessionRows.Num(); }

private:
    auto DoBuild_RunControls() -> TSharedRef<SWidget>;
    auto DoBuild_SessionList() -> TSharedRef<SWidget>;
    auto DoBuild_Results() -> TSharedRef<SWidget>;
    auto DoBuild_ScoreCard() -> TSharedRef<SWidget>;
    auto DoBuild_Compare() -> TSharedRef<SWidget>;

    auto DoRefresh_Sessions() -> void;
    auto DoSelect_Session(const FString& InSessionId) -> void;
    auto DoSelect_Baseline(const FString& InSessionId) -> void;
    auto DoRecompute_Compare() -> void;
    auto DoStart_Run() -> void;
    auto DoCancel_Run() -> void;
    auto DoExport_Session() -> void;

    auto DoPoll(double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType;
    auto DoPoll_Heatmap(double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType;

    auto DoToggle_Heatmap() -> void;
    auto DoPublish_Heatmap() -> void;

    auto Get_StatusText() const -> FText;
    auto Get_StatusTone() const -> ECk_Tone;
    auto Get_IsRunning() const -> bool;

private:
    // The live child, if any. One at a time: a second concurrent run would contend for the same GPU and measure
    // something neither of them describes.
    TUniquePtr<ck::perf_lab::FCk_MeasurementChild> _Child;

    TArray<TSharedPtr<FCk_PerfLab_SessionRow>> _SessionRows;
    TSharedPtr<SListView<TSharedPtr<FCk_PerfLab_SessionRow>>> _SessionListView;

    FCk_PerfLab_Session  _SelectedSession;
    FCk_PerfLab_Analysis _SelectedAnalysis;

    // The A side of a comparison. Empty until the user picks one, because a compare view that
    // guessed its own baseline would present a claim nobody made.
    FCk_PerfLab_Session _BaselineSession;
    FCk_PerfLab_Compare _Compare;

    TSharedPtr<SVerticalBox> _ResultsBox;

    FString _BaselineSessionId;
    FString _SelectedSessionId;
    FString _MapPath;
    FString _LastFailure;
    FString _ExportMessage;
    FString _ClickedPositionId;

    float _BudgetMs = 16.67f;

    ECk_PerfLab_Mode _Mode = ECk_PerfLab_Mode::Quick;

    TSharedPtr<FActiveTimerHandle> _PollTimer;

    // Only ticks while the heatmap is on, and only to notice a viewport click. An overlay nobody switched on
    // costs this page nothing at all.
    TSharedPtr<FActiveTimerHandle> _HeatmapTimer;

    bool _HeatmapEnabled = false;
};

// --------------------------------------------------------------------------------------------------------------------
