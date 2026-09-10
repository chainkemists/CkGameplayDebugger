#pragma once

#include "CoreMinimal.h"

#include "CkInsightsAnalyzer/Report/CkFrameReport.h"
#include "CkInsightsAnalyzer/Report/CkMultiFrameReport.h"

#include <Async/Future.h>
#include <HAL/CriticalSection.h>
#include <Templates/Atomic.h>

// --------------------------------------------------------------------------------------------------------------------

/** Outcome of the automatic post-capture report export performed by a frame-analysis worker. */
struct CKINSIGHTSDEBUGGER_API FCkInsightsAutomatedReportResult
{
    bool Succeeded = false;
    bool HasAnalyzableFrames = false;
    FString MarkdownPath;
    FString JsonPath;
    FString Error;
};

/** One exact timer occurrence, clipped to a selected frame and expressed relative to its start. */
struct CKINSIGHTSDEBUGGER_API FCkInsightsFrameTimingEvent
{
    uint32 TimerIndex = 0;
    double StartMs = 0.0;
    double EndMs = 0.0;
    uint32 Depth = 0;
};

/** Display strings copied out of TraceServices before the timing view reaches Slate. */
struct CKINSIGHTSDEBUGGER_API FCkInsightsFrameTimingTimer
{
    FString RawName;
    FString DisplayName;
};

/**
 * Immutable, UI-ready occurrence hierarchy for one real trace frame.
 *
 * Multi-frame analyses deliberately attach the first selected real frame rather than inventing
 * an averaged call topology which never occurred. Slate only reads these copied values.
 */
struct CKINSIGHTSDEBUGGER_API FCkInsightsFrameTimingView
{
    uint64 FrameIndex = 0;
    double FrameDurationMs = 0.0;
    uint64 SelectedFrameCount = 0;
    bool IsProvisional = false;
    TArray<FCkInsightsFrameTimingEvent> Events;
    TMap<uint32, FCkInsightsFrameTimingTimer> Timers;
    uint32 MaxDepth = 0;
    FString HeaderText;
    TArray<FString> TimeScaleLabels;

    auto IsValid() const -> bool;

    static auto Build(const FCk_FrameAnalysisResult& InResult,
                      const TMap<uint32, FString>& InTimerNames,
                      uint64 InSelectedFrameCount,
                      bool InIsProvisional)
        -> TSharedPtr<const FCkInsightsFrameTimingView, ESPMode::ThreadSafe>;
};

/** The complete, UI-ready result of one interactive frame analysis. */
struct CKINSIGHTSDEBUGGER_API FCkInsightsFrameDetails
{
    FCkInsightsFrameDetails() = default;
    FCkInsightsFrameDetails(FCkInsightsFrameDetails&&) = default;
    auto operator=(FCkInsightsFrameDetails&&) -> FCkInsightsFrameDetails& = default;

    FCkInsightsFrameDetails(const FCkInsightsFrameDetails&) = delete;
    auto operator=(const FCkInsightsFrameDetails&) -> FCkInsightsFrameDetails& = delete;

    FCk_FrameAnalysisResult Result;
    TOptional<FCk_MultiFrameStats> MultiStats;
    TArray<TSharedPtr<FCk_HotPathNode>> HotPaths;
    TArray<FCk_CategorySummaryEntry> Categories;
    TArray<FCk_TopTimerEntry> TopTimers;
    TArray<FCk_WaitThreadSummary> WaitRows;
    TSharedPtr<const FCkInsightsFrameTimingView, ESPMode::ThreadSafe> TimingView;
    TOptional<FCkInsightsAutomatedReportResult> AutomatedReport;
    FString Report;
    uint64 ProcessedFrames = 0;
    uint64 RequestedFrames = 0;
    bool IsIntermediate = false;
    bool IsProvisional = false;
    FString UnavailableReason;
};

/** Cooperative cancellation state shared by one worker and its requester. */
using FCancelToken = TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe>;

/**
 * Keeps interactive frame analysis bounded to one executing request and one newest pending request.
 * Call Request, then Poll from the owner thread; workers never call back into the owner or this queue.
 */
class CKINSIGHTSDEBUGGER_API FCkInsightsFrameRequestQueue
{
public:
    using FWork = TFunction<FCkInsightsFrameDetails(const FCancelToken&)>;
    using FPublish = TFunction<void(FCkInsightsFrameDetails)>;
    using FProgressWork = TFunction<FCkInsightsFrameDetails(const FCancelToken&, const FPublish&)>;

    FCkInsightsFrameRequestQueue() = default;
    ~FCkInsightsFrameRequestQueue();

    FCkInsightsFrameRequestQueue(const FCkInsightsFrameRequestQueue&) = delete;
    auto operator=(const FCkInsightsFrameRequestQueue&) -> FCkInsightsFrameRequestQueue& = delete;

    auto Request(uint64 InFrameIndex, FWork InWork) -> void;
    auto RequestProgressive(uint64 InFrameIndex, FProgressWork InWork) -> void;
    auto Poll() -> TOptional<FCkInsightsFrameDetails>;
    auto Reset() -> void;
    auto IsBusy() const -> bool;

private:
    struct FProgressMailbox
    {
        auto Publish(FCkInsightsFrameDetails InDetails) -> void;
        auto Consume() -> TOptional<FCkInsightsFrameDetails>;

        FCriticalSection _Mutex;
        TOptional<FCkInsightsFrameDetails> _Latest;
    };

    struct FPendingRequest
    {
        uint64 FrameIndex = 0;
        uint64 Revision = 0;
        FCancelToken CancelToken;
        FProgressWork Work;
    };

    struct FActiveRequest
    {
        uint64 FrameIndex = 0;
        uint64 Revision = 0;
        FCancelToken CancelToken;
        TSharedRef<FProgressMailbox, ESPMode::ThreadSafe> Mailbox;
        TFuture<FCkInsightsFrameDetails> Future;
    };

    auto DoStartPending() -> void;

    uint64 _Revision = 0;
    TOptional<FPendingRequest> _Pending;
    TOptional<FActiveRequest> _Active;
};

// --------------------------------------------------------------------------------------------------------------------
