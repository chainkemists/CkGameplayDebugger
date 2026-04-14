#pragma once

#include "CkAStarDebugger/Data/CkAStarDebugger_Types.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

class FCkAStarDebugger_DataCollector
{
public:
    auto
    Collect(
        UWorld* InWorld) -> void;

    auto
    Get_AllSearchEntities() const -> const TArray<FCkAStarDebugger_SearchInfo>&;

    auto
    Get_SearchHistory() const -> const TMap<uint32, TArray<FCkAStarDebugger_HistoryEntry>>&;

private:
    auto
    CollectSearchEntity(
        const FCk_Handle& InEntityHandle) -> FCkAStarDebugger_SearchInfo;

    auto
    OverlayTestGridSearchState(
        FCk_Handle InTransientEntity) -> void;

    auto
    TrackSearchCompletion(
        const FCk_Handle& InEntityHandle,
        const FCkAStarDebugger_SearchInfo& InInfo) -> void;

private:
    TArray<FCkAStarDebugger_SearchInfo> _SearchEntities;
    TMap<uint32, TArray<FCkAStarDebugger_HistoryEntry>> _SearchHistory;
    TMap<uint32, ECk_AStarSearchStatus> _LastKnownStatus;

    struct FAccumulator
    {
        int32 TotalIterations = 0;
        int64 TotalTimeMicroseconds = 0;
    };

    TMap<uint32, FAccumulator> _Accumulators;
};

// --------------------------------------------------------------------------------------------------------------------
