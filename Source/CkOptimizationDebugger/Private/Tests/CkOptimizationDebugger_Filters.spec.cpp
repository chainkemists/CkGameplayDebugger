#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_filters_spec
{
    auto
        MakeAssetFinding(
            const FString& InCheckId,
            const FString& InAssetPath,
            bool InHasAutoFix)
        -> FCkOptimizationDebugger_FindingRow
    {
        auto Finding = FCkOptimizationDebugger_FindingRow{};
        Finding.CheckId = FName{*InCheckId};
        Finding.Severity = ECkOptimizationDebugger_Severity::Major;
        Finding.Category = ECkOptimizationDebugger_Category::Mesh;
        Finding.Target.Kind = ECkOptimizationDebugger_TargetKind::Asset;
        Finding.Target.Path = FSoftObjectPath{InAssetPath};
        Finding.Target.DisplayName = InAssetPath;
        Finding.Title = TEXT("Spec finding");
        Finding.HasAutoFix = InHasAutoFix;
        Finding.StableKey = ck_optimization_debugger_model::Build_StableKey(Finding.CheckId, Finding.Target);
        return Finding;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        MakeSettingsFinding()
        -> FCkOptimizationDebugger_FindingRow
    {
        auto Finding = FCkOptimizationDebugger_FindingRow{};
        Finding.CheckId = FName{TEXT("ProjectSettings.Spec")};
        Finding.Severity = ECkOptimizationDebugger_Severity::Minor;
        Finding.Category = ECkOptimizationDebugger_Category::ProjectSettings;
        Finding.Target.Kind = ECkOptimizationDebugger_TargetKind::ProjectSettings;
        Finding.Target.SettingsSectionName = TEXT("Rendering");
        Finding.Title = TEXT("Spec settings finding");
        Finding.StableKey = ck_optimization_debugger_model::Build_StableKey(Finding.CheckId, Finding.Target);
        return Finding;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        MakeRatioFinding(
            const FString& InCheckId,
            const FString& InAssetPath,
            float InBudgetRatio)
        -> FCkOptimizationDebugger_FindingRow
    {
        constexpr auto HasAutoFix = false;

        auto Finding = MakeAssetFinding(InCheckId, InAssetPath, HasAutoFix);
        Finding.BudgetRatio = InBudgetRatio;
        return Finding;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Filters_PathScope,
    "Ck.OptimizationDebugger.Filters.PathScope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Filters_PathScope::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_filters_spec;
    using namespace ck_optimization_debugger_model;

    const auto Character = MakeAssetFinding(TEXT("Mesh.Spec"), TEXT("/Game/Characters/SM_Hero.SM_Hero"), false);
    const auto Prop = MakeAssetFinding(TEXT("Mesh.Spec"), TEXT("/Game/Props/SM_Crate.SM_Crate"), false);
    const auto Settings = MakeSettingsFinding();

    // ---- An empty scope is not a filter ----
    TestTrue(TEXT("Empty scope admits an asset finding"), Matches_PathScope(Character, FString{}));
    TestTrue(TEXT("Empty scope admits a settings finding"), Matches_PathScope(Settings, FString{}));

    // ---- A scope is a prefix over the TARGET PATH, not a text search ----
    TestTrue(TEXT("A finding under the scope is admitted"),
        Matches_PathScope(Character, TEXT("/Game/Characters")));

    TestFalse(TEXT("A finding outside the scope is excluded"),
        Matches_PathScope(Prop, TEXT("/Game/Characters")));

    // Case-insensitive: package paths are, on the platform this ships on and in the Content Browser, so a scope that
    // split on case would exclude the reader's own folder because they typed it differently.
    TestTrue(TEXT("Scope matching ignores case"),
        Matches_PathScope(Character, TEXT("/game/CHARACTERS")));

    // ---- A ProjectSettings finding has no path, so ANY scope excludes it ----
    // Deliberate, and asserted so it cannot be "fixed" later by somebody reading it as an oversight: a reader who has
    // narrowed to a content folder is not asking about the renderer, and keeping those rows would make the scope look
    // like it had failed to apply.
    TestFalse(TEXT("A non-empty scope excludes a pathless settings finding"),
        Matches_PathScope(Settings, TEXT("/Game")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Filters_SuggestedFixAndSuppression,
    "Ck.OptimizationDebugger.Filters.SuggestedFixAndSuppression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Filters_SuggestedFixAndSuppression::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_filters_spec;
    using namespace ck_optimization_debugger_model;

    const auto Fixable = MakeAssetFinding(TEXT("Mesh.Spec"), TEXT("/Game/A/SM_A.SM_A"), true);
    const auto Unfixable = MakeAssetFinding(TEXT("Mesh.Spec"), TEXT("/Game/B/SM_B.SM_B"), false);

    // ---- A default filter admits both ----
    auto Filter = FCkOptimizationDebugger_FilterState{};

    TestTrue(TEXT("A default filter admits a fixable finding"), Matches_Filter(Fixable, Filter));
    TestTrue(TEXT("A default filter admits an unfixable finding"), Matches_Filter(Unfixable, Filter));

    // ---- Suggested-fix narrowing reads the CHECK's claim ----
    Filter.ShowOnlyWithSuggestedFix = true;

    TestTrue(TEXT("Suggested-fix narrowing keeps a finding whose check offered one"),
        Matches_Filter(Fixable, Filter));

    TestFalse(TEXT("Suggested-fix narrowing drops a finding whose check offered none"),
        Matches_Filter(Unfixable, Filter));

    Filter.ShowOnlyWithSuggestedFix = false;

    // ---- A Finding-scoped suppression hides by STABLE KEY ----
    auto Suppression = FCkOptimizationDebugger_Suppression{};
    Suppression.Scope = ECkOptimizationDebugger_SuppressionScope::Finding;
    Suppression.Pattern = Fixable.StableKey;
    Suppression.Reason = TEXT("Spec reason");

    Filter.Suppressions.Add(Suppression);

    TestFalse(TEXT("A suppressed finding is hidden"), Matches_Filter(Fixable, Filter));
    TestTrue(TEXT("...and its neighbour is not"), Matches_Filter(Unfixable, Filter));

    // The escape hatch. Without it, suppression could permanently hide findings with no way to audit what was
    // hidden, which would make the tool quietly lie about the project — and let a teammate inherit an exception
    // they can never see.
    Filter.ShowSuppressed = true;

    TestTrue(TEXT("Show-suppressed brings a suppressed finding back"), Matches_Filter(Fixable, Filter));

    Filter.ShowSuppressed = false;

    // ---- A Finding scope is keyed by identity, not by asset ----
    // A DIFFERENT check on the same asset is a different finding and must not inherit the suppression. This is the
    // property that makes it safe across re-scans: the same problem stays hidden, a new problem does not arrive
    // pre-hidden. The ASSET scope is how a reader says the wider thing on purpose.
    auto OtherCheckSameAsset = MakeAssetFinding(TEXT("Texture.Spec"), TEXT("/Game/A/SM_A.SM_A"), false);

    TestNotEqual(TEXT("Two checks on one asset have different stable keys"),
        OtherCheckSameAsset.StableKey, Fixable.StableKey);

    TestTrue(TEXT("Suppressing one check does not suppress another on the same asset"),
        Matches_Filter(OtherCheckSameAsset, Filter));

    // ---- The axes compose ----
    // Each narrowing is independent, so a reader who set two of them gets the intersection rather than whichever the
    // predicate happened to check last.
    Filter.Suppressions.Reset();
    Filter.ShowOnlyWithSuggestedFix = true;
    Filter.PathScope = TEXT("/Game/B");

    TestFalse(TEXT("Fixable but out of scope is excluded"), Matches_Filter(Fixable, Filter));
    TestFalse(TEXT("In scope but unfixable is excluded"), Matches_Filter(Unfixable, Filter));

    Filter.PathScope = TEXT("/Game/A");

    TestTrue(TEXT("In scope and fixable is admitted"), Matches_Filter(Fixable, Filter));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Filters_BudgetRatio,
    "Ck.OptimizationDebugger.Filters.BudgetRatio",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Filters_BudgetRatio::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_filters_spec;
    using namespace ck_optimization_debugger_model;

    const auto WayOver = MakeRatioFinding(TEXT("Mesh.Spec"), TEXT("/Game/A/SM_A.SM_A"), 4.0f);
    const auto JustOver = MakeRatioFinding(TEXT("Mesh.Spec"), TEXT("/Game/B/SM_B.SM_B"), 1.2f);
    const auto NoBudget = MakeRatioFinding(TEXT("Mesh.Spec"), TEXT("/Game/C/SM_C.SM_C"), 0.0f);

    // ---- The resting threshold is not a filter ----
    auto Filter = FCkOptimizationDebugger_FilterState{};

    TestTrue(TEXT("A resting threshold admits a finding far over budget"), Matches_Filter(WayOver, Filter));
    TestTrue(TEXT("...one barely over"), Matches_Filter(JustOver, Filter));
    TestTrue(TEXT("...and one with no budget behind it at all"), Matches_Filter(NoBudget, Filter));

    // ---- The threshold admits AT or past itself ----
    Filter.MinBudgetRatio = 2.0f;

    TestTrue(TEXT("A finding past the threshold is admitted"), Matches_Filter(WayOver, Filter));
    TestFalse(TEXT("A finding short of it is excluded"), Matches_Filter(JustOver, Filter));

    Filter.MinBudgetRatio = 4.0f;
    TestTrue(TEXT("A finding exactly at the threshold is admitted"), Matches_Filter(WayOver, Filter));

    // ---- ANY non-zero threshold excludes a finding with no budget behind it ----
    // The same reading that excludes a pathless ProjectSettings finding from a path scope, and asserted explicitly so
    // it cannot later be read as an oversight: a reader asking what is furthest over budget is not asking about the
    // checks that measure nothing, and keeping those rows would make the threshold look like it had failed to apply.
    Filter.MinBudgetRatio = 0.01f;

    TestFalse(TEXT("The smallest non-zero threshold still excludes a finding with no budget"),
        Matches_Filter(NoBudget, Filter));
    TestTrue(TEXT("...while admitting every finding that has one"), Matches_Filter(JustOver, Filter));

    // ---- The model clamps the threshold and counts what is behind it ----
    auto Model = FCkOptimizationDebugger_Model{};
    Model.Set_Findings({WayOver, JustOver, NoBudget});

    TestEqual(TEXT("A fresh threshold rests at zero"), Model.Get_MinBudgetRatio(), 0.0f);
    TestEqual(TEXT("...which admits everything, zero-ratio findings included"),
        Model.Get_VisibleFindings().Num(), 3);

    // A negative threshold is not a reader error to report — it is the same question as "no threshold".
    Model.Set_MinBudgetRatio(-3.0f);

    TestEqual(TEXT("A negative threshold clamps back to the resting zero"), Model.Get_MinBudgetRatio(), 0.0f);
    TestEqual(TEXT("...and still admits everything"), Model.Get_VisibleFindings().Num(), 3);

    Model.Set_MinBudgetRatio(2.0f);

    TestEqual(TEXT("A real threshold narrows to the findings at or past it"), Model.Get_VisibleFindings().Num(), 1);
    TestEqual(TEXT("...and it is the one furthest over"),
        Model.Get_VisibleFindings()[0].StableKey, WayOver.StableKey);

    // The affordance's own number: how many findings have a budget AT ALL, so a reader can tell "nothing is that far
    // over" from "no check here measures anything". It follows the current findings, never the threshold.
    TestEqual(TEXT("The budget count is over the CURRENT findings, not what survived the threshold"),
        Model.Get_FindingsWithBudgetCount(), 2);

    // ---- The badge's wording ----
    TestEqual(TEXT("A finding with no budget renders no badge at all"), Format_BudgetRatio(0.0f), FString{});
    TestEqual(TEXT("...and neither does a negative ratio"), Format_BudgetRatio(-1.0f), FString{});
    TestEqual(TEXT("Below ten carries one decimal"), Format_BudgetRatio(2.4f), FString{TEXT("2.4x")});
    TestEqual(TEXT("Ten itself drops it"), Format_BudgetRatio(10.0f), FString{TEXT("10x")});
    TestEqual(TEXT("...as does anything above it"), Format_BudgetRatio(13.0f), FString{TEXT("13x")});

    return true;
}

#endif

// --------------------------------------------------------------------------------------------------------------------
