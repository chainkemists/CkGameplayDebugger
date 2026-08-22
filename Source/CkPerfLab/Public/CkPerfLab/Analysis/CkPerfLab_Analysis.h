#pragma once

#include "CkPerfLab/Session/CkPerfLab_Session.h"

#include "CkPerfLab_Analysis.generated.h"

// --------------------------------------------------------------------------------------------------------------------
// Turning measurements into a score and into findings.
//
// Everything here is a pure function of a session plus a budget. It never reads the world — the
// session was produced by a different process against a world this one may not even have open, and
// re-deriving anything from the live level would make two runs of the same analysis disagree.
// --------------------------------------------------------------------------------------------------------------------

/** The eight things the score is made of. Published, because a score nobody can decompose is a rating to be trusted rather than a measurement to be read. */
UENUM(BlueprintType)
enum class ECk_PerfLab_ScoreComponent : uint8
{
    AverageFrameAttainment,
    WorstCaseAttainment,
    OnePercentLowAttainment,
    GameThreadHeadroom,
    RenderThreadHeadroom,
    GpuHeadroom,
    SpatialConsistency,
    MeasurementConfidence,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_ScoreComponent);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_PerfLab_Severity : uint8
{
    // Declared worst-first so sorting ASCENDING by value puts the most severe first.
    Critical,
    Major,
    Minor,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_Severity);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_PerfLab_Band : uint8
{
    High,
    Medium,
    Low,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_Band);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_ScoreComponentResult
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_ScoreComponentResult);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_PerfLab_ScoreComponent _Component = ECk_PerfLab_ScoreComponent::AverageFrameAttainment;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    float _Weight = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Value = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    bool _Included = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _ExcludedReason;

public:
    CK_PROPERTY(_Component);
    CK_PROPERTY(_Weight);
    CK_PROPERTY(_Value);
    CK_PROPERTY(_Included);
    CK_PROPERTY(_ExcludedReason);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Score
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Score);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Value = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_ScoreComponentResult> _Components;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Formula;

public:
    CK_PROPERTY(_Value);
    CK_PROPERTY(_Components);
    CK_PROPERTY(_Formula);
};

// --------------------------------------------------------------------------------------------------------------------

/** The measurement that justified a finding. A finding without one is not publishable. */
USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Evidence
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Evidence);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_PerfLab_Metric _Metric = ECk_PerfLab_Metric::Frame;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    float _MeasuredMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    float _BudgetMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    float _OverBudgetRatio = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Signal;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    float _SignalValue = 0.0f;

public:
    CK_PROPERTY(_Metric);
    CK_PROPERTY(_MeasuredMs);
    CK_PROPERTY(_BudgetMs);
    CK_PROPERTY(_OverBudgetRatio);
    CK_PROPERTY(_Signal);
    CK_PROPERTY(_SignalValue);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Contributor
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Contributor);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Rank = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _ObjectPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _ClassName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    float _DistanceCm = 0.0f;

    // Carried on every contributor rather than written once in the UI, so the qualification travels
    // with the claim into exports, logs and anything else that reads a finding.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Note;

public:
    CK_PROPERTY(_Rank);
    CK_PROPERTY(_ObjectPath);
    CK_PROPERTY(_ClassName);
    CK_PROPERTY(_DistanceCm);
    CK_PROPERTY(_Note);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Recommendation
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Recommendation);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Order = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_PerfLab_Band _GainBand = ECk_PerfLab_Band::Medium;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_PerfLab_Band _EffortBand = ECk_PerfLab_Band::Medium;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _Text;

public:
    CK_PROPERTY(_Order);
    CK_PROPERTY(_GainBand);
    CK_PROPERTY(_EffortBand);
    CK_PROPERTY(_Text);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Finding
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Finding);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _CheckId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _StableKey;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_PerfLab_Severity _Severity = ECk_PerfLab_Severity::Minor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _PositionId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FCk_PerfLab_Evidence _Evidence;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_Contributor> _Contributors;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_Recommendation> _Recommendations;

public:
    CK_PROPERTY(_CheckId);
    CK_PROPERTY(_StableKey);
    CK_PROPERTY(_Severity);
    CK_PROPERTY(_PositionId);
    CK_PROPERTY(_Evidence);
    CK_PROPERTY(_Contributors);
    CK_PROPERTY(_Recommendations);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Analysis
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Analysis);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    float _BudgetMs = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FCk_PerfLab_Score _Score;

    /** Every measured position was inside budget. Distinct from "no findings", which a session nobody analysed also shows. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    bool _MeasuredClean = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_Finding> _Findings;

public:
    CK_PROPERTY(_BudgetMs);
    CK_PROPERTY(_Score);
    CK_PROPERTY(_MeasuredClean);
    CK_PROPERTY(_Findings);
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Every position one rule fired at, collapsed into a single entry.
 *
 * A rule firing at twelve positions is ONE thing wrong with the level, not twelve — and a reader handed twelve
 * near-identical blocks has to diff them by eye to discover they say the same thing. The group keeps what differs
 * (which positions, and the worst measurement among them) and merges what does not.
 */
USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_FindingGroup
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_FindingGroup);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _CheckId;

    /** The worst severity anywhere in the group — a rule that was Critical somewhere is a Critical problem. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_PerfLab_Severity _Severity = ECk_PerfLab_Severity::Minor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FString> _PositionIds;

    /** Evidence from the position that measured WORST. A range's midpoint would describe nowhere real. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FCk_PerfLab_Evidence _WorstEvidence;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FString _WorstPositionId;

    /** Contributors seen at any position in the group, deduplicated by object path and re-ranked by proximity. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_Contributor> _Contributors;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_Recommendation> _Recommendations;

public:
    CK_PROPERTY(_CheckId);
    CK_PROPERTY(_Severity);
    CK_PROPERTY(_PositionIds);
    CK_PROPERTY(_WorstEvidence);
    CK_PROPERTY(_WorstPositionId);
    CK_PROPERTY(_Contributors);
    CK_PROPERTY(_Recommendations);
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    /** The wording every contributor carries. Pinned by spec so nobody can soften it into a claim. */
    CKPERFLAB_API extern const TCHAR* const k_ContributorDisclaimer;

    /**
     * Analyse a session against a budget.
     *
     * The budget is a PARAMETER rather than being read from the session's request, so one captured
     * session can be re-analysed at any budget. That is what lets the evidence gate be exercised on
     * real measured data without authoring a deliberately expensive level.
     */
    CKPERFLAB_API auto
    Analyse_Session(
        const FCk_PerfLab_Session& InSession,
        float InBudgetMs) -> FCk_PerfLab_Analysis;

    /** Score alone, for callers that do not want findings. */
    CKPERFLAB_API auto
    Compute_Score(
        const FCk_PerfLab_Session& InSession,
        float InBudgetMs) -> FCk_PerfLab_Score;

    /**
     * Findings collapsed by rule, worst first.
     *
     * Pure and separate from `Analyse_Session` on purpose: the ungrouped findings remain the record — they are what
     * the exports and the compare view read — and this is a projection over them for a reader. Grouping inside the
     * analysis would make the two disagree about how many findings there are.
     */
    CKPERFLAB_API auto
    Group_Findings(
        const FCk_PerfLab_Analysis& InAnalysis) -> TArray<FCk_PerfLab_FindingGroup>;
}

// --------------------------------------------------------------------------------------------------------------------
