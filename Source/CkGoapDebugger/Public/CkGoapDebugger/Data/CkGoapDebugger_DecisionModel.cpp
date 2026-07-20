#include "CkGoapDebugger_DecisionModel.h"

#include "CkCore/Macros/CkMacros.h"

#include "Algo/Reverse.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkGoapDebugger_ActionDefLite::
    Get_IsFallback() const
    -> bool
{
    return Preconditions.IsEmpty() && Cost >= ck_goap_debugger_decision_model::k_FallbackCostFloor;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_goap_debugger_decision_model_internal
{
    // Canonical string key for a constraint set — sorted "Tag=0/1" joins.
    auto ConstraintKey(const TMap<FName, bool>& InConstraints) -> FString
    {
        auto Parts = TArray<FString>{};
        Parts.Reserve(InConstraints.Num());
        for (const auto& [Tag, Value] : InConstraints)
        { Parts.Add(FString::Printf(TEXT("%s=%d"), *Tag.ToString(), Value ? 1 : 0)); }
        Parts.Sort();
        return FString::Join(Parts, TEXT("|"));
    }

    auto ReadWS(const TMap<FName, bool>& InWorldState, FName InKey) -> bool
    {
        const auto* Found = InWorldState.Find(InKey);
        return Found != nullptr && *Found;
    }

    auto CountUnsatisfied(
        const TMap<FName, bool>& InConstraints,
        const TMap<FName, bool>& InWorldState) -> int32
    {
        auto Count = 0;
        for (const auto& [Tag, Required] : InConstraints)
        {
            if (ReadWS(InWorldState, Tag) != Required) { ++Count; }
        }
        return Count;
    }

    struct FSearchNode
    {
        TMap<FName, bool> Constraints;
        float G = 0.0f;
        int32 H = 0;
        TArray<int32> PathDefIndices;   // reverse execution order (goal-side first)
    };
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck_goap_debugger_decision_model::
    Plan(
        const TMap<FName, bool>& InWorldState,
        const TArray<FCkGoapDebugger_ActionDefLite>& InDefs,
        const TMap<FName, bool>& InGoal)
    -> FCkGoapDebugger_PlanLite
{
    using namespace ck_goap_debugger_decision_model_internal;

    auto Result = FCkGoapDebugger_PlanLite{};

    auto Open = TArray<FSearchNode>{};
    auto BestG = TMap<FString, float>{};

    auto Start = FSearchNode{};
    Start.Constraints = InGoal;
    Start.H = CountUnsatisfied(InGoal, InWorldState);
    BestG.Add(ConstraintKey(Start.Constraints), 0.0f);
    Open.Add(MoveTemp(Start));

    while (NOT Open.IsEmpty())
    {
        // Small frontier — a linear best-pick keeps the code simple and the
        // catalogs this runs on are tiny (≤ tens of actions).
        auto BestIndex = 0;
        for (auto Index = 1; Index < Open.Num(); ++Index)
        {
            const auto FBest = Open[BestIndex].G + Open[BestIndex].H;
            const auto FThis = Open[Index].G + Open[Index].H;
            if (FThis < FBest) { BestIndex = Index; }
        }

        const auto Node = Open[BestIndex];
        Open.RemoveAtSwap(BestIndex);
        ++Result.ExpandedCount;

        if (Node.H == 0)
        {
            // Path is goal-side-first; execution order is the reverse.
            Result.StepDefIndices = Node.PathDefIndices;
            Algo::Reverse(Result.StepDefIndices);
            Result.TotalCost = Node.G;
            Result.PoolSize = BestG.Num();
            Result.Found = true;
            return Result;
        }

        if (Result.ExpandedCount >= k_MaxExpandedNodes) { break; }

        for (auto DefIndex = 0; DefIndex < InDefs.Num(); ++DefIndex)
        {
            const auto& Def = InDefs[DefIndex];

            // Relevant iff some effect resolves a constraint; conflicting iff
            // some effect contradicts one.
            auto Relevant = false;
            auto Conflict = false;
            for (const auto& [Tag, Value] : Def.Effects)
            {
                if (const auto* Required = Node.Constraints.Find(Tag))
                {
                    if (*Required == Value) { Relevant = true; }
                    else { Conflict = true; break; }
                }
            }
            if (NOT Relevant || Conflict) { continue; }

            auto Next = FSearchNode{};
            Next.Constraints = Node.Constraints;
            for (const auto& [Tag, Value] : Def.Effects)
            {
                if (const auto* Required = Next.Constraints.Find(Tag); Required != nullptr && *Required == Value)
                { Next.Constraints.Remove(Tag); }
            }

            auto PreConflict = false;
            for (const auto& [Tag, Value] : Def.Preconditions)
            {
                if (const auto* Existing = Next.Constraints.Find(Tag); Existing != nullptr && *Existing != Value)
                { PreConflict = true; break; }
                Next.Constraints.Add(Tag, Value);
            }
            if (PreConflict) { continue; }

            Next.G = Node.G + Def.Cost;
            Next.H = CountUnsatisfied(Next.Constraints, InWorldState);
            Next.PathDefIndices = Node.PathDefIndices;
            Next.PathDefIndices.Add(DefIndex);

            const auto Key = ConstraintKey(Next.Constraints);
            if (const auto* Existing = BestG.Find(Key); Existing != nullptr && *Existing <= Next.G)
            { continue; }
            BestG.Add(Key, Next.G);
            Open.Add(MoveTemp(Next));
        }
    }

    Result.PoolSize = BestG.Num();
    return Result;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck_goap_debugger_decision_model::
    ScoreCandidates(
        const TMap<FName, bool>& InWorldState,
        const TArray<FCkGoapDebugger_ActionDefLite>& InDefs,
        const TMap<FName, bool>& InGoal,
        const FCkGoapDebugger_PlanLite& InChosenPlan)
    -> TArray<FCkGoapDebugger_CandidateScore>
{
    using namespace ck_goap_debugger_decision_model_internal;

    auto Scores = TArray<FCkGoapDebugger_CandidateScore>{};
    Scores.Reserve(InDefs.Num());

    for (auto DefIndex = 0; DefIndex < InDefs.Num(); ++DefIndex)
    {
        const auto& Def = InDefs[DefIndex];

        auto Score = FCkGoapDebugger_CandidateScore{};
        Score.DefIndex = DefIndex;

        const auto PlanStepIndex = InChosenPlan.StepDefIndices.IndexOfByKey(DefIndex);
        if (PlanStepIndex != INDEX_NONE)
        {
            Score.State = ECkGoapDebugger_CandidateState::InPlan;
            Score.PlanStepIndex = PlanStepIndex;
            Scores.Add(MoveTemp(Score));
            continue;
        }

        for (const auto& [Tag, Value] : Def.Preconditions)
        {
            if (ReadWS(InWorldState, Tag) != Value)
            { Score.UnmetPreconditions.Emplace(Tag, Value); }
        }

        const auto ViableNow = Score.UnmetPreconditions.IsEmpty();
        if (NOT ViableNow)
        {
            Score.State = ECkGoapDebugger_CandidateState::Blocked;
            Scores.Add(MoveTemp(Score));
            continue;
        }

        if (Def.Get_IsFallback())
        {
            Score.State = ECkGoapDebugger_CandidateState::Fallback;
            Scores.Add(MoveTemp(Score));
            continue;
        }

        // Viable but not chosen — forward-preview: apply the effects, then
        // re-plan the remainder. Total = own cost + remainder.
        auto ProjectedWS = InWorldState;
        auto AllEffectsAlreadyTrue = true;
        for (const auto& [Tag, Value] : Def.Effects)
        {
            if (ReadWS(InWorldState, Tag) != Value) { AllEffectsAlreadyTrue = false; }
            ProjectedWS.Add(Tag, Value);
        }

        const auto Remainder = Plan(ProjectedWS, InDefs, InGoal);
        Score.State = ECkGoapDebugger_CandidateState::ViableNotChosen;
        Score.ForcedFirstTotal = Def.Cost + Remainder.TotalCost;
        Score.DeltaVsPlan = Score.ForcedFirstTotal - InChosenPlan.TotalCost;
        Score.MakesNoProgress = AllEffectsAlreadyTrue || Remainder.TotalCost >= InChosenPlan.TotalCost;
        Scores.Add(MoveTemp(Score));
    }

    return Scores;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck_goap_debugger_decision_model::
    ComputeCrossTierNotes(
        const TMap<FName, bool>& InWorldState,
        const TArray<FCkGoapDebugger_ActionDefLite>& InTopDefs,
        const TMap<int32, TArray<FCkGoapDebugger_ActionDefLite>>& InSubDefsByComposite,
        const TMap<FName, bool>& InGoal)
    -> TArray<FCkGoapDebugger_CrossTierNote>
{
    auto Notes = TArray<FCkGoapDebugger_CrossTierNote>{};

    for (const auto& [CompositeIndex, SubDefs] : InSubDefsByComposite)
    {
        const auto SubNow = Plan(InWorldState, SubDefs, InGoal);
        if (NOT SubNow.Found) { continue; }

        for (auto DefIndex = 0; DefIndex < InTopDefs.Num(); ++DefIndex)
        {
            if (DefIndex == CompositeIndex) { continue; }
            const auto& Def = InTopDefs[DefIndex];
            if (Def.IsComposite) { continue; }

            auto ProjectedWS = InWorldState;
            for (const auto& [Tag, Value] : Def.Effects)
            { ProjectedWS.Add(Tag, Value); }

            const auto SubIf = Plan(ProjectedWS, SubDefs, InGoal);
            if (NOT SubIf.Found || SubIf.TotalCost >= SubNow.TotalCost) { continue; }

            auto Note = FCkGoapDebugger_CrossTierNote{};
            Note.CandidateDefIndex = DefIndex;
            Note.CompositeDefIndex = CompositeIndex;
            Note.SubCostNow = SubNow.TotalCost;
            Note.SubCostIfApplied = SubIf.TotalCost;
            Notes.Add(MoveTemp(Note));
        }
    }

    return Notes;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck_goap_debugger_decision_model::
    Lint(
        const TArray<FCkGoapDebugger_ActionDefLite>& InTopDefs,
        const TMap<int32, TArray<FCkGoapDebugger_ActionDefLite>>& InSubDefsByComposite,
        const TMap<FName, bool>& InGoal)
    -> TArray<FCkGoapDebugger_LintFinding>
{
    using namespace ck_goap_debugger_decision_model_internal;

    auto Findings = TArray<FCkGoapDebugger_LintFinding>{};

    // ---- Fallback guarantee: some action with no preconditions covers every goal condition ----
    {
        auto FallbackIndex = int32{INDEX_NONE};
        for (auto DefIndex = 0; DefIndex < InTopDefs.Num(); ++DefIndex)
        {
            const auto& Def = InTopDefs[DefIndex];
            if (NOT Def.Preconditions.IsEmpty()) { continue; }

            auto CoversGoal = true;
            for (const auto& [Tag, Value] : InGoal)
            {
                const auto* Effect = Def.Effects.Find(Tag);
                if (Effect == nullptr || *Effect != Value) { CoversGoal = false; break; }
            }
            if (CoversGoal) { FallbackIndex = DefIndex; break; }
        }

        auto Finding = FCkGoapDebugger_LintFinding{};
        Finding.Kind = FallbackIndex != INDEX_NONE
            ? ECkGoapDebugger_LintKind::FallbackPresent
            : ECkGoapDebugger_LintKind::FallbackMissing;
        Finding.DefIndex = FallbackIndex;
        Findings.Add(MoveTemp(Finding));
    }

    // ---- Consumption census across top tier + every sub-catalog + goal ----
    const auto IsConsumedAtTop = [&](FName InKey) -> bool
    {
        if (InGoal.Contains(InKey)) { return true; }
        for (const auto& Def : InTopDefs)
        {
            if (Def.Preconditions.Contains(InKey)) { return true; }
        }
        return false;
    };

    const auto IsConsumedInAnySub = [&](FName InKey) -> bool
    {
        for (const auto& [CompositeIndex, SubDefs] : InSubDefsByComposite)
        {
            for (const auto& Def : SubDefs)
            {
                if (Def.Preconditions.Contains(InKey)) { return true; }
            }
        }
        return false;
    };

    for (auto DefIndex = 0; DefIndex < InTopDefs.Num(); ++DefIndex)
    {
        const auto& Def = InTopDefs[DefIndex];
        for (const auto& [Tag, Value] : Def.Effects)
        {
            const auto ConsumedTop = IsConsumedAtTop(Tag);
            const auto ConsumedSub = IsConsumedInAnySub(Tag);

            if (NOT ConsumedTop && ConsumedSub)
            {
                auto Finding = FCkGoapDebugger_LintFinding{};
                Finding.Kind = ECkGoapDebugger_LintKind::CrossTierUnreachable;
                Finding.DefIndex = DefIndex;
                Finding.Key = Tag;
                Findings.Add(MoveTemp(Finding));
            }
            else if (NOT ConsumedTop && NOT ConsumedSub)
            {
                auto Finding = FCkGoapDebugger_LintFinding{};
                Finding.Kind = ECkGoapDebugger_LintKind::DeadEffect;
                Finding.DefIndex = DefIndex;
                Finding.Key = Tag;
                Findings.Add(MoveTemp(Finding));
            }
        }
    }

    return Findings;
}

// --------------------------------------------------------------------------------------------------------------------
