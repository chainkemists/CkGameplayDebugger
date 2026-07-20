#include "Misc/AutomationTest.h"

#include "CkGoapDebugger/Data/CkGoapDebugger_DecisionModel.h"

// --------------------------------------------------------------------------------------------------------------------
// Fixtures mirror the FEAR gym catalog (CkTests/Script/CkGoap/GymFEAR) and the
// scenario truth-table validated in Mockups/mockup_d_mission_control.html.
// --------------------------------------------------------------------------------------------------------------------

namespace ck_goap_debugger_decision_model_spec
{
    // The model keys on FName (registration-free) precisely so specs can
    // construct fixtures without touching the project's gameplay-tag table.
    const auto Tag_HasAmmo     = FName{TEXT("GoapDebuggerSpec.HasAmmo")};
    const auto Tag_Reserve     = FName{TEXT("GoapDebuggerSpec.Reserve")};
    const auto Tag_Visible     = FName{TEXT("GoapDebuggerSpec.Visible")};
    const auto Tag_Behind      = FName{TEXT("GoapDebuggerSpec.Behind")};
    const auto Tag_AtCover     = FName{TEXT("GoapDebuggerSpec.AtCover")};
    const auto Tag_Heard       = FName{TEXT("GoapDebuggerSpec.Heard")};
    const auto Tag_Neutralized = FName{TEXT("GoapDebuggerSpec.Neutralized")};
    const auto Tag_Patrolling  = FName{TEXT("GoapDebuggerSpec.Patrolling")};

    enum { AttackEnemy = 0, Reload, Investigate, TakeCover, Flank, Patrol, WaitForEnemy };

    auto MakeDef(
        const TCHAR* InName,
        std::initializer_list<TPair<FName, bool>> InPre,
        std::initializer_list<TPair<FName, bool>> InEff,
        float InCost,
        bool InComposite = false) -> FCkGoapDebugger_ActionDefLite
    {
        auto Def = FCkGoapDebugger_ActionDefLite{};
        Def.ClassName = InName;
        for (const auto& [Key, Value] : InPre) { Def.Preconditions.Add(Key, Value); }
        for (const auto& [Key, Value] : InEff) { Def.Effects.Add(Key, Value); }
        Def.Cost = InCost;
        Def.IsComposite = InComposite;
        return Def;
    }

    auto TopCatalog() -> TArray<FCkGoapDebugger_ActionDefLite>
    {
        return {
            MakeDef(TEXT("AttackEnemy"),  {{Tag_HasAmmo, true}, {Tag_Visible, true}}, {{Tag_Neutralized, true}}, 1.0f, true),
            MakeDef(TEXT("Reload"),       {{Tag_Reserve, true}},                      {{Tag_HasAmmo, true}},     0.5f),
            MakeDef(TEXT("Investigate"),  {{Tag_Heard, true}},                        {{Tag_Visible, true}},     1.5f),
            MakeDef(TEXT("TakeCover"),    {},                                         {{Tag_AtCover, true}},     1.0f),
            MakeDef(TEXT("Flank"),        {{Tag_Visible, true}},                      {{Tag_Behind, true}},      2.0f),
            MakeDef(TEXT("Patrol"),       {},                                         {{Tag_Patrolling, true}},  3.0f),
            MakeDef(TEXT("WaitForEnemy"), {},                                         {{Tag_Neutralized, true}}, 999.0f),
        };
    }

    auto SubCatalog() -> TArray<FCkGoapDebugger_ActionDefLite>
    {
        return {
            MakeDef(TEXT("AttackFromFlank"), {{Tag_Behind, true}, {Tag_HasAmmo, true}, {Tag_Visible, true}},  {{Tag_Neutralized, true}}, 0.5f),
            MakeDef(TEXT("AttackFromCover"), {{Tag_AtCover, true}, {Tag_HasAmmo, true}, {Tag_Visible, true}}, {{Tag_Neutralized, true}}, 1.0f),
            MakeDef(TEXT("AttackOpen"),      {{Tag_HasAmmo, true}, {Tag_Visible, true}},                      {{Tag_Neutralized, true}}, 2.0f),
            MakeDef(TEXT("Standby"),         {},                                                              {{Tag_Neutralized, true}}, 999.0f),
        };
    }

    auto Goal() -> TMap<FName, bool>
    {
        auto Result = TMap<FName, bool>{};
        Result.Add(Tag_Neutralized, true);
        return Result;
    }

    auto WS(std::initializer_list<FName> InTrueKeys) -> TMap<FName, bool>
    {
        auto Result = TMap<FName, bool>{};
        for (const auto& Key : InTrueKeys) { Result.Add(Key, true); }
        return Result;
    }

    auto StepNames(
        const TArray<FCkGoapDebugger_ActionDefLite>& InDefs,
        const FCkGoapDebugger_PlanLite& InPlan) -> FString
    {
        auto Parts = TArray<FString>{};
        for (const auto& StepIndex : InPlan.StepDefIndices) { Parts.Add(InDefs[StepIndex].ClassName); }
        return FString::Join(Parts, TEXT(" -> "));
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkGoapDebuggerDecisionModel_PlanScenarios,
    "CkGoapDebugger.Decision.Plan.FearScenarioTable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkGoapDebuggerDecisionModel_PlanScenarios::RunTest(const FString&)
{
    using namespace ck_goap_debugger_decision_model_spec;
    using namespace ck_goap_debugger_decision_model;

    const auto Defs = TopCatalog();
    const auto TheGoal = Goal();

    // s1 — spawn: no contact → fallback wins at 999.
    {
        const auto Result = Plan(WS({Tag_HasAmmo, Tag_Reserve}), Defs, TheGoal);
        TestTrue(TEXT("s1 found"), Result.Found);
        TestEqual(TEXT("s1 plan"), StepNames(Defs, Result), TEXT("WaitForEnemy"));
        TestEqual(TEXT("s1 cost"), Result.TotalCost, 999.0f);
    }

    // s2 — heard a sound: Investigate converts sound to sight.
    {
        const auto Result = Plan(WS({Tag_HasAmmo, Tag_Reserve, Tag_Heard}), Defs, TheGoal);
        TestEqual(TEXT("s2 plan"), StepNames(Defs, Result), TEXT("Investigate -> AttackEnemy"));
        TestEqual(TEXT("s2 cost"), Result.TotalCost, 2.5f);
    }

    // s3 — contact but dry mag: Reload chains in front.
    {
        const auto Result = Plan(WS({Tag_Reserve, Tag_Heard, Tag_Visible}), Defs, TheGoal);
        TestEqual(TEXT("s3 plan"), StepNames(Defs, Result), TEXT("Reload -> AttackEnemy"));
        TestEqual(TEXT("s3 cost"), Result.TotalCost, 1.5f);
    }

    // s4/s5 — sub-planner expansion: open attack without advantages, flank with.
    {
        const auto Sub = SubCatalog();
        const auto Open = Plan(WS({Tag_HasAmmo, Tag_Reserve, Tag_Heard, Tag_Visible}), Sub, TheGoal);
        TestEqual(TEXT("s4 sub"), StepNames(Sub, Open), TEXT("AttackOpen"));
        TestEqual(TEXT("s4 sub cost"), Open.TotalCost, 2.0f);

        const auto Flanked = Plan(WS({Tag_HasAmmo, Tag_Reserve, Tag_Heard, Tag_Visible, Tag_Behind}), Sub, TheGoal);
        TestEqual(TEXT("s5 sub"), StepNames(Sub, Flanked), TEXT("AttackFromFlank"));
        TestEqual(TEXT("s5 sub cost"), Flanked.TotalCost, 0.5f);
    }

    // Sandbox — visibility overridden off at s5: falls back to the sound chain.
    {
        auto Sandbox = WS({Tag_HasAmmo, Tag_Reserve, Tag_Heard, Tag_Behind});
        Sandbox.Add(Tag_Visible, false);
        const auto Result = Plan(Sandbox, Defs, TheGoal);
        TestEqual(TEXT("sandbox plan"), StepNames(Defs, Result), TEXT("Investigate -> AttackEnemy"));
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkGoapDebuggerDecisionModel_Scoring,
    "CkGoapDebugger.Decision.Score.CandidateStates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkGoapDebuggerDecisionModel_Scoring::RunTest(const FString&)
{
    using namespace ck_goap_debugger_decision_model_spec;
    using namespace ck_goap_debugger_decision_model;

    const auto Defs = TopCatalog();
    const auto TheGoal = Goal();

    // s5 world: everything true except AtCover / Patrolling / Neutralized.
    const auto World = WS({Tag_HasAmmo, Tag_Reserve, Tag_Heard, Tag_Visible, Tag_Behind});
    const auto ThePlan = Plan(World, Defs, TheGoal);
    TestEqual(TEXT("chosen plan"), StepNames(Defs, ThePlan), TEXT("AttackEnemy"));

    const auto Scores = ScoreCandidates(World, Defs, TheGoal, ThePlan);
    TestEqual(TEXT("one score per def"), Scores.Num(), Defs.Num());

    TestTrue(TEXT("AttackEnemy in plan"),
        Scores[AttackEnemy].State == ECkGoapDebugger_CandidateState::InPlan && Scores[AttackEnemy].PlanStepIndex == 0);

    TestTrue(TEXT("Reload viable-not-chosen"), Scores[Reload].State == ECkGoapDebugger_CandidateState::ViableNotChosen);
    TestEqual(TEXT("Reload forced total"), Scores[Reload].ForcedFirstTotal, 1.5f);
    TestTrue(TEXT("Reload makes no progress (ammo already true)"), Scores[Reload].MakesNoProgress);

    TestTrue(TEXT("TakeCover viable-not-chosen"), Scores[TakeCover].State == ECkGoapDebugger_CandidateState::ViableNotChosen);
    TestEqual(TEXT("TakeCover forced total"), Scores[TakeCover].ForcedFirstTotal, 2.0f);

    TestTrue(TEXT("WaitForEnemy is fallback"), Scores[WaitForEnemy].State == ECkGoapDebugger_CandidateState::Fallback);

    // Blocked case — drop the reserve and the ammo: Reload's precondition goes unmet.
    {
        const auto DryWorld = WS({Tag_Visible});
        const auto DryPlan  = Plan(DryWorld, Defs, TheGoal);
        TestEqual(TEXT("dry plan is fallback"), StepNames(Defs, DryPlan), TEXT("WaitForEnemy"));

        const auto DryScores = ScoreCandidates(DryWorld, Defs, TheGoal, DryPlan);
        TestTrue(TEXT("Reload blocked"), DryScores[Reload].State == ECkGoapDebugger_CandidateState::Blocked);
        TestEqual(TEXT("Reload unmet count"), DryScores[Reload].UnmetPreconditions.Num(), 1);
        TestTrue(TEXT("Reload unmet key is Reserve"), DryScores[Reload].UnmetPreconditions[0].Key == Tag_Reserve);

        TestTrue(TEXT("AttackEnemy blocked (no ammo)"),
            DryScores[AttackEnemy].State == ECkGoapDebugger_CandidateState::Blocked);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkGoapDebuggerDecisionModel_CrossTierAndLint,
    "CkGoapDebugger.Decision.Lint.CrossTierAndDeadEffects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkGoapDebuggerDecisionModel_CrossTierAndLint::RunTest(const FString&)
{
    using namespace ck_goap_debugger_decision_model_spec;
    using namespace ck_goap_debugger_decision_model;

    const auto Defs = TopCatalog();
    const auto TheGoal = Goal();

    auto SubByComposite = TMap<int32, TArray<FCkGoapDebugger_ActionDefLite>>{};
    SubByComposite.Add(AttackEnemy, SubCatalog());

    // ---- Cross-tier notes at s4 (no cover, not behind): TakeCover 2.0→1.0, Flank 2.0→0.5 ----
    {
        const auto World = WS({Tag_HasAmmo, Tag_Reserve, Tag_Heard, Tag_Visible});
        const auto Notes = ComputeCrossTierNotes(World, Defs, SubByComposite, TheGoal);

        const auto* CoverNote = Notes.FindByPredicate([](const auto& InNote) { return InNote.CandidateDefIndex == TakeCover; });
        TestTrue(TEXT("TakeCover note exists"), CoverNote != nullptr);
        if (CoverNote != nullptr)
        {
            TestEqual(TEXT("cover sub now"), CoverNote->SubCostNow, 2.0f);
            TestEqual(TEXT("cover sub if"),  CoverNote->SubCostIfApplied, 1.0f);
        }

        const auto* FlankNote = Notes.FindByPredicate([](const auto& InNote) { return InNote.CandidateDefIndex == Flank; });
        TestTrue(TEXT("Flank note exists"), FlankNote != nullptr);
        if (FlankNote != nullptr)
        {
            TestEqual(TEXT("flank sub if"), FlankNote->SubCostIfApplied, 0.5f);
        }
    }

    // ---- Lint: fallback present; Flank + TakeCover cross-tier-only; Patrol dead effect ----
    {
        const auto Findings = Lint(Defs, SubByComposite, TheGoal);

        TestTrue(TEXT("fallback present"), Findings.ContainsByPredicate([](const auto& InFinding)
        {
            return InFinding.Kind == ECkGoapDebugger_LintKind::FallbackPresent && InFinding.DefIndex == WaitForEnemy;
        }));

        TestTrue(TEXT("Flank cross-tier"), Findings.ContainsByPredicate([](const auto& InFinding)
        {
            return InFinding.Kind == ECkGoapDebugger_LintKind::CrossTierUnreachable && InFinding.DefIndex == Flank;
        }));

        // The mockup's hand audit flagged only Flank; the census correctly also
        // catches TakeCover (AtCover is consumed only by AttackFromCover).
        TestTrue(TEXT("TakeCover cross-tier"), Findings.ContainsByPredicate([](const auto& InFinding)
        {
            return InFinding.Kind == ECkGoapDebugger_LintKind::CrossTierUnreachable && InFinding.DefIndex == TakeCover;
        }));

        TestTrue(TEXT("Patrol dead effect"), Findings.ContainsByPredicate([](const auto& InFinding)
        {
            return InFinding.Kind == ECkGoapDebugger_LintKind::DeadEffect && InFinding.DefIndex == Patrol;
        }));
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
