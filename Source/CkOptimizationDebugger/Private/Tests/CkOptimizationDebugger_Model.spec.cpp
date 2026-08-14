#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_model_spec
{
    auto
        MakeTarget(
            const FString& InAssetPath,
            const FString& InDisplayName)
        -> FCkOptimizationDebugger_Target
    {
        auto Target = FCkOptimizationDebugger_Target{};
        Target.Kind = ECkOptimizationDebugger_TargetKind::Asset;
        Target.Path = FSoftObjectPath{InAssetPath};
        Target.DisplayName = InDisplayName;
        return Target;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Fixtures go through the real key builder, so the identity the sort tie-breaks on and the identity a row would
     *  be reused by are the same string in the specs as they are in the window. */
    auto
        MakeFinding(
            const FString& InCheckId,
            ECkOptimizationDebugger_Severity InSeverity,
            ECkOptimizationDebugger_Category InCategory,
            const FString& InTitle,
            const FString& InAssetName)
        -> FCkOptimizationDebugger_FindingRow
    {
        auto Finding = FCkOptimizationDebugger_FindingRow{};
        Finding.CheckId = FName{*InCheckId};
        Finding.Severity = InSeverity;
        Finding.Category = InCategory;
        Finding.Target = MakeTarget(
            ck::Format_UE(TEXT("/Game/Spec/{}.{}"), InAssetName, InAssetName),
            InAssetName);
        Finding.Title = InTitle;
        Finding.StableKey = ck_optimization_debugger_model::Build_StableKey(Finding.CheckId, Finding.Target);
        return Finding;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Model_FilterMatching,
    "Ck.OptimizationDebugger.Model.FilterMatching",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Model_FilterMatching::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_model_spec;
    using namespace ck_optimization_debugger_model;

    const auto DenseMesh = MakeFinding(TEXT("Mesh.TriangleBudget"),
        ECkOptimizationDebugger_Severity::Critical, ECkOptimizationDebugger_Category::Mesh,
        TEXT("Triangle count over budget"), TEXT("SM_Boulder"));

    const auto BigTexture = MakeFinding(TEXT("Texture.MaxSize"),
        ECkOptimizationDebugger_Severity::Minor, ECkOptimizationDebugger_Category::Texture,
        TEXT("Texture larger than the budget"), TEXT("T_Gravel"));

    auto Model = FCkOptimizationDebugger_Model{};
    Model.Set_Findings({DenseMesh, BigTexture});

    TestEqual(TEXT("A fresh model filters nothing"), Model.Get_VisibleFindings().Num(), 2);
    TestEqual(TEXT("A fresh severity mask admits every severity"),
        static_cast<int32>(Model.Get_Filter().SeverityMask), static_cast<int32>(k_AllSeverityMask));
    TestEqual(TEXT("A fresh category mask admits every category"),
        static_cast<int32>(Model.Get_Filter().CategoryMask), static_cast<int32>(k_AllCategoryMask));

    // --- text filter: case-insensitive substring across title, target display name and check id ---
    Model.Get_Filter().FilterString = TEXT("BOULDER");
    TestEqual(TEXT("The filter matches the target display name case-insensitively"),
        Model.Get_VisibleFindings().Num(), 1);

    Model.Get_Filter().FilterString = TEXT("larger than");
    TestEqual(TEXT("The filter matches the title"), Model.Get_VisibleFindings().Num(), 1);
    TestEqual(TEXT("...and it is the texture finding"),
        Model.Get_VisibleFindings()[0].CheckId, FName{TEXT("Texture.MaxSize")});

    Model.Get_Filter().FilterString = TEXT("trianglebudget");
    TestEqual(TEXT("The filter matches the check id"), Model.Get_VisibleFindings().Num(), 1);

    Model.Get_Filter().FilterString = TEXT("nothing-matches-this");
    TestEqual(TEXT("A filter nothing matches hides everything"), Model.Get_VisibleFindings().Num(), 0);

    // The cheap count the tab bar binds to must agree with the list it is counting, always.
    TestEqual(TEXT("The visible COUNT agrees with the visible LIST"),
        Model.Get_VisibleFindingCount(), Model.Get_VisibleFindings().Num());

    Model.Get_Filter().FilterString = FString{};
    TestEqual(TEXT("An empty filter passes everything"), Model.Get_VisibleFindings().Num(), 2);

    // --- highlight never hides ---
    Model.Get_Filter().HighlightString = TEXT("nothing-matches-this");
    TestEqual(TEXT("Highlight narrows nothing — it dims"), Model.Get_VisibleFindings().Num(), 2);
    Model.Get_Filter().HighlightString = FString{};

    // --- severity toggles ---
    Model.Set_SeverityVisible(ECkOptimizationDebugger_Severity::Minor, false);
    TestFalse(TEXT("The toggled-off severity reads as hidden"),
        Model.Get_SeverityVisible(ECkOptimizationDebugger_Severity::Minor));
    TestEqual(TEXT("Hiding a severity drops its findings"), Model.Get_VisibleFindings().Num(), 1);
    TestEqual(TEXT("...and keeps the other severity"),
        Model.Get_VisibleFindings()[0].CheckId, FName{TEXT("Mesh.TriangleBudget")});

    Model.Set_SeverityVisible(ECkOptimizationDebugger_Severity::Minor, true);
    TestEqual(TEXT("Restoring the severity restores its findings"), Model.Get_VisibleFindings().Num(), 2);

    // --- category toggles ---
    Model.Set_CategoryVisible(ECkOptimizationDebugger_Category::Mesh, false);
    TestFalse(TEXT("The toggled-off category reads as hidden"),
        Model.Get_CategoryVisible(ECkOptimizationDebugger_Category::Mesh));
    TestEqual(TEXT("Hiding a category drops its findings"), Model.Get_VisibleFindings().Num(), 1);
    TestEqual(TEXT("...and keeps the other category"),
        Model.Get_VisibleFindings()[0].CheckId, FName{TEXT("Texture.MaxSize")});

    // --- the two masks compose, they do not override each other ---
    Model.Set_SeverityVisible(ECkOptimizationDebugger_Severity::Minor, false);
    TestEqual(TEXT("Severity and category filters compose"), Model.Get_VisibleFindings().Num(), 0);

    Model.Set_CategoryVisible(ECkOptimizationDebugger_Category::Mesh, true);
    Model.Set_SeverityVisible(ECkOptimizationDebugger_Severity::Minor, true);
    TestEqual(TEXT("Restoring both masks restores every finding"), Model.Get_VisibleFindings().Num(), 2);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Model_SeverityTones,
    "Ck.OptimizationDebugger.Model.SeverityTones",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Model_SeverityTones::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_model;

    TestEqual(TEXT("Critical reads as an error"),
        static_cast<int32>(Get_SeverityTone(ECkOptimizationDebugger_Severity::Critical)),
        static_cast<int32>(ECk_Tone::Err));
    TestEqual(TEXT("Major reads as a warning"),
        static_cast<int32>(Get_SeverityTone(ECkOptimizationDebugger_Severity::Major)),
        static_cast<int32>(ECk_Tone::Warn));
    TestEqual(TEXT("Minor reads as information"),
        static_cast<int32>(Get_SeverityTone(ECkOptimizationDebugger_Severity::Minor)),
        static_cast<int32>(ECk_Tone::Info));

    // Distinctness is the point: three severities sharing a tone would turn the severity column into decoration.
    auto SeenTones = TSet<int32>{};

    for (const auto Severity : Get_AllSeverities())
    {
        SeenTones.Add(static_cast<int32>(Get_SeverityTone(Severity)));
    }

    TestEqual(TEXT("Every severity maps to a DISTINCT tone"), SeenTones.Num(), k_SeverityCount);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Model_SortDeterminism,
    "Ck.OptimizationDebugger.Model.SortDeterminism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Model_SortDeterminism::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_model_spec;
    using namespace ck_optimization_debugger_model;

    // Two findings equal on severity, category AND title — only the stable key separates them. Without the final
    // tie-break TArray::Sort would be free to swap them between two identical scans.
    const auto TwinA = MakeFinding(TEXT("Mesh.TriangleBudget"),
        ECkOptimizationDebugger_Severity::Major, ECkOptimizationDebugger_Category::Mesh,
        TEXT("Triangle count over budget"), TEXT("SM_Alpha"));

    const auto TwinB = MakeFinding(TEXT("Mesh.TriangleBudget"),
        ECkOptimizationDebugger_Severity::Major, ECkOptimizationDebugger_Category::Mesh,
        TEXT("Triangle count over budget"), TEXT("SM_Bravo"));

    const auto CriticalLate = MakeFinding(TEXT("Lighting.MovableLights"),
        ECkOptimizationDebugger_Severity::Critical, ECkOptimizationDebugger_Category::Lighting,
        TEXT("Too many movable lights"), TEXT("L_Main"));

    const auto MinorTexture = MakeFinding(TEXT("Texture.MaxSize"),
        ECkOptimizationDebugger_Severity::Minor, ECkOptimizationDebugger_Category::Texture,
        TEXT("Texture larger than the budget"), TEXT("T_Gravel"));

    const auto MajorTexture = MakeFinding(TEXT("Texture.Samplers"),
        ECkOptimizationDebugger_Severity::Major, ECkOptimizationDebugger_Category::Texture,
        TEXT("Sampler count over budget"), TEXT("T_Gravel"));

    const auto Sorted = Get_SortedFindings({TwinB, MinorTexture, TwinA, MajorTexture, CriticalLate});

    TestEqual(TEXT("Every finding survives the sort"), Sorted.Num(), 5);
    TestEqual(TEXT("The most severe finding sorts first"), Sorted[0].CheckId, FName{TEXT("Lighting.MovableLights")});
    TestEqual(TEXT("Within Major, Mesh sorts before Texture (category order)"),
        static_cast<int32>(Sorted[1].Category), static_cast<int32>(ECkOptimizationDebugger_Category::Mesh));
    TestEqual(TEXT("The equal-severity twins tie-break on the stable key"),
        Sorted[1].StableKey, TwinA.StableKey);
    TestEqual(TEXT("...and its twin follows immediately"), Sorted[2].StableKey, TwinB.StableKey);
    TestEqual(TEXT("The Minor finding sorts last"), Sorted[4].CheckId, FName{TEXT("Texture.MaxSize")});

    // Determinism is the actual claim: a differently-ordered input must produce the same output.
    const auto Reordered = Get_SortedFindings({MinorTexture, CriticalLate, MajorTexture, TwinA, TwinB});

    TestEqual(TEXT("A reordered input sorts to the same length"), Reordered.Num(), Sorted.Num());

    for (auto Index = 0; Index < Sorted.Num(); ++Index)
    {
        TestEqual(*ck::Format_UE(TEXT("Row {} is identical regardless of input order"), Index),
            Reordered[Index].StableKey, Sorted[Index].StableKey);
    }

    // The model sorts on store, so the same order shows up through the model without a second sort site.
    auto Model = FCkOptimizationDebugger_Model{};
    Model.Set_Findings({MinorTexture, TwinB, CriticalLate, MajorTexture, TwinA});

    TestEqual(TEXT("The model stores the findings already sorted"),
        Model.Get_Findings()[0].StableKey, Sorted[0].StableKey);
    TestTrue(TEXT("Storing findings marks the model as scanned"), Model.Get_HasScanned());

    Model.Reset();
    TestEqual(TEXT("Reset drops the findings"), Model.Get_Findings().Num(), 0);
    TestFalse(TEXT("Reset drops the scanned flag"), Model.Get_HasScanned());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Model_SeverityCounts,
    "Ck.OptimizationDebugger.Model.SeverityCounts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Model_SeverityCounts::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_model_spec;
    using namespace ck_optimization_debugger_model;

    auto Model = FCkOptimizationDebugger_Model{};

    const auto EmptyCounts = Model.Get_CountsBySeverity();
    TestEqual(TEXT("An unscanned model counts nothing"), EmptyCounts.Get_Total(), 0);

    Model.Set_Findings(
    {
        MakeFinding(TEXT("Lighting.MovableLights"), ECkOptimizationDebugger_Severity::Critical,
            ECkOptimizationDebugger_Category::Lighting, TEXT("Too many movable lights"), TEXT("L_Main")),
        MakeFinding(TEXT("Mesh.TriangleBudget"), ECkOptimizationDebugger_Severity::Major,
            ECkOptimizationDebugger_Category::Mesh, TEXT("Triangle count over budget"), TEXT("SM_Alpha")),
        MakeFinding(TEXT("Mesh.NaniteCandidate"), ECkOptimizationDebugger_Severity::Major,
            ECkOptimizationDebugger_Category::Mesh, TEXT("Nanite candidate"), TEXT("SM_Bravo")),
        MakeFinding(TEXT("Texture.MaxSize"), ECkOptimizationDebugger_Severity::Minor,
            ECkOptimizationDebugger_Category::Texture, TEXT("Texture larger than the budget"), TEXT("T_Gravel")),
    });

    const auto Counts = Model.Get_CountsBySeverity();
    TestEqual(TEXT("One Critical"), Counts.CriticalCount, 1);
    TestEqual(TEXT("Two Major"), Counts.MajorCount, 2);
    TestEqual(TEXT("One Minor"), Counts.MinorCount, 1);
    TestEqual(TEXT("Four in total"), Counts.Get_Total(), 4);

    // The visible projection answers the filtered question — the page-bar count and the dashboard headline are
    // deliberately different numbers, and confusing them would misreport a filtered list as a clean level.
    Model.Set_CategoryVisible(ECkOptimizationDebugger_Category::Mesh, false);

    const auto VisibleCounts = Model.Get_VisibleCountsBySeverity();
    TestEqual(TEXT("Filtering out Mesh drops both Major findings from the visible count"),
        VisibleCounts.MajorCount, 0);
    TestEqual(TEXT("The visible total follows the filter"), VisibleCounts.Get_Total(), 2);
    TestEqual(TEXT("The unfiltered total does NOT follow the filter"),
        Model.Get_CountsBySeverity().Get_Total(), 4);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Model_PageIdentity,
    "Ck.OptimizationDebugger.Model.PageIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Model_PageIdentity::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_model;

    const auto Pages = Get_AllPages();
    TestEqual(TEXT("Every page is enumerated"), Pages.Num(), k_PageCount);

    auto SeenIds = TSet<FName>{};

    for (auto Index = 0; Index < Pages.Num(); ++Index)
    {
        const auto Page = Pages[Index];

        // The body's SWidgetSwitcher slots are added in this order — if the index stops agreeing with the position,
        // clicking a tab silently shows a different page.
        TestEqual(*ck::Format_UE(TEXT("Page {} indexes to its position"), Index),
            Get_PageIndex(Page), Index);

        const auto PageId = Get_PageId(Page);
        TestFalse(*ck::Format_UE(TEXT("Page {} has an id"), Index), PageId.IsNone());
        SeenIds.Add(PageId);

        const auto RoundTripped = TryGet_PageFromId(PageId);
        TestTrue(*ck::Format_UE(TEXT("Page {} round-trips through its id"), Index), RoundTripped.IsSet());

        if (RoundTripped.IsSet())
        {
            TestEqual(*ck::Format_UE(TEXT("Page {} round-trips to itself"), Index),
                static_cast<int32>(RoundTripped.GetValue()), static_cast<int32>(Page));
        }

        TestFalse(*ck::Format_UE(TEXT("Page {} has a label"), Index), Get_PageLabel(Page).IsEmpty());
    }

    TestEqual(TEXT("Every page id is distinct"), SeenIds.Num(), k_PageCount);
    TestFalse(TEXT("An unknown page id resolves to nothing"), TryGet_PageFromId(FName{TEXT("NotAPage")}).IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Model_Grouping,
    "Ck.OptimizationDebugger.Model.Grouping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Model_Grouping::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_model_spec;
    using namespace ck_optimization_debugger_model;

    auto Model = FCkOptimizationDebugger_Model{};

    TestEqual(TEXT("An unscanned model groups nothing"), Model.Get_VisibleFindingsGroupedByCheck().Num(), 0);
    TestFalse(TEXT("...and names no worst severity"), Model.TryGet_WorstVisibleSeverity().IsSet());

    Model.Set_Findings(
    {
        MakeFinding(TEXT("Mesh.TriangleBudget"), ECkOptimizationDebugger_Severity::Major,
            ECkOptimizationDebugger_Category::Mesh, TEXT("Triangle count over budget"), TEXT("SM_Alpha")),
        MakeFinding(TEXT("Mesh.TriangleBudget"), ECkOptimizationDebugger_Severity::Critical,
            ECkOptimizationDebugger_Category::Mesh, TEXT("Triangle count over budget"), TEXT("SM_Bravo")),
        MakeFinding(TEXT("Texture.MaxSize"), ECkOptimizationDebugger_Severity::Minor,
            ECkOptimizationDebugger_Category::Texture, TEXT("Texture larger than the budget"), TEXT("T_Gravel")),
    });

    const auto Groups = Model.Get_VisibleFindingsGroupedByCheck();

    TestEqual(TEXT("One group per check id"), Groups.Num(), 2);
    TestEqual(TEXT("The group holding the worst finding comes first"),
        Groups[0].CheckId, FName{TEXT("Mesh.TriangleBudget")});
    TestEqual(TEXT("A group reports its WORST member's severity, not its first member's"),
        static_cast<int32>(Groups[0].WorstSeverity),
        static_cast<int32>(ECkOptimizationDebugger_Severity::Critical));
    TestEqual(TEXT("Both members of the check land in one group"), Groups[0].Findings.Num(), 2);
    TestEqual(TEXT("Members keep the sorted order — most severe first"),
        Groups[0].Findings[0].Target.DisplayName, FString{TEXT("SM_Bravo")});
    TestFalse(TEXT("A group is named"), Groups[0].Title.IsEmpty());
    TestEqual(TEXT("The single-member group follows"), Groups[1].Findings.Num(), 1);

    // Every finding survives grouping — a projection that dropped one would under-report the level.
    auto Total = 0;
    for (const auto& Group : Groups)
    { Total += Group.Findings.Num(); }
    TestEqual(TEXT("Grouping loses no finding"), Total, 3);

    TestEqual(TEXT("The worst VISIBLE severity is the worst finding's"),
        static_cast<int32>(Model.TryGet_WorstVisibleSeverity().Get(ECkOptimizationDebugger_Severity::Minor)),
        static_cast<int32>(ECkOptimizationDebugger_Severity::Critical));

    // The projection follows the filter: it is what the findings PAGE renders, not what the scan produced.
    Model.Set_CategoryVisible(ECkOptimizationDebugger_Category::Mesh, false);

    const auto FilteredGroups = Model.Get_VisibleFindingsGroupedByCheck();
    TestEqual(TEXT("Filtering out a category drops its whole group"), FilteredGroups.Num(), 1);
    TestEqual(TEXT("...and leaves the other one intact"), FilteredGroups[0].CheckId, FName{TEXT("Texture.MaxSize")});
    TestEqual(TEXT("The worst visible severity follows the filter too"),
        static_cast<int32>(Model.TryGet_WorstVisibleSeverity().Get(ECkOptimizationDebugger_Severity::Critical)),
        static_cast<int32>(ECkOptimizationDebugger_Severity::Minor));

    Model.Set_CategoryVisible(ECkOptimizationDebugger_Category::Mesh, true);
    Model.Set_SeverityVisible(ECkOptimizationDebugger_Severity::Critical, false);
    Model.Set_SeverityVisible(ECkOptimizationDebugger_Severity::Major, false);
    Model.Set_SeverityVisible(ECkOptimizationDebugger_Severity::Minor, false);

    TestFalse(TEXT("A filter that admits nothing names no worst severity"),
        Model.TryGet_WorstVisibleSeverity().IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Model_ScanInfo,
    "Ck.OptimizationDebugger.Model.ScanInfo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Model_ScanInfo::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_model_spec;
    using namespace ck_optimization_debugger_model;

    const auto Finding = MakeFinding(TEXT("Mesh.TriangleBudget"), ECkOptimizationDebugger_Severity::Major,
        ECkOptimizationDebugger_Category::Mesh, TEXT("Triangle count over budget"), TEXT("SM_Alpha"));

    auto Model = FCkOptimizationDebugger_Model{};

    // --- highlight is a separate question from the filter ---
    TestTrue(TEXT("An empty highlight matches everything"), Matches_Highlight(Finding, Model.Get_Filter()));

    Model.Get_Filter().HighlightString = TEXT("ALPHA");
    TestTrue(TEXT("Highlight matches case-insensitively across the same haystack the filter uses"),
        Matches_Highlight(Finding, Model.Get_Filter()));

    Model.Get_Filter().HighlightString = TEXT("nothing-matches-this");
    TestFalse(TEXT("A highlight nothing matches marks the finding as dimmed"),
        Matches_Highlight(Finding, Model.Get_Filter()));
    TestTrue(TEXT("...and it is still admitted by the filter — highlight dims, it never hides"),
        Matches_Filter(Finding, Model.Get_Filter()));

    // --- scan info is display state, handed in rather than read off the clock ---
    const auto ScanTime = FDateTime{2026, 8, 11, 13, 45, 0};

    Model.Set_Findings({Finding});
    Model.Set_ScanInfo({TEXT("L_Persistent"), TEXT("L_Sub")}, ScanTime);

    TestEqual(TEXT("The scanned levels are kept in the order the scan walked them"),
        Model.Get_ScannedLevelNames().Num(), 2);
    TestEqual(TEXT("The persistent level is first"),
        Model.Get_ScannedLevelNames()[0], FString{TEXT("L_Persistent")});
    TestTrue(TEXT("The scan time is the one handed in, not the wall clock"),
        Model.Get_LastScanTime() == ScanTime);

    Model.Reset();
    TestEqual(TEXT("Reset drops the scanned level list"), Model.Get_ScannedLevelNames().Num(), 0);

    // Filter state deliberately SURVIVES a reset — a re-scan is the same question asked again.
    TestEqual(TEXT("Reset keeps the user's highlight"),
        Model.Get_Filter().HighlightString, FString{TEXT("nothing-matches-this")});

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
