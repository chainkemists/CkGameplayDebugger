#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_FindingsReport.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity.
namespace ck_optimization_debugger_findings_report_impl
{
    /** HTML-escaped. Asset names carry ampersands and angle brackets more often than anyone expects, and a report
     *  that broke its own markup on one asset would be a report nobody trusts the rest of. */
    auto
        Escape_Html(
            const FString& InText)
        -> FString
    {
        auto Escaped = InText;

        // Ampersand FIRST, or the escapes below get double-escaped by it.
        Escaped.ReplaceInline(TEXT("&"), TEXT("&amp;"));
        Escaped.ReplaceInline(TEXT("<"), TEXT("&lt;"));
        Escaped.ReplaceInline(TEXT(">"), TEXT("&gt;"));
        Escaped.ReplaceInline(TEXT("\""), TEXT("&quot;"));

        return Escaped;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Markdown-escaped for a TABLE CELL. The pipe is the only character that can break the table structure; the
     *  rest of Markdown's punctuation degrades to odd emphasis at worst, and escaping it all would make the source
     *  unreadable for the case nobody hits. */
    auto
        Escape_MarkdownCell(
            const FString& InText)
        -> FString
    {
        auto Escaped = InText;

        Escaped.ReplaceInline(TEXT("|"), TEXT("\\|"));
        Escaped.ReplaceInline(TEXT("\r"), TEXT(" "));
        Escaped.ReplaceInline(TEXT("\n"), TEXT(" "));

        return Escaped;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SeverityWord(
            ECkOptimizationDebugger_Severity InSeverity)
        -> FString
    {
        switch (InSeverity)
        {
            case ECkOptimizationDebugger_Severity::Critical: return FString{TEXT("Critical")};
            case ECkOptimizationDebugger_Severity::Major:    return FString{TEXT("Major")};
            case ECkOptimizationDebugger_Severity::Minor:    return FString{TEXT("Minor")};
            default: break;
        }

        return FString{TEXT("Minor")};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_TargetText(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FString
    {
        const auto Path = InFinding.Target.Path.ToString();

        // The PATH where there is one: a display name alone is ambiguous across folders, and the reader of an
        // exported report cannot click a row to find out which one was meant.
        if (NOT Path.IsEmpty())
        { return Path; }

        if (NOT InFinding.Target.SettingsSectionName.IsEmpty())
        { return ck::Format_UE(TEXT("Project Settings → {}"), InFinding.Target.SettingsSectionName); }

        return InFinding.Target.DisplayName;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_CountsLine(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings)
        -> FString
    {
        auto Critical = 0;
        auto Major = 0;
        auto Minor = 0;

        for (const auto& Finding : InFindings)
        {
            switch (Finding.Severity)
            {
                case ECkOptimizationDebugger_Severity::Critical: ++Critical; break;
                case ECkOptimizationDebugger_Severity::Major:    ++Major;    break;
                default:                                        ++Minor;    break;
            }
        }

        return ck::Format_UE(TEXT("{} finding(s): {} critical, {} major, {} minor"),
            InFindings.Num(), Critical, Major, Minor);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The scope sentences, in one place, so the HTML and the Markdown cannot describe two different scans. */
    auto
        Build_ScopeLines(
            const FCkOptimizationDebugger_ReportContext& InContext)
        -> TArray<FString>
    {
        auto Lines = TArray<FString>{};

        Lines.Add(InContext.ScannedLevelNames.IsEmpty()
            ? FString{TEXT("Levels scanned: none")}
            : ck::Format_UE(TEXT("Levels scanned: {}"), FString::Join(InContext.ScannedLevelNames, TEXT(", "))));

        // "No project scan has run" and "a project scan found nothing" are different statements, and only the
        // second one means the project is clean.
        Lines.Add(InContext.ProjectAssetCount > 0
            ? ck::Format_UE(TEXT("Project assets read from the registry: {} ({} opened for the checks the registry cannot answer)"),
                InContext.ProjectAssetCount, InContext.ProjectDeepLoadedCount)
            : FString{TEXT("Project scan: not run, so assets no open level places are NOT covered by this report")});

        if (InContext.SuppressedCount > 0)
        {
            Lines.Add(ck::Format_UE(
                TEXT("{} finding(s) suppressed and therefore NOT listed below — see the project's suppression config"),
                InContext.SuppressedCount));
        }

        return Lines;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_findings_report
{
    using namespace ck_optimization_debugger_findings_report_impl;

    auto
        Get_ReportOrder(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings)
        -> TArray<FCkOptimizationDebugger_FindingRow>
    {
        auto Ordered = InFindings;

        // Worst first, then furthest over budget, then the check id, then the stable key. The last one is a TOTAL
        // order — without it `TArray::Sort` (unstable) could emit two equal rows in either order, and two exports
        // of one scan would differ for no reason a reader could see.
        Ordered.Sort([](const FCkOptimizationDebugger_FindingRow& InLhs,
                        const FCkOptimizationDebugger_FindingRow& InRhs) -> bool
        {
            // The enum is declared WORST FIRST (`Critical, Major, Minor`), so "most severe first" is ascending by
            // value, not descending. Getting this backwards put the least severe finding at the top of the report.
            if (InLhs.Severity != InRhs.Severity)
            { return static_cast<uint8>(InLhs.Severity) < static_cast<uint8>(InRhs.Severity); }

            if (NOT FMath::IsNearlyEqual(InLhs.BudgetRatio, InRhs.BudgetRatio))
            { return InLhs.BudgetRatio > InRhs.BudgetRatio; }

            const auto CheckCompare = InLhs.CheckId.Compare(InRhs.CheckId);

            if (CheckCompare != 0)
            { return CheckCompare < 0; }

            return InLhs.StableKey.Compare(InRhs.StableKey, ESearchCase::CaseSensitive) < 0;
        });

        return Ordered;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_FindingsReportHtml(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings,
            const FCkOptimizationDebugger_ReportContext& InContext,
            const FDateTime& InGeneratedAt)
        -> FString
    {
        const auto Ordered = Get_ReportOrder(InFindings);

        auto Lines = TArray<FString>{};

        Lines.Add(TEXT("<!DOCTYPE html>"));
        Lines.Add(TEXT("<html lang=\"en\"><head><meta charset=\"utf-8\">"));
        Lines.Add(ck::Format_UE(TEXT("<title>Optimization findings — {}</title>"),
            Escape_Html(InContext.ProjectName)));

        // Inline, because a report that needed a stylesheet beside it is a report that renders as plain text the
        // moment somebody attaches only the file.
        Lines.Add(TEXT("<style>"));
        Lines.Add(TEXT("body{font-family:Segoe UI,Roboto,Helvetica,Arial,sans-serif;margin:24px;color:#1c1c1c;background:#fff}"));
        Lines.Add(TEXT("h1{font-size:20px;margin:0 0 4px}"));
        Lines.Add(TEXT("p.meta{color:#555;margin:2px 0;font-size:13px}"));
        Lines.Add(TEXT("table{border-collapse:collapse;width:100%;margin-top:16px;font-size:13px}"));
        Lines.Add(TEXT("th,td{border:1px solid #ddd;padding:6px 8px;text-align:left;vertical-align:top}"));
        Lines.Add(TEXT("th{background:#f4f4f4}"));
        Lines.Add(TEXT("td.sev-Critical{color:#a01010;font-weight:600}"));
        Lines.Add(TEXT("td.sev-Major{color:#a06000;font-weight:600}"));
        Lines.Add(TEXT("td.sev-Minor{color:#555}"));
        Lines.Add(TEXT("code{font-family:Consolas,monospace;font-size:12px}"));
        Lines.Add(TEXT("</style></head><body>"));

        Lines.Add(ck::Format_UE(TEXT("<h1>Optimization findings — {}</h1>"),
            Escape_Html(InContext.ProjectName)));
        Lines.Add(ck::Format_UE(TEXT("<p class=\"meta\">Generated {}</p>"),
            Escape_Html(InGeneratedAt.ToString())));
        Lines.Add(ck::Format_UE(TEXT("<p class=\"meta\">{}</p>"), Escape_Html(Build_CountsLine(Ordered))));

        for (const auto& ScopeLine : Build_ScopeLines(InContext))
        { Lines.Add(ck::Format_UE(TEXT("<p class=\"meta\">{}</p>"), Escape_Html(ScopeLine))); }

        Lines.Add(TEXT("<table><thead><tr><th>Severity</th><th>Check</th><th>Target</th><th>What it costs</th><th>What to do</th></tr></thead><tbody>"));

        for (const auto& Finding : Ordered)
        {
            const auto SeverityWord = Get_SeverityWord(Finding.Severity);

            Lines.Add(ck::Format_UE(
                TEXT("<tr><td class=\"sev-{}\">{}</td><td><code>{}</code></td><td><code>{}</code></td><td>{}</td><td>{}</td></tr>"),
                SeverityWord,
                SeverityWord,
                Escape_Html(Finding.CheckId.ToString()),
                Escape_Html(Get_TargetText(Finding)),
                Escape_Html(Finding.Explanation),
                Escape_Html(Finding.Recommendation)));
        }

        Lines.Add(TEXT("</tbody></table></body></html>"));

        return FString::Join(Lines, TEXT("\n"));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_FindingsReportMarkdown(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings,
            const FCkOptimizationDebugger_ReportContext& InContext,
            const FDateTime& InGeneratedAt)
        -> FString
    {
        const auto Ordered = Get_ReportOrder(InFindings);

        auto Lines = TArray<FString>{};

        Lines.Add(ck::Format_UE(TEXT("# Optimization findings — {}"), InContext.ProjectName));
        Lines.Add(FString{});
        Lines.Add(ck::Format_UE(TEXT("Generated {}"), InGeneratedAt.ToString()));
        Lines.Add(FString{});
        Lines.Add(Build_CountsLine(Ordered));
        Lines.Add(FString{});

        for (const auto& ScopeLine : Build_ScopeLines(InContext))
        { Lines.Add(ck::Format_UE(TEXT("- {}"), ScopeLine)); }

        Lines.Add(FString{});
        Lines.Add(TEXT("| Severity | Check | Target | What it costs | What to do |"));
        Lines.Add(TEXT("|---|---|---|---|---|"));

        for (const auto& Finding : Ordered)
        {
            Lines.Add(ck::Format_UE(TEXT("| {} | `{}` | `{}` | {} | {} |"),
                Get_SeverityWord(Finding.Severity),
                Escape_MarkdownCell(Finding.CheckId.ToString()),
                Escape_MarkdownCell(Get_TargetText(Finding)),
                Escape_MarkdownCell(Finding.Explanation),
                Escape_MarkdownCell(Finding.Recommendation)));
        }

        return FString::Join(Lines, TEXT("\n"));
    }
}

// --------------------------------------------------------------------------------------------------------------------
