#include "CkPerfLab/Analysis/CkPerfLab_Analysis.h"
#include "CkPerfLab/Session/CkPerfLab_SessionCodec.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------
// The evidence gate, exercised on REAL measured data rather than on a hand-authored fixture.
//
// The session here was captured by an actual child run against the gym map, then checked in
// unmodified. That matters: a fixture written by hand from the schema can only ever prove the code
// agrees with the schema, whereas this one proves it agrees with what the runner actually writes.
//
// The map is deliberately cheap, so the gate is exercised by moving the BUDGET rather than by
// authoring an expensive level — the budget is a request parameter, and a level nobody can make slow
// can still be made over-budget. Both directions of the moat therefore run on real numbers:
// generous budget must publish nothing, punishing budget must publish only the finding that admits
// it cannot attribute anything.
// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_real_session_spec
{
    constexpr auto kFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

    auto
        Get_FixturePath()
        -> FString
    {
        return FPaths::Combine(FPaths::ProjectPluginsDir(),
            TEXT("CkGameplayDebugger"), TEXT("Source"), TEXT("CkPerfLab"), TEXT("Fixtures"), TEXT("ThinMapSession.json"));
    }

    auto
        TryLoad_Fixture(
            FAutomationTestBase& InTest,
            FCk_PerfLab_Session& OutSession)
        -> bool
    {
        auto Json = FString{};

        if (NOT FFileHelper::LoadFileToString(Json, *Get_FixturePath()))
        {
            InTest.AddError(FString::Printf(TEXT("The captured session fixture is missing at [%s]"), *Get_FixturePath()));
            return false;
        }

        if (NOT ck::perf_lab::TryRead_SessionFromJson(Json, OutSession))
        {
            InTest.AddError(TEXT("The captured session fixture no longer decodes — the codec and the runner have drifted apart"));
            return false;
        }

        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_RealSession_DecodesWhatTheRunnerWrote,
    "Ck.PerfLab.RealSession.DecodesWhatTheRunnerWrote",
    ck_perf_lab_real_session_spec::kFlags)

bool FCkPerfLab_RealSession_DecodesWhatTheRunnerWrote::RunTest(const FString& Parameters)
{
    auto Session = FCk_PerfLab_Session{};

    if (NOT ck_perf_lab_real_session_spec::TryLoad_Fixture(*this, Session))
    {
        return false;
    }

    // This is the drift guard: if the runner ever changes what it writes, or the codec what it
    // reads, this fails before any analysis test built on the fixture starts lying.
    TestTrue (TEXT("The captured run completed"), Session.Get_State() == ECk_PerfLab_RunState::Done);
    TestTrue (TEXT("It measured positions"),      Session.Get_Positions().Num() > 0);

    for (const auto& Position : Session.Get_Positions())
    {
        TestFalse(TEXT("Every position carries an id"), Position.Get_PositionId().IsEmpty());

        const auto& Frame = Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::Frame);

        TestTrue(TEXT("Every position measured a frame time"), Frame.Get_IsAvailable());
        TestTrue(TEXT("...over a real sample window"),         Frame.Get_SampleCount() > 1);

        // The bug the first live run exposed: the non-frame metrics were reduced from a single
        // trailing snapshot while claiming to describe the window. Pinned here against real data.
        for (const auto Metric : {ECk_PerfLab_Metric::GameThread, ECk_PerfLab_Metric::RenderThread})
        {
            const auto& Stats = Position.Get_Aggregate().Get_ByMetric(Metric);

            if (Stats.Get_IsAvailable())
            {
                TestEqual(TEXT("Every available metric shares the frame metric's window"),
                    Stats.Get_SampleCount(), Frame.Get_SampleCount());
            }
        }
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_RealSession_GenerousBudget_PublishesNothing,
    "Ck.PerfLab.RealSession.GenerousBudget_PublishesNothing",
    ck_perf_lab_real_session_spec::kFlags)

bool FCkPerfLab_RealSession_GenerousBudget_PublishesNothing::RunTest(const FString& Parameters)
{
    auto Session = FCk_PerfLab_Session{};

    if (NOT ck_perf_lab_real_session_spec::TryLoad_Fixture(*this, Session))
    {
        return false;
    }

    // A cheap map measured against a 30fps budget is comfortably inside it, so an honest tool has
    // nothing to say. Anything published here would be a finding invented from static appearance.
    const auto Analysis = ck::perf_lab::Analyse_Session(Session, 33.3f);

    TestTrue (TEXT("A comfortably fast level is measured clean"), Analysis.Get_MeasuredClean());
    TestEqual(TEXT("...and publishes no findings at all"),        Analysis.Get_Findings().Num(), 0);
    TestTrue (TEXT("...and scores near the top"),                 Analysis.Get_Score().Get_Value() >= 90);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_RealSession_PunishingBudget_AttributesNothingItCannotJustify,
    "Ck.PerfLab.RealSession.PunishingBudget_AttributesNothingItCannotJustify",
    ck_perf_lab_real_session_spec::kFlags)

bool FCkPerfLab_RealSession_PunishingBudget_AttributesNothingItCannotJustify::RunTest(const FString& Parameters)
{
    auto Session = FCk_PerfLab_Session{};

    if (NOT ck_perf_lab_real_session_spec::TryLoad_Fixture(*this, Session))
    {
        return false;
    }

    // Same real measurements, an impossible budget. Every position is now genuinely over budget, so
    // the metric half of the gate is satisfied by real data — but the map is sparse, so no rule's
    // static signal is anywhere near its threshold.
    const auto Analysis = ck::perf_lab::Analyse_Session(Session, 0.5f);

    TestFalse(TEXT("Nothing is clean against an impossible budget"), Analysis.Get_MeasuredClean());
    TestTrue (TEXT("Something is published"),                        Analysis.Get_Findings().Num() > 0);

    // THE SUPPRESSION DIRECTION OF THE MOAT, on real data: over budget everywhere, and still not one
    // word of attribution, because there is nothing there to blame.
    for (const auto& Finding : Analysis.Get_Findings())
    {
        const auto IsAttributing = NOT Finding.Get_CheckId().Equals(TEXT("Perf.General.OverBudgetUnattributed"))
                                && NOT Finding.Get_CheckId().StartsWith(TEXT("Perf.Confidence."));

        TestFalse(*FString::Printf(
            TEXT("[%s] must not attribute a cost on a map with no matching signal"), *Finding.Get_CheckId()),
            IsAttributing);
    }

    TestTrue(TEXT("The unattributed finding is the one that fired"),
        ck::algo::AnyOf(Analysis.Get_Findings(), [](const FCk_PerfLab_Finding& InFinding)
        { return InFinding.Get_CheckId() == TEXT("Perf.General.OverBudgetUnattributed"); }));

    // And what it does publish still cites the measurement that justified it.
    for (const auto& Finding : Analysis.Get_Findings())
    {
        if (Finding.Get_CheckId().StartsWith(TEXT("Perf.Confidence.")))
        {
            continue;
        }

        TestTrue(*FString::Printf(TEXT("[%s] cites a real over-budget measurement"), *Finding.Get_CheckId()),
            Finding.Get_Evidence().Get_MeasuredMs() > Finding.Get_Evidence().Get_BudgetMs());
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
