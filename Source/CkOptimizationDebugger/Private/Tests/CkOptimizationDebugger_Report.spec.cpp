#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_ScanContext.h"
#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_FindingsReport.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and the spec files sit in the same merged translation
// unit as each other.
namespace ck_optimization_debugger_report_spec
{
    auto
        Build_Finding(
            const FString& InCheckId,
            ECkOptimizationDebugger_Severity InSeverity,
            const FString& InAssetPath,
            float InBudgetRatio)
        -> FCkOptimizationDebugger_FindingRow
    {
        auto Finding = ck_optimization_debugger_scan::Build_Finding(FName{*InCheckId},
            InSeverity,
            ECkOptimizationDebugger_Category::Mesh,
            ck_optimization_debugger_scan::Build_AssetTarget(FSoftObjectPath{InAssetPath}, TEXT("Spec")),
            TEXT("Spec finding"),
            TEXT("It costs something."),
            TEXT("Do the thing."));

        Finding.BudgetRatio = InBudgetRatio;

        return Finding;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_Context()
        -> FCkOptimizationDebugger_ReportContext
    {
        auto Context = FCkOptimizationDebugger_ReportContext{};
        Context.ProjectName = TEXT("SpecProject");
        Context.ScannedLevelNames = {TEXT("Gym_A"), TEXT("Gym_B")};
        Context.ProjectAssetCount = 120;
        Context.ProjectDeepLoadedCount = 30;
        Context.SuppressedCount = 4;

        return Context;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Report_Determinism,
    "Ck.OptimizationDebugger.Report.Determinism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Report_Determinism::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_findings_report;
    using namespace ck_optimization_debugger_report_spec;

    const auto Context = Build_Context();
    const auto GeneratedAt = FDateTime{2026, 8, 21, 9, 30, 0};

    const auto Findings = TArray<FCkOptimizationDebugger_FindingRow>{
        Build_Finding(TEXT("Mesh.TriangleBudget"), ECkOptimizationDebugger_Severity::Major, TEXT("/Game/A/SM_A.SM_A"), 2.5f),
        Build_Finding(TEXT("Texture.MaxSize"), ECkOptimizationDebugger_Severity::Critical, TEXT("/Game/B/T_B.T_B"), 4.0f),
        Build_Finding(TEXT("Mesh.NaniteCandidate"), ECkOptimizationDebugger_Severity::Minor, TEXT("/Game/C/SM_C.SM_C"), 1.2f)};

    // ---- Same input, same bytes ----
    // The whole reason the timestamp is a parameter: with the clock read inside, two exports of one scan would
    // differ and neither could be diffed against the other.
    TestEqual(TEXT("Two HTML exports of one model are byte-identical"),
        Build_FindingsReportHtml(Findings, Context, GeneratedAt),
        Build_FindingsReportHtml(Findings, Context, GeneratedAt));

    TestEqual(TEXT("Two Markdown exports of one model are byte-identical"),
        Build_FindingsReportMarkdown(Findings, Context, GeneratedAt),
        Build_FindingsReportMarkdown(Findings, Context, GeneratedAt));

    // ---- The INCOMING order must not survive into the document ----
    auto Shuffled = TArray<FCkOptimizationDebugger_FindingRow>{Findings[2], Findings[0], Findings[1]};

    TestEqual(TEXT("A re-ordered input produces the same document"),
        Build_FindingsReportHtml(Shuffled, Context, GeneratedAt),
        Build_FindingsReportHtml(Findings, Context, GeneratedAt));

    // ---- Worst first ----
    const auto Ordered = Get_ReportOrder(Shuffled);

    TestEqual(TEXT("The most severe finding is first"),
        Ordered[0].CheckId, FName{TEXT("Texture.MaxSize")});
    TestEqual(TEXT("...then the next"), Ordered[1].CheckId, FName{TEXT("Mesh.TriangleBudget")});
    TestEqual(TEXT("...then the least"), Ordered[2].CheckId, FName{TEXT("Mesh.NaniteCandidate")});

    // Two findings equal on severity AND ratio are ordered by check id, then by stable key — a TOTAL order, so an
    // unstable sort cannot emit them in either order between two runs.
    const auto Tied = TArray<FCkOptimizationDebugger_FindingRow>{
        Build_Finding(TEXT("Mesh.Zulu"), ECkOptimizationDebugger_Severity::Major, TEXT("/Game/A/SM_A.SM_A"), 2.0f),
        Build_Finding(TEXT("Mesh.Alpha"), ECkOptimizationDebugger_Severity::Major, TEXT("/Game/A/SM_A.SM_A"), 2.0f)};

    const auto TiedOrder = Get_ReportOrder(Tied);

    TestEqual(TEXT("A tie breaks on the check id"), TiedOrder[0].CheckId, FName{TEXT("Mesh.Alpha")});

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkOptimizationDebugger_Report_Content,
    "Ck.OptimizationDebugger.Report.Content",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkOptimizationDebugger_Report_Content::RunTest(const FString& Parameters)
{
    using namespace ck_optimization_debugger_findings_report;
    using namespace ck_optimization_debugger_report_spec;

    const auto GeneratedAt = FDateTime{2026, 8, 21, 9, 30, 0};

    auto Context = Build_Context();

    const auto Findings = TArray<FCkOptimizationDebugger_FindingRow>{
        Build_Finding(TEXT("Mesh.TriangleBudget"), ECkOptimizationDebugger_Severity::Major, TEXT("/Game/A/SM_A.SM_A"), 2.5f)};

    const auto Html = Build_FindingsReportHtml(Findings, Context, GeneratedAt);

    TestTrue(TEXT("The HTML is self-contained — no external stylesheet"), NOT Html.Contains(TEXT("<link")));
    TestTrue(TEXT("...and carries no script"), NOT Html.Contains(TEXT("<script")));
    TestTrue(TEXT("It names the project"), Html.Contains(TEXT("SpecProject")));
    TestTrue(TEXT("It counts the findings"), Html.Contains(TEXT("1 finding(s)")));
    TestTrue(TEXT("It names the levels the scan covered"), Html.Contains(TEXT("Gym_A, Gym_B")));
    TestTrue(TEXT("It says the target path, not just a display name"), Html.Contains(TEXT("/Game/A/SM_A")));

    // A report that silently omitted suppressed findings would be one a teammate cannot reconcile with their own.
    TestTrue(TEXT("It says how many findings were suppressed"), Html.Contains(TEXT("4 finding(s) suppressed")));

    // "No project scan has run" and "the project scan found nothing" are different statements, and only the second
    // means the project is clean.
    Context.ProjectAssetCount = 0;

    TestTrue(TEXT("With no project scan, the report says its scope is levels only"),
        Build_FindingsReportHtml(Findings, Context, GeneratedAt).Contains(TEXT("not run")));

    // ---- Escaping ----
    auto Awkward = Build_Finding(TEXT("Mesh.TriangleBudget"), ECkOptimizationDebugger_Severity::Major,
        TEXT("/Game/A/SM_A.SM_A"), 1.0f);
    Awkward.Explanation = TEXT("Costs <b>a lot</b> & then some | really");

    const auto AwkwardHtml = Build_FindingsReportHtml({Awkward}, Context, GeneratedAt);

    TestTrue(TEXT("HTML markup in a finding is escaped"), AwkwardHtml.Contains(TEXT("&lt;b&gt;")));
    TestTrue(TEXT("...ampersands too"), AwkwardHtml.Contains(TEXT("&amp; then some")));
    TestFalse(TEXT("...and the raw tag never reaches the document"), AwkwardHtml.Contains(TEXT("<b>a lot</b>")));

    // A pipe is the one character that can break a Markdown table's structure.
    const auto AwkwardMarkdown = Build_FindingsReportMarkdown({Awkward}, Context, GeneratedAt);

    TestTrue(TEXT("A pipe in a finding is escaped for the Markdown table"),
        AwkwardMarkdown.Contains(TEXT("\\|")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
