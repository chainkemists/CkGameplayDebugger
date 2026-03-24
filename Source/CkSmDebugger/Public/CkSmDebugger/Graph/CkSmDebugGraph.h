#pragma once

#include "CkSmDebugger/Data/CkSmDebugger_Types.h"

#include "EdGraph/EdGraph.h"
#include "CoreMinimal.h"

#include "CkSmDebugGraph.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class UCkSmDebugNode_State;
class UCkSmDebugNode_Transition;

// --------------------------------------------------------------------------------------------------------------------

struct FCkSmLayoutParams
{
    bool  bUndirectedBFS           = false;  // false = directed (better depth), true = undirected (compact cycles)
    bool  bExpandTasks             = true;   // show task rows inside state nodes
    int32 SpacingX                 = 350;
    int32 SpacingY                 = 120;
    int32 CrossingReductionPasses  = 4;
    int32 NameDepth                = 1;      // how many tag segments to show (from the leaf). 0 = full name.

    // Convert a class name like "Ck_SmTest_Complex_State_Chase" to a short display name
    // at the configured depth. Depth 1 → "Chase", 2 → "State.Chase", 0 → full tag.
    static auto ComputeDisplayName(const FString& InClassName, int32 InDepth) -> FString
    {
        auto Name = InClassName;
        if (Name.EndsWith(TEXT("_C"))) { Name = Name.LeftChop(2); }

        TArray<FString> Segments;
        Name.ParseIntoArray(Segments, TEXT("_"), true);

        if (InDepth <= 0 || InDepth >= Segments.Num())
        {
            return FString::Join(Segments, TEXT("."));
        }

        auto Result = FString{};
        for (auto i = Segments.Num() - InDepth; i < Segments.Num(); ++i)
        {
            if (Result.Len() > 0) { Result += TEXT("."); }
            Result += Segments[i];
        }
        return Result;
    }
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKSMDEBUGGER_API UCkSmDebugGraph : public UEdGraph
{
    GENERATED_BODY()

public:
    // Real data — rebuild / incremental update from collected SM info
    auto
    RebuildFromSmInfo(
        const FCkSmDebugger_SmInfo& InSmInfo) -> void;

    auto
    UpdateFromSmInfo(
        const FCkSmDebugger_SmInfo& InSmInfo) -> void;

    // Mockup — hard-coded test graph matching the LogicDriverPro reference
    auto BuildMockup() -> void;

    // Force a full rebuild on the next UpdateFromSmInfo call
    auto ForceRebuild() -> void { _TopologyHash = 0; }

    // Find a state node by its state index (returns nullptr if not found)
    auto FindStateNode(int32 InStateIndex) const -> UCkSmDebugNode_State*;

    // Find a transition node by its transition index (returns nullptr if not found)
    auto FindTransitionNode(int32 InTransitionIndex) const -> UCkSmDebugNode_Transition*;

    // Layout parameters — exposed to the toolbar
    FCkSmLayoutParams LayoutParams;

    // Command callback — set by the window, invoked by context menu actions
    TFunction<void(const FCkSmDebugger_Command&)> OnIssueCommand;

    // UEdGraph — suppress change notifications during batch population
    auto SetSuppressNotifications(bool bSuppress) -> void { _SuppressNotifications = bSuppress; }
    virtual void NotifyGraphChanged() override
    {
        if (!_SuppressNotifications) { Super::NotifyGraphChanged(); }
    }

    // Transition data lookup — used by the connection drawing policy
    auto Get_TransitionData() const -> const TArray<FCkSmDebugger_TransitionInfo>& { return _TransitionData; }
    auto FindTransitionBetween(int32 InSourceIdx, int32 InTargetIdx) const -> const FCkSmDebugger_TransitionInfo*;

private:
    auto
    ComputeTopologyHash(
        const FCkSmDebugger_SmInfo& InSmInfo) const -> uint32;

private:
    UPROPERTY()
    uint32 _TopologyHash = 0;

    bool _SuppressNotifications = false;

    // Cached transition info for the connection policy to read
    TArray<FCkSmDebugger_TransitionInfo> _TransitionData;
};

// --------------------------------------------------------------------------------------------------------------------
