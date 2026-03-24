#pragma once

#include "CkSmDebugger/Data/CkSmDebugger_Types.h"

#include "EdGraph/EdGraphNode.h"
#include "CoreMinimal.h"

#include "CkSmDebugNode_State.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS()
class CKSMDEBUGGER_API UCkSmDebugNode_State : public UEdGraphNode
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
    PopulateFromStateInfo(
        const FCkSmDebugger_StateInfo& InState,
        int32 InIndex) -> void;

    auto
    UpdateRuntimeData(
        const FCkSmDebugger_StateInfo& InState) -> void;

    // Population — mockup
    auto
    InitMockup(
        const FString& InName,
        bool InIsActive,
        const FLinearColor& InColor,
        int32 InIndex,
        float InDwellTime = 0.0f) -> void;

    // Accessors
    auto Get_StateName() const -> const FString& { return _StateName; }
    auto Get_StateColor() const -> FLinearColor { return _StateColor; }
    auto Get_IsCurrentState() const -> bool { return _IsCurrentState; }
    auto Get_IsSubSmNode() const -> bool { return _IsSubSmNode; }
    auto Get_HasSubStateMachine() const -> bool { return _HasSubStateMachine; }
    auto Get_HasBeenVisited() const -> bool { return _HasBeenVisited; }
    auto Get_DwellTimeSeconds() const -> float { return _DwellTimeSeconds; }
    auto Get_HasBreakpoint() const -> bool { return _HasEntryBreakpoint || _HasExitBreakpoint; }
    auto Get_HasEntryBreakpoint() const -> bool { return _HasEntryBreakpoint; }
    auto Get_HasExitBreakpoint() const -> bool { return _HasExitBreakpoint; }
    auto Get_IsBreakpointHit() const -> bool { return _IsBreakpointHit; }
    auto Get_StateIndex() const -> int32 { return _StateIndex; }
    auto Get_Tasks() const -> const TArray<FCkSmDebugger_TaskInfo>& { return _Tasks; }

private:
    UPROPERTY()
    FString _StateName;

    UPROPERTY()
    FLinearColor _StateColor = FLinearColor::White;

    UPROPERTY()
    bool _IsCurrentState = false;

    UPROPERTY()
    bool _IsSubSmNode = false;

    UPROPERTY()
    bool _HasSubStateMachine = false;

    UPROPERTY()
    bool _HasBeenVisited = false;

    UPROPERTY()
    float _DwellTimeSeconds = 0.0f;

    UPROPERTY()
    bool _HasEntryBreakpoint = false;

    UPROPERTY()
    bool _HasExitBreakpoint = false;

    UPROPERTY()
    bool _IsBreakpointHit = false;

    UPROPERTY()
    int32 _StateIndex = -1;

    TArray<FCkSmDebugger_TaskInfo> _Tasks;
};

// --------------------------------------------------------------------------------------------------------------------
