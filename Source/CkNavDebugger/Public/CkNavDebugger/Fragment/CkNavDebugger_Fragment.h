#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Tag/CkTag.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    // The agent currently selected in the CK Navigation Debugger panel.
    // Exactly one (or zero) agent carries this tag at any time. Added/removed
    // by the panel selection wiring; consumed by the EnsureCapsule_Selected /
    // EnsurePath_Selected processors AND the recolor logic that picks default
    // vs highlight color for the active visuals.
    CK_DEFINE_ECS_TAG(FTag_NavDebugger_AgentSelected);

    // --------------------------------------------------------------------------------------------------------------------

    // Per-agent capsule visualization handle. Presence indicates "this agent
    // has a live debug capsule"; the EnsureCapsule_X processors use TExclude
    // on this fragment to gate spawn, and the RemoveCapsule processor matches
    // on its presence to drive teardown.
    //
    // _IsHighlighted tracks whether the existing capsule's color is currently
    // the highlight variant — when this disagrees with the FTag_NavDebugger_AgentSelected
    // presence, the recolor processor enqueues a Pmg SetColor request and
    // updates the field.
    struct CKNAVDEBUGGER_API FFragment_NavDebugger_CapsuleVisual
    {
    public:
        CK_GENERATED_BODY(FFragment_NavDebugger_CapsuleVisual);

    public:
        FCk_Handle _CapsuleEntity;
        bool       _IsHighlighted = false;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Per-agent path-line visualization handle. Same gating model as the
    // capsule fragment. Path-snapshot identity (wall time + waypoint count)
    // is tracked so the EnsurePath processor only rebuilds the line geometry
    // when the underlying path actually changes.
    struct CKNAVDEBUGGER_API FFragment_NavDebugger_PathVisual
    {
    public:
        CK_GENERATED_BODY(FFragment_NavDebugger_PathVisual);

    public:
        FCk_Handle _PathEntity;
        bool       _IsHighlighted = false;
        double     _LastPathQueryWallTime = 0.0;
        int32      _LastWaypointCount     = -1;
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Per-agent projection-marker visualization handle. Shows the vertical
    // line + sphere marker between the path query's start/end inputs and
    // their navmesh-projected locations — surface "agent floating above
    // navmesh" / "target outside navmesh" setup mistakes immediately.
    // Driven by the path-mode setting plus the _DrawProjections toggle.
    struct CKNAVDEBUGGER_API FFragment_NavDebugger_ProjectionsVisual
    {
    public:
        CK_GENERATED_BODY(FFragment_NavDebugger_ProjectionsVisual);

    public:
        FCk_Handle _ProjectionsEntity;
        double     _LastPathQueryWallTime = 0.0;
    };
}

// --------------------------------------------------------------------------------------------------------------------
