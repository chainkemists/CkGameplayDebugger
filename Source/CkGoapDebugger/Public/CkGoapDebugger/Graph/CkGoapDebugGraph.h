#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "EdGraph/EdGraph.h"
#include "CoreMinimal.h"

#include "CkGoapDebugGraph.generated.h"

// ====================================================================================================================

class UCkGoapDebugNode_Action;
class UCkGoapDebugNode_Goal;

// ====================================================================================================================
// UEdGraph subclass — owns the action node + goal anchor + edges that
// visualise the selected Planner's catalog. Rebuilt from the data layer
// snapshot every time the ViewModel changes (cheap; node count is bounded
// by the catalog size, typically << 100).
//
// Layered layout:
//   - Layer 0  : actions with zero preconditions (or preconditions that no
//                catalog action produces).
//   - Layer N  : 1 + max(layer of producers) over all preconditions.
//   - Cycle detection : if the topological order can't be completed (cycle),
//                       falls back to a single-row layout so the user still
//                       sees nodes.
//   - Goal anchor is placed one column to the right of the last action layer.
//
// PIE lifetime contract:
//   - The graph + every node holds FCk_Handle copies via the embedded
//     FCkGoapDebugger_ActionInfo snapshot. The pane widget must call
//     ForceClear() from its world-tear-down handler so handles are released
//     while the registry is still alive.
// ====================================================================================================================

UCLASS()
class CKGOAPDEBUGGER_API UCkGoapDebugGraph : public UEdGraph
{
    GENERATED_BODY()

public:
    // Rebuilds the graph from the supplied Planner snapshot.
    // - Clears existing nodes.
    // - Creates one UCkGoapDebugNode_Action per Catalog entry.
    // - Creates a UCkGoapDebugNode_Goal for the selected Action's goal
    //   (or the root Action's goal when no selection is provided).
    // - Lays out nodes in a layered DAG (precondition -> effect chain).
    // - Wires effect-producer -> precondition-consumer pins.
    auto
    RebuildFromSnapshot(
        const FCkGoapDebugger_ActionSetInfo& InPlanner,
        const FCk_Handle_Goap_Action& InSelectedActionHandle) -> void;

    // Updates per-Action render hints (IsInPlan / PlanStepIndex / IsSelected /
    // IsFailureBlocked) in place on the existing nodes. Cheap fast-path used
    // when only the active chain or selection changed but topology (Catalog
    // identity + edges + goal owner) did not — avoids tearing down the
    // SGraphEditor scene every refresh, which was the cause of action-graph
    // flicker. Returns true when any node's render-hint state actually
    // changed (caller can decide whether to NotifyGraphChanged).
    auto
    UpdateRuntimeState(
        const FCkGoapDebugger_ActionSetInfo& InPlanner,
        const FCk_Handle_Goap_Action& InSelectedActionHandle) -> bool;

    // Topology hash — captures everything that affects WHICH nodes exist and
    // HOW they're wired (catalog identity, preconditions/effects, goal owner).
    // Excludes mutable per-tick state (PlanStatus, ActiveChain, selection)
    // which UpdateRuntimeState handles in place. The pane uses this to decide
    // between a fast in-place update and a full destructive rebuild.
    static auto
    ComputeTopologyHash(
        const FCkGoapDebugger_ActionSetInfo& InPlanner) -> uint32;

    // Drops all nodes and clears the topology cache. Must be called from
    // the pane widget's PIE-teardown path so the FCk_Handle copies inside
    // each node's ActionInfo snapshot are released while the registry is
    // still live.
    auto ForceClear() -> void;

    // Finds the Action node whose handle matches; nullptr otherwise.
    auto FindActionNode(const FCk_Handle_Goap_Action& InHandle) const -> UCkGoapDebugNode_Action*;

    auto Get_GoalNode()   const -> UCkGoapDebugNode_Goal* { return _GoalNode; }
    auto Get_ActionCount() const -> int32;
    auto Get_EdgeCount()   const -> int32;

    // Returns true if the (Output, Input) pin pair was added as a parent →
    // child tree edge during RebuildFromSnapshot. The connection policy uses
    // this to apply dashed wire styling to tree edges so they read distinctly
    // from solid dependency edges. Cheap O(log N) lookup on the side set.
    auto Is_TreeEdge(UEdGraphPin* InOutputPin, UEdGraphPin* InInputPin) const -> bool;

    // UEdGraph — suppress change notifications during batch population.
    auto SetSuppressNotifications(bool bSuppress) -> void { _SuppressNotifications = bSuppress; }
    virtual void NotifyGraphChanged() override
    {
        if (NOT _SuppressNotifications) { Super::NotifyGraphChanged(); }
    }

private:
    bool _SuppressNotifications = false;

    UPROPERTY()
    TObjectPtr<UCkGoapDebugNode_Goal> _GoalNode = nullptr;

    // Side set of parent → child tree-edge pin pairs added by
    // RebuildFromSnapshot. NOT a UPROPERTY — pin pointers are owned by the
    // EdGraphNodes, which are themselves rooted via the UEdGraph::Nodes array.
    // ForceClear / RebuildFromSnapshot reset this in lockstep with Nodes.
    TSet<TPair<UEdGraphPin*, UEdGraphPin*>> _TreeEdgePins_Graph;
};

// ====================================================================================================================
