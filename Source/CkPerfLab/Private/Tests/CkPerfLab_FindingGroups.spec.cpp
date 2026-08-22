#include "CkPerfLab/Analysis/CkPerfLab_Analysis.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Macros/CkMacros.h"

#include <Misc/AutomationTest.h>

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_groups_spec
{
    constexpr auto kFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    auto
        Make_Finding(
            const FString& InCheckId,
            const FString& InPositionId,
            ECk_PerfLab_Severity InSeverity,
            float InOverBudgetRatio,
            const TArray<FString>& InContributorPaths = {},
            const FString& InRecommendation = TEXT("Do the thing"))
        -> FCk_PerfLab_Finding
    {
        auto Contributors = TArray<FCk_PerfLab_Contributor>{};

        for (auto Index = 0; Index < InContributorPaths.Num(); ++Index)
        {
            Contributors.Add(FCk_PerfLab_Contributor{}
                .Set_Rank(Index + 1)
                .Set_ObjectPath(InContributorPaths[Index])
                .Set_ClassName(TEXT("StaticMeshActor"))
                // Descending distance, so a correct re-rank has something to actually reorder.
                .Set_DistanceCm(1000.0f - static_cast<float>(Index) * 100.0f)
                .Set_Note(ck::perf_lab::k_ContributorDisclaimer));
        }

        return FCk_PerfLab_Finding{}
            .Set_CheckId(InCheckId)
            .Set_StableKey(ck::Format_UE(TEXT("{}|{}"), InCheckId, InPositionId))
            .Set_Severity(InSeverity)
            .Set_PositionId(InPositionId)
            .Set_Evidence(FCk_PerfLab_Evidence{}
                .Set_Metric(ECk_PerfLab_Metric::Frame)
                .Set_MeasuredMs(16.67f * InOverBudgetRatio)
                .Set_BudgetMs(16.67f)
                .Set_OverBudgetRatio(InOverBudgetRatio)
                .Set_Signal(TEXT("nearbyTriangleCount"))
                .Set_SignalValue(1000.0f))
            .Set_Contributors(Contributors)
            .Set_Recommendations({FCk_PerfLab_Recommendation{}
                .Set_Order(1)
                .Set_GainBand(ECk_PerfLab_Band::High)
                .Set_EffortBand(ECk_PerfLab_Band::Low)
                .Set_Text(InRecommendation)});
    }

    auto
        Make_Analysis(
            const TArray<FCk_PerfLab_Finding>& InFindings)
        -> FCk_PerfLab_Analysis
    {
        return FCk_PerfLab_Analysis{}.Set_BudgetMs(16.67f).Set_Findings(InFindings);
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Groups_CollapsesOneRuleAcrossPositions,
    "Ck.PerfLab.FindingGroups.CollapsesOneRuleAcrossPositions",
    ck_perf_lab_groups_spec::kFlags)

bool FCkPerfLab_Groups_CollapsesOneRuleAcrossPositions::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_groups_spec;

    // The shape a real session produced: one rule, twelve positions, identical advice each time.
    auto Findings = TArray<FCk_PerfLab_Finding>{};

    for (auto Index = 0; Index < 12; ++Index)
    {
        Findings.Add(Make_Finding(TEXT("Perf.General.OverBudgetUnattributed"),
            ck::Format_UE(TEXT("p_{:04d}"), Index), ECk_PerfLab_Severity::Major, 1.6f));
    }

    const auto Groups = ck::perf_lab::Group_Findings(Make_Analysis(Findings));

    TestEqual(TEXT("Twelve findings of one rule are ONE group"), Groups.Num(), 1);

    if (Groups.IsEmpty())
    { return false; }

    TestEqual(TEXT("...that still names every position"), Groups[0].Get_PositionIds().Num(), 12);

    // The advice is identical at all twelve, and repeating it twelve times is what turns a short actionable list
    // into a wall.
    TestEqual(TEXT("...and says the identical advice once"), Groups[0].Get_Recommendations().Num(), 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Groups_LeadsWithTheWorstPositionNotAnAverage,
    "Ck.PerfLab.FindingGroups.LeadsWithTheWorstPositionNotAnAverage",
    ck_perf_lab_groups_spec::kFlags)

bool FCkPerfLab_Groups_LeadsWithTheWorstPositionNotAnAverage::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_groups_spec;

    const auto Groups = ck::perf_lab::Group_Findings(Make_Analysis(
    {
        Make_Finding(TEXT("Perf.Gpu.TriangleDensity"), TEXT("p_mild"),  ECk_PerfLab_Severity::Minor,    1.1f),
        Make_Finding(TEXT("Perf.Gpu.TriangleDensity"), TEXT("p_awful"), ECk_PerfLab_Severity::Critical, 3.0f),
        Make_Finding(TEXT("Perf.Gpu.TriangleDensity"), TEXT("p_bad"),   ECk_PerfLab_Severity::Major,    1.8f),
    }));

    TestEqual(TEXT("One rule, one group"), Groups.Num(), 1);

    if (Groups.IsEmpty())
    { return false; }

    // An averaged evidence row would describe a place that does not exist, and the reader's next action is to go
    // stand somewhere specific.
    TestEqual(TEXT("The worst position leads"), Groups[0].Get_WorstPositionId(), TEXT("p_awful"));
    TestEqual(TEXT("...with its own measurement, not a mean"),
        Groups[0].Get_WorstEvidence().Get_OverBudgetRatio(), 3.0f, 0.001f);

    // Severity is declared worst-first, so the group takes the MINIMUM enum value.
    TestEqual(TEXT("The group is as severe as its worst position"),
        Groups[0].Get_Severity(), ECk_PerfLab_Severity::Critical);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Groups_MergesContributorsWithoutInflatingThem,
    "Ck.PerfLab.FindingGroups.MergesContributorsWithoutInflatingThem",
    ck_perf_lab_groups_spec::kFlags)

bool FCkPerfLab_Groups_MergesContributorsWithoutInflatingThem::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_groups_spec;

    // The same actor is near three measured positions. It is ONE suspect — listing it three times would rank it
    // highest purely for standing in a busy area, which is attribution by coincidence.
    const auto Groups = ck::perf_lab::Group_Findings(Make_Analysis(
    {
        Make_Finding(TEXT("Perf.Gpu.TriangleDensity"), TEXT("p_a"), ECk_PerfLab_Severity::Major, 1.5f, {TEXT("/Game/M.Rock")}),
        Make_Finding(TEXT("Perf.Gpu.TriangleDensity"), TEXT("p_b"), ECk_PerfLab_Severity::Major, 1.5f, {TEXT("/Game/M.Rock")}),
        Make_Finding(TEXT("Perf.Gpu.TriangleDensity"), TEXT("p_c"), ECk_PerfLab_Severity::Major, 1.5f, {TEXT("/Game/M.Rock"), TEXT("/Game/M.Tree")}),
    }));

    TestEqual(TEXT("One rule, one group"), Groups.Num(), 1);

    if (Groups.IsEmpty())
    { return false; }

    TestEqual(TEXT("A repeated actor is listed once"), Groups[0].Get_Contributors().Num(), 2);

    // Nearest first, and the ranks renumbered to match — a merged list carrying its per-finding ranks would show
    // two number ones.
    const auto Ranks = ck::algo::Transform<TArray<int32>>(Groups[0].Get_Contributors(),
        [](const FCk_PerfLab_Contributor& InContributor) { return InContributor.Get_Rank(); });

    TestEqual(TEXT("Ranks are renumbered across the merge"), Ranks, TArray<int32>{1, 2});

    TestTrue(TEXT("...nearest first"),
        Groups[0].Get_Contributors()[0].Get_DistanceCm() <= Groups[0].Get_Contributors()[1].Get_DistanceCm());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Groups_AreOrderedAndStable,
    "Ck.PerfLab.FindingGroups.AreOrderedAndStable",
    ck_perf_lab_groups_spec::kFlags)

bool FCkPerfLab_Groups_AreOrderedAndStable::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_groups_spec;

    const auto Analysis = Make_Analysis(
    {
        Make_Finding(TEXT("Perf.Zzz.Minor"),    TEXT("p_a"), ECk_PerfLab_Severity::Minor,    1.1f),
        Make_Finding(TEXT("Perf.Aaa.Critical"), TEXT("p_b"), ECk_PerfLab_Severity::Critical, 2.0f),
        Make_Finding(TEXT("Perf.Mmm.Major"),    TEXT("p_c"), ECk_PerfLab_Severity::Major,    1.5f),
        Make_Finding(TEXT("Perf.Mmm.Major"),    TEXT("p_d"), ECk_PerfLab_Severity::Major,    1.5f),
    });

    const auto Groups = ck::perf_lab::Group_Findings(Analysis);

    TestEqual(TEXT("Three rules, three groups"), Groups.Num(), 3);

    if (Groups.Num() != 3)
    { return false; }

    TestEqual(TEXT("Worst severity first"),  Groups[0].Get_CheckId(), TEXT("Perf.Aaa.Critical"));
    TestEqual(TEXT("...then the next"),      Groups[1].Get_CheckId(), TEXT("Perf.Mmm.Major"));
    TestEqual(TEXT("...then the least"),     Groups[2].Get_CheckId(), TEXT("Perf.Zzz.Minor"));

    // Grouping runs through a TMap, whose iteration order is NOT stable. Without the final sort the list would
    // reorder between runs of one build, which is the same defect the exports are pinned against.
    const auto Again = ck::perf_lab::Group_Findings(Analysis);

    const auto FirstIds = ck::algo::Transform<TArray<FString>>(Groups,
        [](const FCk_PerfLab_FindingGroup& InGroup) { return InGroup.Get_CheckId(); });

    const auto AgainIds = ck::algo::Transform<TArray<FString>>(Again,
        [](const FCk_PerfLab_FindingGroup& InGroup) { return InGroup.Get_CheckId(); });

    TestEqual(TEXT("Grouping the same analysis twice gives the same order"), FirstIds, AgainIds);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
