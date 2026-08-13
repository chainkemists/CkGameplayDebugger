#pragma once

#include "CkSmDebugger/Data/CkSmDebugger_Types.h"
#include "CoreMinimal.h"

// Value-only graph scene for the shared runtime Slate canvas.  This is deliberately
// independent of UEdGraph so editor and packaged surfaces consume the same topology.
enum class ECkSmRuntimeGraphNodeKind : uint8
{
    Entry,
    State,
    Compound,
    Transition
};

struct FCkSmRuntimeGraphNode
{
    uint64 Id = 0;
    ECkSmRuntimeGraphNodeKind Kind = ECkSmRuntimeGraphNodeKind::State;
    int32 StateIndex = INDEX_NONE;
    int32 TransitionIndex = INDEX_NONE;
    FString Label;
    FVector2D Position = FVector2D::ZeroVector;
    FVector2D Size = FVector2D{140.0f, 44.0f};
    FLinearColor Accent = FLinearColor::White;
    bool bCurrent = false;
    bool bParentActive = true;
    bool bBreakpoint = false;
    bool bScrubActive = false;
    bool bScrubExited = false;
    bool bPrevious = false;
    bool bHasOverride = false;
    bool bFullyEventDriven = false;
    bool bExpandTasks = true;
    // Retained-card presentation only. These never affect topology or widget identity.
    float StateEventAlpha = 0.0f;
    float BorderGlowAlpha = 0.0f;
    float CellGlowAlpha = 0.0f;
    TSharedPtr<FCkSmDebugger_StateInfo> State;
    TSharedPtr<FCkSmDebugger_TransitionInfo> Transition;
};

struct FCkSmRuntimeGraphEdge
{
    uint64 SourceId = 0;
    uint64 TargetId = 0;
    uint64 TransitionId = 0;
    FLinearColor Color = FLinearColor::White;
    float Thickness = 1.5f;
    bool bDirected = true;
    bool bSelfLoop = false;
    bool bReverse = false;
    bool bScrubHighlighted = false;
    float LiveFlashAlpha = 0.0f;
    TArray<FVector2D> RoutePoints;
};

struct FCkSmRuntimeGraphScene
{
    TArray<FCkSmRuntimeGraphNode> Nodes;
    TArray<FCkSmRuntimeGraphEdge> Edges;
};

struct FCkSmRuntimeGraphNodeGeometry
{
    FVector2D Position = FVector2D::ZeroVector;
    FVector2D Size = FVector2D::ZeroVector;
};

enum class ECkSmRuntimeGraphCopyTarget : uint8
{
    State,
    Compound
};

struct FCkSmRuntimeGraphCopyPayload
{
    ECkSmRuntimeGraphCopyTarget Target = ECkSmRuntimeGraphCopyTarget::State;
    FString DisplayName;
    FString ClassName;
    FString GroupLabel;
    TArray<FString> ChildDisplayNames;
    TArray<FString> ChildClassNames;
    FString All;
};

class CKSMDEBUGGER_API FCkSmRuntimeGraphModel
{
  public:
    auto Rebuild(const FCkSmDebugger_SmInfo& InInfo,
                 bool bInExpandTasks,
                 int32 InNameDepth,
                 int32 InSpacingX = 350,
                 int32 InSpacingY = 120,
                 bool bInUndirected = false) -> void;
    auto UpdateRuntimeState(const FCkSmDebugger_SmInfo& InInfo) -> void;
    auto Clear() -> void
    {
        _Scene = {};
        _CachedSubSmOwner = {};
        _CachedSubSmData.Reset();
    }
    auto ApplyScrubHighlight(int32 InActiveStateIndex, int32 InExitedStateIndex) -> void;
    auto ClearPresentation() -> void;
    auto TriggerLivePresentation(const TArray<FCkSmDebugger_HistoryEntry>& InEvents) -> void;
    auto TriggerLivePresentation(int32 InPreviousStateIndex,
                                 int32 InCurrentStateIndex,
                                 const TSet<FString>& InPreviousStateNames) -> void;
    auto TickLivePresentation(float InDeltaTime) -> void;
    auto GetScene() const -> const FCkSmRuntimeGraphScene&
    {
        return _Scene;
    }
    auto FindNodeById(uint64 InNodeId) const -> const FCkSmRuntimeGraphNode*;
    auto BuildCopyPayload(uint64 InNodeId) const -> TOptional<FCkSmRuntimeGraphCopyPayload>;

    static auto GetStateId(int32 InStateIndex) -> uint64;
    static auto GetTransitionId(int32 InTransitionIndex) -> uint64;
    static auto GetCompoundId(int32 InParentStateIndex) -> uint64;
    static auto GetEntryId() -> uint64
    {
        return 1;
    }
    static auto EstimateStateSize(const FCkSmDebugger_StateInfo& InState,
                                  bool bInExpandTasks,
                                  int32 InNameDepth) -> FVector2D;
    static auto ResolveNodeGeometry(const FCkSmRuntimeGraphScene& InScene,
                                    const FCkSmRuntimeGraphNode& InNode,
                                    const TMap<uint64, FVector2D>& InPositionOverrides)
        -> FCkSmRuntimeGraphNodeGeometry;
    static auto ComputeStructureHash(const FCkSmDebugger_SmInfo& InInfo,
                                     bool bInExpandTasks,
                                     int32 InNameDepth,
                                     int32 InSpacingX,
                                     int32 InSpacingY,
                                     bool bInUndirected) -> uint32;

  private:
    struct FCachedSubSmData
    {
        FString Label;
        TArray<FCkSmDebugger_StateInfo> States;
        TArray<FCkSmDebugger_TransitionInfo> Transitions;
    };

    FCkSmRuntimeGraphScene _Scene;
    FCk_Handle_StateMachine _CachedSubSmOwner;
    TMap<int32, FCachedSubSmData> _CachedSubSmData;
};
