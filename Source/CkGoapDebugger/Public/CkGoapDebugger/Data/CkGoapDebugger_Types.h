#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkGoap/CkGoap_Fragment_Data.h"             // ECk_GoapPlanStatus, FCk_GoapWS_Condition_Authored, FCk_GoapDiagnostic_DependencyCycle
#include "CkGoap/Action/CkGoap_Action_Fragment_Data.h"   // FCk_Handle_Goap_Action
#include "CkGoap/Planner/CkGoap_Planner_Fragment_Data.h" // FCk_Handle_Goap_Planner
#include "CkGoap/WorldState/CkGoap_WorldState_Fragment_Data.h"  // FCk_Handle_Goap_WorldState
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
// U11.7-A reshape (Planner/Action collapse):
//   Per-Planner tree model. Two role discriminators per spec §2.1:
//     - FFragment_Goap_Planner_Params  → Planner role  (carries goal + plan)
//     - FFragment_Goap_Action_Params   → Action role   (carries preconditions / effects / cost)
//   A single entity can carry both (mid-tier composite Actions are dual-role).
//   The top-level Planner forest is built per owner; sub-Planners hang off the
//   tree via the Action_Tree fragment's _ParentAction reference.
//
// Legacy shim:
//   The pre-U11 ActionSet/Action shape (FCkGoapDebugger_ActionSetInfo etc.) is
//   retained alongside the new shape so the existing Slate widgets (Sidebar,
//   PrimaryPane, GraphPane, ...) keep rendering. Dispatches U11.7-B/C/D
//   migrate the widgets onto the new shape and remove the legacy structs.
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
//   Root    : Action is the Planner's root.
//   Mid     : Active chain entry between root and leaf.
//   Leaf    : Deepest entry in the active chain.
//   Catalog : Registered Action that is not currently in the active chain.
//
// (Legacy enum — preserved for the existing widget rendering pass; the
//  per-Planner model expresses role via IsPlannerRole / IsActionRole /
//  IsInActiveChain flags on PlannerInfo and ActionInfo.)
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
//
// U11.7-A: gained IsPlannerRole / IsActionRole / ParentPlanner alongside the
// legacy fields. For a dual-role entity (Action that is also a Planner) the
// matching FCkGoapDebugger_PlannerInfo is reachable via PlannerInfo lookup by
// handle on the owner EntitySnapshot.
// ====================================================================================================================

struct FCkGoapDebugger_ActionInfo
{
    // Identity ----------------------------------------------------------------
    FCk_Handle_Goap_Action                     Handle;
    TSubclassOf<UCk_GoapAction_EntityScript>   ActionClass;
    FString                                    ClassName;
    FGameplayTag                               ActionTag;          // class-derived

    // U11.7-A: role badges. IsActionRole is by definition true for ActionInfo.
    bool                                       IsActionRole  = true;
    bool                                       IsPlannerRole = false;  // dual-role if true

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
    // Resolved _Goal — per-Planner, authored at construction (PlannerParams._Goal)
    // or via Request_SetGoal. Independent of any Action-role effects this entity
    // may carry (U11.1 removed the goal=effects coupling).
    TArray<FCkGoapDebugger_Condition> Goal;
    // Effects referencing unregistered WS keys — populated at Setup.
    TArray<FCkGoapDebugger_Condition> InvalidGoal;
    // Human-readable label for the resolved WS source.
    FString                           WorldStateSourceLabel;

    // Tree position -----------------------------------------------------------
    FCk_Handle_Goap_Action                ParentActionHandle;   // invalid for root
    FString                               ParentClassName;      // empty for root
    TArray<FCk_Handle_Goap_Action>        ChildActionHandles;   // registered children

    // U11.7-A: parent Planner reference (per spec §2.5 — derived from
    // _ParentAction). Invalid for top-level Actions; for non-top-level Actions
    // this is the Planner that picks this Action as a plan step. May or may
    // not coincide with the owning Planner the Action is registered under.
    FCk_Handle_Goap_Planner               ParentPlanner;

    // Chain position ----------------------------------------------------------
    ECkGoapDebugger_ActionRole Role            = ECkGoapDebugger_ActionRole::Catalog;
    bool                       IsInActiveChain = false;
    int32                      ChainDepth      = -1;             // 0 = root, -1 = catalog
};

// ====================================================================================================================
// PLANNER INFO — per-Planner snapshot (U11.7-A, spec §2.1 + §3.4).
//
// A Planner is an entity carrying FFragment_Goap_Planner_Params. It may
// additionally carry the Action role (dual-role mid-tier composite). The
// active-chain walk is now implicit — derived from PlanHandles[0] chains
// through child Planners; see ck::UCk_Utils_Goap_Planner_UE::Get_ActiveChain.
//
// ChildPlanners is the sub-forest underneath this Planner. ChildActions is
// the catalog of every direct child Action (atomic + composite). Composite
// (dual-role) children appear in BOTH lists so that consumers walking either
// view see the same content.
// ====================================================================================================================

struct FCkGoapDebugger_PlannerInfo
{
    // Identity ----------------------------------------------------------------
    FCk_Handle_Goap_Planner PlannerHandle;
    FGameplayTag            PlannerTag;
    FString                 DisplayName;

    // Role badges (spec §2.1). IsPlannerRole is by definition true.
    bool                    IsPlannerRole = true;
    bool                    IsActionRole  = false;   // dual-role if true

    // Tree --------------------------------------------------------------------
    // Invalid for top-level Planners. Walked from the Action role's
    // _ParentAction → matching Planner role on that parent entity.
    FCk_Handle_Goap_Planner                ParentPlanner;
    TArray<FCkGoapDebugger_PlannerInfo>    ChildPlanners;
    TArray<FCkGoapDebugger_ActionInfo>     ChildActions;

    // Plan + goal state (Planner role) ---------------------------------------
    TArray<FCk_Handle_Goap_Action>         PlanHandles;            // Plan[0] is current step
    TArray<FString>                        PlanClassNames;         // parallel display strings
    float                                  PlanCost                = 0.0f;
    ECk_GoapPlanStatus                     PlanStatus              = ECk_GoapPlanStatus::Idle;
    int32                                  PlanAttemptCount        = 0;

    // Goal --------------------------------------------------------------------
    TArray<FCk_GoapWS_Condition_Authored>      GoalAuthored;
    TArray<FCkGoapDebugger_Condition>          GoalResolved;
    TArray<FCk_GoapWS_Condition_Authored>      InvalidGoalAuthored;

    // WS source + resolved ---------------------------------------------------
    FCk_Handle_Goap_WorldState                 WorldStateSourceOverride;
    FCk_Handle_Goap_WorldState                 WorldStateSourceResolved;
    FString                                    WorldStateSourceLabel;
    TArray<FCkGoapDebugger_WorldStateEntry>    WorldState;

    // Diagnostics ------------------------------------------------------------
    TArray<FCk_GoapDiagnostic_DependencyCycle> DependencyCycles;
    TArray<FCkGoapDebugger_CycleInfo>          DependencyCyclesDisplay;

    // Enable / activation ----------------------------------------------------
    ECk_EnableDisable                          EnableToggle    = ECk_EnableDisable::Enable;
    bool                                       IsActive        = false;
    bool                                       IsInActiveChain = false;

    // Action role fields (only valid if IsActionRole — populated from the
    // dual-role entity's FFragment_Goap_Action_Definition).
    TArray<FCkGoapDebugger_Condition>          Preconditions;
    TArray<FCkGoapDebugger_Condition>          Effects;
    float                                      Cost = 0.0f;
};

// ====================================================================================================================
// ACTION SET INFO — legacy shim (pre-U11 ActionSet/Action shape).
//
// Retained so the existing Slate widgets keep rendering. Each top-level
// Planner produces ONE ActionSetInfo synthesized from its PlannerInfo:
//   - Handle           = Planner handle (cast to Planner typesafe).
//   - RootActionHandle = Planner's root Action handle (FFragment_Goap_Planner_Current::_RootAction).
//   - Catalog          = every Action registered under the Planner (recursively).
//   - ActiveChainHandles = Get_ActiveChain output (Plan[0] walk).
//
// Dispatches U11.7-B/C/D will retire this struct in favour of PlannerInfo.
// ====================================================================================================================

struct FCkGoapDebugger_ActionSetInfo
{
    FCk_Handle_Goap_Planner Handle;
    FString                   DebugName;        // PlannerTag.LeafAsString()
    FGameplayTag              ActionSetTag;
    ECk_EnableDisable         EnableToggle = ECk_EnableDisable::Enable;

    // Catalog of ALL registered Actions (active + dormant), ordered by
    // tree-walk from the root for stable display.
    TArray<FCkGoapDebugger_ActionInfo> Catalog;

    // Ordered list of currently-active Action handles. [0] is the root.
    TArray<FCk_Handle_Goap_Action>     ActiveChainHandles;

    FCk_Handle_Goap_Action             RootActionHandle;
    FString                            WorldStateSourceLabel;
    // Resolved WS handle for this Planner. Used by the WS rail to consult /
    // mutate the override stack live (Push_Override_SingleKey, etc.).
    FCk_Handle_Goap_WorldState         WorldStateHandle;
    TArray<FCkGoapDebugger_WorldStateEntry> WorldState;

    TArray<FCkGoapDebugger_CycleInfo>  DependencyCycles;
};

// ====================================================================================================================
// ENTITY SNAPSHOT — top-level: one per Goap-bearing entity in the world.
//
// U11.7-A: carries both shapes during the migration window.
//   - TopLevelPlanners : per-spec forest of top-level Planners (PlannerInfo trees).
//   - ActionSets       : legacy shim — one entry per top-level Planner.
// ====================================================================================================================

struct FCkGoapDebugger_EntitySnapshot
{
    // The owner entity that ultimately carries the Goap feature (NPC, pawn).
    FCk_Handle      EntityHandle;
    FString         DebugName;        // owner's display name (label tag, etc.)
    FCk_Handle_Goap_Planner GoapHandle;       // first top-level Planner — legacy field

    // New shape (spec §2.1).
    TArray<FCkGoapDebugger_PlannerInfo> TopLevelPlanners;

    // Legacy shape — synthesized from TopLevelPlanners for widget compatibility.
    TArray<FCkGoapDebugger_ActionSetInfo> ActionSets;

    int64  FrameNumber      = 0;
    double WorldTimeSeconds = 0.0;
};

// ====================================================================================================================
// HISTORY EVENT — entries on the bottom rail / scrub timeline. Each kind tags
// the source so the rail can colour-code. SnapshotAtEvent captures the
// Planner's state at fire-time for scrub-mode inspection.
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

    FCk_Handle_Goap_Planner ActionSetHandle;
    FCk_Handle_Goap_Action    ActionHandle;

    FString Title;
    FString Meta;

    double  WorldTimeSeconds = 0.0;
    int64   FrameNumber      = 0;

    // Snapshot of the Planner/ActionSet at the moment this event fired.
    // Held by shared-ptr so the events list can share a single allocation
    // across rail + scrub views.
    TSharedPtr<FCkGoapDebugger_ActionSetInfo> SnapshotAtEvent;
};

// ====================================================================================================================
