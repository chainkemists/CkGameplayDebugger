#include "CkNavDebugger/Processor/CkNavDebugger_Processor.h"

#include "CkNavDebugger/Settings/CkNavDebugger_UserSettings.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkNavDebugger/CkNavDebuggerStyle.h"

#include "CkPmg/CkPmg_Fragment_Data.h"
#include "CkPmg/CkPmg_Utils.h"
#include "CkPmg/CkPmg_Utils_BasicShapes.h"
#include "CkPmg/CkPmg_Utils_DebugLines.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_EnsureCapsule_All);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_EnsureCapsule_Selected);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_RemoveCapsule);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_SyncCapsuleTransform);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_RecolorCapsule);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_EnsurePath_All);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_EnsurePath_Selected);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_RemovePath);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_RebuildPathOnDirty);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_RecolorPath);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_EnsureProjections);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_RemoveProjections);
CK_REGISTER_PROCESSOR(ck::FProcessor_NavDebugger_RebuildProjectionsOnDirty);

// --------------------------------------------------------------------------------------------------------------------

namespace ck::nav_debugger_internal
{
    constexpr int32 CapsuleSegments      = 16;
    constexpr int32 CapsuleRings         = 8;
    constexpr float CapsuleLineThickness = 1.5f;
    constexpr float PathLineThickness    = 2.5f;

    constexpr float ProjectionLineThickness  = 1.5f;
    constexpr float ProjectionMarkerRadius   = 12.0f;
    constexpr int32 ProjectionMarkerSegments = 12;
    constexpr int32 ProjectionMarkerRings    = 12;

    auto Get_CapsuleColor(bool InIsHighlight) -> FLinearColor
    {
        return InIsHighlight
            ? UCk_Utils_NavDebugger_Settings_UE::Get_AgentCapsuleColor_Selected()
            : UCk_Utils_NavDebugger_Settings_UE::Get_AgentCapsuleColor();
    }

    auto Get_PathColor(ECk_Nav_PathStatus InStatus, bool InIsHighlight) -> FLinearColor
    {
        if (InIsHighlight)
        { return UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathColor_Selected(); }

        switch (InStatus)
        {
            case ECk_Nav_PathStatus::Ready:   return UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathColor_Ready();
            case ECk_Nav_PathStatus::Partial: return UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathColor_Partial();
            case ECk_Nav_PathStatus::Failed:  return UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathColor_Failed();
            default:                          return FLinearColor::Gray;
        }
    }

    // Spawns a translucent capsule procmesh entity at the agent's transform.
    // Owned by the agent so cascade-destroy handles agent death; explicit
    // teardown comes from FProcessor_NavDebugger_RemoveCapsule.
    auto Spawn_CapsuleVisual(
        FCk_Handle& InAgentHandle,
        const FFragment_Nav_AgentParams& InAgentParams,
        const FFragment_Transform& InTransform,
        bool InIsHighlight)
        -> FCk_Handle
    {
        const auto AgentLocation = InTransform.Get_Transform().GetLocation();
        const auto Radius        = InAgentParams.Get_Radius();
        const auto HalfHeight    = InAgentParams.Get_Height() * 0.5f;
        const auto Center        = AgentLocation + FVector(0, 0, HalfHeight);

        auto CapsuleEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InAgentHandle);
        if (NOT ck::IsValid(CapsuleEntity))
        { return {}; }

        auto Color = Get_CapsuleColor(InIsHighlight);

        UCk_Utils_Pmg_BasicShapes::Add_Capsule(
            CapsuleEntity,
            FTransform{Center},
            Radius,
            HalfHeight,
            CapsuleSegments,
            CapsuleRings,
            ECk_Plane_Axis::XY,
            Color,
            /*InDrawLines=*/true,
            CapsuleLineThickness,
            /*InDuration=*/-1.0f);

        return CapsuleEntity;
    }

    // True if the projection markers should be drawn for an agent in the
    // current mode. Mirrors the path-mode gating because projections are
    // path diagnostics — they only make sense alongside a path.
    auto Should_ShowProjections(const FCk_Handle& InAgentHandle) -> bool
    {
        if (NOT UCk_Utils_NavDebugger_Settings_UE::Get_DrawProjections())
        { return false; }

        const auto Mode = UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathMode();
        if (Mode == ECk_NavDebugger_DrawMode::None)
        { return false; }

        if (Mode == ECk_NavDebugger_DrawMode::All)
        { return true; }

        // Selected mode
        return InAgentHandle.Has<FTag_NavDebugger_AgentSelected>();
    }

    // Spawns a single child entity that owns BOTH projection lines (agent→
    // projected-start, target→projected-end) AND the marker spheres at the
    // projected points. Returns invalid handle when the agent has no
    // diagnostics yet (very first frame after Add).
    auto Spawn_ProjectionsVisual(
        FCk_Handle& InAgentHandle,
        const FFragment_Nav_PathResult& InPathResult)
        -> FCk_Handle
    {
        const auto& Diag = InPathResult.Get_Diagnostics();
        const auto AnyProjection = Diag.Get_StartProjected() || Diag.Get_EndProjected();
        if (NOT AnyProjection)
        { return {}; }

        auto ProjectionsEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InAgentHandle);
        if (NOT ck::IsValid(ProjectionsEntity))
        { return {}; }

        UCk_Utils_Transform_UE::Add(
            ProjectionsEntity, FTransform::Identity, ECk_Replication::DoesNotReplicate);

        auto MarkerColor = FCkNavDebuggerStyle::Color_Draw_ProjectionOK;
        MarkerColor.A = 0.6f;

        if (Diag.Get_StartProjected())
        {
            ck::pmg::Append_DebugLine_World(
                ProjectionsEntity,
                Diag.Get_LastAgentLocation(),
                Diag.Get_LastProjectedStart(),
                FCkNavDebuggerStyle::Color_Draw_ProjectionOK,
                ProjectionLineThickness);

            UCk_Utils_Pmg_BasicShapes::Create_Sphere(
                ProjectionsEntity,
                FTransform{Diag.Get_LastProjectedStart()},
                ProjectionMarkerRadius,
                ProjectionMarkerSegments,
                ProjectionMarkerRings,
                ECk_Plane_Axis::XY,
                MarkerColor,
                /*InDrawLines=*/true,
                ProjectionLineThickness,
                /*InDuration=*/-1.0f);
        }

        if (Diag.Get_EndProjected())
        {
            ck::pmg::Append_DebugLine_World(
                ProjectionsEntity,
                Diag.Get_LastTargetLocation(),
                Diag.Get_LastProjectedEnd(),
                FCkNavDebuggerStyle::Color_Draw_ProjectionOK,
                ProjectionLineThickness);

            UCk_Utils_Pmg_BasicShapes::Create_Sphere(
                ProjectionsEntity,
                FTransform{Diag.Get_LastProjectedEnd()},
                ProjectionMarkerRadius,
                ProjectionMarkerSegments,
                ProjectionMarkerRings,
                ECk_Plane_Axis::XY,
                MarkerColor,
                /*InDrawLines=*/true,
                ProjectionLineThickness,
                /*InDuration=*/-1.0f);
        }

        return ProjectionsEntity;
    }

    // Spawns a path-line entity at world origin (lines themselves are
    // appended in world space). Owned by the agent for cascade-destroy.
    auto Spawn_PathVisual(
        FCk_Handle& InAgentHandle,
        const FFragment_Nav_PathResult& InPathResult,
        const FFragment_Transform& InAgentTransform,
        bool InIsHighlight)
        -> FCk_Handle
    {
        const auto& Waypoints = InPathResult.Get_Waypoints();
        if (Waypoints.IsEmpty())
        { return {}; }

        auto PathEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InAgentHandle);
        if (NOT ck::IsValid(PathEntity))
        { return {}; }

        UCk_Utils_Transform_UE::Add(
            PathEntity, FTransform::Identity, ECk_Replication::DoesNotReplicate);

        const auto Color = Get_PathColor(InPathResult.Get_Status(), InIsHighlight);
        auto Prev = InAgentTransform.Get_Transform().GetLocation();
        for (const auto& Wp : Waypoints)
        {
            ck::pmg::Append_DebugLine_World(PathEntity, Prev, Wp, Color, PathLineThickness);
            Prev = Wp;
        }

        return PathEntity;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    using namespace nav_debugger_internal;

    // ------------------------------------------------------------------------
    // EnsureCapsule_All / _Selected
    // ------------------------------------------------------------------------

    auto
        FProcessor_NavDebugger_EnsureCapsule_All::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Transform& InTransform) const
        -> void
    {
        if (UCk_Utils_NavDebugger_Settings_UE::Get_AgentCapsuleMode() != ECk_NavDebugger_DrawMode::All)
        { return; }

        const auto IsHighlight = InHandle.Has<FTag_NavDebugger_AgentSelected>();
        auto CapsuleEntity = Spawn_CapsuleVisual(InHandle, InAgentParams, InTransform, IsHighlight);
        if (NOT ck::IsValid(CapsuleEntity))
        { return; }

        auto& Visual = InHandle.AddOrGet<FFragment_NavDebugger_CapsuleVisual>();
        Visual._CapsuleEntity = CapsuleEntity;
        Visual._IsHighlighted = IsHighlight;
    }

    auto
        FProcessor_NavDebugger_EnsureCapsule_Selected::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Transform& InTransform) const
        -> void
    {
        if (UCk_Utils_NavDebugger_Settings_UE::Get_AgentCapsuleMode() != ECk_NavDebugger_DrawMode::Selected)
        { return; }

        // Selected processor implies the agent has the highlight tag — pick the
        // highlight color directly.
        auto CapsuleEntity = Spawn_CapsuleVisual(InHandle, InAgentParams, InTransform, /*InIsHighlight=*/true);
        if (NOT ck::IsValid(CapsuleEntity))
        { return; }

        auto& Visual = InHandle.AddOrGet<FFragment_NavDebugger_CapsuleVisual>();
        Visual._CapsuleEntity = CapsuleEntity;
        Visual._IsHighlighted = true;
    }

    // ------------------------------------------------------------------------
    // RemoveCapsule
    // ------------------------------------------------------------------------

    auto
        FProcessor_NavDebugger_RemoveCapsule::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_NavDebugger_CapsuleVisual& InVisual) const
        -> void
    {
        const auto Mode = UCk_Utils_NavDebugger_Settings_UE::Get_AgentCapsuleMode();
        const auto HasSelectionTag = InHandle.Has<FTag_NavDebugger_AgentSelected>();

        const auto ShouldHaveCapsule =
            (Mode == ECk_NavDebugger_DrawMode::All) ||
            (Mode == ECk_NavDebugger_DrawMode::Selected && HasSelectionTag);

        if (ShouldHaveCapsule)
        { return; }

        if (ck::IsValid(InVisual._CapsuleEntity))
        {
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InVisual._CapsuleEntity);
        }

        // Removing the fragment lets the matching EnsureCapsule_X processor
        // re-spawn the visual cleanly the next frame the mode (or selection)
        // says it should exist again.
        InHandle.Try_Remove<FFragment_NavDebugger_CapsuleVisual>();
    }

    // ------------------------------------------------------------------------
    // SyncCapsuleTransform
    // ------------------------------------------------------------------------

    auto
        FProcessor_NavDebugger_SyncCapsuleTransform::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Transform& InTransform,
            const FFragment_NavDebugger_CapsuleVisual& InVisual) const
        -> void
    {
        if (NOT ck::IsValid(InVisual._CapsuleEntity))
        { return; }

        // Capsule sits centered on agent X/Y at agent.Z + halfHeight so the
        // base rests on the navmesh surface (matches how Setup placed it).
        const auto AgentLocation = InTransform.Get_Transform().GetLocation();
        const auto HalfHeight    = InAgentParams.Get_Height() * 0.5f;
        const auto Center        = AgentLocation + FVector(0, 0, HalfHeight);

        auto CapsuleEntity = InVisual._CapsuleEntity;
        auto CapsuleTransformH = UCk_Utils_Transform_UE::Cast(CapsuleEntity);
        if (NOT ck::IsValid(CapsuleTransformH))
        { return; }

        UCk_Utils_Transform_UE::Request_SetTransform(
            CapsuleTransformH,
            FCk_Request_Transform_SetTransform{FTransform{Center}});
    }

    // ------------------------------------------------------------------------
    // RecolorCapsule
    // ------------------------------------------------------------------------

    auto
        FProcessor_NavDebugger_RecolorCapsule::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_NavDebugger_CapsuleVisual& InVisual) const
        -> void
    {
        const auto ShouldBeHighlighted = InHandle.Has<FTag_NavDebugger_AgentSelected>();
        if (ShouldBeHighlighted == InVisual._IsHighlighted)
        { return; }

        if (NOT ck::IsValid(InVisual._CapsuleEntity))
        { return; }

        auto CapsuleH = UCk_Utils_Pmg_DebugShape_UE::Cast(InVisual._CapsuleEntity);
        if (NOT ck::IsValid(CapsuleH))
        { return; }

        const auto NewColor = Get_CapsuleColor(ShouldBeHighlighted);
        UCk_Utils_Pmg_DebugShape_UE::Request_SetColor(
            CapsuleH,
            FCk_Request_Pmg_DebugShape_SetColor{NewColor});

        InVisual._IsHighlighted = ShouldBeHighlighted;
    }

    // ------------------------------------------------------------------------
    // EnsurePath_All / _Selected
    // ------------------------------------------------------------------------

    auto
        FProcessor_NavDebugger_EnsurePath_All::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Nav_PathResult& InPathResult) const
        -> void
    {
        if (UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathMode() != ECk_NavDebugger_DrawMode::All)
        { return; }

        // Path needs both PathResult AND the agent's transform (used as the
        // first segment's start point). Bail if either is missing.
        if (NOT InHandle.Has<FFragment_Transform>())
        { return; }
        const auto& Transform = InHandle.Get<FFragment_Transform>();

        const auto IsHighlight = InHandle.Has<FTag_NavDebugger_AgentSelected>();
        auto PathEntity = Spawn_PathVisual(InHandle, InPathResult, Transform, IsHighlight);

        // Always add the fragment so RemovePath / RebuildPath / RecolorPath
        // pick up this agent — even if the spawn produced no entity (empty
        // waypoints). Subsequent path queries can then trigger a rebuild via
        // the dirty-detect processor without us re-iterating all agents.
        auto& Visual = InHandle.AddOrGet<FFragment_NavDebugger_PathVisual>();
        Visual._PathEntity              = PathEntity;
        Visual._IsHighlighted           = IsHighlight;
        Visual._LastPathQueryWallTime   = InPathResult.Get_Diagnostics().Get_LastQueryWallTime();
        Visual._LastWaypointCount       = InPathResult.Get_Waypoints().Num();
    }

    auto
        FProcessor_NavDebugger_EnsurePath_Selected::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Nav_PathResult& InPathResult) const
        -> void
    {
        if (UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathMode() != ECk_NavDebugger_DrawMode::Selected)
        { return; }

        if (NOT InHandle.Has<FFragment_Transform>())
        { return; }
        const auto& Transform = InHandle.Get<FFragment_Transform>();

        auto PathEntity = Spawn_PathVisual(InHandle, InPathResult, Transform, /*InIsHighlight=*/true);

        auto& Visual = InHandle.AddOrGet<FFragment_NavDebugger_PathVisual>();
        Visual._PathEntity              = PathEntity;
        Visual._IsHighlighted           = true;
        Visual._LastPathQueryWallTime   = InPathResult.Get_Diagnostics().Get_LastQueryWallTime();
        Visual._LastWaypointCount       = InPathResult.Get_Waypoints().Num();
    }

    // ------------------------------------------------------------------------
    // RemovePath
    // ------------------------------------------------------------------------

    auto
        FProcessor_NavDebugger_RemovePath::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_NavDebugger_PathVisual& InVisual) const
        -> void
    {
        const auto Mode = UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathMode();
        const auto HasSelectionTag = InHandle.Has<FTag_NavDebugger_AgentSelected>();

        const auto ShouldHavePath =
            (Mode == ECk_NavDebugger_DrawMode::All) ||
            (Mode == ECk_NavDebugger_DrawMode::Selected && HasSelectionTag);

        if (ShouldHavePath)
        { return; }

        if (ck::IsValid(InVisual._PathEntity))
        {
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InVisual._PathEntity);
        }

        InHandle.Try_Remove<FFragment_NavDebugger_PathVisual>();
    }

    // ------------------------------------------------------------------------
    // RebuildPathOnDirty
    // ------------------------------------------------------------------------

    auto
        FProcessor_NavDebugger_RebuildPathOnDirty::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Nav_PathResult& InPathResult,
            FFragment_NavDebugger_PathVisual& InVisual) const
        -> void
    {
        const auto NewWallTime  = InPathResult.Get_Diagnostics().Get_LastQueryWallTime();
        const auto NewWaypoints = InPathResult.Get_Waypoints().Num();

        if (NewWallTime == InVisual._LastPathQueryWallTime &&
            NewWaypoints == InVisual._LastWaypointCount)
        { return; }

        // Path snapshot changed — destroy old entity and spawn a fresh one
        // with the new waypoints.
        if (ck::IsValid(InVisual._PathEntity))
        {
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InVisual._PathEntity);
            InVisual._PathEntity = FCk_Handle{};
        }

        if (NOT InHandle.Has<FFragment_Transform>())
        {
            InVisual._LastPathQueryWallTime = NewWallTime;
            InVisual._LastWaypointCount     = NewWaypoints;
            return;
        }

        const auto& Transform = InHandle.Get<FFragment_Transform>();
        InVisual._PathEntity = Spawn_PathVisual(InHandle, InPathResult, Transform, InVisual._IsHighlighted);
        InVisual._LastPathQueryWallTime = NewWallTime;
        InVisual._LastWaypointCount     = NewWaypoints;
    }

    // ------------------------------------------------------------------------
    // RecolorPath
    // ------------------------------------------------------------------------

    auto
        FProcessor_NavDebugger_RecolorPath::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_NavDebugger_PathVisual& InVisual) const
        -> void
    {
        const auto ShouldBeHighlighted = InHandle.Has<FTag_NavDebugger_AgentSelected>();
        if (ShouldBeHighlighted == InVisual._IsHighlighted)
        { return; }

        // Path lines aren't colored via material params (they're emitted as
        // raw debug-line segments each tick by FProcessor_Pmg_DebugShape_DrawLines).
        // The cleanest way to recolor is to mark the visual dirty so
        // RebuildPathOnDirty re-spawns the line entity with the new color.
        // We do that by zeroing the snapshot identity — the next dirty pass
        // sees the mismatch and rebuilds.
        InVisual._IsHighlighted = ShouldBeHighlighted;
        InVisual._LastWaypointCount = -1;  // forces RebuildPathOnDirty to fire next tick
    }

    // ------------------------------------------------------------------------
    // EnsureProjections / RemoveProjections / RebuildProjectionsOnDirty
    // ------------------------------------------------------------------------

    auto
        FProcessor_NavDebugger_EnsureProjections::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Nav_PathResult& InPathResult) const
        -> void
    {
        if (NOT Should_ShowProjections(InHandle))
        { return; }

        auto ProjectionsEntity = Spawn_ProjectionsVisual(InHandle, InPathResult);

        // Always add the fragment so RemoveProjections / RebuildProjectionsOnDirty
        // can pick this agent up. A no-projection-yet spawn (empty diagnostics)
        // becomes a real visual on the next path query via the rebuild
        // processor without us having to keep iterating the full agent set.
        auto& Visual = InHandle.AddOrGet<FFragment_NavDebugger_ProjectionsVisual>();
        Visual._ProjectionsEntity        = ProjectionsEntity;
        Visual._LastPathQueryWallTime    = InPathResult.Get_Diagnostics().Get_LastQueryWallTime();
    }

    auto
        FProcessor_NavDebugger_RemoveProjections::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_NavDebugger_ProjectionsVisual& InVisual) const
        -> void
    {
        if (Should_ShowProjections(InHandle))
        { return; }

        if (ck::IsValid(InVisual._ProjectionsEntity))
        {
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InVisual._ProjectionsEntity);
        }

        InHandle.Try_Remove<FFragment_NavDebugger_ProjectionsVisual>();
    }

    auto
        FProcessor_NavDebugger_RebuildProjectionsOnDirty::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Nav_PathResult& InPathResult,
            FFragment_NavDebugger_ProjectionsVisual& InVisual) const
        -> void
    {
        const auto NewWallTime = InPathResult.Get_Diagnostics().Get_LastQueryWallTime();
        if (NewWallTime == InVisual._LastPathQueryWallTime)
        { return; }

        // New path query — rebuild the projection markers from the fresh
        // diagnostics. Cheaper than tracking individual diag fields.
        if (ck::IsValid(InVisual._ProjectionsEntity))
        {
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InVisual._ProjectionsEntity);
            InVisual._ProjectionsEntity = FCk_Handle{};
        }

        InVisual._ProjectionsEntity     = Spawn_ProjectionsVisual(InHandle, InPathResult);
        InVisual._LastPathQueryWallTime = NewWallTime;
    }
}
