#include "CkPerfLab/Analysis/CkPerfLab_SessionCompare.h"

#include "CkPerfLab/Stats/CkPerfLab_SampleStats.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Macros/CkMacros.h"

#include <Misc/AutomationTest.h>

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_compare_spec
{
    constexpr auto kFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    constexpr auto k_BudgetMs = 16.67f;

    auto
        Make_Position(
            const FString& InId,
            float InFrameMs,
            bool InGpuAvailable = true)
        -> FCk_PerfLab_PositionResult
    {
        auto Samples = TArray<float>{};
        Samples.Init(InFrameMs, 20);

        const auto Stats = ck::perf_lab::Reduce_Samples(Samples, 3.5f);

        auto Metrics = FCk_PerfLab_MetricSet{};

        Metrics.Set_Frame(Stats);
        Metrics.Set_GameThread(Stats);
        Metrics.Set_RenderThread(Stats);
        Metrics.Set_RhiThread(Stats);
        Metrics.Set_Gpu(InGpuAvailable
            ? Stats
            : ck::perf_lab::Make_UnavailableStats(
                  ECk_Stats_MetricAvailability::Unavailable_NullRhi, TEXT("headless")));

        return FCk_PerfLab_PositionResult{}
            .Set_PositionId(InId)
            .Set_Location(FVector{static_cast<double>(InId.Len()) * 100.0, 0.0, 0.0})
            .Set_Aggregate(Metrics)
            .Set_Confidence(FCk_PerfLab_ConfidenceResult{}.Set_Rating(ECk_PerfLab_Confidence::High));
    }

    auto
        Make_Session(
            const FString& InId,
            const TArray<FCk_PerfLab_PositionResult>& InPositions,
            const FString& InMapPath = TEXT("/Game/Maps/Test.Test"))
        -> FCk_PerfLab_Session
    {
        return FCk_PerfLab_Session{}
            .Set_SessionId(InId)
            .Set_State(ECk_PerfLab_RunState::Done)
            .Set_Request(FCk_PerfLab_Request{}
                .Set_SessionId(InId)
                .Set_MapPath(InMapPath)
                .Set_BudgetMs(k_BudgetMs))
            .Set_Environment(FCk_PerfLab_Environment{}
                .Set_MachineName(TEXT("DESKTOP"))
                .Set_GpuBrand(TEXT("Test GPU"))
                .Set_CpuBrand(TEXT("Test CPU"))
                .Set_RhiName(TEXT("D3D12"))
                .Set_EngineVersion(TEXT("5.6.0"))
                .Set_BuildConfiguration(TEXT("Development"))
                .Set_InstanceMode(TEXT("Game"))
                .Set_ViewportSizeActual(FIntPoint{1280, 720}))
            .Set_Positions(InPositions);
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Compare_MatchesByPositionIdNotByIndex,
    "Ck.PerfLab.Compare.MatchesByPositionIdNotByIndex",
    ck_perf_lab_compare_spec::kFlags)

bool FCkPerfLab_Compare_MatchesByPositionIdNotByIndex::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_compare_spec;

    // The current session gained a position at the FRONT. Matching by index would pair "a" against
    // "new", "b" against "a" and so on, and report the difference between two different places as a
    // change over time — the single most misleading result this tool could produce.
    const auto Baseline = Make_Session(TEXT("base"),
        {Make_Position(TEXT("a"), 10.0f), Make_Position(TEXT("b"), 12.0f)});

    const auto Current = Make_Session(TEXT("curr"),
        {Make_Position(TEXT("new"), 99.0f), Make_Position(TEXT("a"), 10.0f), Make_Position(TEXT("b"), 12.0f)});

    const auto Compare = ck::perf_lab::Compare_Sessions(Baseline, Current, k_BudgetMs);

    TestTrue (TEXT("The comparison is allowed"), Compare.Get_IsComparable());
    TestEqual(TEXT("Only the shared positions are paired"), Compare.Get_Positions().Num(), 2);
    TestEqual(TEXT("Nothing regressed — the shared positions are unchanged"), Compare.Get_RegressedCount(), 0);

    TestEqual(TEXT("The unmatched position is listed, not dropped"), Compare.Get_OnlyInCurrent().Num(), 1);
    TestEqual(TEXT("...by name"), Compare.Get_OnlyInCurrent()[0], TEXT("new"));
    TestEqual(TEXT("Nothing is missing from the baseline side"), Compare.Get_OnlyInBaseline().Num(), 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Compare_OnlyCallsAChangeRealBeyondTheNoiseBand,
    "Ck.PerfLab.Compare.OnlyCallsAChangeRealBeyondTheNoiseBand",
    ck_perf_lab_compare_spec::kFlags)

bool FCkPerfLab_Compare_OnlyCallsAChangeRealBeyondTheNoiseBand::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_compare_spec;

    const auto Baseline = Make_Session(TEXT("base"), {Make_Position(TEXT("a"), 10.0f)});

    // A tenth of a millisecond. Real hardware does not repeat to that precision, and a tool that
    // called this a regression would cry wolf on every run until nobody read it.
    {
        const auto Current = Make_Session(TEXT("curr"), {Make_Position(TEXT("a"), 10.1f)});
        const auto Compare = ck::perf_lab::Compare_Sessions(Baseline, Current, k_BudgetMs);

        TestEqual(TEXT("A tenth of a millisecond is not a regression"),
            Compare.Get_RegressedCount(), 0);
        TestEqual(TEXT("...and the overall verdict is unchanged"),
            Compare.Get_Verdict(), ECk_PerfLab_CompareVerdict::Unchanged);
    }

    // Doubling the frame time is not noise on any hardware.
    {
        const auto Current = Make_Session(TEXT("curr"), {Make_Position(TEXT("a"), 20.0f)});
        const auto Compare = ck::perf_lab::Compare_Sessions(Baseline, Current, k_BudgetMs);

        TestEqual(TEXT("A doubled frame time is a regression"), Compare.Get_RegressedCount(), 1);
        TestEqual(TEXT("...and it drives the overall verdict"),
            Compare.Get_Verdict(), ECk_PerfLab_CompareVerdict::Regressed);

        const auto Frame = Compare.Get_Positions()[0].Get_FrameDelta();

        TestTrue(TEXT("The frame delta is reported"), Frame.IsSet());

        if (Frame.IsSet())
        {
            TestEqual(TEXT("...with the right sign and size"), Frame->Get_DeltaMs(), 10.0f, 0.01f);
            TestTrue (TEXT("...and the band it had to clear"), Frame->Get_NoiseBandMs() > 0.0f);
        }
    }

    // Halving it is an improvement, and the tool has to be able to say so — a regression detector
    // that cannot recognise a win is a detector nobody uses to check their own fix worked.
    {
        const auto Current = Make_Session(TEXT("curr"), {Make_Position(TEXT("a"), 5.0f)});
        const auto Compare = ck::perf_lab::Compare_Sessions(Baseline, Current, k_BudgetMs);

        TestEqual(TEXT("A halved frame time is an improvement"), Compare.Get_ImprovedCount(), 1);
        TestEqual(TEXT("...and nothing regressed"),              Compare.Get_RegressedCount(), 0);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Compare_RefusesAcrossMaps,
    "Ck.PerfLab.Compare.RefusesAcrossMaps",
    ck_perf_lab_compare_spec::kFlags)

bool FCkPerfLab_Compare_RefusesAcrossMaps::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_compare_spec;

    const auto Baseline = Make_Session(TEXT("base"), {Make_Position(TEXT("a"), 10.0f)}, TEXT("/Game/Maps/One.One"));
    const auto Current  = Make_Session(TEXT("curr"), {Make_Position(TEXT("a"), 10.0f)}, TEXT("/Game/Maps/Two.Two"));

    const auto Compare = ck::perf_lab::Compare_Sessions(Baseline, Current, k_BudgetMs);

    // Two maps share no coordinate space, so an id that appears in both describes two unrelated
    // places that happen to hash the same. This is a refusal, not a warning.
    TestFalse(TEXT("Two maps do not compare"), Compare.Get_IsComparable());
    TestEqual(TEXT("...and the reason is named"),
        Compare.Get_Refusal(), ECk_PerfLab_CompareRefusal::DifferentMap);

    TestFalse(TEXT("The refusal has readable text"),
        ck::perf_lab::Get_RefusalText(Compare.Get_Refusal()).IsEmpty());

    TestFalse(TEXT("No score delta is offered for a refused comparison"),
        Compare.Get_ScoreDelta().IsSet());

    // A session sharing a map but no position id is a different refusal: the map changed shape
    // enough that the planner stood somewhere else entirely, which the reader needs told.
    const auto Disjoint = ck::perf_lab::Compare_Sessions(
        Make_Session(TEXT("base"), {Make_Position(TEXT("a"), 10.0f)}),
        Make_Session(TEXT("curr"), {Make_Position(TEXT("z"), 10.0f)}),
        k_BudgetMs);

    TestEqual(TEXT("No shared position is its own refusal"),
        Disjoint.Get_Refusal(), ECk_PerfLab_CompareRefusal::NoPositionsInCommon);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Compare_WarnsAcrossEnvironmentsButStillRenders,
    "Ck.PerfLab.Compare.WarnsAcrossEnvironmentsButStillRenders",
    ck_perf_lab_compare_spec::kFlags)

bool FCkPerfLab_Compare_WarnsAcrossEnvironmentsButStillRenders::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_compare_spec;

    const auto Baseline = Make_Session(TEXT("base"), {Make_Position(TEXT("a"), 10.0f)});

    auto Current = Make_Session(TEXT("curr"), {Make_Position(TEXT("a"), 10.0f)});
    Current.Set_Environment(Current.Get_Environment()
        .Set_MachineName(TEXT("OTHER"))
        .Set_GpuBrand(TEXT("Other GPU")));

    const auto Compare = ck::perf_lab::Compare_Sessions(Baseline, Current, k_BudgetMs);

    // Warned, NOT suppressed. Suppressing would hide the very fact the reader needs in order to
    // discount the numbers correctly — and would leave them with no comparison at all when a
    // caveated one is genuinely useful.
    TestTrue (TEXT("A different machine still compares"), Compare.Get_IsComparable());
    TestEqual(TEXT("...and the table is still built"),    Compare.Get_Positions().Num(), 1);

    TestTrue(TEXT("The machine difference is flagged"),
        Compare.Get_Warnings().Contains(ECk_PerfLab_CompareWarning::DifferentMachine));

    TestTrue(TEXT("The GPU difference is flagged"),
        Compare.Get_Warnings().Contains(ECk_PerfLab_CompareWarning::DifferentGpu));

    TestTrue(TEXT("Every warning carries readable text"),
        ck::algo::AllOf(Compare.Get_Warnings(), [](ECk_PerfLab_CompareWarning InWarning)
        {
            return NOT ck::perf_lab::Get_WarningText(InWarning).IsEmpty();
        }));

    // A differing REQUESTED budget earns a banner — the two runs may have settled differently — but
    // it must NOT suppress the score delta. Both scores are computed against the single budget
    // handed to Compare_Sessions, never against what each session asked for, so they are on one
    // scale by construction. Suppressing here would hide exactly the number a CI job exists to
    // produce, which is why this asserts the delta is present rather than merely that it is right.
    auto OtherBudget = Make_Session(TEXT("curr"), {Make_Position(TEXT("a"), 40.0f)});
    OtherBudget.Set_Request(OtherBudget.Get_Request().Set_BudgetMs(33.33f));

    const auto BudgetCompare = ck::perf_lab::Compare_Sessions(Baseline, OtherBudget, k_BudgetMs);

    TestTrue(TEXT("A budget difference is flagged"),
        BudgetCompare.Get_Warnings().Contains(ECk_PerfLab_CompareWarning::DifferentBudget));
    TestTrue(TEXT("...and STILL reports the score delta"), BudgetCompare.Get_ScoreDelta().IsSet());

    if (BudgetCompare.Get_ScoreDelta().IsSet())
    {
        // 10 ms becoming 40 ms against a 16.67 ms budget: the score must fall, and the delta say so.
        TestTrue(TEXT("...with the sign a real regression earns"),
            *BudgetCompare.Get_ScoreDelta() < 0.0f);
    }
    TestTrue (TEXT("...while still comparing the timings"), BudgetCompare.Get_IsComparable());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Compare_TreatsAnUnmeasuredMetricAsIncomparableNotAsZero,
    "Ck.PerfLab.Compare.TreatsAnUnmeasuredMetricAsIncomparableNotAsZero",
    ck_perf_lab_compare_spec::kFlags)

bool FCkPerfLab_Compare_TreatsAnUnmeasuredMetricAsIncomparableNotAsZero::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_compare_spec;

    // The baseline ran with a GPU; the current run was headless. Treating the missing side as zero
    // would manufacture the largest improvement in the table out of an absent reading.
    const auto Baseline = Make_Session(TEXT("base"), {Make_Position(TEXT("a"), 10.0f, /*InGpuAvailable*/ true)});
    const auto Current  = Make_Session(TEXT("curr"), {Make_Position(TEXT("a"), 10.0f, /*InGpuAvailable*/ false)});

    const auto Compare = ck::perf_lab::Compare_Sessions(Baseline, Current, k_BudgetMs);

    const auto Gpu = ck::algo::FindIf(Compare.Get_Positions()[0].Get_Metrics(),
        [](const FCk_PerfLab_MetricDelta& InDelta) { return InDelta.Get_Metric() == ECk_PerfLab_Metric::Gpu; });

    TestTrue(TEXT("The GPU row is present"), Gpu.IsSet());

    if (Gpu.IsSet())
    {
        TestEqual(TEXT("An unmeasured side is incomparable, not improved"),
            Gpu->Get_Verdict(), ECk_PerfLab_CompareVerdict::Incomparable);

        TestEqual(TEXT("...and reports no delta at all"), Gpu->Get_DeltaMs(), 0.0f, 0.001f);
    }

    // Frame time WAS measured in both, so the position as a whole is still comparable — one absent
    // metric must not blind the rest.
    TestEqual(TEXT("The position is still judged on what was measured"),
        Compare.Get_Positions()[0].Get_Verdict(), ECk_PerfLab_CompareVerdict::Unchanged);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Compare_IsOrderedWorstFirstAndStable,
    "Ck.PerfLab.Compare.IsOrderedWorstFirstAndStable",
    ck_perf_lab_compare_spec::kFlags)

bool FCkPerfLab_Compare_IsOrderedWorstFirstAndStable::RunTest(const FString& Parameters)
{
    using namespace ck_perf_lab_compare_spec;

    const auto Baseline = Make_Session(TEXT("base"),
        {Make_Position(TEXT("aaa"), 10.0f), Make_Position(TEXT("bbb"), 10.0f), Make_Position(TEXT("ccc"), 10.0f)});

    const auto Current = Make_Session(TEXT("curr"),
        {Make_Position(TEXT("aaa"), 10.0f), Make_Position(TEXT("bbb"), 40.0f), Make_Position(TEXT("ccc"), 10.0f)});

    const auto Compare = ck::perf_lab::Compare_Sessions(Baseline, Current, k_BudgetMs);

    TestEqual(TEXT("The regression sorts to the top"),
        Compare.Get_Positions()[0].Get_PositionId(), TEXT("bbb"));

    // The id tie-break is what makes an exported comparison diffable: without it two positions
    // sharing a verdict would order by whatever the session file happened to list first, and a diff
    // would show churn that is not a change in the data.
    TestEqual(TEXT("Ties break by position id"),
        Compare.Get_Positions()[1].Get_PositionId(), TEXT("aaa"));
    TestEqual(TEXT("...consistently"),
        Compare.Get_Positions()[2].Get_PositionId(), TEXT("ccc"));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
