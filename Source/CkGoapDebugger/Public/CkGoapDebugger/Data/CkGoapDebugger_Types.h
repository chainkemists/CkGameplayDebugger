#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkGoap/CkGoap_Fragment_Data.h"             // ECk_GoapPlanStatus, FCk_Handle_Goap
#include "CkGoap/Action/CkGoap_Action_Fragment_Data.h"   // FCk_Handle_Goap_Action
#include "CkGoap/ActionSet/CkGoap_ActionSet_Fragment_Data.h" // FCk_Handle_Goap_ActionSet
#include "CkCore/Enums/CkEnums.h"                    // ECk_EnableDisable

class UCk_GoapAction_EntityScript;

// ====================================================================================================================
// CkGoap Debugger — display-side mirror types.
//
// All structs in this header are plain (non-UObject, non-USTRUCT). They are
// produced by the data collector each refresh tick from the live ECS
// fragments and consumed by Slate widgets to render the debugger UI.
// They do NOT participate in serialization or reflection.
//
// Type layout reflects the unified ActionSet/Action model (post-Phase U7):
//   Entity → ActionSet[] → Catalog of Action entities
//                       + ActiveChain (root → leaf list of currently-active
//                                       Action handles)
//                       + DependencyCycles diagnostic
//
// PIE lifetime note: FCk_Handle / FCk_Handle_Goap_* hold a TOptional<FCk_Registry>
// by value. Any container of these types that outlives a PIE session must be
// cleared on world-tear-down (see CkSmDebugger CLAUDE.md "Handle lifetime
// contract"). Snapshots stored in HistoryEvent::SnapshotAtEvent fall into this
// category.
// ====================================================================================================================

// ====================================================================================================================
// CONDITION — authored boolean (preconditions, effects, goal entries, WS).
// ====================================================================================================================

struct FCkGoapDebugger_Condition
{
    FGameplayTag Key;
    bool         Value = false;
};

// ====================================================================================================================
// WORLD STATE ENTRY — one (tag, bool) row in the resolved world-state snapshot.
// RecentlyChanged is set by the collector when the value differs from the
// previous tick's snapshot.
// ====================================================================================================================

struct FCkGoapDebugger_WorldStateEntry
{
    FGameplayTag Key;
    bool         Value             = false;
    bool         RecentlyChanged   = false;
    int64        LastChangedFrame  = 0;
};

// ====================================================================================================================
// DEPENDENCY CYCLE — mirror of FCk_GoapDiagnostic_DependencyCycle, but flattened
// to strings for display (class refs are unsafe to hold across PIE teardown).
// ====================================================================================================================

struct FCkGoapDebugger_CycleInfo
{
    TArray<FString>      ActionsInCycle;
    TArray<FGameplayTag> CycleConditions;
};

// ====================================================================================================================
// ACTION ROLE — inferred from tree + active-chain position at snapshot time.
//   Root    : Action is the ActionSet's root.
//   Mid     : Active chain entry between root and leaf.
//   Leaf    : Deepest entry in the active chain.
//   Catalog : Registered Action that is not currently in the active chain.
// ====================================================================================================================

enum class ECkGoapDebugger_ActionRole : uint8
{
    Root,
    Mid,
    Leaf,
    Catalog
};

// ====================================================================================================================
// ACTION INFO — per-Action snapshot. Unit of inspection in the primary panel
// and the unit of selection in the breadcrumb / graph.
// ====================================================================================================================

struct FCkGoapDebugger_ActionInfo
{
    // Identity ----------------------------------------------------------------
    FCk_Handle_Goap_Action                     Handle;
    TSubclassOf<UCk_GoapAction_EntityScript>   ActionClass;
    FString                                    ClassName;
    FGameplayTag                               ActionTag;          // class-derived

    // Plan state --------------------------------------------------------------
    ECk_GoapPlanStatus PlanStatus              = ECk_GoapPlanStatus::Idle;
    TArray<FString>    PlanClassNames;                             // ordered children
    float              PlanCost                = 0.0f;
    int32              PlanAttemptCount        = 0;
    float              SecondsSinceLastReplan  = 0.0f;

    // CDO-extracted definition ------------------------------------------------
    TArray<FCkGoapDebugger_Condition> Preconditions;
    TArray<FCkGoapDebugger_Condition> Effects;
    float                             Cost = 1.0f;

    // Goal / WS ---------------------------------------------------------------
    // _Goal (effects, or _InitialGoal_RootOnly for the root).
    TArray<FCkGoapDebugger_Condition> Goal;
    // Effects referencing unregistered WS keys — populated at Setup.
    TArray<FCkGoapDebugger_Condition> InvalidGoal;
    // Human-readable label for the resolved WS source.
    FString                           WorldStateSourceLabel;

    // Tree position -----------------------------------------------------------
    FCk_Handle_Goap_Action                ParentActionHandle;   // invalid for root
    FString                               ParentClassName;      // empty for root
    TArray<FCk_Handle_Goap_Action>        ChildActionHandles;   // registered children

    // Chain position ----------------------------------------------------------
    ECkGoapDebugger_ActionRole Role            = ECkGoapDebugger_ActionRole::Catalog;
    bool                       IsInActiveChain = false;
    int32                      ChainDepth      = -1;             // 0 = root, -1 = catalog
};

// ====================================================================================================================
// ACTION SET INFO — per-ActionSet snapshot.
// ====================================================================================================================

struct FCkGoapDebugger_ActionSetInfo
{
    FCk_Handle_Goap_ActionSet Handle;
    FString                   DebugName;        // ActionSetTag.LeafAsString()
    FGameplayTag              ActionSetTag;
    ECk_EnableDisable         EnableToggle = ECk_EnableDisable::Enable;

    // Catalog of ALL registered Actions (active + dormant), ordered by
    // tree-walk from the root for stable display.
    TArray<FCkGoapDebugger_ActionInfo> Catalog;

    // Ordered list of currently-active Action handles. [0] is the root.
    TArray<FCk_Handle_Goap_Action>     ActiveChainHandles;

    FCk_Handle_Goap_Action             RootActionHandle;
    FString                            WorldStateSourceLabel;
    TArray<FCkGoapDebugger_WorldStateEntry> WorldState;

    TArray<FCkGoapDebugger_CycleInfo>  DependencyCycles;
};

// ====================================================================================================================
// ENTITY SNAPSHOT — top-level: one per Goap-bearing entity in the world.
// ====================================================================================================================

struct FCkGoapDebugger_EntitySnapshot
{
    // The owner entity that ultimately carries the Goap feature (NPC, pawn).
    FCk_Handle      EntityHandle;
    FString         DebugName;        // owner's display name (label tag, etc.)
    FCk_Handle_Goap GoapHandle;       // the Goap root for this entity

    TArray<FCkGoapDebugger_ActionSetInfo> ActionSets;

    int64  FrameNumber      = 0;
    double WorldTimeSeconds = 0.0;
};

// ====================================================================================================================
// HISTORY EVENT — entries on the bottom rail / scrub timeline. Each kind tags
// the source so the rail can colour-code. SnapshotAtEvent captures the
// ActionSet's state at fire-time for scrub-mode inspection.
// ====================================================================================================================

enum class ECkGoapDebugger_HistoryEventKind : uint8
{
    ActionSetEnabled,
    ActionSetDisabled,
    ChainActivated,
    ActionActivated,
    ActionDeactivated,
    PlanFound,
    PlanFailed,
    ChainReset
};

struct FCkGoapDebugger_HistoryEvent
{
    ECkGoapDebugger_HistoryEventKind Kind = ECkGoapDebugger_HistoryEventKind::ChainActivated;

    FCk_Handle_Goap_ActionSet ActionSetHandle;
    FCk_Handle_Goap_Action    ActionHandle;

    FString Title;
    FString Meta;

    double  WorldTimeSeconds = 0.0;
    int64   FrameNumber      = 0;

    // Snapshot of the ActionSet at the moment this event fired.
    // Held by shared-ptr so the events list can share a single allocation
    // across rail + scrub views.
    TSharedPtr<FCkGoapDebugger_ActionSetInfo> SnapshotAtEvent;
};

// ====================================================================================================================
