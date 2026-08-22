#include "CkPerfLab/Planner/CkPerfLab_Planner.h"

#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_planner_spec
{
    constexpr auto kFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    auto
        Make_Actor(
            const FVector& InLocation,
            float InWeight)
        -> FCk_PerfLab_SurveyActor
    {
        return FCk_PerfLab_SurveyActor{}.Set_Location(InLocation).Set_CostWeight(InWeight);
    }

    auto
        Make_Request(
            int32 InPositionBudget,
            int32 InDirections)
        -> FCk_PerfLab_Request
    {
        auto ModeParams = FCk_PerfLab_ModeParams{};
        ModeParams.Set_PositionBudget(InPositionBudget)
                  .Set_DirectionsPerPosition(InDirections)
                  .Set_MinPositionSpacingCm(500.0f)
                  .Set_CensusRadiusCm(1000.0f);

        return FCk_PerfLab_Request{}.Set_Seed(0).Set_ModeParams(ModeParams);
    }

    // A spread of content wide enough that the spacing rule has something to reject.
    auto
        Make_Survey()
        -> FCk_PerfLab_WorldSurvey
    {
        auto Actors = TArray<FCk_PerfLab_SurveyActor>{};
        Actors.Add(Make_Actor(FVector{0.0, 0.0, 0.0},       10.0f));
        Actors.Add(Make_Actor(FVector{2000.0, 0.0, 0.0},    50.0f));
        Actors.Add(Make_Actor(FVector{4000.0, 0.0, 0.0},     5.0f));
        Actors.Add(Make_Actor(FVector{0.0, 3000.0, 0.0},    20.0f));

        auto NavPoints = TArray<FVector>{};
        NavPoints.Add(FVector{0.0, 0.0, 0.0});
        NavPoints.Add(FVector{2000.0, 0.0, 0.0});
        NavPoints.Add(FVector{4000.0, 0.0, 0.0});
        NavPoints.Add(FVector{0.0, 3000.0, 0.0});

        return FCk_PerfLab_WorldSurvey{}
            .Set_Actors(Actors)
            .Set_NavmeshPoints(NavPoints)
            .Set_WorldBounds(FBox{FVector{-500.0, -500.0, -100.0}, FVector{4500.0, 3500.0, 500.0}});
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Planner_IsDeterministicAndOrderIndependent,
    "Ck.PerfLab.Planner.IsDeterministicAndOrderIndependent",
    ck_perf_lab_planner_spec::kFlags)

bool FCkPerfLab_Planner_IsDeterministicAndOrderIndependent::RunTest(const FString& Parameters)
{
    const auto Request = ck_perf_lab_planner_spec::Make_Request(10, 4);
    const auto Survey  = ck_perf_lab_planner_spec::Make_Survey();

    const auto First  = ck::perf_lab::Generate_Plan(Survey, Request);
    const auto Second = ck::perf_lab::Generate_Plan(Survey, Request);

    TestEqual(TEXT("The same survey plans the same number of positions"),
        First.Get_Positions().Num(), Second.Get_Positions().Num());

    for (auto Index = 0; Index < First.Get_Positions().Num(); ++Index)
    {
        TestEqual(TEXT("The same survey plans the same positions in the same order"),
            First.Get_Positions()[Index].Get_PositionId(), Second.Get_Positions()[Index].Get_PositionId());
    }

    // The survey is gathered by iterating a world, and iteration order is not a contract. A planner
    // sensitive to it would produce different position ids on every run and silently break the
    // session-compare feature that ids exist to serve.
    auto Shuffled = Survey;
    auto Actors   = Shuffled.Get_Actors();
    Algo::Reverse(Actors);
    Shuffled.Set_Actors(Actors);

    auto NavPoints = Shuffled.Get_NavmeshPoints();
    Algo::Reverse(NavPoints);
    Shuffled.Set_NavmeshPoints(NavPoints);

    const auto FromShuffled = ck::perf_lab::Generate_Plan(Shuffled, Request);

    TestEqual(TEXT("Reversing the survey plans the same count"),
        FromShuffled.Get_Positions().Num(), First.Get_Positions().Num());

    for (auto Index = 0; Index < First.Get_Positions().Num(); ++Index)
    {
        TestEqual(TEXT("Reversing the survey plans the same positions"),
            FromShuffled.Get_Positions()[Index].Get_PositionId(), First.Get_Positions()[Index].Get_PositionId());
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Planner_PositionIdsAreLocationDerived,
    "Ck.PerfLab.Planner.PositionIdsAreLocationDerived",
    ck_perf_lab_planner_spec::kFlags)

bool FCkPerfLab_Planner_PositionIdsAreLocationDerived::RunTest(const FString& Parameters)
{
    const auto Location = FVector{1234.0, -567.0, 89.0};

    TestEqual(TEXT("The same location yields the same id"),
        ck::perf_lab::Get_PositionId(Location), ck::perf_lab::Get_PositionId(Location));

    // Sub-quantum jitter must not change the id, or two runs of the same map would fail to match up
    // and every comparison would report every position as new.
    TestEqual(TEXT("Jitter below the quantisation step yields the same id"),
        ck::perf_lab::Get_PositionId(Location), ck::perf_lab::Get_PositionId(Location + FVector{1.0, -1.0, 0.5}));

    TestNotEqual(TEXT("A genuinely different location yields a different id"),
        ck::perf_lab::Get_PositionId(Location), ck::perf_lab::Get_PositionId(Location + FVector{5000.0, 0.0, 0.0}));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Planner_RespectsSpacingAndBudget,
    "Ck.PerfLab.Planner.RespectsSpacingAndBudget",
    ck_perf_lab_planner_spec::kFlags)

bool FCkPerfLab_Planner_RespectsSpacingAndBudget::RunTest(const FString& Parameters)
{
    const auto Survey  = ck_perf_lab_planner_spec::Make_Survey();
    const auto Request = ck_perf_lab_planner_spec::Make_Request(2, 4);

    const auto Plan = ck::perf_lab::Generate_Plan(Survey, Request);

    TestTrue(TEXT("The budget is not exceeded"), Plan.Get_Positions().Num() <= 2);

    const auto SpacingSq = 500.0 * 500.0;

    for (auto OuterIndex = 0; OuterIndex < Plan.Get_Positions().Num(); ++OuterIndex)
    {
        for (auto InnerIndex = OuterIndex + 1; InnerIndex < Plan.Get_Positions().Num(); ++InnerIndex)
        {
            TestTrue(TEXT("No two positions sit closer than the minimum spacing"),
                FVector::DistSquared(
                    Plan.Get_Positions()[OuterIndex].Get_Location(),
                    Plan.Get_Positions()[InnerIndex].Get_Location()) >= SpacingSq);
        }
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Planner_PrefersTheHeaviestContent,
    "Ck.PerfLab.Planner.PrefersTheHeaviestContent",
    ck_perf_lab_planner_spec::kFlags)

bool FCkPerfLab_Planner_PrefersTheHeaviestContent::RunTest(const FString& Parameters)
{
    // With room for exactly one position, the planner should spend it on the heaviest cluster —
    // the actor weighted 50, not the ones weighted 5, 10 or 20.
    const auto Plan = ck::perf_lab::Generate_Plan(
        ck_perf_lab_planner_spec::Make_Survey(), ck_perf_lab_planner_spec::Make_Request(1, 1));

    TestEqual(TEXT("Exactly one position is planned"), Plan.Get_Positions().Num(), 1);

    if (Plan.Get_Positions().Num() == 1)
    {
        TestEqual(TEXT("The heaviest cluster is chosen"),
            Plan.Get_Positions()[0].Get_Location().X, 2000.0);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Planner_FallsBackToGridWithoutNavmesh,
    "Ck.PerfLab.Planner.FallsBackToGridWithoutNavmesh",
    ck_perf_lab_planner_spec::kFlags)

bool FCkPerfLab_Planner_FallsBackToGridWithoutNavmesh::RunTest(const FString& Parameters)
{
    auto Survey = ck_perf_lab_planner_spec::Make_Survey();

    TestTrue(TEXT("With navmesh points the plan says it used them"),
        ck::perf_lab::Generate_Plan(Survey, ck_perf_lab_planner_spec::Make_Request(10, 1)).Get_Outcome() ==
            ECk_PerfLab_PlanOutcome::NavmeshSeeded);

    // A map with no navigation data is a normal case, not an error, and must still be measurable.
    Survey.Set_NavmeshPoints({});

    const auto Plan = ck::perf_lab::Generate_Plan(Survey, ck_perf_lab_planner_spec::Make_Request(10, 1));

    TestTrue (TEXT("Without navmesh points the plan says it fell back"),
        Plan.Get_Outcome() == ECk_PerfLab_PlanOutcome::GridFallback);
    TestTrue (TEXT("The fallback still plans positions"), Plan.Get_Positions().Num() > 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Planner_EmptyWorld_IsAReasonNotACrash,
    "Ck.PerfLab.Planner.EmptyWorld_IsAReasonNotACrash",
    ck_perf_lab_planner_spec::kFlags)

bool FCkPerfLab_Planner_EmptyWorld_IsAReasonNotACrash::RunTest(const FString& Parameters)
{
    const auto Plan = ck::perf_lab::Generate_Plan(
        FCk_PerfLab_WorldSurvey{}, ck_perf_lab_planner_spec::Make_Request(10, 4));

    TestEqual(TEXT("An empty world plans nothing"), Plan.Get_Positions().Num(), 0);
    TestTrue (TEXT("An empty world says why"), Plan.Get_Outcome() == ECk_PerfLab_PlanOutcome::Empty_NoContent);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Planner_ModePresetsDriveDirectionCount,
    "Ck.PerfLab.Planner.ModePresetsDriveDirectionCount",
    ck_perf_lab_planner_spec::kFlags)

bool FCkPerfLab_Planner_ModePresetsDriveDirectionCount::RunTest(const FString& Parameters)
{
    const auto Survey = ck_perf_lab_planner_spec::Make_Survey();

    for (const auto& Pair : TArray<TPair<ECk_PerfLab_Mode, int32>>{{ECk_PerfLab_Mode::Quick, 1},
                                                                   {ECk_PerfLab_Mode::Standard, 4},
                                                                   {ECk_PerfLab_Mode::Deep, 16}})
    {
        auto ModeParams = FCk_PerfLab_ModeParams::Get_ForMode(Pair.Key);
        ModeParams.Set_MinPositionSpacingCm(500.0f).Set_CensusRadiusCm(1000.0f);

        const auto Request = FCk_PerfLab_Request{}.Set_Seed(0).Set_Mode(Pair.Key).Set_ModeParams(ModeParams);
        const auto Plan    = ck::perf_lab::Generate_Plan(Survey, Request);

        TestTrue(TEXT("The mode planned at least one position"), Plan.Get_Positions().Num() > 0);

        if (Plan.Get_Positions().Num() > 0)
        {
            TestEqual(TEXT("The mode's direction count reaches the plan"),
                Plan.Get_Positions()[0].Get_Yaws().Num(), Pair.Value);
        }
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
