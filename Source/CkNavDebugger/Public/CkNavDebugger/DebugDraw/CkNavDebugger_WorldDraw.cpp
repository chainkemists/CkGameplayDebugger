#include "CkNavDebugger/DebugDraw/CkNavDebugger_WorldDraw.h"

#include "CkNavDebugger/CkNavDebuggerStyle.h"
#include "CkNavDebugger/Data/CkNavDebugger_DataCollector.h"
#include "CkNavDebugger/Data/CkNavDebugger_Types.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle.h"

#include "CkEcsExt/Transform/CkTransform_Fragment_Data.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkPmg/CkPmg_Utils_BasicShapes.h"
#include "CkPmg/CkPmg_Utils_DebugLines.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    // Two-entity model:
    //   AgentEntity — transform follows the agent each tick. Holds capsule + projection
    //                 markers anchored relative to the agent.
    //   PathEntity  — transform = world origin. Holds path-line geometry. Rebuilt only
    //                 when path data changes (selection, new path query).
    //
    // PMG appends inverse-transform incoming world points into entity-local space — so
    // moving the entity moves all of its geometry as a rigid unit. That's why path lines
    // (which are world-anchored) live on a separate, never-moved entity.
    struct FNavDebugEntities
    {
        TWeakObjectPtr<UWorld> World;

        FCk_Handle           AgentEntity;
        FCk_Handle_Transform AgentTransform;

        FCk_Handle           PathEntity;

        // Selection identity — wipe both entities when this changes.
        int32  LastSelectedHash = -1;

        // Path snapshot identity — rebuild PathEntity when this changes.
        double LastQueryWallTime = 0.0;
        int32  LastWaypointCount = -1;
    };

    static FNavDebugEntities GState;

    auto GetCVarInt(const TCHAR* InName) -> int32
    {
        const auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName);
        return CVar != nullptr ? CVar->GetInt() : 0;
    }

    auto GetStatusColor(ECk_Nav_PathStatus InStatus) -> FLinearColor
    {
        switch (InStatus)
        {
            case ECk_Nav_PathStatus::Ready:   return FCkNavDebuggerStyle::Color_Draw_PathReady;
            case ECk_Nav_PathStatus::Partial: return FCkNavDebuggerStyle::Color_Draw_PathPartial;
            case ECk_Nav_PathStatus::Failed:  return FCkNavDebuggerStyle::Color_Draw_PathFailed;
            default:                          return FLinearColor::Gray;
        }
    }

    auto DestroyAgentEntity() -> void
    {
        if (ck::IsValid(GState.AgentEntity))
        {
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(GState.AgentEntity);
        }
        GState.AgentEntity    = FCk_Handle{};
        GState.AgentTransform = FCk_Handle_Transform{};
    }

    auto DestroyPathEntity() -> void
    {
        if (ck::IsValid(GState.PathEntity))
        {
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(GState.PathEntity);
        }
        GState.PathEntity = FCk_Handle{};
        GState.LastQueryWallTime = 0.0;
        GState.LastWaypointCount = -1;
    }

    auto DestroyAll() -> void
    {
        DestroyAgentEntity();
        DestroyPathEntity();
        GState.LastSelectedHash = -1;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    static bool GIsWindowOpen = false;
}

auto
    FCkNavDebugger_WorldDraw::
    Set_WindowOpen(
        bool InOpen)
    -> void
{
    GIsWindowOpen = InOpen;
    if (NOT InOpen)
    {
        // Tear down any in-world geometry the moment the window closes — even if the
        // OverlayAlwaysOn cvar is set, this is a clean reset point.
        DestroyAll();
    }
}

auto
    FCkNavDebugger_WorldDraw::
    Get_WindowOpen()
    -> bool
{
    return GIsWindowOpen;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkNavDebugger_WorldDraw::
    DrawAll(
        UWorld* InWorld)
    -> void
{
    if (NOT IsValid(InWorld))
    { return; }

    // Only PIE / Game worlds host nav agents.
    if (InWorld->WorldType != EWorldType::PIE
        && InWorld->WorldType != EWorldType::Game)
    { return; }

    // World identity changed (PIE stop/start) — wipe stale entity handles from the
    // previous world before doing anything else.
    if (GState.World.Get() != InWorld)
    {
        GState.World = InWorld;
        // We can't safely destroy entities owned by a defunct world; reset references and
        // let the lifetime system clean them up with the world.
        GState.AgentEntity      = FCk_Handle{};
        GState.AgentTransform   = FCk_Handle_Transform{};
        GState.PathEntity       = FCk_Handle{};
        GState.LastSelectedHash = -1;
        GState.LastQueryWallTime = 0.0;
        GState.LastWaypointCount = -1;
    }

    // Gate: opted in (debugger window open or OverlayAlwaysOn cvar) AND have a selection.
    const auto AlwaysOn = GetCVarInt(TEXT("Ck.NavDebugger.OverlayAlwaysOn")) != 0;
    if (NOT (AlwaysOn || GIsWindowOpen))
    { DestroyAll(); return; }

    const auto SelectedId = GetCVarInt(TEXT("Ck.NavDebugger.SelectedEntityId"));
    if (SelectedId < 0)
    { DestroyAll(); return; }

    // Resolve selected agent.
    auto Collector = FCkNavDebugger_DataCollector{};
    Collector.Collect(InWorld);

    const FCkNavDebugger_AgentInfo* SelectedAgent = nullptr;
    for (const auto& Agent : Collector.Get_AllAgents())
    {
        if (static_cast<int32>(GetTypeHash(Agent.EntityHandle)) == SelectedId)
        { SelectedAgent = &Agent; break; }
    }

    if (SelectedAgent == nullptr)
    { DestroyAll(); return; }

    const auto& Agent = *SelectedAgent;

    // Selection changed → wipe both entities so they're rebuilt under the new agent.
    if (GState.LastSelectedHash != SelectedId)
    {
        DestroyAgentEntity();
        DestroyPathEntity();
        GState.LastSelectedHash = SelectedId;
    }

    const auto DrawPaths       = GetCVarInt(TEXT("Ck.NavDebugger.DrawPaths"))       != 0;
    const auto DrawProjections = GetCVarInt(TEXT("Ck.NavDebugger.DrawProjections")) != 0;
    const auto DrawCapsules    = GetCVarInt(TEXT("Ck.NavDebugger.DrawAgentCapsules")) != 0;

    // ------------------------------------------------------------------------------------
    // AgentEntity — filled PMG capsule (with auto wireframe) centered at agent origin.
    // Add_Capsule does Common + Capsule_Params + Current + NeedsSetup tag + Transform in
    // one call; the Setup processor builds the procmesh AND draws the wireframe overlay
    // (because InDrawLines=true). Per tick we update the entity transform to track the
    // agent — the procmesh transform follows via FProcessor_Pmg_DebugShape_UpdateTransform.
    // ------------------------------------------------------------------------------------

    if (DrawCapsules)
    {
        const auto CapsuleHalfHeight = Agent.AgentParams.Get_Height() * 0.5f;
        const auto CapsuleRadius     = Agent.AgentParams.Get_Radius();
        const auto CapsuleCenter     = Agent.AgentLocation + FVector(0, 0, CapsuleHalfHeight);

        if (NOT ck::IsValid(GState.AgentEntity))
        {
            GState.AgentEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity_TransientOwner(InWorld);

            if (ck::IsValid(GState.AgentEntity))
            {
                constexpr auto Segments      = 16;
                constexpr auto Rings         = 8;
                constexpr auto LineThickness = 1.5f;

                auto FilledColor = FCkNavDebuggerStyle::Color_Draw_AgentCapsule;
                FilledColor.A = 0.35f;  // translucent so the agent inside remains visible

                UCk_Utils_Pmg_BasicShapes::Add_Capsule(
                    GState.AgentEntity,
                    FTransform{CapsuleCenter},
                    CapsuleRadius,
                    CapsuleHalfHeight,
                    Segments,
                    Rings,
                    ECk_Plane_Axis::XY,
                    FilledColor,
                    /*InDrawLines=*/true,
                    LineThickness,
                    /*InDuration=*/-1.0f);

                GState.AgentTransform = UCk_Utils_Transform_UE::Cast(GState.AgentEntity);
            }
        }
        else if (ck::IsValid(GState.AgentTransform))
        {
            UCk_Utils_Transform_UE::Request_SetTransform(
                GState.AgentTransform,
                FCk_Request_Transform_SetTransform{FTransform{CapsuleCenter}});
        }
    }
    else
    {
        DestroyAgentEntity();
    }

    // ------------------------------------------------------------------------------------
    // PathEntity — path lines + projection markers. World-anchored; entity stays at origin.
    // Rebuilt only when the path snapshot identity changes.
    // ------------------------------------------------------------------------------------

    const auto QueryTime  = Agent.Diagnostics.Get_LastQueryWallTime();
    const auto WaypointN  = Agent.Waypoints.Num();
    const auto PathDirty  = GState.LastQueryWallTime != QueryTime
                         || GState.LastWaypointCount != WaypointN;

    const auto WantsPathOverlay = DrawPaths || DrawProjections;

    if (NOT WantsPathOverlay)
    {
        DestroyPathEntity();
    }
    else if (PathDirty)
    {
        DestroyPathEntity();

        GState.PathEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity_TransientOwner(InWorld);

        if (ck::IsValid(GState.PathEntity))
        {
            UCk_Utils_Transform_UE::Add(
                GState.PathEntity,
                FTransform::Identity,
                ECk_Replication::DoesNotReplicate);

            constexpr auto LineThickness = 2.5f;
            const auto StatusColor       = GetStatusColor(Agent.PathStatus);

            if (DrawPaths && WaypointN > 0)
            {
                auto Prev = Agent.AgentLocation;
                for (auto i = 0; i < WaypointN; ++i)
                {
                    const auto& Wp = Agent.Waypoints[i];
                    ck::pmg::Append_DebugLine_World(
                        GState.PathEntity, Prev, Wp, StatusColor, LineThickness);
                    Prev = Wp;
                }
            }

            if (DrawProjections)
            {
                constexpr auto ProjectionMarkerThickness = 1.5f;
                constexpr auto MarkerRadius              = 12.0f;
                constexpr auto MarkerSegments            = 12;
                constexpr auto MarkerRings               = 12;

                auto MarkerColor = FCkNavDebuggerStyle::Color_Draw_ProjectionOK;
                MarkerColor.A = 0.6f;

                // Drop line + filled sphere marker at each projection point.
                if (Agent.Diagnostics.Get_StartProjected())
                {
                    ck::pmg::Append_DebugLine_World(
                        GState.PathEntity,
                        Agent.Diagnostics.Get_LastAgentLocation(),
                        Agent.Diagnostics.Get_LastProjectedStart(),
                        FCkNavDebuggerStyle::Color_Draw_ProjectionOK,
                        ProjectionMarkerThickness);

                    UCk_Utils_Pmg_BasicShapes::Create_Sphere(
                        GState.PathEntity,
                        FTransform{Agent.Diagnostics.Get_LastProjectedStart()},
                        MarkerRadius,
                        MarkerSegments,
                        MarkerRings,
                        ECk_Plane_Axis::XY,
                        MarkerColor,
                        /*InDrawLines=*/true,
                        ProjectionMarkerThickness,
                        /*InDuration=*/-1.0f);
                }

                if (Agent.Diagnostics.Get_EndProjected())
                {
                    ck::pmg::Append_DebugLine_World(
                        GState.PathEntity,
                        Agent.Diagnostics.Get_LastTargetLocation(),
                        Agent.Diagnostics.Get_LastProjectedEnd(),
                        FCkNavDebuggerStyle::Color_Draw_ProjectionOK,
                        ProjectionMarkerThickness);

                    UCk_Utils_Pmg_BasicShapes::Create_Sphere(
                        GState.PathEntity,
                        FTransform{Agent.Diagnostics.Get_LastProjectedEnd()},
                        MarkerRadius,
                        MarkerSegments,
                        MarkerRings,
                        ECk_Plane_Axis::XY,
                        MarkerColor,
                        /*InDrawLines=*/true,
                        ProjectionMarkerThickness,
                        /*InDuration=*/-1.0f);
                }
            }
        }

        GState.LastQueryWallTime = QueryTime;
        GState.LastWaypointCount = WaypointN;
    }
}

// --------------------------------------------------------------------------------------------------------------------
