#include "CkOptimizationDebugger/Fixes/CkOptimizationDebugger_FixPreflight.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and the spec files sit in the same merged translation
// unit as each other.
namespace ck_optimization_debugger_preflight_spec
{
    auto
        Build_Audit(
            const FString& InName,
            const TArray<ECkOptimizationDebugger_RefusalKind>& InKinds)
        -> FCkOptimizationDebugger_PlacementAudit
    {
        auto Audit = FCkOptimizationDebugger_PlacementAudit{};
        Audit.Path = FSoftObjectPath{ck::Format_UE(TEXT("/Game/Spec/Map.Map:PersistentLevel.{}"), InName)};
        Audit.DisplayName = InName;

        for (const auto Kind : InKinds)
        {
            auto Refusal = FCkOptimizationDebugger_Refusal{};
            Refusal.Kind = Kind;

            Audit.Refusals.Add(Refusal);
        }

        return Audit;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Preflight_RefusalWording,
    "Ck.OptimizationDebugger.Preflight.RefusalWording",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Preflight_RefusalWording::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_preflight;

    // Every kind has to produce a sentence. A missing case would fall through to the catch-all and tell the reader
    // "cannot be converted", which is exactly the answerless gate the audit replaced.
    constexpr auto LastKind = static_cast<int32>(ECkOptimizationDebugger_RefusalKind::LevelLocked);

    for (auto KindIndex = 0; KindIndex <= LastKind; ++KindIndex)
    {
        auto Refusal = FCkOptimizationDebugger_Refusal{};
        Refusal.Kind = static_cast<ECkOptimizationDebugger_RefusalKind>(KindIndex);

        const auto Sentence = Get_RefusalSentence(Refusal);

        TestFalse(ck::Format_UE(TEXT("Kind {} produces a sentence"), KindIndex), Sentence.IsEmpty());
        TestFalse(ck::Format_UE(TEXT("Kind {} is not the catch-all"), KindIndex),
            Sentence == TEXT("cannot be converted"));
    }

    // The detail is what makes a refusal actionable — the tag that blocks it, the property that differs. A sentence
    // that dropped it would send the reader hunting for which of thirty properties the audit meant.
    auto Tagged = FCkOptimizationDebugger_Refusal{};
    Tagged.Kind = ECkOptimizationDebugger_RefusalKind::CarriesActorTags;
    Tagged.Detail = TEXT("Expansion_02");

    const auto TaggedSentence = Get_RefusalSentence(Tagged);
    TestTrue(TEXT("The sentence names the reason"), TaggedSentence.Contains(TEXT("actor tags")));
    TestTrue(TEXT("...and carries the detail"), TaggedSentence.Contains(TEXT("Expansion_02")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Preflight_RefusalSummary,
    "Ck.OptimizationDebugger.Preflight.RefusalSummary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Preflight_RefusalSummary::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_preflight;
    using namespace ck_optimization_debugger_preflight_spec;

    const auto Audits = TArray<FCkOptimizationDebugger_PlacementAudit>
    {
        Build_Audit(TEXT("Rock_0"), {}),
        Build_Audit(TEXT("Rock_1"), {ECkOptimizationDebugger_RefusalKind::CarriesActorTags}),
        Build_Audit(TEXT("Rock_2"), {ECkOptimizationDebugger_RefusalKind::CarriesActorTags,
                                     ECkOptimizationDebugger_RefusalKind::InEditorGroup}),
        Build_Audit(TEXT("Rock_3"), {ECkOptimizationDebugger_RefusalKind::DiffersFromTemplate}),
        Build_Audit(TEXT("Rock_4"), {}),
    };

    TestEqual(TEXT("The convertible placements are the ones with no refusals"),
        Get_ConvertiblePlacements(Audits).Num(), 2);
    TestEqual(TEXT("...and the refused ones are the rest"), Get_RefusedPlacements(Audits).Num(), 3);

    // Order is the input's own: the caller pairs these back up with actors it walked in a sorted order, and a
    // partition that re-ordered them would pair the wrong audit with the wrong placement.
    TestEqual(TEXT("The partition preserves input order"),
        Get_ConvertiblePlacements(Audits)[0].DisplayName, FString{TEXT("Rock_0")});

    const auto Summary = Build_RefusalSummary(Audits);

    TestTrue(TEXT("The summary counts refused against total"), Summary.Contains(TEXT("3 of 5")));
    TestTrue(TEXT("...counting a reason once per placement"), Summary.Contains(TEXT("2 carries actor tags")));
    TestTrue(TEXT("...and every reason that stands in the way"), Summary.Contains(TEXT("1 belongs to an editor Group")));

    // Reasons print in ENUM order, so two runs over one selection produce the same sentence. Counting order would
    // let two reasons that tie swap places between identical presses.
    const auto TagIndex = Summary.Find(TEXT("carries actor tags"));
    const auto GroupIndex = Summary.Find(TEXT("belongs to an editor Group"));
    const auto DiffersIndex = Summary.Find(TEXT("differs from the template"));

    TestTrue(TEXT("Tags print before Group"), TagIndex < GroupIndex);
    TestTrue(TEXT("Group prints before the template comparison"), GroupIndex < DiffersIndex);

    // A placement refused for the SAME reason twice is one placement, not two. Without the per-placement dedup a
    // mesh with six differing properties would report as six refused placements out of five.
    const auto DoubleCounted = TArray<FCkOptimizationDebugger_PlacementAudit>
    {
        Build_Audit(TEXT("Rock_5"), {ECkOptimizationDebugger_RefusalKind::DiffersFromTemplate,
                                     ECkOptimizationDebugger_RefusalKind::DiffersFromTemplate,
                                     ECkOptimizationDebugger_RefusalKind::DiffersFromTemplate}),
    };

    TestTrue(TEXT("One placement counts once per reason"),
        Build_RefusalSummary(DoubleCounted).Contains(TEXT("1 differs from the template")));

    // Nothing refused is an EMPTY summary, not "0 of N left alone" — the success message appends it verbatim, and a
    // sentence saying nothing happened is worse than no sentence.
    const auto AllClear = TArray<FCkOptimizationDebugger_PlacementAudit>{Build_Audit(TEXT("Rock_6"), {})};
    TestTrue(TEXT("A clean audit summarises to nothing"), Build_RefusalSummary(AllClear).IsEmpty());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Preflight_ComparisonExclusions,
    "Ck.OptimizationDebugger.Preflight.ComparisonExclusions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Preflight_ComparisonExclusions::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_preflight;

    const auto& Excluded = Get_ExcludedComparisonProperties();

    // The transform IS carried — it becomes the instance's transform — so comparing it would refuse every placement
    // that is not standing exactly where the template is, which is all of them.
    TestTrue(TEXT("Location is excluded"), Excluded.Contains(FName{TEXT("RelativeLocation")}));
    TestTrue(TEXT("Rotation is excluded"), Excluded.Contains(FName{TEXT("RelativeRotation")}));
    TestTrue(TEXT("Scale is excluded"), Excluded.Contains(FName{TEXT("RelativeScale3D")}));

    // Tags are refused by NAME, with the tags in the message. Letting the reflection comparison report them instead
    // would say "differs from the template (Tags)" and lose both the tag and the reason it matters.
    TestTrue(TEXT("Actor tags are excluded from the comparison"), Excluded.Contains(FName{TEXT("Tags")}));
    TestTrue(TEXT("Component tags are too"), Excluded.Contains(FName{TEXT("ComponentTags")}));

    // The three that caused the reported defect must NOT be excluded: they are exactly what the old hand-written
    // gate forgot, and the whole point of driving the comparison off reflection is that they are compared for free.
    TestFalse(TEXT("Custom primitive data is compared"), Excluded.Contains(FName{TEXT("CustomPrimitiveData")}));
    TestFalse(TEXT("The custom-depth stencil value is compared"),
        Excluded.Contains(FName{TEXT("CustomDepthStencilValue")}));
    TestFalse(TEXT("Cast Shadow is compared"), Excluded.Contains(FName{TEXT("CastShadow")}));

    for (const auto& PropertyName : Excluded)
    {
        auto Occurrences = 0;
        for (const auto& Other : Excluded)
        {
            if (Other == PropertyName)
            { ++Occurrences; }
        }

        TestEqual(ck::Format_UE(TEXT("{} is listed once"), PropertyName), Occurrences, 1);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
