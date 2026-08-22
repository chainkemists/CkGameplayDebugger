#pragma once

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkProfile/Stats/CkStats_Utils.h"

#include "CkPerfLab_Session.generated.h"

// --------------------------------------------------------------------------------------------------------------------
// The on-disk contract for a measurement session. This header IS the contract; the codec is the only
// thing allowed to translate between it and JSON, so a field's meaning lives here and nowhere else.
// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_PerfLab_Mode : uint8
{
    Quick,
    Standard,
    Deep,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_Mode);

// --------------------------------------------------------------------------------------------------------------------

/**
 * Where a run got to. The child writes every state up to Done or a Failed_*; the host writes the
 * Aborted_* states when it is the one that stopped the child. Keeping the two families distinct is
 * what makes "the child gave up" diagnosable separately from "we killed it".
 */
UENUM(BlueprintType)
enum class ECk_PerfLab_RunState : uint8
{
    Booting,
    Planning,
    WarmSweep,
    Measuring,
    Writing,
    Done,

    Failed_BadRequest,
    Failed_MapMismatch,
    Failed_Timeout,
    Failed_Internal,

    Aborted_HostTimeout,
    Aborted_UserCancel,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_RunState);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_PerfLab_Confidence : uint8
{
    High,
    Medium,
    Low,
    Unusable,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_Confidence);

// --------------------------------------------------------------------------------------------------------------------

/** Why a position's confidence was downgraded. A High-confidence position carries none of these. */
UENUM(BlueprintType)
enum class ECk_PerfLab_ConfidenceReason : uint8
{
    SettleTimedOut,
    StreamingActive,
    LowSampleCount,
    HighVariance,
    OutlierHeavy,
    ShaderCompileActivity,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_ConfidenceReason);

// --------------------------------------------------------------------------------------------------------------------

/** Which timing a metric block describes. */
UENUM(BlueprintType)
enum class ECk_PerfLab_Metric : uint8
{
    Frame,
    GameThread,
    RenderThread,
    RhiThread,
    Gpu,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_Metric);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_SettleParams
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_SettleParams);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _WarmupFrames = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    float _StabilityCv = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _StabilityFrames = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _StreamingQuietFrames = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    float _SettleTimeoutSec = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _TargetSampleCount = 120;

public:
    CK_PROPERTY(_WarmupFrames);
    CK_PROPERTY(_StabilityCv);
    CK_PROPERTY(_StabilityFrames);
    CK_PROPERTY(_StreamingQuietFrames);
    CK_PROPERTY(_SettleTimeoutSec);
    CK_PROPERTY(_TargetSampleCount);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_ModeParams
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_ModeParams);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _PositionBudget = 25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _DirectionsPerPosition = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _Repeats = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    float _MinPositionSpacingCm = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    float _EyeHeightCm = 160.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    float _CensusRadiusCm = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    bool _WarmSweep = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    bool _AdaptiveRevisit = true;

    // Off by default: 25 positions x 4 directions x 120 samples is 12,000 floats per metric, and the
    // summary statistics are what every consumer actually reads.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    bool _RetainRawSamples = false;

public:
    CK_PROPERTY(_PositionBudget);
    CK_PROPERTY(_DirectionsPerPosition);
    CK_PROPERTY(_Repeats);
    CK_PROPERTY(_MinPositionSpacingCm);
    CK_PROPERTY(_EyeHeightCm);
    CK_PROPERTY(_CensusRadiusCm);
    CK_PROPERTY(_WarmSweep);
    CK_PROPERTY(_AdaptiveRevisit);
    CK_PROPERTY(_RetainRawSamples);

public:
    /** The preset behind each mode. Modes are parameter sets, not separate code paths. */
    static auto
    Get_ForMode(
        ECk_PerfLab_Mode InMode) -> FCk_PerfLab_ModeParams;
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Request
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Request);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FString _SessionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FString _MapPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    float _BudgetMs = 16.67f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _Seed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _RequestingHostPid = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FString _CreatedUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    ECk_PerfLab_Mode _Mode = ECk_PerfLab_Mode::Standard;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_ModeParams _ModeParams;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_SettleParams _Settle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    float _OutlierMadK = 3.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _ChildWallClockBudgetSec = 1800;

public:
    CK_PROPERTY(_SessionId);
    CK_PROPERTY(_MapPath);
    CK_PROPERTY(_BudgetMs);
    CK_PROPERTY(_Seed);
    CK_PROPERTY(_RequestingHostPid);
    CK_PROPERTY(_CreatedUtc);
    CK_PROPERTY(_Mode);
    CK_PROPERTY(_ModeParams);
    CK_PROPERTY(_Settle);
    CK_PROPERTY(_OutlierMadK);
    CK_PROPERTY(_ChildWallClockBudgetSec);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Environment
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Environment);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _BuildConfiguration;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _InstanceMode;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _RhiName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _GpuBrand;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _CpuBrand;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int32 _CpuCoreCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _MachineName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _EngineVersion;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _ChildBinary;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FIntPoint _ViewportSizeActual = FIntPoint::ZeroValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    bool _SourceControlDisabled = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _StartedUtc;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _FinishedUtc;

    // The run states its own conditions: which cvars it forced, and to what. A reader auditing a
    // suspicious number should not have to guess whether vsync was on.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TMap<FString, FString> _ForcedCvars;

public:
    CK_PROPERTY(_BuildConfiguration);
    CK_PROPERTY(_InstanceMode);
    CK_PROPERTY(_RhiName);
    CK_PROPERTY(_GpuBrand);
    CK_PROPERTY(_CpuBrand);
    CK_PROPERTY(_CpuCoreCount);
    CK_PROPERTY(_MachineName);
    CK_PROPERTY(_EngineVersion);
    CK_PROPERTY(_ChildBinary);
    CK_PROPERTY(_ViewportSizeActual);
    CK_PROPERTY(_SourceControlDisabled);
    CK_PROPERTY(_StartedUtc);
    CK_PROPERTY(_FinishedUtc);
    CK_PROPERTY(_ForcedCvars);
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * One metric's summary over a sample window. AvgMs EXCLUDES classified outliers; WorstMs and
 * OnePercentLowMs deliberately RETAIN them, so a reader sees both typical and bad-frame behaviour.
 * Every field is meaningless unless Availability is Available — check it before reading a value.
 */
USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_MetricStats
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_MetricStats);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_Stats_MetricAvailability _Availability = ECk_Stats_MetricAvailability::Unavailable_NotYetSampled;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _UnavailableReason;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _AvgMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _WorstMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _P99Ms = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _OnePercentLowMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int32 _SampleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int32 _OutlierCount = 0;

public:
    CK_PROPERTY(_Availability);
    CK_PROPERTY(_UnavailableReason);
    CK_PROPERTY(_AvgMs);
    CK_PROPERTY(_WorstMs);
    CK_PROPERTY(_P99Ms);
    CK_PROPERTY(_OnePercentLowMs);
    CK_PROPERTY(_SampleCount);
    CK_PROPERTY(_OutlierCount);

public:
    auto Get_IsAvailable() const -> bool
    { return _Availability == ECk_Stats_MetricAvailability::Available; }
};

// --------------------------------------------------------------------------------------------------------------------

/** The five metrics measured at one place, kept together so a reader never mixes windows. */
USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_MetricSet
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_MetricSet);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_MetricStats _Frame;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_MetricStats _GameThread;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_MetricStats _RenderThread;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_MetricStats _RhiThread;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_MetricStats _Gpu;

public:
    CK_PROPERTY(_Frame);
    CK_PROPERTY(_GameThread);
    CK_PROPERTY(_RenderThread);
    CK_PROPERTY(_RhiThread);
    CK_PROPERTY(_Gpu);

public:
    auto
    Get_ByMetric(
        ECk_PerfLab_Metric InMetric) const -> const FCk_PerfLab_MetricStats&;
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_ConfidenceResult
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_ConfidenceResult);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_PerfLab_Confidence _Rating = ECk_PerfLab_Confidence::High;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<ECk_PerfLab_ConfidenceReason> _Reasons;

public:
    CK_PROPERTY(_Rating);
    CK_PROPERTY(_Reasons);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_DirectionResult
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_DirectionResult);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _YawDegrees = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _PitchDegrees = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_MetricSet _Metrics;

public:
    CK_PROPERTY(_YawDegrees);
    CK_PROPERTY(_PitchDegrees);
    CK_PROPERTY(_Metrics);
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * An actor near a measured position. Object PATHS only — no handles and no pointers cross the
 * process boundary, which is what keeps the debugger's no-live-handle invariant intact when the
 * host reads a session the child wrote.
 */
USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_ActorCensusRow
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_ActorCensusRow);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _ObjectPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _ClassName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _DistanceCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<float> _InViewYaws;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int64 _TriangleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int32 _MaterialSlotCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int32 _LightCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int32 _NiagaraComponentCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    bool _TickEnabled = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    bool _CastsDynamicShadow = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    bool _HasCollision = false;

public:
    CK_PROPERTY(_ObjectPath);
    CK_PROPERTY(_ClassName);
    CK_PROPERTY(_DistanceCm);
    CK_PROPERTY(_InViewYaws);
    CK_PROPERTY(_TriangleCount);
    CK_PROPERTY(_MaterialSlotCount);
    CK_PROPERTY(_LightCount);
    CK_PROPERTY(_NiagaraComponentCount);
    CK_PROPERTY(_TickEnabled);
    CK_PROPERTY(_CastsDynamicShadow);
    CK_PROPERTY(_HasCollision);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_PositionResult
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_PositionResult);

private:
    // Stable across runs of the same map: derived from the quantised location, which is what lets
    // two sessions be compared position by position.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _PositionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FVector _Location = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _EyeHeightCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    bool _Revisited = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_DirectionResult> _Directions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_MetricSet _Aggregate;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_ConfidenceResult _Confidence;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_ActorCensusRow> _ActorCensus;

public:
    CK_PROPERTY(_PositionId);
    CK_PROPERTY(_Location);
    CK_PROPERTY(_EyeHeightCm);
    CK_PROPERTY(_Revisited);
    CK_PROPERTY(_Directions);
    CK_PROPERTY(_Aggregate);
    CK_PROPERTY(_Confidence);
    CK_PROPERTY(_ActorCensus);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Heartbeat
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Heartbeat);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _SessionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_PerfLab_RunState _State = ECk_PerfLab_RunState::Booting;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int32 _PositionsTotal = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int32 _PositionsDone = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _CurrentPositionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _Message;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _LastUpdateUtc;

public:
    CK_PROPERTY(_SessionId);
    CK_PROPERTY(_State);
    CK_PROPERTY(_PositionsTotal);
    CK_PROPERTY(_PositionsDone);
    CK_PROPERTY(_CurrentPositionId);
    CK_PROPERTY(_Message);
    CK_PROPERTY(_LastUpdateUtc);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Session
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Session);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _SessionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_PerfLab_RunState _State = ECk_PerfLab_RunState::Booting;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_Request _Request;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_Environment _Environment;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_PositionResult> _Positions;

public:
    CK_PROPERTY(_SessionId);
    CK_PROPERTY(_State);
    CK_PROPERTY(_Request);
    CK_PROPERTY(_Environment);
    CK_PROPERTY(_Positions);
};

// --------------------------------------------------------------------------------------------------------------------
