#pragma once

#include "CoreMinimal.h"

#include "CkInsightsAnalyzer/Core/CkTraceSession.h"
#include "CkInsightsAnalyzer/Report/CkFrameReport.h"
#include "CkInsightsAnalyzer/Report/CkMultiFrameReport.h"
#include "CkInsightsDebugger/Capture/CkInsightsCaptureController.h"
#include "CkInsightsDebugger/Widgets/SCkFrameBarChart.h"

#include "CkEditorTools/Style/CkStyle.h"

#include <Containers/Ticker.h>
#include <Styling/SlateTypes.h>
#include <Widgets/SCompoundWidget.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Text/SMultiLineEditableText.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/STextComboBox.h>
#include <Widgets/Views/STreeView.h>

struct FSlateDynamicImageBrush;

// --------------------------------------------------------------------------------------------------------------------

/**
 * Main editor tab for the Insights Analyzer.
 *
 * Layout (top to bottom):
 *   1. Common chrome — Debuggers menu, analyzer toolbar, and trace-analysis status
 *   2. Summary strip — stat tiles (trace info; frame/aggregate numbers after analysis)
 *   3. Frame bar chart (SCkFrameBarChart, ~200px)
 *   4. Results       — splitter: hot-path tree (left) | categories / waits / top timers /
 *                      worst frames / category averages (right)
 *   5. Raw report    — collapsed expandable area with the markdown text (also what
 *                      "Copy Report" puts on the clipboard)
 *
 * All colors/fonts route through CkStyle:: (CkEditorTools). Side panels are
 * rebuild-on-analysis (not SListView) — they are small, and rebuilding avoids
 * the listview-inside-scrollbox desired-size pitfalls.
 */
class CKINSIGHTSDEBUGGER_API SCkInsightsAnalyzerTab : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInsightsAnalyzerTab) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInsightsAnalyzerTab() override;

    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:

    // ---- Results mode ----

    enum class EResultsMode : uint8
    {
        None,
        SingleFrame,
        MultiFrame,
    };

    // ---- UI Construction ----

    auto DoCreateCaptureControls() -> TSharedRef<SWidget>;
    auto DoCreateProfilingActions() -> TSharedRef<SWidget>;
    auto DoCreateLimitsControls() -> TSharedRef<SWidget>;
    auto DoCreateTraceSourceControls() -> TSharedRef<SWidget>;
    auto DoCreateAnalysisControls() -> TSharedRef<SWidget>;
    auto DoCreateStatus() -> TSharedRef<SWidget>;
    auto DoCreateSummaryStrip() -> TSharedRef<SWidget>;
    auto DoCreateResultsArea() -> TSharedRef<SWidget>;
    auto DoCreateHotPathPanel() -> TSharedRef<SWidget>;
    auto DoCreateSidePanels() -> TSharedRef<SWidget>;
    auto DoCreateScreenshotPanel() -> TSharedRef<SWidget>;
    auto DoCreateRawReportArea() -> TSharedRef<SWidget>;

    // ---- Hot-path tree ----

    auto DoGenerateHotPathRow(TSharedPtr<FCk_HotPathNode> InNode,
                              const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto DoExpandHotPathDefaults() -> void;

    // ---- Side panel row rebuilds ----

    auto DoRebuildCategoryRows() -> void;
    auto DoRebuildTopTimerRows() -> void;
    auto DoRebuildWorstFrameRows() -> void;
    auto DoRebuildCategoryAvgRows() -> void;
    auto DoRebuildWaitRows() -> void;
    auto DoRebuildAllSidePanelRows() -> void;

    // ---- Style live-apply ----

    // Mirrors SCkDebugger_WindowBase::Poll_StyleRevision — this tab is a plain SCompoundWidget, so it
    // cannot inherit the suite watcher and keeps its own transient revision baseline.
    auto Poll_StyleRevision() -> void;

    uint32 _LastSeenStyleRevision = 0;

    // ---- Button Handlers ----

    auto DoOnOpenTraceClicked() -> FReply;

    /** Build the "Recent" dropdown menu — newest-first .utrace files from Saved/Profiling and the local trace store. */
    auto DoMakeRecentTracesMenu() -> TSharedRef<SWidget>;

    /** Close the current session and start async-opening the given trace. Shared by the file dialog
     *  and the Recent menu. By-value: bound as a delegate payload, which decays reference types. */
    auto DoOpenTracePath(FString TracePath) -> void;
    auto DoOnCaptureClicked() -> FReply;
    auto DoOnCaptureUiTick(float DeltaTime) -> bool;
    auto DoCancelCaptureUiTick() -> void;
    auto DoQueueAutoOpenTrace(FString TracePath, FGuid TraceGuid) -> void;
    auto DoOnAutoOpenTraceTick(float DeltaTime) -> bool;
    auto DoCancelAutoOpenTrace() -> void;
    auto DoOnAnalyzeWorstClicked() -> FReply;
    auto DoOnCopyToClipboardClicked() -> FReply;
    auto DoOnExportJsonClicked() -> FReply;
    auto DoOnWorstFrameClicked(uint64 FrameIndex) -> FReply;
    auto DoOnScreenshotMarkerClicked(uint32 ScreenshotId, uint64 FrameIndex) -> void;

    // ---- Depth Selector ----

    auto DoOnDepthSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo) -> void;
    auto DoGet_ShowAllChildren() const -> bool;
    auto DoOnShowAllChildrenChanged(bool InShowAllChildren) -> void;

    // ---- Chart Delegate ----

    auto DoOnFrameSelectionChanged(uint64 StartFrame, uint64 EndFrame) -> void;

    // ---- Helpers ----

    auto DoSetStatus(const FString& Text, ECk_Tone InTone = ECk_Tone::Neutral) -> void;
    auto DoSetReport(const FString& ReportText) -> void;
    auto DoClearResults() -> void;
    auto DoAnalyzeSingleFrame(uint64 FrameIndex) -> void;
    auto DoAnalyzeFrameRange(uint64 StartFrame, uint64 EndFrame) -> void;
    auto DoRerunCurrentSelection() -> void;
    auto DoPopulateMultiFrame(const FCk_MultiFrameStats& Stats) -> void;
    auto DoGenerateAutomatedCaptureReport() -> bool;
    auto DoLoadScreenshots() -> void;
    auto DoClearScreenshots() -> void;
    auto DoClearScreenshotSelection() -> void;
    auto DoSelectScreenshot(uint32 ScreenshotId) -> bool;
    auto DoFindScreenshot(uint32 ScreenshotId) const -> const FCk_TraceScreenshot*;
    auto DoCreateScreenshotBrush(uint32 ScreenshotId,
                                 int32 MaxWidth,
                                 int32 MaxHeight,
                                 const TCHAR* ResourceSuffix) -> TSharedPtr<FSlateDynamicImageBrush>;

    // ---- Summary strip ----

    auto DoAddSummaryTile(const FString& InLabel, const FString& InValue, const FLinearColor& InValueColor) -> void;
    auto DoRebuildSummaryStrip_TraceInfo() -> void;
    auto DoRebuildSummaryStrip_SingleFrame(const FCk_FrameAnalysisResult& Result) -> void;
    auto DoRebuildSummaryStrip_MultiFrame(const FCk_MultiFrameStats& Stats) -> void;

    // ---- Async Loading ----

    enum class ELoadingState : uint8
    {
        Idle,
        Opening,
        ReadingFrames,
    };

    auto DoStartAsyncOpen(const FString& TracePath) -> void;
    auto DoOnLoadingTick(float DeltaTime) -> bool;
    auto DoLoadFrameChunk() -> bool;
    auto DoFinishLoading() -> void;
    auto DoCancelLoading() -> void;
    auto DoIsLoading() const -> bool { return _LoadingState != ELoadingState::Idle; }

private:
    FCk_TraceSession _Session;

    TSharedPtr<SCkFrameBarChart> _FrameBarChart;
    TSharedPtr<STextBlock> _StatusText;
    TSharedPtr<SMultiLineEditableText> _ReportText;

    ECk_Tone _StatusTone = ECk_Tone::Neutral;

    TArray<TSharedPtr<FString>> _DepthOptions;
    TSharedPtr<STextComboBox> _DepthCombo;
    ECkReportDepth _ReportDepth = ECkReportDepth::Standard;

    // "Show all" toggle — bypasses the hot-path tree's child threshold/cap (FCk_FrameReportConfig::ShowAllChildren)
    bool _ShowAllChildren = false;

    FString _CurrentReport;

    TSharedPtr<SHorizontalBox> _SummaryBox;

    EResultsMode _ResultsMode = EResultsMode::None;
    double _AnalyzedFrameMs = 0.0; // frame duration backing the hot-path %-of-frame column

    TArray<TSharedPtr<FCk_HotPathNode>> _HotPathRoots;
    TSharedPtr<STreeView<TSharedPtr<FCk_HotPathNode>>> _HotPathTree;

    TArray<FCk_CategorySummaryEntry> _Categories;
    TArray<FCk_TopTimerEntry> _TopTimers;
    TArray<FCk_FrameSummary> _WorstFrames;          // persists across single-frame drills
    TArray<FCk_MultiFrameStats::FCategoryStats> _CategoryAverages;
    TArray<FCk_WaitThreadSummary> _WaitRows;        // single-frame only

    TArray<FCk_TraceScreenshot> _TraceScreenshots;
    TMap<uint32, TSharedPtr<FSlateDynamicImageBrush>> _ScreenshotThumbnailBrushes;
    TSharedPtr<FSlateDynamicImageBrush> _SelectedScreenshotBrush;
    TOptional<uint32> _SelectedScreenshotId;
    FString _SelectedScreenshotError;
    uint64 _ScreenshotBrushGeneration = 0;

    // Last analysis, retained so Export JSON can regenerate without re-analyzing
    TOptional<FCk_FrameAnalysisResult> _LastSingleResult;
    TOptional<FCk_MultiFrameStats> _LastMultiStats;

    TSharedPtr<SVerticalBox> _CategoryRowsBox;
    TSharedPtr<SVerticalBox> _TopTimerRowsBox;
    TSharedPtr<SVerticalBox> _WorstFrameRowsBox;
    TSharedPtr<SVerticalBox> _CategoryAvgRowsBox;
    TSharedPtr<SVerticalBox> _WaitRowsBox;

    // Analysis starts on the game thread and completes on TraceServices' own thread — see
    // DoStartAsyncOpen; the ticker polls for completion.
    ELoadingState _LoadingState = ELoadingState::Idle;
    FTSTicker::FDelegateHandle _LoadingTickerHandle;
    FTSTicker::FDelegateHandle _CaptureUiTickerHandle;
    FTSTicker::FDelegateHandle _AutoOpenTickerHandle;
    FString _PendingTracePath;
    FString _PendingAutoOpenTracePath;
    FString _PendingAutoReportTracePath;
    FGuid _PendingAutoOpenTraceGuid;
    FGuid _SuppressedAutoOpenTraceGuid;
    bool _PendingAutoOpenWriterFinalized = false;
    bool _AutoOpenDelayWarningShown = false;
    bool _AutoOpenTraceOpeningStarted = false;
    bool _AutoOpenReportGenerated = false;
    double _AutoOpenDeadlineSeconds = 0.0;
    TSharedPtr<FCk_TraceSession> _PendingSession;
    uint64 _TotalFrameCount = 0;
    uint64 _LoadedFrameCount = 0;
    TArray<double> _PendingFrameDurations;
    int32 _CaptureDurationSeconds = 30;
    int32 _CaptureScreenshotCount = FCkInsightsCaptureController::DefaultTimedCaptureScreenshotCount;
};

// --------------------------------------------------------------------------------------------------------------------
