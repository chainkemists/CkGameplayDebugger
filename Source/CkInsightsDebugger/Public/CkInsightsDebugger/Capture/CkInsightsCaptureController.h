#pragma once

#include "CoreMinimal.h"

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

struct CKINSIGHTSDEBUGGER_API FCkInsightsCaptureSnapshot
{
    bool bIsTracing = false;
    bool bIsOwnedByTool = false;
    FString Destination;
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
#endif

    auto DoOnTraceStarted(FTraceAuxiliary::EConnectionType InType, const FString& InDestination) -> void;
    auto DoOnTraceConnection() -> void;
    auto DoOnTraceStopped(FTraceAuxiliary::EConnectionType InType, const FString& InDestination) -> void;

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
    FTraceAuxiliary::EConnectionType _ConnectionType = FTraceAuxiliary::EConnectionType::File;
    FString _ConnectionDestination;
    FTraceAuxiliary::EConnectionType _OwnedTraceType = FTraceAuxiliary::EConnectionType::None;
    FString _OwnedTraceDestination;
    FGuid _OwnedTraceGuid;
    FDelegateHandle _TraceStartedHandle;
    FDelegateHandle _TraceConnectionHandle;
    FDelegateHandle _TraceStoppedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
