#include "CkCrowdDebugger/Commands/CkCrowdDebugger_PathNetworkCommand.h"

#include "CkCrowd/Agent/CkCrowdAgent_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkNavigation/Settings/CkNav_ProjectSettings.h"
#include "CkNavigation/Utils/CkNav_Utils.h"
#include "CkPathNetwork/Actor/CkPathNetwork_Actor.h"
#include "CkPathNetwork/Network/CkPathNetwork_Fragment_Data.h"
#include "CkPathNetwork/Network/CkPathNetwork_Utils.h"

#include "Kismet/GameplayStatics.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::crowd_debugger
{
    const FName PathNetworkFollowerOwnerToken = TEXT("CkCrowdDebugger.ManualMove");

    auto
    Try_IssueManualMove(
        FCk_Handle_CrowdAgent& InAgent,
        const FVector& InRawDestination,
        FVector& OutProjectedDestination)
        -> bool
    {
        const auto AgentIsValid = ck::IsValid(InAgent);
        CK_ENSURE_IF_NOT(AgentIsValid,
            TEXT("Invalid CrowdAgent handle [{}] passed to Try_IssueManualMove"), InAgent)
        {}
        if (NOT AgentIsValid)
        { return false; }

        auto AgentHandle = FCk_Handle{InAgent};
        const auto ProjectionExtent =
            UCk_Utils_Nav_Settings_UE::Get_NavQueryProjectionExtentVec();
        const auto HorizontalHalfExtent =
            FMath::Max(ProjectionExtent.X, ProjectionExtent.Y);
        auto ProjectedDestination = FVector{};
        const auto DestinationIsOnNavmesh =
            UCk_Utils_Nav_UE::Try_ProjectOntoNavmesh(
                AgentHandle,
                InRawDestination,
                HorizontalHalfExtent,
                ProjectedDestination,
                ProjectionExtent.Z);
        if (NOT DestinationIsOnNavmesh)
        { return false; }

        Try_EnsurePathNetworkFollower(InAgent);
        if (NOT UCk_Utils_CrowdAgent_UE::Get_HasDebugOverride(InAgent))
        { UCk_Utils_CrowdAgent_UE::Request_SetDebugOverride(InAgent, true, {}); }

        UCk_Utils_CrowdAgent_UE::Request_MoveTo(
            InAgent, FCk_Request_CrowdAgent_MoveTo{ProjectedDestination}, {});
        OutProjectedDestination = ProjectedDestination;
        return true;
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
    Try_EnsurePathNetworkFollower(
        FCk_Handle_CrowdAgent& InAgent)
        -> bool
    {
        if (ck::Is_NOT_Valid(InAgent))
        { return false; }

        FCk_Handle AgentHandle = InAgent;
        if (UCk_Utils_PathNetworkFollower_UE::Has(AgentHandle))
        { return true; }

        auto* const World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InAgent);
        if (ck::Is_NOT_Valid(World))
        { return false; }

        TArray<AActor*> PathNetworkActors;
        UGameplayStatics::GetAllActorsOfClass(World, ACk_PathNetwork_UE::StaticClass(), PathNetworkActors);

        if (PathNetworkActors.IsEmpty())
        { return false; }

        const bool HasExactlyOnePathNetwork = PathNetworkActors.Num() == 1;
        CK_ENSURE_IF_NOT(HasExactlyOnePathNetwork,
            TEXT("Manual debugger path-network move requires exactly one ACk_PathNetwork_UE in [{}], found [{}]"),
            World, PathNetworkActors.Num())
        {}
        if (NOT HasExactlyOnePathNetwork)
        { return false; }

        const auto* const PathNetworkActor = Cast<ACk_PathNetwork_UE>(PathNetworkActors[0]);
        if (ck::Is_NOT_Valid(PathNetworkActor))
        { return false; }

        const auto Network = PathNetworkActor->Get_NetworkHandle();
        if (ck::Is_NOT_Valid(Network) || NOT UCk_Utils_PathNetwork_UE::Get_IsBuilt(Network))
        { return false; }

        auto FollowerParams = FCk_Fragment_PathNetworkFollower_ParamsData{};
        FollowerParams.Set_Network(Network);
        FollowerParams.Set_OwnerToken(PathNetworkFollowerOwnerToken);
        auto Follower = UCk_Utils_PathNetworkFollower_UE::Add(AgentHandle, FollowerParams);
        if (ck::Is_NOT_Valid(Follower))
        { return false; }

        const bool IsDebuggerOwned =
            UCk_Utils_PathNetworkFollower_UE::Get_OwnerToken(Follower) == PathNetworkFollowerOwnerToken;
        CK_ENSURE_IF_NOT(IsDebuggerOwned,
            TEXT("Manual debugger path-network follower on [{}] was not composed with its owner token"), InAgent)
        {}
        if (NOT IsDebuggerOwned)
        {
            UCk_Utils_PathNetworkFollower_UE::Remove(Follower);
            return false;
        }

        return true;
    }

    auto
    ReleaseManualMove(
        FCk_Handle_CrowdAgent& InAgent)
        -> void
    {
        if (ck::Is_NOT_Valid(InAgent))
        { return; }

        FCk_Handle AgentHandle = InAgent;
        if (NOT UCk_Utils_PathNetworkFollower_UE::Has(AgentHandle))
        { return; }

        auto Follower = UCk_Utils_PathNetworkFollower_UE::CastChecked(AgentHandle);
        const bool IsDebuggerOwned =
            UCk_Utils_PathNetworkFollower_UE::Get_OwnerToken(Follower) == PathNetworkFollowerOwnerToken;
        if (NOT IsDebuggerOwned)
        { return; }

        UCk_Utils_CrowdAgent_UE::Request_Stop(InAgent, {});
        UCk_Utils_PathNetworkFollower_UE::Remove(Follower);
    }
}
