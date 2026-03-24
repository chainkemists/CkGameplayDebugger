#include "CkSmDebugNode_State.h"

#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_State::
    AllocateDefaultPins()
    -> void
{
    CreatePin(EGPD_Input, TEXT("Transition"), NAME_None);
    CreatePin(EGPD_Output, TEXT("Transition"), NAME_None);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_State::
    GetNodeTitle(
        ENodeTitleType::Type InTitleType) const
    -> FText
{
    return FText::FromString(_StateName);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_State::
    PopulateFromStateInfo(
        const FCkSmDebugger_StateInfo& InState,
        int32 InIndex)
    -> void
{
    _StateIndex = InIndex;
    _StateName = InState.StateName;
    _StateColor = CkSmDebugger::ComputeStateColor(InState.StateName);
    _IsCurrentState = InState.IsCurrentState;
    _IsSubSmNode = InState.IsSubSmNode;
    _HasSubStateMachine = InState.HasSubStateMachine;
    _HasBeenVisited = InState.HasBeenVisited;
    _DwellTimeSeconds = static_cast<float>(InState.DwellTimeSeconds);
    _HasBreakpoint = InState.HasEntryBreakpoint || InState.HasExitBreakpoint;
    _IsBreakpointHit = InState.IsBreakpointHit;
    _Tasks = InState.Tasks;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_State::
    UpdateRuntimeData(
        const FCkSmDebugger_StateInfo& InState)
    -> void
{
    _IsCurrentState = InState.IsCurrentState;
    _HasBeenVisited = InState.HasBeenVisited;
    _DwellTimeSeconds = static_cast<float>(InState.DwellTimeSeconds);
    _HasBreakpoint = InState.HasEntryBreakpoint || InState.HasExitBreakpoint;
    _IsBreakpointHit = InState.IsBreakpointHit;
    _Tasks = InState.Tasks;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_State::
    InitMockup(
        const FString& InName,
        bool InIsActive,
        const FLinearColor& InColor,
        int32 InIndex,
        float InDwellTime)
    -> void
{
    _StateName = InName;
    _IsCurrentState = InIsActive;
    _StateColor = InColor;
    _StateIndex = InIndex;
    _DwellTimeSeconds = InDwellTime;
}

// --------------------------------------------------------------------------------------------------------------------
