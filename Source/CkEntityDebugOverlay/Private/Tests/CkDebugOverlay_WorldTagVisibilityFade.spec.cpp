#include <limits>

#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_WorldTag.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugOverlay_WorldTagVisibilityFade_Test,
    "Ck.DebugOverlay.Presentation.WorldTagVisibilityFade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_WorldTagVisibilityFade_Test::RunTest(const FString&)
{
    using ck_debugoverlay::Advance_WorldTagVisibilityFade;
    using ck_debugoverlay::FWorldTagVisibilityFadeState;
    constexpr auto Duration = ck_debugoverlay::WorldTagVisibilityFadeDurationSeconds;

    auto Fade = FWorldTagVisibilityFadeState{};
    const auto Initial = Advance_WorldTagVisibilityFade(Fade, true, 10.0);
    TestTrue(TEXT("an entering tag starts transparent"), FMath::IsNearlyZero(Initial));

    const auto HalfVisible = Advance_WorldTagVisibilityFade(Fade, true, 10.0 + Duration * 0.5);
    TestTrue(TEXT("an entering tag linearly reaches half opacity"),
        FMath::IsNearlyEqual(HalfVisible, 0.5f));
    const auto FullyVisible = Advance_WorldTagVisibilityFade(Fade, true, 10.0 + Duration);
    TestTrue(TEXT("an entering tag reaches full opacity"),
        FMath::IsNearlyEqual(FullyVisible, 1.0f));

    const auto ExitStart = Advance_WorldTagVisibilityFade(Fade, false, 10.0 + Duration);
    TestTrue(TEXT("leaving range does not snap opacity"),
        FMath::IsNearlyEqual(ExitStart, FullyVisible));
    const auto HalfHidden = Advance_WorldTagVisibilityFade(Fade, false, 10.0 + Duration * 1.5);
    TestTrue(TEXT("an exiting tag linearly reaches half opacity"),
        FMath::IsNearlyEqual(HalfHidden, 0.5f));

    const auto ReentryStart = Advance_WorldTagVisibilityFade(Fade, true, 10.0 + Duration * 1.5);
    TestTrue(TEXT("re-entering mid-fade preserves the current opacity"),
        FMath::IsNearlyEqual(ReentryStart, HalfHidden));
    const auto ReentryHalf = Advance_WorldTagVisibilityFade(Fade, true, 10.0 + Duration * 2.0);
    TestTrue(TEXT("re-entry fades toward full opacity from the reversal point"),
        FMath::IsNearlyEqual(ReentryHalf, 0.75f));
    const auto ReentryComplete = Advance_WorldTagVisibilityFade(Fade, true, 10.0 + Duration * 2.5);
    TestTrue(TEXT("re-entry completes at full opacity"),
        FMath::IsNearlyEqual(ReentryComplete, 1.0f));

    auto SparseFade = FWorldTagVisibilityFadeState{};
    Advance_WorldTagVisibilityFade(SparseFade, true, 20.0);
    const auto SparseExitStart = Advance_WorldTagVisibilityFade(
        SparseFade, false, 20.0 + Duration * 2.0);
    TestTrue(TEXT("a sparse update advances the old target before reversing"),
        FMath::IsNearlyEqual(SparseExitStart, 1.0f));
    const auto SparseExitHalf = Advance_WorldTagVisibilityFade(
        SparseFade, false, 20.0 + Duration * 2.5);
    TestTrue(TEXT("a sparse reversal fades from the elapsed opacity"),
        FMath::IsNearlyEqual(SparseExitHalf, 0.5f));

    auto MalformedFade = FWorldTagVisibilityFadeState{};
    MalformedFade.bInitialized = true;
    MalformedFade.CurrentOpacity = std::numeric_limits<float>::quiet_NaN();
    const auto RecoveredOpacity = Advance_WorldTagVisibilityFade(MalformedFade, true, 30.0);
    TestTrue(TEXT("a malformed fade state resets to a finite bounded opacity"),
        FMath::IsFinite(RecoveredOpacity) &&
        FMath::IsWithinInclusive(RecoveredOpacity, 0.0f, 1.0f));
    TestTrue(TEXT("a recovered fade can still complete normally"), FMath::IsNearlyEqual(
        Advance_WorldTagVisibilityFade(MalformedFade, true, 30.0 + Duration), 1.0f));

    const auto BeforeInvalidTime = Fade.CurrentOpacity;
    const auto InvalidTimeResult = Advance_WorldTagVisibilityFade(
        Fade, false, std::numeric_limits<double>::quiet_NaN());
    TestTrue(TEXT("a non-finite clock does not snap or poison opacity"),
        FMath::IsFinite(InvalidTimeResult) &&
        FMath::IsNearlyEqual(InvalidTimeResult, BeforeInvalidTime));
    const auto RewoundTimeResult = Advance_WorldTagVisibilityFade(Fade, false, -100.0);
    TestTrue(TEXT("a rewound clock cannot reverse transition progress"),
        FMath::IsFinite(RewoundTimeResult) &&
        FMath::IsNearlyEqual(RewoundTimeResult, InvalidTimeResult));
    const auto HugeStepResult = Advance_WorldTagVisibilityFade(Fade, false, 1000.0);
    TestTrue(TEXT("a large forward step clamps cleanly to hidden"),
        FMath::IsNearlyZero(HugeStepResult));

    auto OutOfRangeFirst = FWorldTagVisibilityFadeState{};
    TestTrue(TEXT("an out-of-range tag never flashes on first observation"), FMath::IsNearlyZero(
        Advance_WorldTagVisibilityFade(OutOfRangeFirst, false, 5.0)));
    TestTrue(TEXT("independent tag state does not inherit another entity's opacity"),
        FMath::IsNearlyZero(OutOfRangeFirst.CurrentOpacity) &&
        FMath::IsNearlyEqual(Fade.CurrentOpacity, 0.0f));

    auto Root = SNew(SCkDebugOverlay_Root);
    auto BudgetTags = TArray<FCk_DebugOverlay_WorldTagInfo>{};
    BudgetTags.Reserve(ck_debugoverlay::WorldTagPresentationBudget + 1);
    for (auto Index = 0; Index <= ck_debugoverlay::WorldTagPresentationBudget; ++Index)
    {
        auto& Tag = BudgetTags.AddDefaulted_GetRef();
        Tag.EntityKey = static_cast<uint32>(100 + Index);
        Tag.bInRange = true;
    }

    Root->Update_WorldTags(BudgetTags, 40.0);
    Root->Update_WorldTags(BudgetTags, 40.0 + Duration);
    auto AdmittedKeys = Root->Get_AdmittedWorldTagKeys();
    TestEqual(TEXT("the root enforces the world-tag presentation budget"),
        AdmittedKeys.Num(), ck_debugoverlay::WorldTagPresentationBudget);
    TestFalse(TEXT("the first waiting tag is not admitted over budget"),
        AdmittedKeys.Contains(BudgetTags.Last().EntityKey));

    BudgetTags[0].bInRange = false;
    Root->Update_WorldTags(BudgetTags, 40.0 + Duration);
    AdmittedKeys = Root->Get_AdmittedWorldTagKeys();
    TestTrue(TEXT("an outgoing tag retains its slot while fading"),
        AdmittedKeys.Contains(BudgetTags[0].EntityKey));
    TestFalse(TEXT("an incoming tag waits for the outgoing fade"),
        AdmittedKeys.Contains(BudgetTags.Last().EntityKey));

    Root->Update_WorldTags(BudgetTags, 40.0 + Duration * 2.0);
    AdmittedKeys = Root->Get_AdmittedWorldTagKeys();
    TestFalse(TEXT("a fully faded tag releases its slot"),
        AdmittedKeys.Contains(BudgetTags[0].EntityKey));
    TestTrue(TEXT("the waiting in-range tag fades in after admission"),
        AdmittedKeys.Contains(BudgetTags.Last().EntityKey));
    TestEqual(TEXT("replacing an outgoing tag preserves the hard budget"),
        AdmittedKeys.Num(), ck_debugoverlay::WorldTagPresentationBudget);

    return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
