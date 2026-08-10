#pragma once

#include "CoreMinimal.h"

#include <Containers/Ticker.h>
#include <ProfilingDebugging/TraceAuxiliary.h>

// --------------------------------------------------------------------------------------------------------------------

enum class ECkInsightsStatProfile : uint8
{
    CkProcessors,
    CkScheduler,
    Script,
    UObjects,
    Rhi,
    Rendering,
};

enum class ECkInsightsCaptureState : uint8
{
    Idle,
    Starting,
    Recording,
    Stopping,
};

struct CKINSIGHTSDEBUGGER_API FCkInsightsCaptureSnapshot
{
    bool bIsTracing = false;
    bool bIsOwnedByTool = false;
    FString Destination;
    ECkInsightsCaptureState State = ECkInsightsCaptureState::Idle;
    double TargetDurationSeconds = 0.0;
    double ElapsedSeconds = 0.0;
    double RemainingSeconds = 0.0;
    double Progress = 0.0;
};

/**
 * Process-wide trace capture state for the Insights Analyzer.
 *
 * The module owns this object so closing the analyzer tab does not stop a capture.
 * It will only stop a trace that was started through this controller.
 */
class CKINSIGHTSDEBUGGER_API FCkInsightsCaptureController
{
public:
    static constexpr int32 MinTimedCaptureScreenshotCount = 0;
    static constexpr int32 DefaultTimedCaptureScreenshotCount = 3;
    static constexpr int32 MaxTimedCaptureScreenshotCount = 12;

    FCkInsightsCaptureController();
    FCkInsightsCaptureController(
        FTraceAuxiliary::EConnectionType InConnectionType,
        FString InConnectionDestination);
    ~FCkInsightsCaptureController();

    auto Get_Snapshot() const -> FCkInsightsCaptureSnapshot;
    auto Can_ToggleTracing() const -> bool;
    auto TrySet_Tracing(
        bool InShouldTrace,
        FString& OutError,
        FString* OutStoppedTracePath = nullptr,
        FGuid* OutStoppedTraceGuid = nullptr)
        -> bool;
    auto TryStart_TimedCapture(double InDurationSeconds, FString& OutError) -> bool;
    auto TryStart_TimedCapture(double InDurationSeconds, int32 InScreenshotCount, FString& OutError) -> bool;
    auto TryStop_TimedCapture(FString& OutError) -> bool;
    auto Get_CompletedCapture(FString& OutTracePath, FGuid& OutTraceGuid) const -> bool;
    auto Acknowledge_CompletedCapture(FGuid InTraceGuid) -> bool;
    auto IsTraceWriterFinalized(FGuid InStoppedTraceGuid) const -> bool;

    auto Get_NamedEventsEnabled() const -> bool;
    auto TrySet_NamedEventsEnabled(bool InEnabled, FString& OutError) -> bool;

    auto Get_StatMaxPerGroup() const -> TOptional<int32>;
    auto TrySet_StatMaxPerGroup(int32 InValue, FString& OutError) -> bool;

    auto Get_StatProfileEnabled(ECkInsightsStatProfile InProfile) const -> bool;
    auto TrySet_StatProfileEnabled(
        ECkInsightsStatProfile InProfile,
        bool InEnabled,
        FString& OutError)
        -> bool;

    static auto Get_TraceChannels() -> FString;
    static auto Get_StatGroups(ECkInsightsStatProfile InProfile) -> TArray<FName>;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCkInsightsCaptureController_TogglesCkStatProfiles;
    friend class FCkInsightsCaptureController_MapsTimedCaptureProgress;
    friend class FCkInsightsCaptureController_MapsTimedCaptureScreenshotThresholds;
    friend class FCkInsightsCaptureController_RejectsTimedCaptureScreenshotCount;
#endif

    auto DoOnTraceStarted(FTraceAuxiliary::EConnectionType InType, const FString& InDestination) -> void;
    auto DoOnTraceConnection() -> void;
    auto DoOnTraceStopped(FTraceAuxiliary::EConnectionType InType, const FString& InDestination) -> void;
    auto DoTick_TimedCapture(float InDeltaSeconds) -> bool;
    auto DoAdvance_TimedCapture(double InNowSeconds) -> void;
    auto DoTryStop_TimedCapture(bool InAllowScreenshotDrain, FString& OutError) -> bool;
    auto DoQueue_CompletedCapture(FString InTracePath, FGuid InTraceGuid) -> void;
    static auto DoGet_TimedCaptureProgress(double InElapsedSeconds, double InDurationSeconds) -> double;
    static auto DoGet_ScreenshotMilestones(
        double InElapsedSeconds,
        double InDurationSeconds,
        int32 InScreenshotCount)
        -> TArray<int32>;

    static auto DoGet_StatCommandName(FName InGroup) -> FString;
    static auto DoIs_StatGroupEnabled(FName InGroup) -> bool;
    static auto DoTrySet_StatGroupDisplayEnabled(FName InGroup, bool InEnabled) -> bool;
    static auto DoSet_StatGroupCollectionEnabled(FName InGroup, bool InEnabled) -> void;

    mutable FCriticalSection _OwnershipMutex;
    bool _OwnsActiveTrace = false;
    bool _CanStopOwnedTrace = false;
    bool _StartInProgress = false;
    bool _ConnectionReadyDuringStart = false;
    bool _TraceStartedDuringStart = false;
    bool _TraceStoppedDuringStart = false;
    bool _TimedCaptureRequested = false;
    bool _TimedCaptureStopping = false;
    double _TimedCaptureDurationSeconds = 0.0;
    double _TimedCaptureStartedSeconds = 0.0;
    double _TimedCaptureScreenshotFlushDeadlineSeconds = 0.0;
    int32 _TimedCaptureScreenshotCount = DefaultTimedCaptureScreenshotCount;
    uint16 _TimedCaptureScreenshotMask = 0;
    FString _CompletedTracePath;
    FGuid _CompletedTraceGuid;
    FTraceAuxiliary::EConnectionType _ConnectionType = FTraceAuxiliary::EConnectionType::File;
    FString _ConnectionDestination;
    FTraceAuxiliary::EConnectionType _OwnedTraceType = FTraceAuxiliary::EConnectionType::None;
    FString _OwnedTraceDestination;
    FGuid _OwnedTraceGuid;
    FDelegateHandle _TraceStartedHandle;
    FDelegateHandle _TraceConnectionHandle;
    FDelegateHandle _TraceStoppedHandle;
    FTSTicker::FDelegateHandle _TimedCaptureTickerHandle;
};

// --------------------------------------------------------------------------------------------------------------------
