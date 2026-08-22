#pragma once

#include "CkPerfLab/Session/CkPerfLab_Session.h"

#include "CkPerfLab_Planner.generated.h"

// --------------------------------------------------------------------------------------------------------------------
// Choosing where to measure.
//
// The split here is the whole reason this is testable: reading the world produces a plain-data
// survey, and planning is a pure function of that survey plus the request. The planner never touches
// a UWorld, so its determinism can be pinned against synthetic input instead of against a level.
// --------------------------------------------------------------------------------------------------------------------

/** One actor as the planner sees it: somewhere to be, and a reason to care. */
USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_SurveyActor
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_SurveyActor);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FVector _Location = FVector::ZeroVector;

    // A rough stand-in for "how much is this likely to cost", summed over neighbours to rank a
    // candidate position. It decides where to look, never what to report.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _CostWeight = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_PerfLab_ActorCensusRow _Census;

public:
    CK_PROPERTY(_Location);
    CK_PROPERTY(_CostWeight);
    CK_PROPERTY(_Census);
};

// --------------------------------------------------------------------------------------------------------------------

/** Everything the planner is allowed to know about the level. Gathered by the runner; faked by specs. */
USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_WorldSurvey
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_WorldSurvey);

private:
    // Player-reachable points, when the map has navigation data. Preferred over a grid because a
    // position a player can never stand in is a position whose cost nobody will ever pay.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FVector> _NavmeshPoints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_SurveyActor> _Actors;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FBox _WorldBounds = FBox{ForceInit};

public:
    CK_PROPERTY(_NavmeshPoints);
    CK_PROPERTY(_Actors);
    CK_PROPERTY(_WorldBounds);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_PlannedPosition
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_PlannedPosition);

private:
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
    TArray<float> _Yaws;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    float _CostWeight = 0.0f;

public:
    CK_PROPERTY(_PositionId);
    CK_PROPERTY(_Location);
    CK_PROPERTY(_EyeHeightCm);
    CK_PROPERTY(_Yaws);
    CK_PROPERTY(_CostWeight);
};

// --------------------------------------------------------------------------------------------------------------------

/** Why the planner produced what it produced — surfaced so an empty plan is never a mystery. */
UENUM(BlueprintType)
enum class ECk_PerfLab_PlanOutcome : uint8
{
    NavmeshSeeded,
    GridFallback,
    Empty_NoContent,
    Empty_NoBounds,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_PerfLab_PlanOutcome);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPERFLAB_API FCk_PerfLab_Plan
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_PerfLab_Plan);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TArray<FCk_PerfLab_PlannedPosition> _Positions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_PerfLab_PlanOutcome _Outcome = ECk_PerfLab_PlanOutcome::Empty_NoContent;

public:
    CK_PROPERTY(_Positions);
    CK_PROPERTY(_Outcome);
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    /**
     * Choose measurement positions.
     *
     * Deterministic by construction: the same survey and request always produce byte-identical
     * plans, and permuting the survey's actor order changes nothing. That is not a nicety — position
     * ids are the key two sessions are compared on, so a planner that wandered between runs would
     * make the compare view meaningless.
     *
     * Candidates come from the navmesh where the map has one and from a content-weighted grid where
     * it does not; empty space is never sampled, because a position with nothing around it measures
     * nothing worth reporting.
     */
    CKPERFLAB_API auto
    Generate_Plan(
        const FCk_PerfLab_WorldSurvey& InSurvey,
        const FCk_PerfLab_Request& InRequest) -> FCk_PerfLab_Plan;

    /**
     * The id for a location, stable across runs of the same map because it is derived from the
     * location quantised to a coarse grid rather than from an index into an ordering.
     */
    CKPERFLAB_API auto
    Get_PositionId(
        const FVector& InLocation) -> FString;
}

// --------------------------------------------------------------------------------------------------------------------
