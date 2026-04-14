#pragma once

#include "CkAStar/CkAStar_Fragment_Data.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// ====================================================================================================================
// SEARCH INFO — per-entity collected data snapshot
// ====================================================================================================================

struct FCkAStarDebugger_SearchInfo
{
    FCk_Handle EntityHandle;
    FString DebugName;
    ECk_AStarSearchStatus SearchStatus = ECk_AStarSearchStatus::Idle;

    int32 GridWidth = 0;
    int32 GridHeight = 0;

    TArray<int32> Path;
    float TotalCost = 0.0f;
    int32 TotalIterations = 0;
    int64 TotalTimeMicroseconds = 0;

    int32 OpenSetSize = 0;
    int32 ClosedSetSize = 0;
    int32 IterationsThisFrame = 0;
    int64 TimeThisFrameMicroseconds = 0;
    float BudgetUsagePercent = 0.0f;

    int64 BudgetMicroseconds = 0;
    float CostThreshold = 0.0f;

    int32 StartNode = -1;
    int32 GoalNode = -1;

    TSet<int32> BlockedCells;
    TSet<int32> OpenSetCells;
    TSet<int32> ClosedSetCells;

    TMap<int32, float> GScores;
    TMap<int32, int32> CameFrom;

    bool HasCellData = false;
};

// ====================================================================================================================
// HISTORY ENTRY — records a search completion event
// ====================================================================================================================

struct FCkAStarDebugger_HistoryEntry
{
    double WallTime = 0.0;
    uint64 FrameNumber = 0;
    ECk_AStarSearchStatus FinalStatus = ECk_AStarSearchStatus::Idle;
    int32 TotalIterations = 0;
    int64 TotalTimeMicroseconds = 0;
    float TotalCost = 0.0f;
    int32 PathLength = 0;
};

// ====================================================================================================================
// HELPER — get color for a search status
// ====================================================================================================================

namespace CkAStarDebugger
{

inline auto
    GetStatusColor(
        ECk_AStarSearchStatus InStatus)
    -> FLinearColor
{
    switch (InStatus)
    {
    case ECk_AStarSearchStatus::Idle:                 return FLinearColor(0.565f, 0.565f, 0.565f);
    case ECk_AStarSearchStatus::InProgress:           return FLinearColor(0.231f, 0.510f, 0.965f);
    case ECk_AStarSearchStatus::Complete:             return FLinearColor(0.133f, 0.773f, 0.369f);
    case ECk_AStarSearchStatus::Failed:               return FLinearColor(0.937f, 0.267f, 0.267f);
    case ECk_AStarSearchStatus::CostThresholdReached: return FLinearColor(0.961f, 0.620f, 0.043f);
    default:                                          return FLinearColor(0.565f, 0.565f, 0.565f);
    }
}

inline auto
    GetStatusString(
        ECk_AStarSearchStatus InStatus)
    -> FString
{
    switch (InStatus)
    {
    case ECk_AStarSearchStatus::Idle:                 return TEXT("Idle");
    case ECk_AStarSearchStatus::InProgress:           return TEXT("InProgress");
    case ECk_AStarSearchStatus::Complete:             return TEXT("Complete");
    case ECk_AStarSearchStatus::Failed:               return TEXT("Failed");
    case ECk_AStarSearchStatus::CostThresholdReached: return TEXT("CostThreshold");
    default:                                          return TEXT("Unknown");
    }
}

} // namespace CkAStarDebugger

// ====================================================================================================================
