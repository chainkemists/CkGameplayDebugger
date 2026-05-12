#pragma once

#include "CkEqs/Query/CkEqs_Fragment_Data.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// ====================================================================================================================
// QUERY LIFECYCLE STATUS — derived from the tag set on the query entity
// ====================================================================================================================

enum class ECkEqsDebugger_QueryStatus : uint8
{
    Unknown,        // No lifecycle tag found (transient state, shouldn't normally happen)
    Pending,        // FTag_EqsQuery_Pending — generated, awaiting Generate
    InProgress,     // FTag_EqsQuery_InProgress — generated; tests running
    Complete,       // FTag_EqsQuery_Complete — finalized; results available
    Failed,         // FTag_EqsQuery_Failed (paired with Complete)
    Cancelled,      // FTag_EqsQuery_Cancelled — caller-issued cancel
};

// ====================================================================================================================
// PER-TEST INFO — single row in the candidate's per-test breakdown panel
// ====================================================================================================================

struct FCkEqsDebugger_PerTestInfo
{
    int32                 TestIndex     = -1;
    ECk_Eqs_TestType      TestType      = ECk_Eqs_TestType::Distance;
    ECk_Eqs_TestPurpose   TestPurpose   = ECk_Eqs_TestPurpose::FilterAndScore;
    float                 Weight        = 1.0f;

    float                 RawValue              = 0.0f;
    float                 NormalizedScore       = 0.0f;
    float                 WeightedContribution  = 1.0f;
    bool                  PassedThisTest        = true;
};

// ====================================================================================================================
// CANDIDATE INFO — single row in the candidate panel (one query → many candidates)
// ====================================================================================================================

struct FCkEqsDebugger_CandidateInfo
{
    // Index in FCk_Eqs_QueryResults._Candidates (post-Finalize order, sorted by score for SingleBest/AllMatchingSorted).
    int32                 ResultIndex   = -1;

    FVector               Location      = FVector::ZeroVector;
    FCk_Handle            EntityHandle;          // valid only for EntitiesWithTag generator
    float                 FinalScore    = 1.0f;  // accumulator product across all tests
    bool                  Passed        = true;  // false → filter-rejected; Finalize drops these from results
    bool                  IsBestPick    = false; // first entry in Results._Candidates after RunMode applied

    TArray<FCkEqsDebugger_PerTestInfo> PerTest;  // size == query._Tests.Num(), parallel-indexed
};

// ====================================================================================================================
// QUERY INFO — one row in the query list panel
// ====================================================================================================================

struct FCkEqsDebugger_QueryInfo
{
    FCk_Handle_EqsQuery   QueryHandle;
    FString               DebugName;
    FCk_Handle            Querier;
    FCk_Handle            ContextEntity;

    ECkEqsDebugger_QueryStatus Status = ECkEqsDebugger_QueryStatus::Unknown;

    ECk_Eqs_GeneratorType GeneratorType = ECk_Eqs_GeneratorType::SimpleGrid;
    int32                 TestCount     = 0;
    ECk_Eqs_RunMode       RunMode       = ECk_Eqs_RunMode::SingleBest;

    // SimpleGrid / Grid generator params, surfaced so the overlay can build a CkGrid 2dGridSystem
    // entity matching the EQS lattice and call DebugDraw_Grid on it. Zero for non-grid generators.
    float                 GridSpaceBetween = 0.0f;
    float                 GridHalfSize     = 0.0f;

    // Test-types preview (so the query list row can show "[Distance, Dot, Trace]" without selecting).
    TArray<ECk_Eqs_TestType> TestTypes;

    // Result summary (post-Finalize)
    int32                 CandidateCount   = 0;   // size of Results._Candidates after RunMode applied
    bool                  HasResults       = false;
    FVector               BestLocation     = FVector::ZeroVector;
    FCk_Handle            BestEntity;

    // Cursor state (so the query list can show "test 2 of 5" for in-progress multi-frame queries).
    int32                 NextTestIndex    = 0;

    // Detailed candidate + per-test breakdown (populated only for the selected query to keep the
    // collector cheap on large query sets). Empty for unselected queries.
    TArray<FCkEqsDebugger_CandidateInfo> Candidates;
};

// ====================================================================================================================
// HELPERS — color / string mappings for status + test type
// ====================================================================================================================

namespace CkEqsDebugger
{

inline auto
    GetStatusColor(
        ECkEqsDebugger_QueryStatus InStatus)
    -> FLinearColor
{
    switch (InStatus)
    {
    case ECkEqsDebugger_QueryStatus::Pending:    return FLinearColor(0.565f, 0.565f, 0.565f);
    case ECkEqsDebugger_QueryStatus::InProgress: return FLinearColor(0.231f, 0.510f, 0.965f);
    case ECkEqsDebugger_QueryStatus::Complete:   return FLinearColor(0.133f, 0.773f, 0.369f);
    case ECkEqsDebugger_QueryStatus::Failed:     return FLinearColor(0.937f, 0.267f, 0.267f);
    case ECkEqsDebugger_QueryStatus::Cancelled:  return FLinearColor(0.961f, 0.620f, 0.043f);
    case ECkEqsDebugger_QueryStatus::Unknown:    return FLinearColor(0.290f, 0.333f, 0.408f);
    }
    return FLinearColor::White;
}

inline auto
    GetStatusString(
        ECkEqsDebugger_QueryStatus InStatus)
    -> FString
{
    switch (InStatus)
    {
    case ECkEqsDebugger_QueryStatus::Pending:    return TEXT("Pending");
    case ECkEqsDebugger_QueryStatus::InProgress: return TEXT("InProgress");
    case ECkEqsDebugger_QueryStatus::Complete:   return TEXT("Complete");
    case ECkEqsDebugger_QueryStatus::Failed:     return TEXT("Failed");
    case ECkEqsDebugger_QueryStatus::Cancelled:  return TEXT("Cancelled");
    case ECkEqsDebugger_QueryStatus::Unknown:    return TEXT("Unknown");
    }
    return TEXT("?");
}

inline auto
    GetGeneratorTypeString(
        ECk_Eqs_GeneratorType InType)
    -> FString
{
    switch (InType)
    {
    case ECk_Eqs_GeneratorType::SimpleGrid:        return TEXT("SimpleGrid");
    case ECk_Eqs_GeneratorType::Grid:              return TEXT("Grid");
    case ECk_Eqs_GeneratorType::Donut:             return TEXT("Donut");
    case ECk_Eqs_GeneratorType::Cone:              return TEXT("Cone");
    case ECk_Eqs_GeneratorType::EntitiesWithTag:   return TEXT("EntitiesWithTag");
    }
    return TEXT("?");
}

inline auto
    GetTestTypeString(
        ECk_Eqs_TestType InType)
    -> FString
{
    switch (InType)
    {
    case ECk_Eqs_TestType::Distance:    return TEXT("Distance");
    case ECk_Eqs_TestType::Dot:         return TEXT("Dot");
    case ECk_Eqs_TestType::Trace:       return TEXT("Trace");
    case ECk_Eqs_TestType::GameplayTag: return TEXT("GameplayTag");
    case ECk_Eqs_TestType::Overlap:     return TEXT("Overlap");
    }
    return TEXT("?");
}

inline auto
    GetTestPurposeString(
        ECk_Eqs_TestPurpose InPurpose)
    -> FString
{
    switch (InPurpose)
    {
    case ECk_Eqs_TestPurpose::Filter:          return TEXT("Filter");
    case ECk_Eqs_TestPurpose::Score:           return TEXT("Score");
    case ECk_Eqs_TestPurpose::FilterAndScore:  return TEXT("Filter+Score");
    }
    return TEXT("?");
}

inline auto
    GetRunModeString(
        ECk_Eqs_RunMode InMode)
    -> FString
{
    switch (InMode)
    {
    case ECk_Eqs_RunMode::SingleBest:         return TEXT("SingleBest");
    case ECk_Eqs_RunMode::AllMatching:        return TEXT("AllMatching");
    case ECk_Eqs_RunMode::AllMatchingSorted:  return TEXT("AllMatchingSorted");
    case ECk_Eqs_RunMode::RandomBest5Pct:     return TEXT("RandomBest5%");
    case ECk_Eqs_RunMode::RandomBest25Pct:    return TEXT("RandomBest25%");
    }
    return TEXT("?");
}

// Lerp a candidate's score in [0, 1] from low (blue) → high (green). Failed candidates muted gray.
inline auto
    GetScoreColor(
        float InNormalizedScore,
        bool  InPassed)
    -> FLinearColor
{
    if (NOT InPassed)
    { return FLinearColor(0.290f, 0.333f, 0.408f); }

    const auto T = FMath::Clamp(InNormalizedScore, 0.0f, 1.0f);
    return FLinearColor::LerpUsingHSV(
        FLinearColor(0.231f, 0.510f, 0.965f),  // low: blue
        FLinearColor(0.133f, 0.773f, 0.369f),  // high: green
        T);
}

} // namespace CkEqsDebugger

// ====================================================================================================================
