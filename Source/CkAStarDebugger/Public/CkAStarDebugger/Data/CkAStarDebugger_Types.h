#pragma once

#include "CkAStar/CkAStar_Fragment_Data.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEditorTools/Style/CkStyle.h"

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

// THE status→appearance mapping for this debugger. Toolbar badge, history rows, and the stats
// call-out all resolve through the tone, so a palette edit moves every surface at once.
inline auto
    GetStatusTone(
        ECk_AStarSearchStatus InStatus)
    -> ECk_Tone
{
    switch (InStatus)
    {
    case ECk_AStarSearchStatus::Idle:                 return ECk_Tone::Neutral;
    case ECk_AStarSearchStatus::InProgress:           return ECk_Tone::Info;
    case ECk_AStarSearchStatus::Complete:             return ECk_Tone::Ok;
    case ECk_AStarSearchStatus::Failed:               return ECk_Tone::Err;
    case ECk_AStarSearchStatus::CostThresholdReached: return ECk_Tone::Warn;
    default:                                          return ECk_Tone::Neutral;
    }
}

inline auto
    GetStatusColor(
        ECk_AStarSearchStatus InStatus)
    -> FLinearColor
{
    return CkStyle::GetToneColor(GetStatusTone(InStatus));
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
