#pragma once

#include "CkSmDebugger/Data/CkSmDebugger_Types.h"

#include "EdGraph/EdGraphNode.h"
#include "CoreMinimal.h"

#include "CkSmDebugNode_Transition.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class UCkSmDebugNode_State;

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKSMDEBUGGER_API UCkSmDebugNode_Transition : public UEdGraphNode
{
    GENERATED_BODY()

public:
    // UEdGraphNode
    virtual auto AllocateDefaultPins() -> void override;
    virtual auto GetNodeTitle(ENodeTitleType::Type InTitleType) const -> FText override;
    virtual auto CanUserDeleteNode() const -> bool override { return false; }
    virtual auto CanDuplicateNode() const -> bool override { return false; }

    // Population — real data
    auto
    PopulateFromTransitionInfo(
        const FCkSmDebugger_TransitionInfo& InTransition,
        int32 InIndex) -> void;

    auto
    UpdateRuntimeData(
        const FCkSmDebugger_TransitionInfo& InTransition) -> void;

    // Population — mockup
    auto
    InitMockup(
        const FString& InSourceName,
        const FString& InTargetName,
        int32 InIndex,
        int32 InSourceIdx,
        int32 InTargetIdx) -> void;

    // Navigation
    auto GetSourceNode() const -> UCkSmDebugNode_State*;
    auto GetTargetNode() const -> UCkSmDebugNode_State*;

    // Accessors
    auto Get_SourceStateName() const -> const FString& { return _SourceStateName; }
    auto Get_TargetStateName() const -> const FString& { return _TargetStateName; }
    auto Get_SatisfiedCount() const -> int32 { return _SatisfiedCount; }
    auto Get_TotalCount() const -> int32 { return _TotalCount; }
    auto Get_AreAllConditionsSatisfied() const -> bool { return _AreAllConditionsSatisfied; }
    auto Get_IsSubSmTransition() const -> bool { return _IsSubSmTransition; }
    auto Get_TransitionIndex() const -> int32 { return _TransitionIndex; }
    auto Get_Conditions() const -> const TArray<FCkSmDebugger_ConditionInfo>& { return _Conditions; }

private:
    UPROPERTY()
    FString _SourceStateName;

    UPROPERTY()
    FString _TargetStateName;

    UPROPERTY()
    int32 _SatisfiedCount = 0;

    UPROPERTY()
    int32 _TotalCount = 0;

    UPROPERTY()
    bool _AreAllConditionsSatisfied = false;

    UPROPERTY()
    bool _IsSubSmTransition = false;

    UPROPERTY()
    int32 _TransitionIndex = -1;

    TArray<FCkSmDebugger_ConditionInfo> _Conditions;
};

// --------------------------------------------------------------------------------------------------------------------
