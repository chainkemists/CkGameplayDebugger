#include "CkAStarDebugger/Data/CkAStarDebugger_DataCollector.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkAStar/CkAStar_Fragment.h"
#include "CkAStar/Test/CkAStar_TestFragments.h"
#include "CkAStar/Test/CkAStar_TestGraph.h"

#include "HAL/PlatformTime.h"
#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAStarDebugger_DataCollector::
    Collect(
        UWorld* InWorld)
    -> void
{
    _SearchEntities.Reset();

    if (NOT IsValid(InWorld))
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);

    if (NOT ck::IsValid(TransientEntity))
    { return; }

    TransientEntity.View<ck::FFragment_AStar_Debug>().ForEach(
        [this, &TransientEntity](FCk_Entity InEntity, const ck::FFragment_AStar_Debug&)
        {
            auto Handle = ck::MakeHandle(InEntity, TransientEntity);
            auto Info = CollectSearchEntity(Handle);
            TrackSearchCompletion(Handle, Info);
            _SearchEntities.Add(MoveTemp(Info));
        });

    OverlayTestGridSearchState(TransientEntity);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAStarDebugger_DataCollector::
    Get_AllSearchEntities() const
    -> const TArray<FCkAStarDebugger_SearchInfo>&
{
    return _SearchEntities;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAStarDebugger_DataCollector::
    Get_SearchHistory() const
    -> const TMap<uint32, TArray<FCkAStarDebugger_HistoryEntry>>&
{
    return _SearchHistory;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAStarDebugger_DataCollector::
    CollectSearchEntity(
        const FCk_Handle& InEntityHandle)
    -> FCkAStarDebugger_SearchInfo
{
    auto Info = FCkAStarDebugger_SearchInfo{};
    Info.EntityHandle = InEntityHandle;

    Info.DebugName = UCk_Utils_Handle_UE::Get_DebugName(InEntityHandle).ToString();

    if (Info.DebugName.IsEmpty())
    {
        Info.DebugName = TEXT("(unnamed)");
    }

    const auto& Debug = InEntityHandle.Get<ck::FFragment_AStar_Debug>();
    Info.SearchStatus = Debug.Get_SearchStatus();
    Info.OpenSetSize = Debug.Get_OpenSetSize();
    Info.ClosedSetSize = Debug.Get_ClosedSetSize();
    Info.IterationsThisFrame = Debug.Get_IterationsThisFrame();
    Info.BudgetUsagePercent = Debug.Get_BudgetUsagePercent();

    // NOTE: Despite field name, processor writes cumulative GetTotalTimeMicroseconds() here
    Info.TotalTimeMicroseconds = Debug.Get_TimeThisFrameMicroseconds();
    Info.TotalIterations = Info.ClosedSetSize;

    if (InEntityHandle.Has<ck::FFragment_AStar_Params>())
    {
        const auto& Params = InEntityHandle.Get<ck::FFragment_AStar_Params>();
        Info.BudgetMicroseconds = Params.Get_BudgetMicroseconds();
        Info.CostThreshold = Params.Get_CostThreshold();
    }

    if (InEntityHandle.Has<ck::FFragment_AStarTest_GridGraph>())
    {
        const auto& GridGraphFrag = InEntityHandle.Get<ck::FFragment_AStarTest_GridGraph>();
        const auto& Graph = GridGraphFrag._Graph;

        Info.GridWidth = Graph.Get_Width();
        Info.GridHeight = Graph.Get_Height();
        Info.StartNode = GridGraphFrag._StartNode;
        Info.GoalNode = Graph.Get_GoalNode();

        for (int32 CellIdx = 0; CellIdx < Info.GridWidth * Info.GridHeight; ++CellIdx)
        {
            if (Graph.IsCellBlocked(CellIdx))
            {
                Info.BlockedCells.Add(CellIdx);
            }
        }
    }

    return Info;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAStarDebugger_DataCollector::
    OverlayTestGridSearchState(
        FCk_Handle InTransientEntity)
    -> void
{
    InTransientEntity.View<ck::FFragment_AStarTest_SearchState, ck::FFragment_AStarTest_Result>().ForEach(
        [this](FCk_Entity InEntity, const ck::FFragment_AStarTest_SearchState& InSearchState,
               const ck::FFragment_AStarTest_Result& InResult)
        {
            for (auto& Info : _SearchEntities)
            {
                if (Info.EntityHandle.Get_Entity() != InEntity)
                { continue; }

                const auto& State = InSearchState._State;

                Info.HasCellData = true;
                Info.TotalIterations = State.GetTotalIterations();

                Info.StartNode = State.GetStart();
                Info.GoalNode = State.GetGoal();

                Info.ClosedSetCells = State.GetClosedSet();

                for (const auto& Entry : State.GetOpenSet())
                {
                    Info.OpenSetCells.Add(Entry.Node);
                }

                Info.GScores = State.GetGScores();
                Info.CameFrom = State.GetCameFrom();

                Info.Path = InResult._Path;
                Info.TotalCost = InResult._TotalCost;

                break;
            }
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAStarDebugger_DataCollector::
    TrackSearchCompletion(
        const FCk_Handle& InEntityHandle,
        const FCkAStarDebugger_SearchInfo& InInfo)
    -> void
{
    auto HandleHash = GetTypeHash(InEntityHandle);
    auto* PreviousStatus = _LastKnownStatus.Find(HandleHash);

    auto WasSearching = PreviousStatus
        && *PreviousStatus == ECk_AStarSearchStatus::InProgress;

    auto IsTerminal =
        InInfo.SearchStatus == ECk_AStarSearchStatus::Complete
        || InInfo.SearchStatus == ECk_AStarSearchStatus::Failed
        || InInfo.SearchStatus == ECk_AStarSearchStatus::CostThresholdReached;

    if (WasSearching && IsTerminal)
    {
        auto Entry = FCkAStarDebugger_HistoryEntry{};
        Entry.WallTime = FPlatformTime::Seconds();
        Entry.FrameNumber = GFrameNumber;
        Entry.FinalStatus = InInfo.SearchStatus;
        Entry.TotalIterations = InInfo.TotalIterations;
        Entry.TotalTimeMicroseconds = InInfo.TotalTimeMicroseconds;
        Entry.TotalCost = InInfo.TotalCost;
        Entry.PathLength = InInfo.Path.Num();

        _SearchHistory.FindOrAdd(HandleHash).Add(MoveTemp(Entry));

        _Accumulators.FindOrAdd(HandleHash) = {};
    }

    auto IsFirstSeen = NOT PreviousStatus;

    if (IsFirstSeen && IsTerminal)
    {
        auto Entry = FCkAStarDebugger_HistoryEntry{};
        Entry.WallTime = FPlatformTime::Seconds();
        Entry.FrameNumber = GFrameNumber;
        Entry.FinalStatus = InInfo.SearchStatus;
        Entry.TotalIterations = InInfo.TotalIterations;
        Entry.TotalTimeMicroseconds = InInfo.TotalTimeMicroseconds;
        Entry.TotalCost = InInfo.TotalCost;
        Entry.PathLength = InInfo.Path.Num();

        _SearchHistory.FindOrAdd(HandleHash).Add(MoveTemp(Entry));
    }

    _LastKnownStatus.Add(HandleHash, InInfo.SearchStatus);
}

// --------------------------------------------------------------------------------------------------------------------
