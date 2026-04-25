#pragma once

#include "CkNavigation/Nav/CkNav_Fragment_Data.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Per-agent snapshot — debugger-side copy of nav state, sized for rendering.
// --------------------------------------------------------------------------------------------------------------------

struct FCkNavDebugger_AgentInfo
{
    FCk_Handle EntityHandle;
    FString DebugName;

    // Live agent state
    FVector AgentLocation = FVector::ZeroVector;
    FCk_Nav_AgentParams AgentParams;
    ECk_Nav_PathStatus PathStatus = ECk_Nav_PathStatus::None;

    // Path output
    TArray<FVector> Waypoints;
    FVector DestinationLocation = FVector::ZeroVector;

    // Diagnostics (snapshot of FCk_Nav_PathDiagnostics)
    FCk_Nav_PathDiagnostics Diagnostics;

    // Pipeline tags
    bool HasPathPendingTag    = false;
    bool HasPathReadyTag      = false;
    bool HasPathFailedTag     = false;
    bool HasCrowdRegisteredTag = false;
    int32 QueuedRequestCount  = 0;
};

// --------------------------------------------------------------------------------------------------------------------
// System-level navmesh snapshot — captured each frame.
// --------------------------------------------------------------------------------------------------------------------

struct FCkNavDebugger_NavmeshInfo
{
    bool HasWorld = false;
    bool HasNavSystem = false;
    bool HasNavData = false;
    bool DefaultFilterValid = false;
    bool BoundsValid = false;
    bool BoundsHasNonZeroVolume = false;

    FString NavDataName;
    FString NavDataClassName;
    FBox NavMeshBounds = FBox(ForceInit);

    int32 RegisteredAgentCount = 0;
    int32 NavDataSetCount = 0;     // > 1 → multi-navmesh detected (we use only Default)

    // Configured agent (from NavData->GetConfig())
    float AgentRadius = 0.0f;
    float AgentHeight = 0.0f;
    float AgentStepHeight = 0.0f;
    FName AgentName;

    // Project-settings snapshot
    int32 MaxPathQueriesPerFrame = 0;
    float NavRebuildDebounceSeconds = 0.0f;

    // Auto-create config (from project settings INI / NavSys properties)
    bool AutoCreateNavigationData = false;
    bool SpawnNavDataInNavBoundsLevel = false;
};

// --------------------------------------------------------------------------------------------------------------------
// Failure log entry — appended every time an agent's path query fails.
// --------------------------------------------------------------------------------------------------------------------

struct FCkNavDebugger_FailureLogEntry
{
    double WallTime = 0.0;
    uint64 FrameNumber = 0;
    FCk_Handle EntityHandle;
    FString DebugName;
    ECk_Nav_PathFailReason FailReason = ECk_Nav_PathFailReason::None;
    FVector AgentLocation = FVector::ZeroVector;
    FVector TargetLocation = FVector::ZeroVector;
    FVector ProjectedStart = FVector::ZeroVector;
    FVector ProjectedEnd = FVector::ZeroVector;
    bool StartProjected = false;
    bool EndProjected = false;
    int32 RawPathPointCount = 0;
    float DurationMs = 0.0f;
};

// --------------------------------------------------------------------------------------------------------------------
// Health-check result — produced by FCkNavDebugger_HealthCheck::Run.
// --------------------------------------------------------------------------------------------------------------------

enum class ECkNavDebugger_HealthStatus : uint8
{
    Pass,    // green
    Warn,    // amber — non-fatal but worth flagging
    Fail     // red — will block all path queries
};

struct FCkNavDebugger_HealthCheckItem
{
    FString Name;
    ECkNavDebugger_HealthStatus Status = ECkNavDebugger_HealthStatus::Pass;
    FString Detail;
};

struct FCkNavDebugger_HealthCheckReport
{
    double WallTime = 0.0;
    TArray<FCkNavDebugger_HealthCheckItem> Items;

    auto AnyFail() const -> bool
    {
        for (const auto& Item : Items)
        {
            if (Item.Status == ECkNavDebugger_HealthStatus::Fail) { return true; }
        }
        return false;
    }

    auto AnyWarn() const -> bool
    {
        for (const auto& Item : Items)
        {
            if (Item.Status == ECkNavDebugger_HealthStatus::Warn) { return true; }
        }
        return false;
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Helpers for human-readable strings (status, fail reason).
// --------------------------------------------------------------------------------------------------------------------

namespace CkNavDebugger
{
    inline auto GetStatusString(ECk_Nav_PathStatus InStatus) -> FString
    {
        switch (InStatus)
        {
            case ECk_Nav_PathStatus::None:    return TEXT("None");
            case ECk_Nav_PathStatus::Pending: return TEXT("Pending");
            case ECk_Nav_PathStatus::Ready:   return TEXT("Ready");
            case ECk_Nav_PathStatus::Failed:  return TEXT("Failed");
            case ECk_Nav_PathStatus::Partial: return TEXT("Partial");
            default:                          return TEXT("Unknown");
        }
    }

    inline auto GetFailReasonString(ECk_Nav_PathFailReason InReason) -> FString
    {
        switch (InReason)
        {
            case ECk_Nav_PathFailReason::None:                return TEXT("None");
            case ECk_Nav_PathFailReason::NoNavSystem:         return TEXT("NoNavSystem");
            case ECk_Nav_PathFailReason::NoNavData:           return TEXT("NoNavData");
            case ECk_Nav_PathFailReason::NoDefaultFilter:     return TEXT("NoDefaultFilter");
            case ECk_Nav_PathFailReason::StartProjectFailed:  return TEXT("StartProjectFailed");
            case ECk_Nav_PathFailReason::EndProjectFailed:    return TEXT("EndProjectFailed");
            case ECk_Nav_PathFailReason::FindPathError:       return TEXT("FindPathError");
            case ECk_Nav_PathFailReason::FindPathNoPath:      return TEXT("FindPathNoPath");
            case ECk_Nav_PathFailReason::FindPathInvalid:     return TEXT("FindPathInvalid");
            case ECk_Nav_PathFailReason::EmptyPath:           return TEXT("EmptyPath");
            case ECk_Nav_PathFailReason::NotAuthority:        return TEXT("NotAuthority");
            case ECk_Nav_PathFailReason::BudgetDisabled:      return TEXT("BudgetDisabled");
            default:                                          return TEXT("Unknown");
        }
    }

    // Hint shown in the UI for a given failure reason, explaining the most likely root cause +
    // a fix. These are the strings that "make a future debugging session take 30 seconds."
    inline auto GetFailReasonHint(ECk_Nav_PathFailReason InReason) -> FString
    {
        switch (InReason)
        {
            case ECk_Nav_PathFailReason::None:
                return TEXT("");
            case ECk_Nav_PathFailReason::NoNavSystem:
                return TEXT("No UNavigationSystemV1 in the world. Check world type (Editor/Game) — only Game/PIE supports nav.");
            case ECk_Nav_PathFailReason::NoNavData:
                return TEXT("No ARecastNavMesh resolved. Likely missing NavMeshBoundsVolume in level OR bAutoCreateNavigationData=false in DefaultEngine.ini.");
            case ECk_Nav_PathFailReason::NoDefaultFilter:
                return TEXT("NavMesh's default query filter is null. The navmesh exists but isn't initialized — try a re-bake.");
            case ECk_Nav_PathFailReason::StartProjectFailed:
                return TEXT("Agent's location is too far from the navmesh surface. Common cause: agent is in mid-air or under the floor; check Z compared to navmesh bounds.");
            case ECk_Nav_PathFailReason::EndProjectFailed:
                return TEXT("Target location is too far from the navmesh. The destination is off-mesh — caller should clamp to a reachable point.");
            case ECk_Nav_PathFailReason::FindPathError:
                return TEXT("Recast returned Error. Most common: NavAgentProperties mismatch with registered agents in DefaultEngine.ini SupportedAgents.");
            case ECk_Nav_PathFailReason::FindPathNoPath:
                return TEXT("No route between start and end polys. Endpoints are on disconnected navmesh islands.");
            case ECk_Nav_PathFailReason::FindPathInvalid:
                return TEXT("Recast returned Invalid — degenerate query (duplicate polys, NaN inputs, etc.).");
            case ECk_Nav_PathFailReason::EmptyPath:
                return TEXT("Path returned with zero waypoints (start ≈ end after de-dup). Caller likely requested a target that resolves to the agent's current poly.");
            case ECk_Nav_PathFailReason::NotAuthority:
                return TEXT("Client-side request was rejected. Nav v1 is server-only; call Request_FindPath only on authoritative entities.");
            case ECk_Nav_PathFailReason::BudgetDisabled:
                return TEXT("MaxPathQueriesPerFrame=0 in CkNavigation project settings. Set it to >= 1 to enable path queries.");
            default:
                return TEXT("");
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
