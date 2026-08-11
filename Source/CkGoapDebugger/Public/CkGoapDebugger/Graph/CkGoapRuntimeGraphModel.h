#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"
#include "CoreMinimal.h"

// Runtime-safe mirror of the GOAP graph.  It deliberately owns no UEdGraph
// objects: the Slate canvas consumes this value model in editor and game builds.
enum class ECkGoapRuntimeGraphNodeKind : uint8
{
    Action,
    Goal
};
enum class ECkGoapRuntimeGraphEdgeKind : uint8
{
    Dependency,
    Tree
};

struct FCkGoapRuntimeGraphNode
{
    uint64 Id = 0;
    ECkGoapRuntimeGraphNodeKind Kind = ECkGoapRuntimeGraphNodeKind::Action;
    FVector2D Position = FVector2D::ZeroVector;
    FVector2D Size = FVector2D{180.0f, 110.0f};
    FCkGoapDebugger_ActionInfo Action;
    FString GoalOwnerName;
    TArray<FCkGoapDebugger_Condition> GoalConditions;
    bool IsInPlan = false;
    int32 PlanStepIndex = 0;
    bool IsSelected = false;
    bool IsFailureBlocked = false;
    TMap<FGameplayTag, bool> WorldState;
};

struct FCkGoapRuntimeGraphEdge
{
    uint64 SourceId = 0;
    uint64 TargetId = 0;
    ECkGoapRuntimeGraphEdgeKind Kind = ECkGoapRuntimeGraphEdgeKind::Dependency;
};

class CKGOAPDEBUGGER_API FCkGoapRuntimeGraphModel
{
  public:
    auto Rebuild(const FCkGoapDebugger_PlannerInfo& InPlanner,
                 const FCk_Handle_Goap_Action& InSelectedAction,
                 int32 InNameDepth) -> void;
    auto UpdateRuntimeState(const FCkGoapDebugger_PlannerInfo& InPlanner,
                            const FCk_Handle_Goap_Action& InSelectedAction) -> void;
    auto Reset() -> void;

    auto GetTopologyHash() const -> uint32
    {
        return _TopologyHash;
    }
    auto GetNodes() const -> const TArray<TSharedPtr<FCkGoapRuntimeGraphNode>>&
    {
        return _Nodes;
    }
    auto GetEdges() const -> const TArray<FCkGoapRuntimeGraphEdge>&
    {
        return _Edges;
    }
    auto FindActionId(const FCk_Handle_Goap_Action& InHandle) const -> uint64;
    auto FindActionById(uint64 InId) const -> const FCk_Handle_Goap_Action*;
    auto GetMaxNameDepth() const -> int32
    {
        return _MaxNameDepth;
    }
    auto GetActionCount() const -> int32
    {
        return _ActionCount;
    }
    auto GetEdgeCount() const -> int32
    {
        return _Edges.Num();
    }

    static auto ComputeTopologyHash(const FCkGoapDebugger_PlannerInfo& InPlanner) -> uint32;
    static auto ComputeEffectiveGoalHash(const FCkGoapDebugger_PlannerInfo& InPlanner,
                                          const FCkGoapDebugger_ActionInfo* InSelectedAction)
        -> uint32;
    static auto ComputeMaxNameDepth(const FString& InClassName) -> int32;

  private:
    TArray<TSharedPtr<FCkGoapRuntimeGraphNode>> _Nodes;
    TArray<FCkGoapRuntimeGraphEdge> _Edges;
    TMap<uint64, FCk_Handle_Goap_Action> _ActionById;
    uint32 _TopologyHash = 0;
    int32 _MaxNameDepth = 1;
    int32 _ActionCount = 0;
};
