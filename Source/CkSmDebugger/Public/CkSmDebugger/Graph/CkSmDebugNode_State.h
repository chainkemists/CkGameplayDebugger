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
    auto Get_IsParentStateActive() const -> bool { return _IsParentStateActive; }
    auto Set_IsParentStateActive(bool InValue) -> void { _IsParentStateActive = InValue; }
    auto Get_HasBeenVisited() const -> bool { return _HasBeenVisited; }
    auto Get_DwellTimeSeconds() const -> float { return _DwellTimeSeconds; }
    auto Get_HasOverride() const -> bool { return _HasOverride; }
    auto Get_IsFullyEventDriven() const -> bool { return _IsFullyEventDriven; }
    auto Get_HasAnyTickingTask()  const -> bool { return _HasAnyTickingTask;  }
    auto Get_HasCompleteData()    const -> bool { return _HasCompleteData;    }
    auto Set_IsFullyEventDriven(bool InValue) -> void { _IsFullyEventDriven = InValue; }
    auto Get_HasBreakpoint() const -> bool { return _HasEntryBreakpoint || _HasExitBreakpoint; }
    auto Get_HasEntryBreakpoint() const -> bool { return _HasEntryBreakpoint; }
    auto Get_HasExitBreakpoint() const -> bool { return _HasExitBreakpoint; }

    auto ToggleEntryBreakpoint() -> void { _HasEntryBreakpoint = !_HasEntryBreakpoint; }
    auto ToggleExitBreakpoint() -> void { _HasExitBreakpoint = !_HasExitBreakpoint; }
    auto Get_IsBreakpointHit() const -> bool { return _IsBreakpointHit; }
    auto Get_StateIndex() const -> int32 { return _StateIndex; }
    auto Get_Tasks() const -> const TArray<FCkSmDebugger_TaskInfo>& { return _Tasks; }

    auto Get_IsScrubActiveState() const -> bool { return _IsScrubActiveState; }
    auto Set_IsScrubActiveState(bool InValue) -> void { _IsScrubActiveState = InValue; }
    auto Get_IsScrubExitedState() const -> bool { return _IsScrubExitedState; }
    auto Set_IsScrubExitedState(bool InValue) -> void { _IsScrubExitedState = InValue; }
    auto Get_IsPreviousState() const -> bool { return _IsPreviousState; }
    auto Set_IsPreviousState(bool InValue) -> void { _IsPreviousState = InValue; }

    // Live "current state" highlight, driven by TickLiveFlash and read every
    // frame by the pill (no widget rebuild). Split into two channels so entry
    // and exit animate differently:
    //   - Border alpha fades BOTH ways: grey -> blue on becoming current,
    //     blue -> grey once left (the outline fade that marks the state we just
    //     jumped from).
    //   - Cell alpha fades IN on becoming current (whole node brightens) and
    //     then HOLDS — leaving a state animates only the border, not the cell,
    //     so the node doesn't dim as a whole on exit.
    auto Get_BorderGlowAlpha() const -> float { return _BorderGlowAlpha; }
    auto Set_BorderGlowAlpha(float InValue) -> void { _BorderGlowAlpha = InValue; }
    auto Get_CellGlowAlpha() const -> float { return _CellGlowAlpha; }
    auto Set_CellGlowAlpha(float InValue) -> void { _CellGlowAlpha = InValue; }

    // Grey "previous state" glow intensity, fading in/out (both ways) as this
    // node enters/leaves the previous-state set — mirrors the blue border fade
    // instead of snapping on/off.
    auto Get_PreviousGlowAlpha() const -> float { return _PreviousGlowAlpha; }
    auto Set_PreviousGlowAlpha(float InValue) -> void { _PreviousGlowAlpha = InValue; }

    // One-shot "just became current" entry pulse: set to 1.0 on the false->true
    // IsCurrentState edge, decays to 0 over Sm_EntryPulseDuration. Drives the
    // entry overshoot — a brief brightening of the border colour (in the border
    // lambda, not a drawn box). _WasCurrentState is the edge-detect latch the
    // driver updates each tick.
    auto Get_EntryPulseAlpha() const -> float { return _EntryPulseAlpha; }
    auto Set_EntryPulseAlpha(float InValue) -> void { _EntryPulseAlpha = InValue; }
    auto Get_WasCurrentState() const -> bool { return _WasCurrentState; }
    auto Set_WasCurrentState(bool InValue) -> void { _WasCurrentState = InValue; }


    // Breakpoint visual style variant (0 = default, used for A/B testing in mockup)
    auto Get_BreakpointStyle() const -> int32 { return _BreakpointStyle; }
    auto Set_BreakpointStyle(int32 InStyle) -> void { _BreakpointStyle = InStyle; }

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
    bool _HasOverride = false;

    UPROPERTY()
    bool _IsFullyEventDriven = false;

    UPROPERTY()
    bool _HasAnyTickingTask = false;

    UPROPERTY()
    bool _HasCompleteData = false;

    UPROPERTY()
    int32 _StateIndex = -1;

    TArray<FCkSmDebugger_TaskInfo> _Tasks;

    bool _IsScrubActiveState = false;
    bool _IsScrubExitedState = false;
    bool _IsPreviousState = false;
    bool _IsParentStateActive = false;
    float _BorderGlowAlpha = 0.0f;
    float _CellGlowAlpha = 0.0f;
    float _PreviousGlowAlpha = 0.0f;
    float _EntryPulseAlpha = 0.0f;
    bool _WasCurrentState = false;

    int32 _BreakpointStyle = 23;
};

// --------------------------------------------------------------------------------------------------------------------
