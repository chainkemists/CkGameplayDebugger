#include "CkNavDebugger/Data/CkNavDebugger_HealthCheck.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkNavigation/Nav/CkNav_Algorithm.h"
#include "CkNavigation/Settings/CkNav_ProjectSettings.h"

#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    auto MakeItem(
        const TCHAR* Name,
        ECkNavDebugger_HealthStatus Status,
        FString Detail) -> FCkNavDebugger_HealthCheckItem
    {
        auto Item = FCkNavDebugger_HealthCheckItem{};
        Item.Name   = Name;
        Item.Status = Status;
        Item.Detail = MoveTemp(Detail);
        return Item;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkNavDebugger_HealthCheck::
    Run(
        UWorld* InWorld)
    -> FCkNavDebugger_HealthCheckReport
{
    auto Report = FCkNavDebugger_HealthCheckReport{};
    Report.WallTime = FPlatformTime::Seconds();

    // 1. World present
    if (NOT IsValid(InWorld))
    {
        Report.Items.Add(MakeItem(TEXT("World"), ECkNavDebugger_HealthStatus::Fail,
            TEXT("No UWorld — debugger ticked outside a valid world.")));
        return Report;
    }
    Report.Items.Add(MakeItem(TEXT("World"), ECkNavDebugger_HealthStatus::Pass,
        FString::Printf(TEXT("Type=%d, Name=%s"),
            static_cast<int32>(InWorld->WorldType.GetValue()), *InWorld->GetName())));

    // 2. UNavigationSystemV1 present
    auto* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(InWorld);
    if (NOT ck::IsValid(NavSys, ck::IsValid_Policy_NullptrOnly{}))
    {
        Report.Items.Add(MakeItem(TEXT("Nav System"), ECkNavDebugger_HealthStatus::Fail,
            TEXT("No UNavigationSystemV1 — most likely an editor-only world. Path queries cannot run.")));
        return Report;
    }
    Report.Items.Add(MakeItem(TEXT("Nav System"), ECkNavDebugger_HealthStatus::Pass,
        TEXT("UNavigationSystemV1 resolved.")));

    // 3. Auto-create + Supported agents (a common-mistake trap from the last session).
    // bAutoCreateNavigationData is protected on UNavigationSystemV1, so read from GConfig.
    auto AutoCreate = false;
    GConfig->GetBool(TEXT("/Script/Engine.NavigationSystemV1"),
        TEXT("bAutoCreateNavigationData"), AutoCreate, GEngineIni);
    if (NOT AutoCreate)
    {
        Report.Items.Add(MakeItem(TEXT("bAutoCreateNavigationData"), ECkNavDebugger_HealthStatus::Warn,
            TEXT("Disabled. NavData won't auto-spawn unless a NavMeshBoundsVolume is in the level.")));
    }
    else
    {
        Report.Items.Add(MakeItem(TEXT("bAutoCreateNavigationData"), ECkNavDebugger_HealthStatus::Pass,
            TEXT("Enabled.")));
    }

    const auto SupportedAgents = NavSys->GetSupportedAgents();
    if (SupportedAgents.Num() == 0)
    {
        Report.Items.Add(MakeItem(TEXT("SupportedAgents"), ECkNavDebugger_HealthStatus::Fail,
            TEXT("No SupportedAgents in DefaultEngine.ini [/Script/Engine.NavigationSystemV1]. NavData will never spawn.")));
    }
    else
    {
        Report.Items.Add(MakeItem(TEXT("SupportedAgents"), ECkNavDebugger_HealthStatus::Pass,
            FString::Printf(TEXT("%d configured."), SupportedAgents.Num())));
    }

    // 4. Default NavData resolved
    auto* NavData = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
    if (NOT ck::IsValid(NavData, ck::IsValid_Policy_NullptrOnly{}))
    {
        Report.Items.Add(MakeItem(TEXT("Default NavData"), ECkNavDebugger_HealthStatus::Fail,
            TEXT("No ARecastNavMesh resolved. Place a NavMeshBoundsVolume + bake (or wait for runtime auto-create).")));
        return Report;
    }
    Report.Items.Add(MakeItem(TEXT("Default NavData"), ECkNavDebugger_HealthStatus::Pass,
        FString::Printf(TEXT("%s"), *NavData->GetName())));

    // 5. Multi-navmesh detection (Pass-3 P3-5 — we use only Default)
    if (NavSys->NavDataSet.Num() > 1)
    {
        Report.Items.Add(MakeItem(TEXT("NavDataSet count"), ECkNavDebugger_HealthStatus::Warn,
            FString::Printf(TEXT("%d navmeshes found. CkNavigation v1 uses only Default — others are ignored."),
                NavSys->NavDataSet.Num())));
    }

    // 6. NavMesh bounds non-zero
    // A flat-floor navmesh legitimately reports zero Z extent (all polys at one height) —
    // GetNavMeshBounds() collapses to a thin shell. Use 2D footprint (X*Y) as the
    // "is there any walkable surface" signal instead of full 3D volume; otherwise we
    // false-positive on every flat gym/test level.
    const auto Bounds = NavData->GetNavMeshBounds();
    const auto BoundsValid    = Bounds.IsValid != 0;
    const auto BoundsExtent   = BoundsValid ? Bounds.GetExtent() : FVector::ZeroVector;
    const auto FootprintArea  = BoundsExtent.X * BoundsExtent.Y * 4.0;   // (2*Ex)*(2*Ey)
    const auto HasFootprint   = BoundsValid && FootprintArea > KINDA_SMALL_NUMBER;

    if (NOT BoundsValid)
    {
        Report.Items.Add(MakeItem(TEXT("NavMesh bounds"), ECkNavDebugger_HealthStatus::Fail,
            TEXT("Bounds invalid (zero). Navmesh has no tiles — place a NavMeshBoundsVolume + bake.")));
    }
    else if (NOT HasFootprint)
    {
        Report.Items.Add(MakeItem(TEXT("NavMesh bounds"), ECkNavDebugger_HealthStatus::Warn,
            FString::Printf(TEXT("Bounds have zero footprint (%s → %s). Navmesh likely empty."),
                *Bounds.Min.ToString(), *Bounds.Max.ToString())));
    }
    else
    {
        Report.Items.Add(MakeItem(TEXT("NavMesh bounds"), ECkNavDebugger_HealthStatus::Pass,
            FString::Printf(TEXT("%s → %s (footprint %.0f cm²)"),
                *Bounds.Min.ToString(), *Bounds.Max.ToString(), FootprintArea)));
    }

    // 7. Default query filter present
    if (NOT NavData->GetDefaultQueryFilter().IsValid())
    {
        Report.Items.Add(MakeItem(TEXT("Default query filter"), ECkNavDebugger_HealthStatus::Fail,
            TEXT("Null DefaultQueryFilter. NavData isn't initialized — every FindPath returns Error.")));
    }
    else
    {
        Report.Items.Add(MakeItem(TEXT("Default query filter"), ECkNavDebugger_HealthStatus::Pass,
            TEXT("Present.")));
    }

    // 8. Project settings — budget cap
    const auto BudgetCap = UCk_Utils_Nav_ProjectSettings::Get_MaxPathQueriesPerFrame();
    if (BudgetCap == 0)
    {
        Report.Items.Add(MakeItem(TEXT("MaxPathQueriesPerFrame"), ECkNavDebugger_HealthStatus::Fail,
            TEXT("Set to 0 — all path queries silently fail with BudgetDisabled. Set to >= 1 in CkNavigation project settings.")));
    }
    else if (BudgetCap < 4)
    {
        Report.Items.Add(MakeItem(TEXT("MaxPathQueriesPerFrame"), ECkNavDebugger_HealthStatus::Warn,
            FString::Printf(TEXT("%d — low budget; large queues may starve."), BudgetCap)));
    }
    else
    {
        Report.Items.Add(MakeItem(TEXT("MaxPathQueriesPerFrame"), ECkNavDebugger_HealthStatus::Pass,
            FString::Printf(TEXT("%d"), BudgetCap)));
    }

    // 9. Synthetic FindPath at the navmesh center (only run if everything above passed)
    if (HasFootprint && NavData->GetDefaultQueryFilter().IsValid())
    {
        const auto Center = Bounds.GetCenter();
        const auto Offset = FVector{Bounds.GetExtent().X * 0.4, 0.0, 0.0};
        const auto Start  = Center - Offset;
        const auto End    = Center + Offset;

        auto SyntheticParams = FCk_Nav_AgentParams{NavData->GetConfig().AgentRadius, NavData->GetConfig().AgentHeight};
        auto SyntheticResult = FCk_Nav_PathResult{};

        const auto bOk = FCk_Nav_Algorithm::FindPathSync(
            *NavSys, *NavData, Start, End, SyntheticParams, /*AllowPartial*/ true, SyntheticResult);

        if (bOk)
        {
            Report.Items.Add(MakeItem(TEXT("Synthetic FindPath"), ECkNavDebugger_HealthStatus::Pass,
                FString::Printf(TEXT("OK — %d waypoints, %.2fms"),
                    SyntheticResult.Get_Waypoints().Num(),
                    SyntheticResult.Get_Diagnostics().Get_LastQueryDurationMs())));
        }
        else
        {
            Report.Items.Add(MakeItem(TEXT("Synthetic FindPath"), ECkNavDebugger_HealthStatus::Fail,
                FString::Printf(TEXT("FAILED at navmesh center — reason: %s. Stack-level setup looks OK; navmesh likely has no walkable tiles."),
                    *CkNavDebugger::GetFailReasonString(SyntheticResult.Get_Diagnostics().Get_LastFailReason()))));
        }
    }

    return Report;
}

// --------------------------------------------------------------------------------------------------------------------
