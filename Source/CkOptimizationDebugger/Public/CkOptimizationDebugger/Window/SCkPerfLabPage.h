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

    /** Clears the published snapshot and deactivates the EdMode: the overlay must not outlive the page that owns
     *  its only off switch. */
    virtual ~SCkPerfLabPage() override;

public:
    /** Sessions on disk, so the tab badge can show a count without this page being the one asked. */
    auto Get_SessionCount() const -> int32 { return _SessionRows.Num(); }

private:
    auto DoBuild_RunControls() -> TSharedRef<SWidget>;
    auto DoBuild_TargetPresets() -> TSharedRef<SWidget>;
    auto DoBuild_SessionList() -> TSharedRef<SWidget>;
    auto DoBuild_Results() -> TSharedRef<SWidget>;
    auto DoBuild_ScoreCard() -> TSharedRef<SWidget>;
    auto DoBuild_Compare() -> TSharedRef<SWidget>;

    /** The editor opened a different level. Everything on this page is scoped to one map, so all of it has to move. */
    auto DoHandle_MapOpened(const FString& InFilename, bool InAsTemplate) -> void;

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

    /**
     * Activates or deactivates the EdMode on the level editor, and reports whether it took.
     *
     * Publishing a snapshot alone draws nothing — the mode is registered by its module but never activated until
     * something asks. The return value is load-bearing: if activation fails the page must SAY so, because a toggle
     * reading "on" over an unchanged viewport is indistinguishable from a broken heatmap.
     */
    auto DoSet_HeatmapModeActive(bool InEnabled) -> bool;

    auto Get_StatusText() const -> FText;
    auto Get_StatusTone() const -> ECk_Tone;
    auto Get_IsRunning() const -> bool;

private:
    // The live child, if any. One at a time: a second concurrent run would contend for the same GPU and measure
    // something neither of them describes.
    TUniquePtr<ck::perf_lab::FCk_MeasurementChild> _Child;

    // SListView holds the ADDRESS of _SessionRows as its item source, and the widget is kept alive by the base
    // class's ChildSlot — which destroys after every member here, whichever order they are declared in. The
    // destructor detaches the source explicitly; declaration order cannot fix this on its own.
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

    /** Unsubscribed in the destructor: FEditorDelegates outlives every window bound to it. */
    FDelegateHandle _MapOpenedHandle;

    TSharedPtr<FActiveTimerHandle> _PollTimer;

    // Only ticks while the heatmap is on, and only to notice a viewport click. An overlay nobody switched on
    // costs this page nothing at all.
    TSharedPtr<FActiveTimerHandle> _HeatmapTimer;

    bool _HeatmapEnabled = false;
};

// --------------------------------------------------------------------------------------------------------------------
