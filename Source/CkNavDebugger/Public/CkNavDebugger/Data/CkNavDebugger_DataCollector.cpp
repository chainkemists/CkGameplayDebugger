#include "CkNavDebugger/Data/CkNavDebugger_DataCollector.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkNavigation/Nav/CkNav_Fragment.h"
#include "CkNavigation/Settings/CkNav_ProjectSettings.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"

#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkNavDebugger_DataCollector::
    Collect(
        UWorld* InWorld)
    -> void
{
    _Agents.Reset();
    _NavmeshInfo = FCkNavDebugger_NavmeshInfo{};

    DoCollectNavmesh(InWorld);

    if (NOT IsValid(InWorld))
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);

    if (NOT ck::IsValid(TransientEntity))
    { return; }

    TransientEntity.View<ck::FFragment_Nav_AgentParams>().ForEach(
        [this, &TransientEntity](FCk_Entity InEntity, const ck::FFragment_Nav_AgentParams&)
        {
            auto Handle = ck::MakeHandle(InEntity, TransientEntity);
            auto Info = DoCollectAgent(Handle);
            DoTrackFailure(Info);
            _Agents.Add(MoveTemp(Info));
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkNavDebugger_DataCollector::
    DoCollectAgent(
        const FCk_Handle& InHandle)
    -> FCkNavDebugger_AgentInfo
{
    auto Info = FCkNavDebugger_AgentInfo{};
    Info.EntityHandle = InHandle;

    Info.DebugName = UCk_Utils_Handle_UE::Get_DebugName(InHandle).ToString();
    if (Info.DebugName.IsEmpty())
    { Info.DebugName = TEXT("(unnamed)"); }

    if (InHandle.Has<ck::FFragment_Nav_AgentParams>())
    {
        Info.AgentParams = InHandle.Get<ck::FFragment_Nav_AgentParams>();
    }

    if (InHandle.Has<ck::FFragment_Nav_PathResult>())
    {
        const auto& PathResult = InHandle.Get<ck::FFragment_Nav_PathResult>();
        Info.PathStatus          = PathResult.Get_Status();
        Info.Waypoints           = PathResult.Get_Waypoints();
        Info.DestinationLocation = PathResult.Get_DestinationLocation();
        Info.Diagnostics         = PathResult.Get_Diagnostics();
    }

    if (InHandle.Has<ck::FFragment_Transform>())
    {
        Info.AgentLocation = InHandle.Get<ck::FFragment_Transform>().Get_Transform().GetLocation();
    }
    else
    {
        // Fall back to the diagnostic snapshot if Transform isn't on the entity.
        Info.AgentLocation = Info.Diagnostics.Get_LastAgentLocation();
    }

    if (InHandle.Has<ck::FFragment_Nav_Requests>())
    {
        Info.QueuedRequestCount = InHandle.Get<ck::FFragment_Nav_Requests>().Get_Requests().Num();
    }

    Info.HasPathPendingTag     = InHandle.Has<ck::FTag_Nav_PathPending>();
    Info.HasPathReadyTag       = InHandle.Has<ck::FTag_Nav_PathReady>();
    Info.HasPathFailedTag      = InHandle.Has<ck::FTag_Nav_PathFailed>();
    Info.HasCrowdRegisteredTag = InHandle.Has<ck::FTag_Nav_CrowdRegistered>();

    return Info;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkNavDebugger_DataCollector::
    DoCollectNavmesh(
        UWorld* InWorld)
    -> void
{
    auto& Out = _NavmeshInfo;
    Out.MaxPathQueriesPerFrame     = UCk_Utils_Nav_ProjectSettings::Get_MaxPathQueriesPerFrame();
    Out.NavRebuildDebounceSeconds  = UCk_Utils_Nav_ProjectSettings::Get_NavRebuildDebounceSeconds();

    Out.HasWorld = IsValid(InWorld);
    if (NOT Out.HasWorld)
    { return; }

    auto* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(InWorld);
    Out.HasNavSystem = ck::IsValid(NavSys, ck::IsValid_Policy_NullptrOnly{});
    if (NOT Out.HasNavSystem)
    { return; }

    // bAutoCreateNavigationData / bSpawnNavDataInNavBoundsLevel are protected on
    // UNavigationSystemV1; pull from GConfig (config UPROPERTYs match the live values).
    auto AutoCreate = false;
    auto SpawnInBounds = false;
    GConfig->GetBool(TEXT("/Script/Engine.NavigationSystemV1"),
        TEXT("bAutoCreateNavigationData"), AutoCreate, GEngineIni);
    GConfig->GetBool(TEXT("/Script/Engine.NavigationSystemV1"),
        TEXT("bSpawnNavDataInNavBoundsLevel"), SpawnInBounds, GEngineIni);
    Out.AutoCreateNavigationData    = AutoCreate;
    Out.SpawnNavDataInNavBoundsLevel = SpawnInBounds;
    Out.NavDataSetCount             = NavSys->NavDataSet.Num();

    auto* NavData = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
    Out.HasNavData = ck::IsValid(NavData, ck::IsValid_Policy_NullptrOnly{});
    if (NOT Out.HasNavData)
    { return; }

    Out.NavDataName      = NavData->GetName();
    Out.NavDataClassName = NavData->GetClass()->GetName();

    const auto Bounds = NavData->GetNavMeshBounds();
    Out.NavMeshBounds  = Bounds;
    Out.BoundsValid    = Bounds.IsValid != 0;
    Out.BoundsHasNonZeroVolume = Bounds.IsValid && Bounds.GetVolume() > KINDA_SMALL_NUMBER;

    Out.DefaultFilterValid = NavData->GetDefaultQueryFilter().IsValid();

    const auto& Config = NavData->GetConfig();
    Out.AgentRadius     = Config.AgentRadius;
    Out.AgentHeight     = Config.AgentHeight;
    Out.AgentStepHeight = Config.AgentStepHeight;
    Out.AgentName       = Config.Name;

    // Count NavSys's registered supported-agents (matches what's in DefaultEngine.ini SupportedAgents)
    Out.RegisteredAgentCount = NavSys->GetSupportedAgents().Num();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkNavDebugger_DataCollector::
    DoTrackFailure(
        const FCkNavDebugger_AgentInfo& InInfo)
    -> void
{
    const auto Hash = GetTypeHash(InInfo.EntityHandle);
    const auto* PrevStatus = _LastKnownStatus.Find(Hash);
    const auto WasFailed = PrevStatus && *PrevStatus == ECk_Nav_PathStatus::Failed;
    const auto IsFailed  = InInfo.PathStatus == ECk_Nav_PathStatus::Failed;

    // Log the FRAME-LEVEL transition into Failed (or first observation if it's already failed).
    // We also re-log when fail-reason changes between two consecutive failed frames so we
    // don't miss "fail reason flipped from StartProjectFailed to FindPathError" sequences.
    auto ShouldLog = false;
    if (IsFailed && NOT WasFailed)
    {
        ShouldLog = true;
    }
    else if (IsFailed && WasFailed)
    {
        // Has the fail reason changed? Compare against the most-recent log entry for this handle.
        for (auto i = _FailureLog.Num() - 1; i >= 0; --i)
        {
            if (GetTypeHash(_FailureLog[i].EntityHandle) == Hash)
            {
                if (_FailureLog[i].FailReason != InInfo.Diagnostics.Get_LastFailReason())
                { ShouldLog = true; }
                break;
            }
        }
    }

    if (ShouldLog)
    {
        auto Entry = FCkNavDebugger_FailureLogEntry{};
        Entry.WallTime          = FPlatformTime::Seconds();
        Entry.FrameNumber       = GFrameNumber;
        Entry.EntityHandle      = InInfo.EntityHandle;
        Entry.DebugName         = InInfo.DebugName;
        Entry.FailReason        = InInfo.Diagnostics.Get_LastFailReason();
        Entry.AgentLocation     = InInfo.Diagnostics.Get_LastAgentLocation();
        Entry.TargetLocation    = InInfo.Diagnostics.Get_LastTargetLocation();
        Entry.ProjectedStart    = InInfo.Diagnostics.Get_LastProjectedStart();
        Entry.ProjectedEnd      = InInfo.Diagnostics.Get_LastProjectedEnd();
        Entry.StartProjected    = InInfo.Diagnostics.Get_StartProjected();
        Entry.EndProjected      = InInfo.Diagnostics.Get_EndProjected();
        Entry.RawPathPointCount = InInfo.Diagnostics.Get_RawPathPointCount();
        Entry.DurationMs        = InInfo.Diagnostics.Get_LastQueryDurationMs();

        _FailureLog.Add(MoveTemp(Entry));

        if (_FailureLog.Num() > FailureLog_MaxEntries)
        {
            _FailureLog.RemoveAt(0, _FailureLog.Num() - FailureLog_MaxEntries, EAllowShrinking::No);
        }
    }

    _LastKnownStatus.Add(Hash, InInfo.PathStatus);
}

// --------------------------------------------------------------------------------------------------------------------
