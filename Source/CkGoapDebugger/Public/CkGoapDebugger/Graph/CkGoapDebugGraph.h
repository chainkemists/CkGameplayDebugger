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

// Display-name shortening for the action graph — and every other site that
// renders an EntityScript class name — lives in the shared
// SCkDebug_NameLabel::Get_ShortName (CkDebuggerCommon): one shortener for
// every debugger. This header used to own a local copy; it is gone.

UCLASS()
class CKGOAPDEBUGGER_API UCkGoapDebugGraph : public UEdGraph
{
    GENERATED_BODY()

public:
    // Rebuilds the graph from the supplied Planner snapshot.
    // - Clears existing nodes.
    // - Creates one UCkGoapDebugNode_Action per Action found by recursing
    //   ChildActions + ChildPlanners of the supplied PlannerInfo.
    // - Creates a UCkGoapDebugNode_Goal for the Planner's goal (the selected
    //   PlannerInfo's GoalResolved drives the goal pins).
    // - Lays out nodes in a layered DAG (precondition -> effect chain).
    // - Wires effect-producer -> precondition-consumer pins + tree edges.
    auto
    RebuildFromSnapshot(
        const FCkGoapDebugger_PlannerInfo& InPlanner,
        const FCk_Handle_Goap_Action& InSelectedActionHandle) -> void;

    // Updates per-Action render hints (IsInPlan / PlanStepIndex / IsSelected /
    // IsFailureBlocked) in place on the existing nodes. Cheap fast-path used
    // when only the plan / selection changed but topology (catalog identity +
    // edges + goal owner) did not — avoids tearing down the SGraphEditor
    // scene every refresh, which was the cause of action-graph flicker.
    // Returns true when any node's render-hint state actually changed.
    auto
    UpdateRuntimeState(
        const FCkGoapDebugger_PlannerInfo& InPlanner,
        const FCk_Handle_Goap_Action& InSelectedActionHandle) -> bool;

    // Topology hash — captures everything that affects WHICH nodes exist and
    // HOW they're wired (catalog identity, preconditions/effects, goal owner).
    // Excludes mutable per-tick state (PlanStatus, plan handles, selection)
    // which UpdateRuntimeState handles in place. The pane uses this to decide
    // between a fast in-place update and a full destructive rebuild.
    static auto
    ComputeTopologyHash(
        const FCkGoapDebugger_PlannerInfo& InPlanner) -> uint32;

    // Drops all nodes and clears the topology cache. Must be called from
    // the pane widget's PIE-teardown path so the FCk_Handle copies inside
    // each node's ActionInfo snapshot are released while the registry is
    // still live.
    auto ForceClear() -> void;

    // Finds the Action node whose handle matches; nullptr otherwise.
    auto FindActionNode(const FCk_Handle_Goap_Action& InHandle) const -> UCkGoapDebugNode_Action*;

    // Live resolved world-state view for the displayed Planner (key → value),
    // refreshed by BOTH RebuildFromSnapshot and UpdateRuntimeState so the
    // per-precondition satisfaction dots on the action cards track the world
    // every tick without a topology rebuild. Unset when the key isn't part of
    // the Planner's resolved WS snapshot (rendered as "unknown" by the cards).
    auto Get_LiveWorldStateValue(const FGameplayTag& InKey) const -> TOptional<bool>
    {
        if (const auto* Found = _LiveWorldState.Find(InKey))
        { return *Found; }
        return {};
    }

    auto Get_GoalNode()   const -> UCkGoapDebugNode_Goal* { return _GoalNode; }
    auto Get_ActionCount() const -> int32;
    auto Get_EdgeCount()   const -> int32;

    // Display-name verbosity (0 = full joined name, 1 = leaf segment, 2 = last
    // two joined, ...). Read by every site that renders an action/planner class
    // name through SCkDebug_NameLabel::Get_ShortName. The graph
    // pane owns the live value and exposes it via Get_NameDepth so all panes
    // converge on one source of truth.
    int32 NameDepth = 1;

    // Largest segment count across every action class name currently in the
    // graph. Used by the toolbar to clamp the depth-cycle button so we don't
    // step past meaningful values. Returns at least 1.
    auto Get_MaxNameDepth() const -> int32;

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

    // Flattened resolved WS snapshot for the displayed Planner. Plain types —
    // no UPROPERTY needed; cleared in lockstep with Nodes via ForceClear.
    TMap<FGameplayTag, bool> _LiveWorldState;
};

// ====================================================================================================================
