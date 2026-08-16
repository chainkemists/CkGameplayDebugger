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
    FCkOptimizationDebugger_Filters_SuggestedFixAndMute,
    "Ck.OptimizationDebugger.Filters.SuggestedFixAndMute",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Filters_SuggestedFixAndMute::RunTest(const FString& Parameters)
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

    // ---- Muting hides by STABLE KEY ----
    Filter.MutedStableKeys.Add(Fixable.StableKey);

    TestFalse(TEXT("A muted finding is hidden"), Matches_Filter(Fixable, Filter));
    TestTrue(TEXT("...and its neighbour is not"), Matches_Filter(Unfixable, Filter));

    // The escape hatch. Without it, muting could permanently hide findings with no way to audit what was hidden,
    // which would make the tool quietly lie about the project.
    Filter.ShowMuted = true;

    TestTrue(TEXT("Show-muted brings a muted finding back"), Matches_Filter(Fixable, Filter));

    Filter.ShowMuted = false;

    // ---- Mute keys are keyed by identity, not by asset ----
    // A DIFFERENT check on the same asset is a different finding and must not inherit the mute. This is the property
    // that makes muting safe across re-scans: the same problem stays muted, a new problem does not arrive pre-hidden.
    auto OtherCheckSameAsset = MakeAssetFinding(TEXT("Texture.Spec"), TEXT("/Game/A/SM_A.SM_A"), false);

    TestNotEqual(TEXT("Two checks on one asset have different stable keys"),
        OtherCheckSameAsset.StableKey, Fixable.StableKey);

    TestTrue(TEXT("Muting one check does not mute another on the same asset"),
        Matches_Filter(OtherCheckSameAsset, Filter));

    // ---- The axes compose ----
    // Each narrowing is independent, so a reader who set two of them gets the intersection rather than whichever the
    // predicate happened to check last.
    Filter.MutedStableKeys.Reset();
    Filter.ShowOnlyWithSuggestedFix = true;
    Filter.PathScope = TEXT("/Game/B");

    TestFalse(TEXT("Fixable but out of scope is excluded"), Matches_Filter(Fixable, Filter));
    TestFalse(TEXT("In scope but unfixable is excluded"), Matches_Filter(Unfixable, Filter));

    Filter.PathScope = TEXT("/Game/A");

    TestTrue(TEXT("In scope and fixable is admitted"), Matches_Filter(Fixable, Filter));

    return true;
}

#endif

// --------------------------------------------------------------------------------------------------------------------
