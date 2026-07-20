#pragma once

#include "CoreMinimal.h"


// --------------------------------------------------------------------------------------------------------------------
// Pure (Slate-free, ECS-free) decision-scoring + catalog-lint transforms for the GOAP debugger's
// Decision panel and Catalog Audit tab. Operates on lightweight mirrors of the collected action
// definitions plus a (tag → bool) world-state map, so it is unit-testable headlessly — see
// Tests/CkGoapDebugger_DecisionModel.spec.cpp, whose fixtures encode the FEAR-gym truth table
// from Mockups/mockup_d_mission_control.html.
//
// The embedded planner is a faithful miniature of ck::goap's regressive A* (constraint sets traded
// backward from the goal; heuristic = unsatisfied-count) in tag-keyed form. It exists so the
// debugger can answer "what would the plan cost if X were forced first / if this key flipped"
// WITHOUT round-tripping through the engine's time-sliced pipeline. It must never drive gameplay.
// --------------------------------------------------------------------------------------------------------------------

struct FCkGoapDebugger_ActionDefLite
{
    FString ClassName;
    TMap<FName, bool> Preconditions;
    TMap<FName, bool> Effects;
    float Cost = 1.0f;
    bool  IsComposite = false;

    // Fallback convention: no preconditions + cost at/above the floor (the
    // "always-valid-plan" tenet actions, e.g. WaitForEnemy at 999).
    auto Get_IsFallback() const -> bool;
};

// One entry per plan step; index into the defs array handed to Plan().
struct FCkGoapDebugger_PlanLite
{
    TArray<int32> StepDefIndices;
    float  TotalCost = 0.0f;
    int32  ExpandedCount = 0;
    int32  PoolSize = 0;
    bool   Found = false;
};

enum class ECkGoapDebugger_CandidateState : uint8
{
    InPlan,
    ViableNotChosen,
    Blocked,
    Fallback,
};

struct FCkGoapDebugger_CandidateScore
{
    int32 DefIndex = INDEX_NONE;
    ECkGoapDebugger_CandidateState State = ECkGoapDebugger_CandidateState::Blocked;

    // InPlan: position in the plan (0-based).
    int32 PlanStepIndex = INDEX_NONE;

    // ViableNotChosen: total plan cost if this action were forced first, and
    // the delta vs the chosen plan.
    float ForcedFirstTotal = 0.0f;
    float DeltaVsPlan = 0.0f;

    // ViableNotChosen: true when forcing this action makes no progress (its
    // effects are already true, or they don't shorten the path at this tier).
    bool  MakesNoProgress = false;

    // Blocked: the unmet (key, required-value) pairs, in catalog order.
    TArray<TPair<FName, bool>> UnmetPreconditions;
};

// Cross-tier coaching note for a top-tier candidate whose effects change what a
// composite's sub-planner would pick ("Take Cover would flip Attack Enemy's
// expansion from 2.0 to 1.0 — invisible to this tier").
struct FCkGoapDebugger_CrossTierNote
{
    int32 CandidateDefIndex = INDEX_NONE;
    int32 CompositeDefIndex = INDEX_NONE;
    float SubCostNow = 0.0f;
    float SubCostIfApplied = 0.0f;
};

enum class ECkGoapDebugger_LintKind : uint8
{
    FallbackPresent,        // PASS — catalog has an unconditional goal-covering action
    FallbackMissing,        // FAIL — the always-valid-plan tenet is violated
    CrossTierUnreachable,   // INFO — action's effect is only consumed inside a sub-catalog
    DeadEffect,             // INFO — effect consumed by no precondition and no goal key
};

struct FCkGoapDebugger_LintFinding
{
    ECkGoapDebugger_LintKind Kind = ECkGoapDebugger_LintKind::FallbackPresent;
    int32 DefIndex = INDEX_NONE;          // the action the finding is about (when applicable)
    FName Key;                     // the key the finding is about (when applicable)
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_goap_debugger_decision_model
{
    // Fallback detection floor — matches the framework convention (999).
    inline constexpr float k_FallbackCostFloor = 900.0f;

    // Node-expansion safety valve for the miniature search.
    inline constexpr int32 k_MaxExpandedNodes = 512;

    // Regressive A* over the given defs toward InGoal from InWorldState (absent
    // keys read false, matching classical GOAP).
    CKGOAPDEBUGGER_API auto Plan(
        const TMap<FName, bool>& InWorldState,
        const TArray<FCkGoapDebugger_ActionDefLite>& InDefs,
        const TMap<FName, bool>& InGoal) -> FCkGoapDebugger_PlanLite;

    // Score every def against the chosen plan: in-plan position, forced-first
    // totals for viable-but-not-chosen, unmet keys for blocked, fallbacks.
    CKGOAPDEBUGGER_API auto ScoreCandidates(
        const TMap<FName, bool>& InWorldState,
        const TArray<FCkGoapDebugger_ActionDefLite>& InDefs,
        const TMap<FName, bool>& InGoal,
        const FCkGoapDebugger_PlanLite& InChosenPlan) -> TArray<FCkGoapDebugger_CandidateScore>;

    // For each non-composite top-tier candidate whose effects would change a
    // composite's sub-plan cost, emit a coaching note. InSubDefsByComposite maps
    // top-tier composite def index → that composite's sub-catalog.
    CKGOAPDEBUGGER_API auto ComputeCrossTierNotes(
        const TMap<FName, bool>& InWorldState,
        const TArray<FCkGoapDebugger_ActionDefLite>& InTopDefs,
        const TMap<int32, TArray<FCkGoapDebugger_ActionDefLite>>& InSubDefsByComposite,
        const TMap<FName, bool>& InGoal) -> TArray<FCkGoapDebugger_CrossTierNote>;

    // Catalog lint: fallback guarantee, cross-tier reachability, dead effects.
    CKGOAPDEBUGGER_API auto Lint(
        const TArray<FCkGoapDebugger_ActionDefLite>& InTopDefs,
        const TMap<int32, TArray<FCkGoapDebugger_ActionDefLite>>& InSubDefsByComposite,
        const TMap<FName, bool>& InGoal) -> TArray<FCkGoapDebugger_LintFinding>;
}

// --------------------------------------------------------------------------------------------------------------------
