#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

// --------------------------------------------------------------------------------------------------------------------

class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;

// ====================================================================================================================
// Pure-data snapshot of one local player's Enhanced Input state, gathered from
// the UEnhancedInputLocalPlayerSubsystem. Deliberately free of any Slate /
// EnhancedInput-header types in this header so the window only ever sees PODs.
//
//   Contexts        — the applied mapping-context STACK (priority-ordered),
//                     each with its raw action<->key mappings. The thing the
//                     user means by "which contexts & inputs are registered".
//                     Discovered via the public HasMappingContext query since
//                     the applied-context map itself is engine-protected.
//   ResolvedActions — the unique actions across the stack with the keys the
//                     subsystem reports as mapped to each (QueryKeysMappedToAction),
//                     i.e. what actually wins after priority resolution.
//
// Live per-action runtime state (current value, trigger event) is fetched
// separately via Gather_LiveActionState so the structural snapshot stays stable
// across frames (only changes when the stack/mappings change → rebuild gate).
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
// A single action<->key mapping inside one mapping context.
// --------------------------------------------------------------------------------------------------------------------

struct FCkInputDebugger_MappingRow
{
    TWeakObjectPtr<const UInputAction> Action;
    FString ActionName;
    FKey    Key;
    FString KeyName;
    FString ValueType;
    int32   TriggerCount  = 0;
    int32   ModifierCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------
// One applied mapping context (one rung of the stack).
// --------------------------------------------------------------------------------------------------------------------

struct FCkInputDebugger_ContextRow
{
    TWeakObjectPtr<const UInputMappingContext> Context;
    FString ContextName;
    FString ContextPath;
    int32   Priority = 0;
    TArray<FCkInputDebugger_MappingRow> Mappings;
};

// --------------------------------------------------------------------------------------------------------------------
// One unique action from the resolved (flattened) binding set.
// --------------------------------------------------------------------------------------------------------------------

struct FCkInputDebugger_ActionRow
{
    TWeakObjectPtr<const UInputAction> Action;
    FString ActionName;
    FString ValueType;
    FString KeysJoined;
};

// --------------------------------------------------------------------------------------------------------------------
// How "live" an action is right now — drives the runtime value/trigger colour.
// --------------------------------------------------------------------------------------------------------------------

enum class ECkInputDebugger_ActionActivity : uint8
{
    NoInstance,   // never evaluated yet (no FInputActionInstance)
    Idle,         // instance exists, currently not firing
    Ongoing,      // hold-style trigger mid-evaluation
    Active,       // Started / Triggered this frame
    Canceled,
    Completed,
};

struct FCkInputDebugger_LiveActionState
{
    ECkInputDebugger_ActionActivity Activity = ECkInputDebugger_ActionActivity::NoInstance;
    FString ValueText;
    FString TriggerEventText;
    float   Magnitude   = 0.0f;
    float   ElapsedTime = 0.0f;
};

// --------------------------------------------------------------------------------------------------------------------
// Full structural snapshot for the selected player.
// --------------------------------------------------------------------------------------------------------------------

struct FCkInputDebugger_Snapshot
{
    bool    HasPlayer = false;
    FString PlayerLabel;
    TArray<FCkInputDebugger_ContextRow> Contexts;        // sorted by priority DESC
    TArray<FCkInputDebugger_ActionRow>  ResolvedActions; // sorted by name

    // Stable string that changes only when the structure (contexts / priorities /
    // mappings / resolved actions / player) changes — used to gate full rebuilds.
    FString StructuralSignature;

    // Gather the structural snapshot from the subsystem. InPlayerLabel is folded
    // into the signature so switching player forces a rebuild.
    static auto Gather(
        UEnhancedInputLocalPlayerSubsystem* InSubsystem,
        const FString& InPlayerLabel) -> FCkInputDebugger_Snapshot;

    // Fetch live runtime state for a single action (cheap; called per gated tick).
    static auto Gather_LiveActionState(
        UEnhancedInputLocalPlayerSubsystem* InSubsystem,
        const UInputAction* InAction) -> FCkInputDebugger_LiveActionState;
};

// ====================================================================================================================
