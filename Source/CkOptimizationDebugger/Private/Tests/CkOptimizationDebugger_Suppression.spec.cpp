#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_ScanContext.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Suppression.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and the spec files sit in the same merged translation
// unit as each other.
namespace ck_optimization_debugger_suppression_spec
{
    auto
        Build_Finding(
            const FString& InCheckId,
            const FString& InAssetPath)
        -> FCkOptimizationDebugger_FindingRow
    {
        // Through the REAL `Build_Finding`, so the stable key a Finding-scoped suppression matches on is the same
        // string the window would reuse a row by.
        return ck_optimization_debugger_scan::Build_Finding(FName{*InCheckId},
            ECkOptimizationDebugger_Severity::Major,
            ECkOptimizationDebugger_Category::Mesh,
            ck_optimization_debugger_scan::Build_AssetTarget(FSoftObjectPath{InAssetPath}, TEXT("Spec")),
            TEXT("Spec finding"),
            TEXT("Why."),
            TEXT("What to do."));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_Suppression(
            ECkOptimizationDebugger_SuppressionScope InScope,
            const FString& InPattern,
            const FString& InCheckId)
        -> FCkOptimizationDebugger_Suppression
    {
        auto Suppression = FCkOptimizationDebugger_Suppression{};
        Suppression.Scope = InScope;
        Suppression.Tier = ECkOptimizationDebugger_SuppressionTier::Project;
        Suppression.Pattern = InPattern;
        Suppression.CheckId = InCheckId.IsEmpty() ? FName{} : FName{*InCheckId};
        Suppression.Reason = TEXT("Spec reason");
        Suppression.Author = TEXT("spec");
        Suppression.Date = TEXT("2026-08-21T00:00:00Z");

        return Suppression;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Suppression_ScopeMatching,
    "Ck.OptimizationDebugger.Suppression.ScopeMatching",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Suppression_ScopeMatching::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_suppression;
    using namespace ck_optimization_debugger_suppression_spec;

    const auto Rock = Build_Finding(TEXT("Mesh.TriangleBudget"), TEXT("/Game/Props/SM_Rock.SM_Rock"));
    const auto RockOtherCheck = Build_Finding(TEXT("Mesh.MissingLods"), TEXT("/Game/Props/SM_Rock.SM_Rock"));
    const auto Cliff = Build_Finding(TEXT("Mesh.TriangleBudget"), TEXT("/Game/Props/SM_Cliff.SM_Cliff"));
    const auto Far = Build_Finding(TEXT("Mesh.TriangleBudget"), TEXT("/Game/Characters/SM_Hero.SM_Hero"));

    // ---- Finding: the narrowest. One row, and nothing else. ----
    const auto FindingScope = Build_Suppression(
        ECkOptimizationDebugger_SuppressionScope::Finding, Rock.StableKey, FString{});

    TestTrue(TEXT("A finding scope hides its own row"), Matches(FindingScope, Rock));
    TestFalse(TEXT("...not another check on the same asset"), Matches(FindingScope, RockOtherCheck));
    TestFalse(TEXT("...and not a different asset"), Matches(FindingScope, Cliff));

    // ---- Asset: this check, on this asset. ----
    const auto AssetScope = Build_Suppression(ECkOptimizationDebugger_SuppressionScope::Asset,
        TEXT("/Game/Props/SM_Rock.SM_Rock"), TEXT("Mesh.TriangleBudget"));

    TestTrue(TEXT("An asset scope hides the named check on the named asset"), Matches(AssetScope, Rock));
    TestFalse(TEXT("...but not a different check on it"), Matches(AssetScope, RockOtherCheck));
    TestFalse(TEXT("...and not the same check elsewhere"), Matches(AssetScope, Cliff));

    // A check id NARROWS rather than being required: "everything about this asset" is a thing a reader means, and
    // an unset check id is how the record spells it.
    const auto AssetScopeAnyCheck = Build_Suppression(ECkOptimizationDebugger_SuppressionScope::Asset,
        TEXT("/Game/Props/SM_Rock.SM_Rock"), FString{});

    TestTrue(TEXT("An asset scope with no check hides every check on that asset"),
        Matches(AssetScopeAnyCheck, RockOtherCheck));

    // ---- Folder: prefix, so a subfolder is covered too. ----
    const auto FolderScope = Build_Suppression(ECkOptimizationDebugger_SuppressionScope::Folder,
        TEXT("/Game/Props"), TEXT("Mesh.TriangleBudget"));

    TestTrue(TEXT("A folder scope covers assets under it"), Matches(FolderScope, Rock));
    TestTrue(TEXT("...including siblings"), Matches(FolderScope, Cliff));
    TestFalse(TEXT("...and stops at the folder boundary"), Matches(FolderScope, Far));

    // ---- Check: everywhere. ----
    const auto CheckScope = Build_Suppression(ECkOptimizationDebugger_SuppressionScope::Check,
        FString{}, TEXT("Mesh.TriangleBudget"));

    TestTrue(TEXT("A check scope covers every asset"), Matches(CheckScope, Far));
    TestFalse(TEXT("...but only its own check"), Matches(CheckScope, RockOtherCheck));

    // ---- A rule that covers nothing must never match everything ----
    // An empty pattern on a scope that needs one is the shape a truncated config line takes, and matching on it
    // would hide the entire list on the strength of a corrupt file.
    const auto EmptyAsset = Build_Suppression(ECkOptimizationDebugger_SuppressionScope::Asset, FString{}, FString{});
    const auto EmptyCheck = Build_Suppression(ECkOptimizationDebugger_SuppressionScope::Check, FString{}, FString{});

    TestFalse(TEXT("An empty asset pattern matches nothing"), Matches(EmptyAsset, Rock));
    TestFalse(TEXT("An empty check id matches nothing"), Matches(EmptyCheck, Rock));

    // ---- The match is REPORTED, not just counted ----
    const auto All = TArray<FCkOptimizationDebugger_Suppression>{FolderScope};
    const auto* Match = TryGet_Match(All, Rock);

    TestTrue(TEXT("The covering rule is retrievable"), Match != nullptr);

    if (Match != nullptr)
    { TestTrue(TEXT("...carrying its reason"), Build_Label(*Match).Contains(TEXT("Spec reason"))); }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Suppression_ConfigRoundTrip,
    "Ck.OptimizationDebugger.Suppression.ConfigRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Suppression_ConfigRoundTrip::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_suppression;
    using namespace ck_optimization_debugger_suppression_spec;

    auto Original = Build_Suppression(ECkOptimizationDebugger_SuppressionScope::Folder,
        TEXT("/Game/Props"), TEXT("Mesh.TriangleBudget"));

    // The reason is free text and CONTAINS the field separator. Putting it last is what lets it, and a round trip
    // that mangled it would quietly rewrite somebody's rationale in a committed file.
    Original.Reason = TEXT("Hero props; seen at 2 m; 4096 is deliberate");

    auto Parsed = FCkOptimizationDebugger_Suppression{};

    TestTrue(TEXT("A serialized record parses"), TryParse(Serialize(Original), Parsed));
    TestEqual(TEXT("Scope survives"), static_cast<int32>(Parsed.Scope), static_cast<int32>(Original.Scope));
    TestEqual(TEXT("Pattern survives"), Parsed.Pattern, Original.Pattern);
    TestEqual(TEXT("Check id survives"), Parsed.CheckId, Original.CheckId);
    TestEqual(TEXT("Author survives"), Parsed.Author, Original.Author);
    TestEqual(TEXT("Date survives"), Parsed.Date, Original.Date);
    TestEqual(TEXT("A reason containing the separator survives whole"), Parsed.Reason, Original.Reason);

    // ---- A malformed line is DROPPED, never half-applied ----
    auto Ignored = FCkOptimizationDebugger_Suppression{};

    TestFalse(TEXT("Rubbish does not parse"), TryParse(TEXT("this is not a record"), Ignored));
    TestFalse(TEXT("An unknown scope does not parse"),
        TryParse(TEXT("Scope=Everything;Check=;Pattern=/Game;Author=x;Date=y;Reason=z"), Ignored));
    TestFalse(TEXT("A folder rule with no pattern does not parse"),
        TryParse(TEXT("Scope=Folder;Check=;Pattern=;Author=x;Date=y;Reason=z"), Ignored));
    TestFalse(TEXT("A check rule with no check id does not parse"),
        TryParse(TEXT("Scope=Check;Check=;Pattern=;Author=x;Date=y;Reason=z"), Ignored));

    auto DroppedCount = 0;

    const auto Round = Parse_All(
        TArray<FString>{Serialize(Original), TEXT("garbage"), FString{}},
        ECkOptimizationDebugger_SuppressionTier::Personal,
        DroppedCount);

    TestEqual(TEXT("The good line survives"), Round.Num(), 1);
    TestEqual(TEXT("The bad one is counted"), DroppedCount, 1);
    TestEqual(TEXT("A blank line is neither kept nor counted as a failure"), DroppedCount, 1);
    TestEqual(TEXT("The tier is stamped by the store that loaded it"),
        static_cast<int32>(Round[0].Tier), static_cast<int32>(ECkOptimizationDebugger_SuppressionTier::Personal));

    // ---- Serialization is SORTED, because this file is committed ----
    const auto Lines = Serialize_All({
        Build_Suppression(ECkOptimizationDebugger_SuppressionScope::Folder, TEXT("/Game/Zulu"), TEXT("Mesh.A")),
        Build_Suppression(ECkOptimizationDebugger_SuppressionScope::Folder, TEXT("/Game/Alpha"), TEXT("Mesh.A"))});

    TestEqual(TEXT("Both records serialize"), Lines.Num(), 2);
    TestTrue(TEXT("Lines are sorted, so two people adding entries do not fight over the whole file"),
        Lines[0].Compare(Lines[1], ESearchCase::CaseSensitive) < 0);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Suppression_BuildForFinding,
    "Ck.OptimizationDebugger.Suppression.BuildForFinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Suppression_BuildForFinding::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_suppression;
    using namespace ck_optimization_debugger_suppression_spec;

    const auto Rock = Build_Finding(TEXT("Mesh.TriangleBudget"), TEXT("/Game/Props/Rocks/SM_Rock.SM_Rock"));

    const auto AsFolder = Build_ForFinding(Rock,
        ECkOptimizationDebugger_SuppressionScope::Folder,
        ECkOptimizationDebugger_SuppressionTier::Project,
        TEXT("Quarry set is deliberately dense"), TEXT("adam"), TEXT("2026-08-21"));

    // The asset's OWN folder, which is the scope a reader means by "and everything beside it" — not the asset path
    // with a slash on the end, which would match nothing.
    TestEqual(TEXT("A folder rule uses the asset's folder"), AsFolder.Pattern, FString{TEXT("/Game/Props/Rocks")});
    TestTrue(TEXT("...and covers the finding it was built from"), Matches(AsFolder, Rock));

    const auto AsAsset = Build_ForFinding(Rock,
        ECkOptimizationDebugger_SuppressionScope::Asset,
        ECkOptimizationDebugger_SuppressionTier::Project,
        TEXT("Reason"), TEXT("adam"), TEXT("2026-08-21"));

    TestTrue(TEXT("An asset rule covers its own finding"), Matches(AsAsset, Rock));
    TestEqual(TEXT("...narrowed to the check it came from"), AsAsset.CheckId, Rock.CheckId);

    const auto AsCheck = Build_ForFinding(Rock,
        ECkOptimizationDebugger_SuppressionScope::Check,
        ECkOptimizationDebugger_SuppressionTier::Project,
        TEXT("Reason"), TEXT("adam"), TEXT("2026-08-21"));

    TestTrue(TEXT("A check rule covers its own finding"), Matches(AsCheck, Rock));
    TestTrue(TEXT("...and needs no pattern"), AsCheck.Pattern.IsEmpty());

    const auto AsFinding = Build_ForFinding(Rock,
        ECkOptimizationDebugger_SuppressionScope::Finding,
        ECkOptimizationDebugger_SuppressionTier::Personal,
        TEXT("Reason"), TEXT("adam"), TEXT("2026-08-21"));

    TestEqual(TEXT("A finding rule keys on the stable key"), AsFinding.Pattern, Rock.StableKey);
    TestEqual(TEXT("...and keeps the tier it was asked for"),
        static_cast<int32>(AsFinding.Tier), static_cast<int32>(ECkOptimizationDebugger_SuppressionTier::Personal));

    // Every scope carries the who and the when, because the person deciding whether a suppression still applies is
    // usually not the person who made it.
    TestEqual(TEXT("The author is stamped"), AsFolder.Author, FString{TEXT("adam")});
    TestEqual(TEXT("The date is stamped"), AsFolder.Date, FString{TEXT("2026-08-21")});

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
