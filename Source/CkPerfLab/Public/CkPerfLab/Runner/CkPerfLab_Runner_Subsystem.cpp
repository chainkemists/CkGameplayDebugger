#include "CkPerfLab_Runner_Subsystem.h"

#include "CkPerfLab_Log.h"

#include "CkPerfLab/Runner/CkPerfLab_WorldSurvey_Builder.h"
#include "CkPerfLab/Session/CkPerfLab_SessionCodec.h"
#include "CkPerfLab/Stats/CkPerfLab_SampleStats.h"

#include "CkProfile/Stats/CkStats_Utils.h"

#include <Engine/Engine.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>
#include <GameFramework/Pawn.h>
#include <GameFramework/PlayerController.h>
#include <HAL/FileManager.h>
#include <HAL/IConsoleManager.h>
#include <HAL/PlatformMisc.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/App.h>
#include <Misc/CommandLine.h>
#include <Misc/FileHelper.h>
#include <Misc/Parse.h>
#include <Misc/Paths.h>
#include <RHIGlobals.h>
#include <UObject/UObjectGlobals.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_runner
{
    auto
        Set_Cvar(
            const TCHAR* InName,
            const TCHAR* InValue)
        -> void
    {
        if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(InName))
        {
            Variable->Set(InValue, ECVF_SetByCode);
        }
    }

    auto
        Get_CvarString(
            const TCHAR* InName)
        -> FString
    {
        const auto* Variable = IConsoleManager::Get().FindConsoleVariable(InName);
        return ck::IsValid(Variable, ck::IsValid_Policy_NullptrOnly{}) ? Variable->GetString() : FString{TEXT("<absent>")};
    }

    // Streaming quiescence has no single signal, so this is the composite the Phase 0 addendum
    // settled on. It is deliberately conservative: anything in flight counts as noisy.
    auto
        Get_IsStreamingQuiet()
        -> bool
    {
        return GetNumAsyncPackages() == 0;
    }

    auto
        Get_NowUtc()
        -> FString
    {
        return FDateTime::UtcNow().ToIso8601();
    }
}

// --------------------------------------------------------------------------------------------------------------------

void UCk_PerfLab_Runner_Subsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
    Super::Initialize(InCollection);

    auto RequestPath = FString{};

    if (NOT FParse::Value(FCommandLine::Get(), TEXT("CkPerfLab-Request="), RequestPath))
    {
        return;
    }

    auto Json = FString{};

    if (NOT FFileHelper::LoadFileToString(Json, *RequestPath))
    {
        ck::perf_lab::Error(TEXT("PerfLab request file could not be read: [{}]"), RequestPath);
        FPlatformMisc::RequestExitWithStatus(false, 2);
        return;
    }

    if (NOT ck::perf_lab::TryRead_RequestFromJson(Json, _Request))
    {
        ck::perf_lab::Error(TEXT("PerfLab request file could not be decoded: [{}]"), RequestPath);
        FPlatformMisc::RequestExitWithStatus(false, 2);
        return;
    }

    _IsArmed = true;

    _Session.Set_SessionId(_Request.Get_SessionId())
            .Set_Request(_Request)
            .Set_State(ECk_PerfLab_RunState::Booting);

    DoWrite_Heartbeat(ECk_PerfLab_RunState::Booting, TEXT(""));

    _MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UCk_PerfLab_Runner_Subsystem::DoHandle_MapLoaded);

    ck::perf_lab::Log(TEXT("PerfLab armed for session [{}] on map [{}]"),
        _Request.Get_SessionId(), _Request.Get_MapPath());
}

void UCk_PerfLab_Runner_Subsystem::Deinitialize()
{
    if (_TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_TickHandle);
    }

    if (_MapLoadedHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(_MapLoadedHandle);
    }

    Super::Deinitialize();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_PerfLab_Runner_Subsystem::
    Get_SessionDir() const
    -> FString
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CkPerfLab"), TEXT("Sessions"), _Request.Get_SessionId());
}

auto
    UCk_PerfLab_Runner_Subsystem::
    DoWrite_Heartbeat(
        ECk_PerfLab_RunState InState,
        const FString& InMessage) const
    -> void
{
    const auto Heartbeat = FCk_PerfLab_Heartbeat{}
        .Set_SessionId(_Request.Get_SessionId())
        .Set_State(InState)
        .Set_PositionsTotal(_Plan.Get_Positions().Num())
        .Set_PositionsDone(_PositionIndex)
        .Set_CurrentPositionId(_Plan.Get_Positions().IsValidIndex(_PositionIndex)
            ? _Plan.Get_Positions()[_PositionIndex].Get_PositionId()
            : FString{})
        .Set_Message(InMessage)
        .Set_LastUpdateUtc(ck_perf_lab_runner::Get_NowUtc());

    FFileHelper::SaveStringToFile(
        ck::perf_lab::Write_HeartbeatToJson(Heartbeat),
        *FPaths::Combine(Get_SessionDir(), TEXT("heartbeat.json")));
}

auto
    UCk_PerfLab_Runner_Subsystem::
    DoWrite_Session() const
    -> void
{
    // Rewritten in full after every position rather than appended to: a child that is killed then
    // leaves a session that still decodes, with its state saying how far it got.
    FFileHelper::SaveStringToFile(
        ck::perf_lab::Write_SessionToJson(_Session),
        *FPaths::Combine(Get_SessionDir(), TEXT("session.json")));
}

auto
    UCk_PerfLab_Runner_Subsystem::
    DoFail(
        ECk_PerfLab_RunState InState,
        const FString& InMessage)
    -> void
{
    ck::perf_lab::Error(TEXT("PerfLab run failed: [{}]"), InMessage);

    _Stage = EStage::Finished;

    _Session.Set_State(InState);
    DoWrite_Session();
    DoWrite_Heartbeat(InState, InMessage);

    FPlatformMisc::RequestExitWithStatus(false, 3);
}

auto
    UCk_PerfLab_Runner_Subsystem::
    DoComplete()
    -> void
{
    _Stage = EStage::Finished;

    auto Environment = _Session.Get_Environment();
    Environment.Set_FinishedUtc(ck_perf_lab_runner::Get_NowUtc());
    _Session.Set_Environment(Environment).Set_State(ECk_PerfLab_RunState::Done);

    DoWrite_Session();
    DoWrite_Heartbeat(ECk_PerfLab_RunState::Done, TEXT(""));

    ck::perf_lab::Log(TEXT("PerfLab session [{}] complete: [{}] positions"),
        _Request.Get_SessionId(), _Session.Get_Positions().Num());

    FPlatformMisc::RequestExit(false);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_PerfLab_Runner_Subsystem::
    DoForce_MeasurementCvars()
    -> void
{
    using namespace ck_perf_lab_runner;

    Set_Cvar(TEXT("t.MaxFPS"), TEXT("0"));
    Set_Cvar(TEXT("r.VSync"), TEXT("0"));
    Set_Cvar(TEXT("t.IdleWhenNotForeground"), TEXT("0"));

    // The runtime lever that wins regardless of what any ini said: frame-rate smoothing clamps fast
    // frames into a configured range, which would quietly floor every measurement taken here.
    if (ck::IsValid(GEngine, ck::IsValid_Policy_NullptrOnly{}))
    {
        GEngine->bForceDisableFrameRateSmoothing = true;
    }

    auto Environment = _Session.Get_Environment();

    auto Cvars = TMap<FString, FString>{};
    Cvars.Add(TEXT("t.MaxFPS"),                 Get_CvarString(TEXT("t.MaxFPS")));
    Cvars.Add(TEXT("r.VSync"),                  Get_CvarString(TEXT("r.VSync")));
    Cvars.Add(TEXT("t.IdleWhenNotForeground"),  Get_CvarString(TEXT("t.IdleWhenNotForeground")));
    Cvars.Add(TEXT("bForceDisableFrameRateSmoothing"), TEXT("true"));

    Environment.Set_ForcedCvars(Cvars);
    _Session.Set_Environment(Environment);
}

auto
    UCk_PerfLab_Runner_Subsystem::
    DoAssert_Environment(
        UWorld* InWorld)
    -> bool
{
    // Refuse to measure rather than hope. Every condition below silently corrupts numbers while
    // reporting success, which is the worst failure mode this tool can have.
    if (GUsingNullRHI)
    {
        // Not fatal: the game-thread numbers are still real, and the GPU metric will carry its own
        // unavailability reason. Recorded so nobody reads the result as a full measurement.
        ck::perf_lab::Warning(TEXT("PerfLab is running without an RHI; GPU and render timings will be unavailable"));
    }

    if (FApp::UseFixedTimeStep())
    {
        DoFail(ECk_PerfLab_RunState::Failed_Internal,
            TEXT("fixed time step is enabled, which fabricates frame times"));
        return false;
    }

    if (ck::Is_NOT_Valid(UGameplayStatics::GetPlayerController(InWorld, 0), ck::IsValid_Policy_NullptrOnly{}))
    {
        DoFail(ECk_PerfLab_RunState::Failed_Internal,
            TEXT("no player controller to position the camera with"));
        return false;
    }

    return true;
}

auto
    UCk_PerfLab_Runner_Subsystem::
    DoApply_CameraFor(
        const FCk_PerfLab_PlannedPosition& InPosition,
        float InYaw)
    -> bool
{
    auto* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;

    if (ck::Is_NOT_Valid(World, ck::IsValid_Policy_NullptrOnly{}))
    {
        return false;
    }

    auto* Controller = UGameplayStatics::GetPlayerController(World, 0);

    if (ck::Is_NOT_Valid(Controller, ck::IsValid_Policy_NullptrOnly{}))
    {
        return false;
    }

    const auto Location = InPosition.Get_Location() + FVector{0.0, 0.0, static_cast<double>(InPosition.Get_EyeHeightCm())};
    const auto Rotation = FRotator{0.0, static_cast<double>(InYaw), 0.0};

    // Move whatever the player is actually looking through: the pawn when there is one, the
    // controller itself when there is not.
    if (APawn* Pawn = Controller->GetPawn(); ck::IsValid(Pawn, ck::IsValid_Policy_NullptrOnly{}))
    {
        Pawn->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    }
    else
    {
        // AController hides SetActorLocationAndRotation; this is the sanctioned way to place one.
        Controller->SetInitialLocationAndRotation(Location, Rotation);
    }

    Controller->SetControlRotation(Rotation);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_PerfLab_Runner_Subsystem::
    DoBegin_Dwell()
    -> void
{
    // One place to start a dwell, because every per-metric buffer has to be cleared in step with the
    // settle detector. A reset site that forgot one array would silently carry the previous
    // direction's frames into this one's statistics.
    _Settle = ck::perf_lab::FCk_SettleDetector{_Request.Get_Settle()};

    _GameThreadSamples.Reset();
    _RenderThreadSamples.Reset();
    _RhiThreadSamples.Reset();
    _GpuSamples.Reset();

    _GpuAvailability = ECk_Stats_MetricAvailability::Unavailable_NotYetSampled;

    _DwellElapsedSec = 0.0f;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_PerfLab_Runner_Subsystem::
    DoHandle_MapLoaded(
        UWorld* InWorld)
    -> void
{
    if (NOT _IsArmed || _Stage != EStage::AwaitingMap)
    {
        return;
    }

    DoBegin_Run(InWorld);
}

auto
    UCk_PerfLab_Runner_Subsystem::
    DoBegin_Run(
        UWorld* InWorld)
    -> void
{
    if (ck::Is_NOT_Valid(InWorld, ck::IsValid_Policy_NullptrOnly{}))
    {
        DoFail(ECk_PerfLab_RunState::Failed_MapMismatch, TEXT("no world after map load"));
        return;
    }

    DoForce_MeasurementCvars();

    auto Environment = _Session.Get_Environment();
    Environment.Set_BuildConfiguration(UCk_Utils_Stats_UE::Get_BuildConfig())
               .Set_InstanceMode(TEXT("game"))
               .Set_RhiName(GUsingNullRHI ? TEXT("NullRHI") : FString{GDynamicRHI ? GDynamicRHI->GetName() : TEXT("Unknown")})
               .Set_GpuBrand(UCk_Utils_Stats_UE::Get_GPUBrand())
               .Set_CpuBrand(UCk_Utils_Stats_UE::Get_CPUBrand())
               .Set_CpuCoreCount(UCk_Utils_Stats_UE::Get_CPUCoreCount())
               .Set_MachineName(FPlatformProcess::ComputerName())
               .Set_EngineVersion(FEngineVersion::Current().ToString())
               .Set_ChildBinary(FPaths::GetCleanFilename(FPlatformProcess::ExecutablePath()))
               .Set_SourceControlDisabled(FParse::Param(FCommandLine::Get(), TEXT("SCCProvider=None")))
               .Set_StartedUtc(ck_perf_lab_runner::Get_NowUtc());
    _Session.Set_Environment(Environment);

    if (NOT DoAssert_Environment(InWorld))
    {
        return;
    }

    _Stage = EStage::Surveying;
    DoWrite_Heartbeat(ECk_PerfLab_RunState::Planning, TEXT(""));

    _Survey = ck::perf_lab::Build_WorldSurvey(InWorld, _Request.Get_ModeParams());
    _Plan   = ck::perf_lab::Generate_Plan(_Survey, _Request);

    if (_Plan.Get_Positions().IsEmpty())
    {
        DoFail(ECk_PerfLab_RunState::Failed_Internal,
            ck::Format_UE(TEXT("nothing to measure: [{}]"), _Plan.Get_Outcome()));
        return;
    }

    ck::perf_lab::Log(TEXT("PerfLab planned [{}] positions ([{}])"),
        _Plan.Get_Positions().Num(), _Plan.Get_Outcome());

    _Stage = _Request.Get_ModeParams().Get_WarmSweep() ? EStage::WarmSweep : EStage::Measuring;

    DoBegin_Dwell();
    DoApply_CameraFor(_Plan.Get_Positions()[0], _Plan.Get_Positions()[0].Get_Yaws()[0]);

    _TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UCk_PerfLab_Runner_Subsystem::DoTick), 0.0f);
}

auto
    UCk_PerfLab_Runner_Subsystem::
    DoTick(
        float InDeltaSeconds)
    -> bool
{
    if (_Stage == EStage::Finished)
    {
        return false;
    }

    _RunElapsedSec += InDeltaSeconds;

    if (_RunElapsedSec > static_cast<float>(_Request.Get_ChildWallClockBudgetSec()))
    {
        // Flush what was measured before giving up: a partial session beats none, and the state
        // says why it is partial.
        _Session.Set_State(ECk_PerfLab_RunState::Failed_Timeout);
        DoWrite_Session();
        DoFail(ECk_PerfLab_RunState::Failed_Timeout, TEXT("child wall-clock budget exceeded"));
        return false;
    }

    auto* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;

    if (ck::Is_NOT_Valid(World, ck::IsValid_Policy_NullptrOnly{}))
    {
        return true;
    }

    // The warm sweep visits every position without recording anything, so the shader compiles, PSO
    // creation and streaming that a first visit triggers are paid before the numbers start counting.
    if (_Stage == EStage::WarmSweep)
    {
        if (_WarmSweepIndex >= _Plan.Get_Positions().Num())
        {
            _Stage = EStage::Measuring;
            DoBegin_Dwell();
            DoApply_CameraFor(_Plan.Get_Positions()[0], _Plan.Get_Positions()[0].Get_Yaws()[0]);
            DoWrite_Heartbeat(ECk_PerfLab_RunState::Measuring, TEXT(""));
            return true;
        }

        const auto& Position = _Plan.Get_Positions()[_WarmSweepIndex];
        DoApply_CameraFor(Position, Position.Get_Yaws()[0]);
        ++_WarmSweepIndex;

        DoWrite_Heartbeat(ECk_PerfLab_RunState::WarmSweep, TEXT(""));
        return true;
    }

    DoAdvance_Measurement(World, InDeltaSeconds);

    return _Stage != EStage::Finished;
}

auto
    UCk_PerfLab_Runner_Subsystem::
    DoAdvance_Measurement(
        UWorld* InWorld,
        float InDeltaSeconds)
    -> void
{
    const auto& Position = _Plan.Get_Positions()[_PositionIndex];

    _DwellElapsedSec += InDeltaSeconds;

    const auto Timings = UCk_Utils_Stats_UE::Get_ThreadTimings();

    const auto Verdict = _Settle.Observe(
        Timings.Get_FrameTimeMs(), ck_perf_lab_runner::Get_IsStreamingQuiet(), _DwellElapsedSec);

    if (Verdict == ck::perf_lab::ECk_SettleVerdict::Waiting)
    {
        return;
    }

    // Accepted by the settle gate: every metric from this frame joins the window, so all five end up
    // describing the same set of frames rather than the frame that happened to be last.
    _GameThreadSamples.Add(Timings.Get_GameThreadTimeMs());
    _RenderThreadSamples.Add(Timings.Get_RenderThreadTimeMs());
    _RhiThreadSamples.Add(Timings.Get_RhiThreadTimeMs());

    if (Timings.Get_GpuAvailability() == ECk_Stats_MetricAvailability::Available)
    {
        _GpuSamples.Add(Timings.Get_GpuTimeMs());

        if (_GpuAvailability == ECk_Stats_MetricAvailability::Unavailable_NotYetSampled)
        {
            _GpuAvailability = ECk_Stats_MetricAvailability::Available;
        }
    }
    else
    {
        // One bad frame poisons the window's GPU figure: a partial set of timestamps would produce a
        // confident-looking average over whichever frames happened to report.
        _GpuAvailability = Timings.Get_GpuAvailability();
    }

    if (Verdict != ck::perf_lab::ECk_SettleVerdict::Complete)
    {
        return;
    }

    const auto MadK = _Request.Get_OutlierMadK();

    auto Metrics = FCk_PerfLab_MetricSet{};
    Metrics.Set_Frame(ck::perf_lab::Reduce_Samples(_Settle.Get_Accepted(), MadK))
           .Set_GameThread(ck::perf_lab::Reduce_Samples(_GameThreadSamples, MadK))
           .Set_RenderThread(ck::perf_lab::Reduce_Samples(_RenderThreadSamples, MadK))
           .Set_RhiThread(ck::perf_lab::Reduce_Samples(_RhiThreadSamples, MadK))
           .Set_Gpu(_GpuAvailability == ECk_Stats_MetricAvailability::Available
               ? ck::perf_lab::Reduce_Samples(_GpuSamples, MadK)
               : ck::perf_lab::Make_UnavailableStats(_GpuAvailability,
                   TEXT("the RHI reported no GPU timestamp for at least one frame of this window")));

    _CompletedDirections.Add(FCk_PerfLab_DirectionResult{}
        .Set_YawDegrees(Position.Get_Yaws()[_DirectionIndex])
        .Set_Metrics(Metrics));

    ++_DirectionIndex;

    if (_DirectionIndex < Position.Get_Yaws().Num())
    {
        DoBegin_Dwell();
        DoApply_CameraFor(Position, Position.Get_Yaws()[_DirectionIndex]);
        return;
    }

    DoFinish_Position();
}

auto
    UCk_PerfLab_Runner_Subsystem::
    DoFinish_Position()
    -> void
{
    const auto& Planned = _Plan.Get_Positions()[_PositionIndex];

    auto Inputs = ck::perf_lab::FCk_ConfidenceInputs{};
    Inputs._WarmupConverged        = NOT _Settle.Get_SettleTimedOut();
    Inputs._StreamingQuiet         = NOT _Settle.Get_StreamingWasActive();
    Inputs._SampleCount            = _Settle.Get_AcceptedCount();
    Inputs._TargetSampleCount      = _Request.Get_Settle().Get_TargetSampleCount();
    Inputs._CoefficientOfVariation = _Settle.Get_CoefficientOfVariation();

    // The aggregate is the worst direction's frame metric: a position is as slow as the ugliest
    // thing you can turn and look at from it.
    auto Aggregate = FCk_PerfLab_MetricSet{};

    for (const auto& Direction : _CompletedDirections)
    {
        if (Direction.Get_Metrics().Get_Frame().Get_WorstMs() >= Aggregate.Get_Frame().Get_WorstMs())
        {
            Aggregate = Direction.Get_Metrics();
        }
    }

    auto Positions = _Session.Get_Positions();
    Positions.Add(FCk_PerfLab_PositionResult{}
        .Set_PositionId(Planned.Get_PositionId())
        .Set_Location(Planned.Get_Location())
        .Set_EyeHeightCm(Planned.Get_EyeHeightCm())
        .Set_Directions(_CompletedDirections)
        .Set_Aggregate(Aggregate)
        .Set_Confidence(ck::perf_lab::Compute_Confidence(Inputs))
        .Set_ActorCensus(ck::perf_lab::Build_CensusForPosition(
            _Survey, Planned.Get_Location(), _Request.Get_ModeParams().Get_CensusRadiusCm())));

    _Session.Set_Positions(Positions).Set_State(ECk_PerfLab_RunState::Measuring);

    _CompletedDirections.Reset();
    _DirectionIndex = 0;
    ++_PositionIndex;

    DoWrite_Session();
    DoWrite_Heartbeat(ECk_PerfLab_RunState::Measuring, TEXT(""));

    if (_PositionIndex >= _Plan.Get_Positions().Num())
    {
        DoComplete();
        return;
    }

    DoBegin_Dwell();

    const auto& Next = _Plan.Get_Positions()[_PositionIndex];
    DoApply_CameraFor(Next, Next.Get_Yaws()[0]);
}

// --------------------------------------------------------------------------------------------------------------------
