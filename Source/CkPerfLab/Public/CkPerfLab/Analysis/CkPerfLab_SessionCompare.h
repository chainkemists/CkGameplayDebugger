#pragma once

#include "CkPerfLab/Analysis/CkPerfLab_Analysis.h"
#include "CkPerfLab/Session/CkPerfLab_Session.h"

#include "CkPerfLab_SessionCompare.generated.h"

// --------------------------------------------------------------------------------------------------------------------
// Comparing two sessions of the same map.
//
// Positions are matched BY POSITION ID, never by index or by proximity. An id is a hash of the
// location quantised to 50 cm, so an unchanged map plans the same ids every run and a matched pair
// genuinely describes the same spot. An index match would silently pair unrelated positions the
// moment the plan gained or lost one, and report the difference between two places as a regression
// in time.
//
// Pure, like the analysis: a function of two sessions and nothing else.
// --------------------------------------------------------------------------------------------------------------------

/** What a single measured difference means once the noise band has been subtracted from it. */
UENUM(BlueprintType)
enum class ECk_PerfLab_CompareVerdict : uint8
{
    // Declared worst-first so sorting ASCENDING by value puts regressions at the top.
    Regressed,
    Improved,
    Unchanged,

    // One side or the other was never measured, so there is no difference to report. Distinct from
    // Unchanged on purpose: "we looked and nothing moved" and "we cannot tell" are different answers
    // and collapsing them would let an unmeasured metric read as a clean result.
    Incomparable,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_CompareVerdict);

// --------------------------------------------------------------------------------------------------------------------

/**
 * Why a comparison is being shown with a banner over it.
 *
 * These never suppress the compare — they annotate it. Suppressing would hide the very fact the
 * reader needs; a timing difference across two different GPUs is real data about two machines, and
 * only becomes a lie when presented as a change in the level.
 */
UENUM(BlueprintType)
enum class ECk_PerfLab_CompareWarning : uint8
{
    DifferentMachine,
    DifferentGpu,
    DifferentCpu,
    DifferentRhi,
    DifferentBuildConfiguration,
    DifferentInstanceMode,
    DifferentViewportSize,
    DifferentEngineVersion,
    DifferentBudget,
    DifferentMode,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_CompareWarning);

// --------------------------------------------------------------------------------------------------------------------

/** Why two sessions could not be compared at all. */
UENUM(BlueprintType)
enum class ECk_PerfLab_CompareRefusal : uint8
{
    None,
    DifferentMap,
    BaselineHasNoPositions,
    CurrentHasNoPositions,
    NoPositionsInCommon,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_CompareRefusal);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_MetricDelta
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_MetricDelta);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_PerfLab_Metric _Metric = ECk_PerfLab_Metric::Frame;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _BaselineMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _CurrentMs = 0.0f;

    // Positive means slower than the baseline, in the same direction as the metric itself.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _DeltaMs = 0.0f;

    // How far the two runs would have to differ before the difference is worth reporting, derived
    // from the spread each session measured rather than from a fixed threshold. A jittery scene
    // earns a wide band and a stable one a narrow band, which is the only way a single constant can
    // avoid being both too twitchy on one map and too deaf on another.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _NoiseBandMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_PerfLab_CompareVerdict _Verdict = ECk_PerfLab_CompareVerdict::Incomparable;

public:
    CK_PROPERTY(_Metric);
    CK_PROPERTY(_BaselineMs);
    CK_PROPERTY(_CurrentMs);
    CK_PROPERTY(_DeltaMs);
    CK_PROPERTY(_NoiseBandMs);
    CK_PROPERTY(_Verdict);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_PositionDelta
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_PositionDelta);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _PositionId;

    // Taken from the CURRENT session: it is the one the reader can still fly to.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FVector _Location = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_MetricDelta> _Metrics;

    // The worst verdict across the metrics — a position that regressed anywhere has regressed.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_PerfLab_CompareVerdict _Verdict = ECk_PerfLab_CompareVerdict::Incomparable;

public:
    CK_PROPERTY(_PositionId);
    CK_PROPERTY(_Location);
    CK_PROPERTY(_Metrics);
    CK_PROPERTY(_Verdict);

    /** The frame-time delta, which is the number the summary row shows. Unset when frame time was not comparable. */
    auto Get_FrameDelta() const -> TOptional<FCk_PerfLab_MetricDelta>;
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Compare
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Compare);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FString _MapPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_PerfLab_CompareRefusal _Refusal = ECk_PerfLab_CompareRefusal::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<ECk_PerfLab_CompareWarning> _Warnings;

    // Sorted worst-first, then by position id, so the same pair of sessions always produces the same
    // table and a reader's eye lands on the regressions.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_PositionDelta> _Positions;

    // Position ids present on one side only. Listed rather than dropped: a position that vanished
    // between two runs of the "same" map is itself the finding, and quietly comparing what remains
    // would hide a map that changed shape.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FString> _OnlyInBaseline;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FString> _OnlyInCurrent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _BaselineScore = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _CurrentScore = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_PerfLab_CompareVerdict _Verdict = ECk_PerfLab_CompareVerdict::Incomparable;

public:
    CK_PROPERTY(_MapPath);
    CK_PROPERTY(_Refusal);
    CK_PROPERTY(_Warnings);
    CK_PROPERTY(_Positions);
    CK_PROPERTY(_OnlyInBaseline);
    CK_PROPERTY(_OnlyInCurrent);
    CK_PROPERTY(_BaselineScore);
    CK_PROPERTY(_CurrentScore);
    CK_PROPERTY(_Verdict);

    auto Get_IsComparable() const -> bool;

    /**
     * Score delta, positive meaning better. Unset only when the comparison itself was refused.
     *
     * Both scores are computed against the SINGLE budget handed to `Compare_Sessions`, never against
     * the budget each session happened to request, so they are on one scale by construction — even
     * when the two runs targeted different budgets. A differing requested budget is therefore worth
     * a banner but never a withheld delta: suppressing it would hide the number a CI job exists to
     * produce, on a condition that does not affect comparability.
     */
    auto Get_ScoreDelta() const -> TOptional<float>;

    auto Get_RegressedCount() const -> int32;
    auto Get_ImprovedCount() const -> int32;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    /** The banner text for a warning. Pinned by spec, because the wording is the whole mitigation. */
    CKPERFLAB_API auto
    Get_WarningText(
        ECk_PerfLab_CompareWarning InWarning) -> FString;

    CKPERFLAB_API auto
    Get_RefusalText(
        ECk_PerfLab_CompareRefusal InRefusal) -> FString;

    /**
     * Compare two sessions of the same map.
     *
     * The budget is a parameter for the same reason it is in `Analyse_Session`: the two sessions may
     * have been requested at different budgets, and the caller decides which one the comparison is
     * read against. BOTH scores use it, so they are always on one scale.
     */
    CKPERFLAB_API auto
    Compare_Sessions(
        const FCk_PerfLab_Session& InBaseline,
        const FCk_PerfLab_Session& InCurrent,
        float InBudgetMs) -> FCk_PerfLab_Compare;
}

// --------------------------------------------------------------------------------------------------------------------
