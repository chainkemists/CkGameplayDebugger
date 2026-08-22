#pragma once

#include "CkPerfLab/Planner/CkPerfLab_Planner.h"
#include "CkPerfLab/Runner/CkPerfLab_SettleDetector.h"
#include "CkPerfLab/Session/CkPerfLab_Session.h"

#include <Containers/Ticker.h>
#include <Subsystems/GameInstanceSubsystem.h>

#include "CkPerfLab_Runner_Subsystem.generated.h"

// --------------------------------------------------------------------------------------------------------------------

/**
 * Runs a measurement session inside a child `-game` process.
 *
 * Armed only by `-CkPerfLab-Request=<file>` on the command line; without it the subsystem is inert
 * and costs a command-line parse. That matters because this module ships in packaged Development
 * builds, where nobody asked for a profiler to start ticking.
 *
 * The whole run is driven from a frame ticker rather than a thread: measurements are per-frame
 * quantities, so being on the game thread once per frame is not a limitation, it is the requirement.
 */
UCLASS()
class CKPERFLAB_API UCk_PerfLab_Runner_Subsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& InCollection) override;
    virtual void Deinitialize() override;

public:
    /** True when a request was supplied and parsed. Lets a spec assert the inert path. */
    auto Get_IsArmed() const -> bool { return _IsArmed; }

private:
    enum class EStage : uint8
    {
        AwaitingMap,
        Surveying,
        WarmSweep,
        Measuring,
        Finished,
    };

private:
    auto DoHandle_MapLoaded(UWorld* InWorld) -> void;
    auto DoTick(float InDeltaSeconds) -> bool;

    auto DoBegin_Run(UWorld* InWorld) -> void;
    auto DoAssert_Environment(UWorld* InWorld) -> bool;
    auto DoForce_MeasurementCvars() -> void;
    auto DoApply_CameraFor(const FCk_PerfLab_PlannedPosition& InPosition, float InYaw) -> bool;
    auto DoAdvance_Measurement(UWorld* InWorld, float InDeltaSeconds) -> void;
    auto DoFinish_Position() -> void;
    auto DoBegin_Dwell() -> void;

    auto DoWrite_Heartbeat(ECk_PerfLab_RunState InState, const FString& InMessage) const -> void;
    auto DoWrite_Session() const -> void;
    auto DoFail(ECk_PerfLab_RunState InState, const FString& InMessage) -> void;
    auto DoComplete() -> void;

    auto Get_SessionDir() const -> FString;

private:
    FCk_PerfLab_Request _Request;
    FCk_PerfLab_Session _Session;
    FCk_PerfLab_Plan    _Plan;

    FCk_PerfLab_WorldSurvey _Survey;

    ck::perf_lab::FCk_SettleDetector _Settle;

    // The settle detector owns the frame-time window because it gates on it. The other four metrics
    // have to be accumulated in step with it: reducing them from a single snapshot would leave their
    // worst case, p99 and 1% low describing one arbitrary frame while claiming to describe a window.
    TArray<float> _GameThreadSamples;
    TArray<float> _RenderThreadSamples;
    TArray<float> _RhiThreadSamples;
    TArray<float> _GpuSamples;

    // Latched across the dwell: one frame without a GPU timestamp makes the whole window's GPU
    // figure untrustworthy, so the reason is kept rather than the last frame's luck.
    ECk_Stats_MetricAvailability _GpuAvailability = ECk_Stats_MetricAvailability::Unavailable_NotYetSampled;

    TArray<FCk_PerfLab_DirectionResult> _CompletedDirections;

    FTSTicker::FDelegateHandle _TickHandle;
    FDelegateHandle            _MapLoadedHandle;

    EStage _Stage = EStage::AwaitingMap;

    int32 _PositionIndex  = 0;
    int32 _DirectionIndex = 0;
    int32 _WarmSweepIndex = 0;

    float _DwellElapsedSec = 0.0f;
    float _RunElapsedSec   = 0.0f;

    bool _IsArmed = false;
};

// --------------------------------------------------------------------------------------------------------------------
