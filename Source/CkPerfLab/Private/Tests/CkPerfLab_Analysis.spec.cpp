#include "CkPerfLab/Analysis/CkPerfLab_Analysis.h"

#include "CkPerfLab/Stats/CkPerfLab_SampleStats.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_analysis_spec
{
    constexpr auto kFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    auto
        Make_Metrics(
            float InFrameMs,
            float InGameThreadMs,
            float InGpuMs,
            bool InGpuAvailable)
        -> FCk_PerfLab_MetricSet
    {
        const auto Steady = [](float InValue)
        {
            auto Samples = TArray<float>{};
            Samples.Init(InValue, 20);
            return ck::perf_lab::Reduce_Samples(Samples, 3.5f);
        };

        return FCk_PerfLab_MetricSet{}
            .Set_Frame(Steady(InFrameMs))
            .Set_GameThread(Steady(InGameThreadMs))
            .Set_RenderThread(Steady(InFrameMs * 0.5f))
            .Set_RhiThread(Steady(1.0f))
            .Set_Gpu(InGpuAvailable
                ? Steady(InGpuMs)
                : ck::perf_lab::Make_UnavailableStats(
                    ECk_Stats_MetricAvailability::Unavailable_NullRhi, TEXT("headless")));
    }

    /** A position with a census heavy enough to trip any rule, so the gate is the only thing deciding. */
    auto
        Make_Position(
            const FString& InId,
            const FCk_PerfLab_MetricSet& InMetrics,
            bool InHeavyCensus)
        -> FCk_PerfLab_PositionResult
    {
        auto Census = TArray<FCk_PerfLab_ActorCensusRow>{};

        if (InHeavyCensus)
        {
            for (auto Index = 0; Index < 200; ++Index)
            {
                Census.Add(FCk_PerfLab_ActorCensusRow{}
                    .Set_ObjectPath(FString::Printf(TEXT("/Game/Map.Map:PersistentLevel.SM_%03d"), Index))
                    .Set_ClassName(TEXT("StaticMeshActor"))
                    .Set_DistanceCm(static_cast<float>(Index))
                    .Set_TriangleCount(50000)
                    .Set_MaterialSlotCount(4)
                    .Set_LightCount(1)
                    .Set_NiagaraComponentCount(1)
                    .Set_TickEnabled(true)
                    .Set_CastsDynamicShadow(true));
            }
        }

        return FCk_PerfLab_PositionResult{}
            .Set_PositionId(InId)
            .Set_Aggregate(InMetrics)
            .Set_Confidence(FCk_PerfLab_ConfidenceResult{}.Set_Rating(ECk_PerfLab_Confidence::High))
            .Set_ActorCensus(Census);
    }

    auto
        Make_Session(
            const TArray<FCk_PerfLab_PositionResult>& InPositions)
        -> FCk_PerfLab_Session
    {
        return FCk_PerfLab_Session{}.Set_SessionId(TEXT("spec")).Set_Positions(InPositions);
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Gate_UnderBudget_PublishesNothingHoweverHeavy,
    "Ck.PerfLab.Gate.UnderBudget_PublishesNothingHoweverHeavy",
    ck_perf_lab_analysis_spec::kFlags)

bool FCkPerfLab_Gate_UnderBudget_PublishesNothingHoweverHeavy::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_analysis_spec;

    // THE MOAT. Two hundred shadow-casting, ticking, million-triangle actors around a position that
    // measured comfortably inside budget. Every static signal any rule looks for is present and
    // screaming, and the tool must still say nothing, because none of it cost anybody anything.
    const auto Session = Make_Session({Make_Position(TEXT("p_1"), Make_Metrics(4.0f, 2.0f, 3.0f, true), true)});

    const auto Analysis = ck::perf_lab::Analyse_Session(Session, 16.67f);

    TestEqual(TEXT("A heavy but fast position publishes no findings"), Analysis.Get_Findings().Num(), 0);
    TestTrue (TEXT("...and is reported as measured clean"),            Analysis.Get_MeasuredClean());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Gate_OverBudgetWithoutSignal_OnlyAttributesGenerically,
    "Ck.PerfLab.Gate.OverBudgetWithoutSignal_OnlyAttributesGenerically",
    ck_perf_lab_analysis_spec::kFlags)

bool FCkPerfLab_Gate_OverBudgetWithoutSignal_OnlyAttributesGenerically::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_analysis_spec;

    // The other direction: genuinely over budget, with content nearby that clears NO threshold. Nothing may be
    // blamed, and the only publishable finding is the one that admits it does not know.
    //
    // The census is deliberately light rather than empty. An empty one would mean something different and worse —
    // that the survey never saw the level at all — which is its own finding, and conflating the two is exactly the
    // confusion this pair of rules exists to separate.
    const auto Light = TArray<FCk_PerfLab_ActorCensusRow>{FCk_PerfLab_ActorCensusRow{}
        .Set_ObjectPath(TEXT("/Game/Map.Map:PersistentLevel.SM_Lonely"))
        .Set_ClassName(TEXT("StaticMeshActor"))
        .Set_DistanceCm(500.0f)
        .Set_TriangleCount(12)};

    const auto Session = Make_Session({Make_Position(TEXT("p_1"), Make_Metrics(40.0f, 20.0f, 30.0f, true), false)
        .Set_ActorCensus(Light)});

    const auto Analysis = ck::perf_lab::Analyse_Session(Session, 16.67f);

    TestFalse(TEXT("An over-budget position is not measured clean"), Analysis.Get_MeasuredClean());
    TestEqual(TEXT("Exactly one finding is published"),              Analysis.Get_Findings().Num(), 1);

    if (Analysis.Get_Findings().Num() > 0)
    {
        TestEqual(TEXT("...and it is the unattributed one"),
            Analysis.Get_Findings()[0].Get_CheckId(), TEXT("Perf.General.OverBudgetUnattributed"));
        TestEqual(TEXT("It has no contributors to point at"),
            Analysis.Get_Findings()[0].Get_Contributors().Num(), 0);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Gate_NothingSurveyed_IsAMeasurementProblemNotALevelProblem,
    "Ck.PerfLab.Gate.NothingSurveyed_IsAMeasurementProblemNotALevelProblem",
    ck_perf_lab_analysis_spec::kFlags)

bool FCkPerfLab_Gate_NothingSurveyed_IsAMeasurementProblemNotALevelProblem::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_analysis_spec;

    // No actors found near ANY position. Every static signal is then zero by construction, so every over-budget
    // position falls through to the generic fallback and the report reads "your level is slow and I cannot say
    // why" — when the truth is "I never saw your level". Saying so is the difference between a finding a reader
    // can act on and one that quietly wastes their afternoon.
    const auto Session = Make_Session(
    {
        Make_Position(TEXT("p_1"), Make_Metrics(40.0f, 20.0f, 30.0f, true), false),
        Make_Position(TEXT("p_2"), Make_Metrics(40.0f, 20.0f, 30.0f, true), false),
    });

    const auto Analysis = ck::perf_lab::Analyse_Session(Session, 16.67f);

    const auto Surveyed = ck::algo::FindIf(Analysis.Get_Findings(),
        [](const FCk_PerfLab_Finding& InFinding)
        { return InFinding.Get_CheckId() == TEXT("Perf.Survey.NothingSurveyed"); });

    TestTrue(TEXT("An empty survey is reported"), Surveyed.IsSet());

    if (Surveyed.IsSet())
    {
        // Critical, and above the per-position noise: it invalidates the attribution of everything else in the
        // session, so a reader must not have to scroll past twelve fallbacks to reach it.
        TestEqual(TEXT("...as Critical"), Surveyed->Get_Severity(), ECk_PerfLab_Severity::Critical);
        TestEqual(TEXT("...naming the signal it checked"),
            Surveyed->Get_Evidence().Get_Signal(), TEXT("actorsSurveyedNearAnyPosition"));
    }

    TestEqual(TEXT("It sorts to the top"),
        Analysis.Get_Findings()[0].Get_CheckId(), TEXT("Perf.Survey.NothingSurveyed"));

    // A survey that DID see content must not raise it — otherwise the warning is permanent wallpaper.
    const auto Surveyed_Ok = ck::perf_lab::Analyse_Session(
        Make_Session({Make_Position(TEXT("p_1"), Make_Metrics(40.0f, 20.0f, 30.0f, true), true)}), 16.67f);

    TestFalse(TEXT("A survey that saw content does not raise it"),
        ck::algo::AnyOf(Surveyed_Ok.Get_Findings(), [](const FCk_PerfLab_Finding& InFinding)
        { return InFinding.Get_CheckId() == TEXT("Perf.Survey.NothingSurveyed"); }));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Gate_OverBudgetWithSignal_AttributesAndCitesEvidence,
    "Ck.PerfLab.Gate.OverBudgetWithSignal_AttributesAndCitesEvidence",
    ck_perf_lab_analysis_spec::kFlags)

bool FCkPerfLab_Gate_OverBudgetWithSignal_AttributesAndCitesEvidence::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_analysis_spec;

    const auto Session = Make_Session({Make_Position(TEXT("p_1"), Make_Metrics(40.0f, 20.0f, 30.0f, true), true)});

    const auto Analysis = ck::perf_lab::Analyse_Session(Session, 16.67f);

    TestTrue(TEXT("Specific rules fire when both halves of the gate are satisfied"),
        Analysis.Get_Findings().Num() > 1);

    // Every published finding must carry the measurement that justified it. A finding that cannot
    // say what it measured is exactly what this tool exists not to produce.
    for (const auto& Finding : Analysis.Get_Findings())
    {
        TestTrue(*FString::Printf(TEXT("[%s] cites a measurement over budget"), *Finding.Get_CheckId()),
            Finding.Get_Evidence().Get_MeasuredMs() > Finding.Get_Evidence().Get_BudgetMs());

        TestTrue(*FString::Printf(TEXT("[%s] names the signal it matched"), *Finding.Get_CheckId()),
            NOT Finding.Get_Evidence().Get_Signal().IsEmpty());

        TestTrue(*FString::Printf(TEXT("[%s] offers a recommendation"), *Finding.Get_CheckId()),
            Finding.Get_Recommendations().Num() > 0);
    }

    // The unattributed rule is a fallback, not a companion: it must stay silent once something
    // specific has been said about a position.
    TestFalse(TEXT("The generic finding is absent when a specific one fired"),
        ck::algo::AnyOf(Analysis.Get_Findings(), [](const FCk_PerfLab_Finding& InFinding)
        { return InFinding.Get_CheckId() == TEXT("Perf.General.OverBudgetUnattributed"); }));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Gate_UnavailableMetricNeverJustifiesAFinding,
    "Ck.PerfLab.Gate.UnavailableMetricNeverJustifiesAFinding",
    ck_perf_lab_analysis_spec::kFlags)

bool FCkPerfLab_Gate_UnavailableMetricNeverJustifiesAFinding::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_analysis_spec;

    // GPU unavailable, everything else inside budget. No GPU rule may fire off a metric that was
    // never measured — the whole never-zero-as-data contract exists to stop exactly this.
    const auto Session = Make_Session({Make_Position(TEXT("p_1"), Make_Metrics(4.0f, 2.0f, 0.0f, false), true)});

    const auto Analysis = ck::perf_lab::Analyse_Session(Session, 16.67f);

    TestFalse(TEXT("No GPU rule fires from an unavailable GPU metric"),
        ck::algo::AnyOf(Analysis.Get_Findings(), [](const FCk_PerfLab_Finding& InFinding)
        { return InFinding.Get_CheckId().StartsWith(TEXT("Perf.Gpu.")); }));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Score_CleanLevelScoresHighAndSlowLevelScoresLow,
    "Ck.PerfLab.Score.CleanLevelScoresHighAndSlowLevelScoresLow",
    ck_perf_lab_analysis_spec::kFlags)

bool FCkPerfLab_Score_CleanLevelScoresHighAndSlowLevelScoresLow::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_analysis_spec;

    const auto Fast = ck::perf_lab::Compute_Score(
        Make_Session({Make_Position(TEXT("p_1"), Make_Metrics(4.0f, 2.0f, 3.0f, true), false)}), 16.67f);

    const auto Slow = ck::perf_lab::Compute_Score(
        Make_Session({Make_Position(TEXT("p_1"), Make_Metrics(60.0f, 40.0f, 50.0f, true), false)}), 16.67f);

    TestTrue(TEXT("A level inside budget scores near the top"), Fast.Get_Value() >= 95);

    // The slow level does not score near zero, and that is the design rather than a leak: two of the
    // eight components describe how EVENLY the level performs and how well it was measured, and a
    // uniformly slow level that was measured cleanly genuinely scores full marks on both. The score
    // is a weighted composite, not a synonym for frame time, and the component table beside it says
    // exactly where its points came from. What must hold is a large gap...
    TestTrue(TEXT("A slow level scores far below a fast one"), Fast.Get_Value() - Slow.Get_Value() >= 40);

    // ...and that the components which ARE about speed are scored harshly.
    const auto Attainment = ck::algo::FindIf(Slow.Get_Components(),
        [](const FCk_PerfLab_ScoreComponentResult& InResult)
        { return InResult.Get_Component() == ECk_PerfLab_ScoreComponent::AverageFrameAttainment; });

    TestTrue(TEXT("The average-attainment component is present"), Attainment.IsSet());

    if (Attainment.IsSet())
    {
        TestTrue(TEXT("A level 3.6x over budget scores badly on attainment"), Attainment->Get_Value() <= 30);
    }

    TestTrue(TEXT("The score publishes its formula"), NOT Fast.Get_Formula().IsEmpty());
    TestEqual(TEXT("The score publishes all eight components"), Fast.Get_Components().Num(), 8);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Score_UnavailableComponentIsExcludedNotZeroed,
    "Ck.PerfLab.Score.UnavailableComponentIsExcludedNotZeroed",
    ck_perf_lab_analysis_spec::kFlags)

bool FCkPerfLab_Score_UnavailableComponentIsExcludedNotZeroed::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_analysis_spec;

    // Identical fast level, measured once with a GPU and once without. Losing the GPU axis must not
    // cost the level points: a machine that could not report GPU time says nothing about the level.
    const auto WithGpu = ck::perf_lab::Compute_Score(
        Make_Session({Make_Position(TEXT("p_1"), Make_Metrics(4.0f, 2.0f, 3.0f, true), false)}), 16.67f);

    const auto WithoutGpu = ck::perf_lab::Compute_Score(
        Make_Session({Make_Position(TEXT("p_1"), Make_Metrics(4.0f, 2.0f, 0.0f, false), false)}), 16.67f);

    TestTrue(TEXT("A missing GPU measurement does not depress the score"),
        WithoutGpu.Get_Value() >= WithGpu.Get_Value() - 1);

    const auto GpuComponent = ck::algo::FindIf(WithoutGpu.Get_Components(),
        [](const FCk_PerfLab_ScoreComponentResult& InResult)
        { return InResult.Get_Component() == ECk_PerfLab_ScoreComponent::GpuHeadroom; });

    TestTrue(TEXT("The GPU component is present in the table"), GpuComponent.IsSet());

    if (GpuComponent.IsSet())
    {
        TestFalse(TEXT("...marked excluded rather than scored zero"), GpuComponent->Get_Included());
        TestFalse(TEXT("...and it says why"),                        GpuComponent->Get_ExcludedReason().IsEmpty());
        TestEqual(TEXT("...carrying no weight"),                     GpuComponent->Get_Weight(), 0.0f);
    }

    // Whatever survived must still be a full share of the score.
    const auto TotalWeight = ck::algo::SumBy(WithoutGpu.Get_Components(),
        [](const FCk_PerfLab_ScoreComponentResult& InResult) { return InResult.Get_Weight(); });

    TestEqual(TEXT("The surviving weights renormalise to one"), TotalWeight, 1.0, 1e-4);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Score_OneTerribleAreaDragsTheWholeLevel,
    "Ck.PerfLab.Score.OneTerribleAreaDragsTheWholeLevel",
    ck_perf_lab_analysis_spec::kFlags)

bool FCkPerfLab_Score_OneTerribleAreaDragsTheWholeLevel::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_analysis_spec;

    // Nine good rooms and one that stutters badly. An arithmetic mean would call this level fine;
    // the harmonic aggregation exists because a player does not experience it that way.
    auto Positions = TArray<FCk_PerfLab_PositionResult>{};

    for (auto Index = 0; Index < 9; ++Index)
    {
        Positions.Add(Make_Position(FString::Printf(TEXT("p_%d"), Index),
            Make_Metrics(4.0f, 2.0f, 3.0f, true), false));
    }

    Positions.Add(Make_Position(TEXT("p_bad"), Make_Metrics(120.0f, 80.0f, 100.0f, true), false));

    const auto Score = ck::perf_lab::Compute_Score(Make_Session(Positions), 16.67f);

    TestTrue(TEXT("One terrible area pulls the level well below the good rooms' own score"),
        Score.Get_Value() < 80);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Analysis_IsDeterministicAndBudgetDriven,
    "Ck.PerfLab.Analysis.IsDeterministicAndBudgetDriven",
    ck_perf_lab_analysis_spec::kFlags)

bool FCkPerfLab_Analysis_IsDeterministicAndBudgetDriven::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_analysis_spec;

    const auto Session = Make_Session({Make_Position(TEXT("p_1"), Make_Metrics(4.0f, 2.0f, 3.0f, true), true)});

    const auto First  = ck::perf_lab::Analyse_Session(Session, 16.67f);
    const auto Second = ck::perf_lab::Analyse_Session(Session, 16.67f);

    TestEqual(TEXT("The same session and budget analyse identically"),
        First.Get_Findings().Num(), Second.Get_Findings().Num());
    TestEqual(TEXT("...to the same score"), First.Get_Score().Get_Value(), Second.Get_Score().Get_Value());

    // The budget is a parameter, not a property of the capture. This is what lets one real thin-map
    // session exercise the gate's suppression direction without authoring an expensive level.
    const auto Punishing = ck::perf_lab::Analyse_Session(Session, 0.5f);

    TestFalse(TEXT("A punishing budget makes the same session over budget"), Punishing.Get_MeasuredClean());
    TestTrue (TEXT("...and publishes findings the generous budget did not"),
        Punishing.Get_Findings().Num() > First.Get_Findings().Num());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Analysis_ContributorDisclaimerIsNotSoftened,
    "Ck.PerfLab.Analysis.ContributorDisclaimerIsNotSoftened",
    ck_perf_lab_analysis_spec::kFlags)

bool FCkPerfLab_Analysis_ContributorDisclaimerIsNotSoftened::RunTest(const FString& Parameters)
{
    // The honesty of the whole contributor feature rests on this wording. Pinned so that a later
    // edit toward "likely cause" has to break a test to happen.
    const auto Disclaimer = FString{ck::perf_lab::k_ContributorDisclaimer};

    TestTrue(TEXT("The disclaimer says proximity"),        Disclaimer.Contains(TEXT("near the measured cost")));
    TestTrue(TEXT("The disclaimer disclaims causation"),   Disclaimer.Contains(TEXT("not proven cause")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
