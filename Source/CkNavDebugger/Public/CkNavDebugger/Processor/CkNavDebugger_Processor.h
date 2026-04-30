#pragma once

#include "CkNavDebugger/Fragment/CkNavDebugger_Fragment.h"

#include "CkNavigation/Nav/CkNav_Fragment.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"

// --------------------------------------------------------------------------------------------------------------------
// CkNavDebugger processors — ECS-driven replacement for the old per-tick
// FCkNavDebugger_WorldDraw delegate.
//
// Architecture:
//   Selection state    — single FTag_NavDebugger_AgentSelected on at most one
//                        agent (managed by the panel-click wiring).
//   Capsule state      — FFragment_NavDebugger_CapsuleVisual on agents that
//                        currently have a debug capsule. Spawn/teardown is
//                        purely a function of (mode, AgentSelected tag).
//   Path state         — FFragment_NavDebugger_PathVisual likewise, with
//                        wall-time + waypoint-count snapshots used to dirty-
//                        rebuild lines only when a new path lands.
//
// Per visual type there are five processors:
//   EnsureX_All        — runs only when the relevant mode is All;
//                        spawns the visual for every agent that lacks it.
//   EnsureX_Selected   — runs only when the relevant mode is Selected;
//                        spawns the visual only for the AgentSelected agent.
//   RemoveX            — destroys visuals whose owning agent no longer
//                        satisfies the active mode (mode flipped to None,
//                        moved to Selected and the agent isn't selected, etc).
//   SyncX              — pushes per-tick state into the visual entity:
//                        Capsule = agent transform; Path doesn't need this
//                        because lines live on a static entity.
//   RecolorX           — when the AgentSelected presence disagrees with the
//                        cached _IsHighlighted flag, enqueues a Pmg SetColor
//                        request and updates the flag.
// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    // --------------------------------------------------------------------------------------------------------------------
    // Capsule visual
    // --------------------------------------------------------------------------------------------------------------------

    class CKNAVDEBUGGER_API FProcessor_NavDebugger_EnsureCapsule_All : public ck_exp::TProcessor<
            FProcessor_NavDebugger_EnsureCapsule_All,
            FCk_Handle,
            ck::TReadOnly<FFragment_Nav_AgentParams>,
            ck::TReadOnly<FFragment_Transform>,
            TExclude<FFragment_NavDebugger_CapsuleVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Transform& InTransform) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKNAVDEBUGGER_API FProcessor_NavDebugger_EnsureCapsule_Selected : public ck_exp::TProcessor<
            FProcessor_NavDebugger_EnsureCapsule_Selected,
            FCk_Handle,
            ck::TReadOnly<FFragment_Nav_AgentParams>,
            ck::TReadOnly<FFragment_Transform>,
            FTag_NavDebugger_AgentSelected,
            TExclude<FFragment_NavDebugger_CapsuleVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Transform& InTransform) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKNAVDEBUGGER_API FProcessor_NavDebugger_RemoveCapsule : public ck_exp::TProcessor<
            FProcessor_NavDebugger_RemoveCapsule,
            FCk_Handle,
            ck::TReadWrite<FFragment_NavDebugger_CapsuleVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_NavDebugger_CapsuleVisual& InVisual) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKNAVDEBUGGER_API FProcessor_NavDebugger_SyncCapsuleTransform : public ck_exp::TProcessor<
            FProcessor_NavDebugger_SyncCapsuleTransform,
            FCk_Handle,
            ck::TReadOnly<FFragment_Nav_AgentParams>,
            ck::TReadOnly<FFragment_Transform>,
            ck::TReadOnly<FFragment_NavDebugger_CapsuleVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Transform& InTransform,
            const FFragment_NavDebugger_CapsuleVisual& InVisual) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKNAVDEBUGGER_API FProcessor_NavDebugger_RecolorCapsule : public ck_exp::TProcessor<
            FProcessor_NavDebugger_RecolorCapsule,
            FCk_Handle,
            ck::TReadWrite<FFragment_NavDebugger_CapsuleVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_NavDebugger_CapsuleVisual& InVisual) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------
    // Path visual
    // --------------------------------------------------------------------------------------------------------------------

    class CKNAVDEBUGGER_API FProcessor_NavDebugger_EnsurePath_All : public ck_exp::TProcessor<
            FProcessor_NavDebugger_EnsurePath_All,
            FCk_Handle,
            ck::TReadOnly<FFragment_Nav_AgentParams>,
            ck::TReadOnly<FFragment_Nav_PathResult>,
            TExclude<FFragment_NavDebugger_PathVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Nav_PathResult& InPathResult) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKNAVDEBUGGER_API FProcessor_NavDebugger_EnsurePath_Selected : public ck_exp::TProcessor<
            FProcessor_NavDebugger_EnsurePath_Selected,
            FCk_Handle,
            ck::TReadOnly<FFragment_Nav_AgentParams>,
            ck::TReadOnly<FFragment_Nav_PathResult>,
            FTag_NavDebugger_AgentSelected,
            TExclude<FFragment_NavDebugger_PathVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Nav_PathResult& InPathResult) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKNAVDEBUGGER_API FProcessor_NavDebugger_RemovePath : public ck_exp::TProcessor<
            FProcessor_NavDebugger_RemovePath,
            FCk_Handle,
            ck::TReadWrite<FFragment_NavDebugger_PathVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_NavDebugger_PathVisual& InVisual) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Rebuilds the path entity when the agent's PathResult snapshot changes
    // (compares PathDiagnostics.LastQueryWallTime + waypoint count to the
    // cached values on PathVisual). The lifetime cost is one entity destroy +
    // one re-spawn per *new* path query — usually <1 Hz per agent.
    class CKNAVDEBUGGER_API FProcessor_NavDebugger_RebuildPathOnDirty : public ck_exp::TProcessor<
            FProcessor_NavDebugger_RebuildPathOnDirty,
            FCk_Handle,
            ck::TReadOnly<FFragment_Nav_AgentParams>,
            ck::TReadOnly<FFragment_Nav_PathResult>,
            ck::TReadWrite<FFragment_NavDebugger_PathVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Nav_PathResult& InPathResult,
            FFragment_NavDebugger_PathVisual& InVisual) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Path lines pick their color from the agent's path status (or the
    // highlight color when the agent is selected). Re-color happens by
    // destroying and re-spawning the path entity — line geometry doesn't
    // re-color via material params (PMG draws lines as raw debug-line
    // segments, not via a procmesh material). RecolorPath sets a flag that
    // RebuildPathOnDirty observes; cheaper than touching paths every tick.
    class CKNAVDEBUGGER_API FProcessor_NavDebugger_RecolorPath : public ck_exp::TProcessor<
            FProcessor_NavDebugger_RecolorPath,
            FCk_Handle,
            ck::TReadWrite<FFragment_NavDebugger_PathVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_NavDebugger_PathVisual& InVisual) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------
    // Projection markers
    //
    // Vertical lines + filled spheres between the agent's path query inputs
    // (start = agent location at query time, end = target) and their navmesh-
    // projected counterparts. Visibility is gated by the _DrawProjections
    // setting AND the path-draw mode (so projections share visibility scope
    // with the paths they describe).
    // --------------------------------------------------------------------------------------------------------------------

    class CKNAVDEBUGGER_API FProcessor_NavDebugger_EnsureProjections : public ck_exp::TProcessor<
            FProcessor_NavDebugger_EnsureProjections,
            FCk_Handle,
            ck::TReadOnly<FFragment_Nav_AgentParams>,
            ck::TReadOnly<FFragment_Nav_PathResult>,
            TExclude<FFragment_NavDebugger_ProjectionsVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Nav_PathResult& InPathResult) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKNAVDEBUGGER_API FProcessor_NavDebugger_RemoveProjections : public ck_exp::TProcessor<
            FProcessor_NavDebugger_RemoveProjections,
            FCk_Handle,
            ck::TReadWrite<FFragment_NavDebugger_ProjectionsVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_NavDebugger_ProjectionsVisual& InVisual) const
            -> void;
    };

    // --------------------------------------------------------------------------------------------------------------------

    class CKNAVDEBUGGER_API FProcessor_NavDebugger_RebuildProjectionsOnDirty : public ck_exp::TProcessor<
            FProcessor_NavDebugger_RebuildProjectionsOnDirty,
            FCk_Handle,
            ck::TReadOnly<FFragment_Nav_AgentParams>,
            ck::TReadOnly<FFragment_Nav_PathResult>,
            ck::TReadWrite<FFragment_NavDebugger_ProjectionsVisual>,
            CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Gameplay_Rendering;
        using TProcessor::TProcessor;

    public:
        auto ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Nav_AgentParams& InAgentParams,
            const FFragment_Nav_PathResult& InPathResult,
            FFragment_NavDebugger_ProjectionsVisual& InVisual) const
            -> void;
    };
}
