#include "CkPerfLab_Export.h"

#include "CkPerfLab/Session/CkPerfLab_SessionCodec.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include <Dom/JsonObject.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity.
namespace ck_perf_lab_export
{
    /**
     * Every number in every export goes through here.
     *
     * fmt formats without consulting the locale unless asked to, which is the property the
     * determinism spec depends on: `FString::SanitizeFloat` and `%f` both hand back a comma for the
     * decimal separator under a European locale, and an export that changed shape with the machine's
     * regional settings would fail the byte-identical contract on somebody else's desk rather than
     * on the author's.
     */
    auto
        Format_Ms(
            float InValue)
        -> FString
    {
        return ck::Format_UE(TEXT("{:.3f}"), InValue);
    }

    auto
        Format_Score(
            float InValue)
        -> FString
    {
        return ck::Format_UE(TEXT("{:.1f}"), InValue);
    }

    /** `WxH`, so the viewport reads the way a person writes it rather than as a struct dump. */
    auto
        Format_ViewportSize(
            const FIntPoint& InSize)
        -> FString
    {
        return ck::Format_UE(TEXT("{}x{}"), InSize.X, InSize.Y);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Escape_Html(
            const FString& InText)
        -> FString
    {
        auto Escaped = InText;

        // Ampersand FIRST, or the escapes below get double-escaped by it.
        Escaped.ReplaceInline(TEXT("&"),  TEXT("&amp;"));
        Escaped.ReplaceInline(TEXT("<"),  TEXT("&lt;"));
        Escaped.ReplaceInline(TEXT(">"),  TEXT("&gt;"));
        Escaped.ReplaceInline(TEXT("\""), TEXT("&quot;"));

        return Escaped;
    }

    /** RFC 4180: always quote, and double any embedded quote. Object paths carry commas often enough. */
    auto
        Escape_Csv(
            const FString& InText)
        -> FString
    {
        auto Escaped = InText;

        Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
        Escaped.ReplaceInline(TEXT("\r\n"), TEXT(" "));
        Escaped.ReplaceInline(TEXT("\r"), TEXT(" "));
        Escaped.ReplaceInline(TEXT("\n"), TEXT(" "));

        return ck::Format_UE(TEXT("\"{}\""), Escaped);
    }

    auto
        Build_CsvRow(
            const TArray<FString>& InCells)
        -> FString
    {
        return FString::Join(
            ck::algo::Transform<TArray<FString>>(InCells, [](const FString& InCell)
            {
                return Escape_Csv(InCell);
            }),
            TEXT(","));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MetricWord(
            ECk_PerfLab_Metric InMetric)
        -> FString
    {
        switch (InMetric)
        {
            case ECk_PerfLab_Metric::Frame:        return FString{TEXT("Frame")};
            case ECk_PerfLab_Metric::GameThread:   return FString{TEXT("GameThread")};
            case ECk_PerfLab_Metric::RenderThread: return FString{TEXT("RenderThread")};
            case ECk_PerfLab_Metric::RhiThread:    return FString{TEXT("RhiThread")};
            case ECk_PerfLab_Metric::Gpu:          return FString{TEXT("Gpu")};
        }

        return FString{TEXT("Frame")};
    }

    auto
        Get_SeverityWord(
            ECk_PerfLab_Severity InSeverity)
        -> FString
    {
        switch (InSeverity)
        {
            case ECk_PerfLab_Severity::Critical: return FString{TEXT("Critical")};
            case ECk_PerfLab_Severity::Major:    return FString{TEXT("Major")};
            case ECk_PerfLab_Severity::Minor:    return FString{TEXT("Minor")};
        }

        return FString{TEXT("Minor")};
    }

    auto
        Get_BandWord(
            ECk_PerfLab_Band InBand)
        -> FString
    {
        switch (InBand)
        {
            case ECk_PerfLab_Band::High:   return FString{TEXT("High")};
            case ECk_PerfLab_Band::Medium: return FString{TEXT("Medium")};
            case ECk_PerfLab_Band::Low:    return FString{TEXT("Low")};
        }

        return FString{TEXT("Low")};
    }

    auto
        Get_ComponentWord(
            ECk_PerfLab_ScoreComponent InComponent)
        -> FString
    {
        switch (InComponent)
        {
            case ECk_PerfLab_ScoreComponent::AverageFrameAttainment:  return FString{TEXT("AverageFrameAttainment")};
            case ECk_PerfLab_ScoreComponent::WorstCaseAttainment:     return FString{TEXT("WorstCaseAttainment")};
            case ECk_PerfLab_ScoreComponent::OnePercentLowAttainment: return FString{TEXT("OnePercentLowAttainment")};
            case ECk_PerfLab_ScoreComponent::GameThreadHeadroom:      return FString{TEXT("GameThreadHeadroom")};
            case ECk_PerfLab_ScoreComponent::RenderThreadHeadroom:    return FString{TEXT("RenderThreadHeadroom")};
            case ECk_PerfLab_ScoreComponent::GpuHeadroom:             return FString{TEXT("GpuHeadroom")};
            case ECk_PerfLab_ScoreComponent::SpatialConsistency:      return FString{TEXT("SpatialConsistency")};
            case ECk_PerfLab_ScoreComponent::MeasurementConfidence:   return FString{TEXT("MeasurementConfidence")};
        }

        return FString{TEXT("Unknown")};
    }

    /**
     * The JSON vocabulary, which is camelCase and lowercase-initial throughout — SCHEMA.md §3.2
     * declares these as CLOSED sets, so they are spelled here rather than derived from a display
     * name that a rename or a localisation pass could quietly change underneath a consumer.
     */
    auto
        Get_JsonKey(
            const FString& InWord)
        -> FString
    {
        if (InWord.IsEmpty())
        { return InWord; }

        return InWord.Left(1).ToLower() + InWord.RightChop(1);
    }

    auto
        Get_ConfidenceWord(
            ECk_PerfLab_Confidence InConfidence)
        -> FString
    {
        switch (InConfidence)
        {
            case ECk_PerfLab_Confidence::High:   return FString{TEXT("High")};
            case ECk_PerfLab_Confidence::Medium: return FString{TEXT("Medium")};
            case ECk_PerfLab_Confidence::Low:    return FString{TEXT("Low")};
        }

        return FString{TEXT("Low")};
    }

    auto
        Get_VerdictWord(
            ECk_PerfLab_CompareVerdict InVerdict)
        -> FString
    {
        switch (InVerdict)
        {
            case ECk_PerfLab_CompareVerdict::Regressed:    return FString{TEXT("Regressed")};
            case ECk_PerfLab_CompareVerdict::Improved:     return FString{TEXT("Improved")};
            case ECk_PerfLab_CompareVerdict::Unchanged:    return FString{TEXT("Unchanged")};
            case ECk_PerfLab_CompareVerdict::Incomparable: return FString{TEXT("Incomparable")};
        }

        return FString{TEXT("Incomparable")};
    }

    /** An unavailable metric prints why rather than printing a zero somebody would read as fast. */
    auto
        Format_Metric(
            const FCk_PerfLab_MetricStats& InStats)
        -> FString
    {
        if (InStats.Get_Availability() != ECk_Stats_MetricAvailability::Available)
        { return FString{TEXT("not measured")}; }

        return Format_Ms(InStats.Get_AvgMs());
    }

    /**
     * Any OTHER figure from a metric — worst, p99, the 1% low.
     *
     * Separate from `Format_Metric` only because it takes the value explicitly, and gated by the
     * same availability check for the same reason: an unmeasured p99 printed as `0.000` sorts to
     * the top of a spreadsheet as the fastest position in the level, which is precisely the
     * inversion the HTML's own position order goes out of its way to avoid.
     */
    auto
        Format_MetricValue(
            const FCk_PerfLab_MetricStats& InStats,
            float InValue)
        -> FString
    {
        if (InStats.Get_Availability() != ECk_Stats_MetricAvailability::Available)
        { return FString{TEXT("not measured")}; }

        return Format_Ms(InValue);
    }

    /** A world-space distance. Kept double all the way to the string: at large-world coordinates a
     *  float32 cannot represent centimetres, and this is the column a reader uses to find the spot. */
    auto
        Format_Cm(
            double InValue)
        -> FString
    {
        return ck::Format_UE(TEXT("{:.3f}"), InValue);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Positions in report order: worst average frame time first, then position id as the final tie-break. */
    auto
        Get_PositionOrder(
            const FCk_PerfLab_Session& InSession)
        -> TArray<FCk_PerfLab_PositionResult>
    {
        return ck::algo::Sort(InSession.Get_Positions(),
            [](const FCk_PerfLab_PositionResult& InA, const FCk_PerfLab_PositionResult& InB)
            {
                const auto& FrameA = InA.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::Frame);
                const auto& FrameB = InB.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::Frame);

                // An unmeasured position sorts last rather than first: a zero would otherwise put the
                // positions carrying no information at the top of every report.
                const auto UsableA = FrameA.Get_Availability() == ECk_Stats_MetricAvailability::Available;
                const auto UsableB = FrameB.Get_Availability() == ECk_Stats_MetricAvailability::Available;

                if (UsableA != UsableB)
                { return UsableA; }

                // EXACT comparison, not IsNearlyEqual. "Nearly equal" is not transitive — a≈b and
                // b≈c does not give a≈c — so a comparator built on it is not a strict weak ordering
                // and the sort is free to misbehave on a set of closely spaced timings, which is
                // exactly what a set of frame times is. Genuinely equal values fall to the id
                // tie-break below, which is deterministic anyway.
                if (UsableA && FrameA.Get_AvgMs() != FrameB.Get_AvgMs())
                { return FrameA.Get_AvgMs() > FrameB.Get_AvgMs(); }

                return InA.Get_PositionId() < InB.Get_PositionId();
            });
    }

    /** Findings in report order: worst severity first, then check id, then stable key as the final tie-break. */
    auto
        Get_FindingOrder(
            const FCk_PerfLab_Analysis& InAnalysis)
        -> TArray<FCk_PerfLab_Finding>
    {
        return ck::algo::Sort(InAnalysis.Get_Findings(),
            [](const FCk_PerfLab_Finding& InA, const FCk_PerfLab_Finding& InB)
            {
                if (InA.Get_Severity() != InB.Get_Severity())
                { return InA.Get_Severity() < InB.Get_Severity(); }

                if (InA.Get_CheckId() != InB.Get_CheckId())
                { return InA.Get_CheckId() < InB.Get_CheckId(); }

                return InA.Get_StableKey() < InB.Get_StableKey();
            });
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_StyleBlock()
        -> TArray<FString>
    {
        // Inline, because a report that needed a stylesheet beside it is a report that renders as
        // plain text the moment somebody attaches only the file.
        return TArray<FString>
        {
            TEXT("<style>"),
            TEXT("body{font-family:Segoe UI,Roboto,Helvetica,Arial,sans-serif;margin:24px;color:#1c1c1c;background:#fff}"),
            TEXT("h1{font-size:20px;margin:0 0 4px}"),
            TEXT("h2{font-size:15px;margin:22px 0 6px;border-bottom:1px solid #e4e4e4;padding-bottom:3px}"),
            TEXT("p.meta{color:#555;margin:2px 0;font-size:13px}"),
            TEXT("p.limit{color:#404040;background:#f7f4ec;border-left:3px solid #c9a227;padding:8px 10px;font-size:12px;margin:14px 0}"),
            TEXT("table{border-collapse:collapse;width:100%;margin-top:10px;font-size:13px}"),
            TEXT("th,td{border:1px solid #ddd;padding:6px 8px;text-align:left;vertical-align:top}"),
            TEXT("th{background:#f4f4f4}"),
            TEXT("td.num{text-align:right;font-variant-numeric:tabular-nums}"),
            TEXT("td.sev-Critical{color:#a01010;font-weight:600}"),
            TEXT("td.sev-Major{color:#a06000;font-weight:600}"),
            TEXT("td.sev-Minor{color:#555}"),
            TEXT("td.v-Regressed{color:#a01010;font-weight:600}"),
            TEXT("td.v-Improved{color:#106010;font-weight:600}"),
            TEXT("td.v-Unchanged{color:#555}"),
            TEXT("td.v-Incomparable{color:#888;font-style:italic}"),
            TEXT("span.score{font-size:34px;font-weight:700}"),
            TEXT("code{font-family:Consolas,monospace;font-size:12px}"),
            TEXT("</style></head><body>"),
        };
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab::exporter
{
    const TCHAR* const k_LimitationParagraph =
        TEXT("This report measures where the camera stood, not what the level costs everywhere. "
             "Positions were chosen automatically and the camera was stationary at each one, so "
             "nothing here accounts for movement, streaming under travel, gameplay load, or any "
             "platform other than the one named above. Listed contributors are actors NEAR a "
             "measured cost — they are worth investigating, not proven to be the cause. Treat every "
             "number as a reading taken under these conditions rather than as the level's "
             "performance.");

    auto
        Get_AllCsvTables()
        -> TArray<ECk_CsvTable>
    {
        return TArray<ECk_CsvTable>
        {
            ECk_CsvTable::SessionInfo,
            ECk_CsvTable::Positions,
            ECk_CsvTable::Directions,
            ECk_CsvTable::Findings,
            ECk_CsvTable::Contributors,
            ECk_CsvTable::ScoreComponents,
        };
    }

    auto
        Get_CsvTableName(
            ECk_CsvTable InTable)
        -> FString
    {
        switch (InTable)
        {
            case ECk_CsvTable::SessionInfo:     return FString{TEXT("sessionInfo")};
            case ECk_CsvTable::Positions:       return FString{TEXT("positions")};
            case ECk_CsvTable::Directions:      return FString{TEXT("directions")};
            case ECk_CsvTable::Findings:        return FString{TEXT("findings")};
            case ECk_CsvTable::Contributors:    return FString{TEXT("contributors")};
            case ECk_CsvTable::ScoreComponents: return FString{TEXT("scoreComponents")};
        }

        return FString{TEXT("unknown")};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_SessionHtml(
            const FCk_PerfLab_Session& InSession,
            const FCk_PerfLab_Analysis& InAnalysis,
            const FDateTime& InGeneratedAt)
        -> FString
    {
        using namespace ck_perf_lab_export;

        const auto& Environment = InSession.Get_Environment();
        const auto& Request     = InSession.Get_Request();

        auto Lines = TArray<FString>{};

        Lines.Add(TEXT("<!DOCTYPE html>"));
        Lines.Add(TEXT("<html lang=\"en\"><head><meta charset=\"utf-8\">"));
        Lines.Add(ck::Format_UE(TEXT("<title>Level performance — {}</title>"),
            Escape_Html(Request.Get_MapPath())));

        Lines.Append(Build_StyleBlock());

        Lines.Add(ck::Format_UE(TEXT("<h1>Level performance — {}</h1>"),
            Escape_Html(Request.Get_MapPath())));
        Lines.Add(ck::Format_UE(TEXT("<p class=\"meta\">Session <code>{}</code> · generated {}</p>"),
            Escape_Html(InSession.Get_SessionId()), Escape_Html(InGeneratedAt.ToString())));

        // Score, then the components it is made of. A score nobody can decompose is a rating to be
        // trusted rather than a measurement to be read, so the decomposition is not optional.
        Lines.Add(TEXT("<h2>Score</h2>"));
        Lines.Add(ck::Format_UE(TEXT("<p><span class=\"score\">{}</span> / 100 &nbsp; <span class=\"meta\">against a {} ms budget</span></p>"),
            Format_Score(InAnalysis.Get_Score().Get_Value()), Format_Ms(InAnalysis.Get_BudgetMs())));
        Lines.Add(ck::Format_UE(TEXT("<p class=\"meta\">{}</p>"),
            Escape_Html(InAnalysis.Get_Score().Get_Formula())));

        Lines.Add(TEXT("<table><thead><tr><th>Component</th><th>Weight</th><th>Value</th><th>Included</th><th>Why not</th></tr></thead><tbody>"));

        for (const auto& Component : InAnalysis.Get_Score().Get_Components())
        {
            Lines.Add(ck::Format_UE(
                TEXT("<tr><td>{}</td><td class=\"num\">{}</td><td class=\"num\">{}</td><td>{}</td><td>{}</td></tr>"),
                Escape_Html(Get_ComponentWord(Component.Get_Component())),
                Format_Score(Component.Get_Weight()),
                Format_Score(Component.Get_Value()),
                Component.Get_Included() ? TEXT("yes") : TEXT("no"),
                Escape_Html(Component.Get_ExcludedReason())));
        }

        Lines.Add(TEXT("</tbody></table>"));

        // Environment before results, because every number below is a reading taken on this machine
        // under these settings and means nothing detached from them.
        Lines.Add(TEXT("<h2>Measured on</h2>"));
        Lines.Add(TEXT("<table><tbody>"));

        const auto AppendRow = [&Lines](const TCHAR* InLabel, const FString& InValue)
        {
            Lines.Add(ck::Format_UE(TEXT("<tr><th>{}</th><td>{}</td></tr>"),
                InLabel, Escape_Html(InValue)));
        };

        AppendRow(TEXT("Machine"),       Environment.Get_MachineName());
        AppendRow(TEXT("CPU"),           Environment.Get_CpuBrand());
        AppendRow(TEXT("GPU"),           Environment.Get_GpuBrand());
        AppendRow(TEXT("RHI"),           Environment.Get_RhiName());
        AppendRow(TEXT("Engine"),        Environment.Get_EngineVersion());
        AppendRow(TEXT("Configuration"), Environment.Get_BuildConfiguration());
        AppendRow(TEXT("Instance mode"), Environment.Get_InstanceMode());
        AppendRow(TEXT("Viewport"),      Format_ViewportSize(Environment.Get_ViewportSizeActual()));
        AppendRow(TEXT("Started"),       Environment.Get_StartedUtc());
        AppendRow(TEXT("Finished"),      Environment.Get_FinishedUtc());

        Lines.Add(TEXT("</tbody></table>"));

        Lines.Add(TEXT("<h2>Positions</h2>"));
        Lines.Add(TEXT("<table><thead><tr><th>Position</th><th>Frame</th><th>Game</th><th>Render</th><th>RHI</th><th>GPU</th><th>Confidence</th></tr></thead><tbody>"));

        for (const auto& Position : Get_PositionOrder(InSession))
        {
            Lines.Add(ck::Format_UE(
                TEXT("<tr><td><code>{}</code></td><td class=\"num\">{}</td><td class=\"num\">{}</td>")
                TEXT("<td class=\"num\">{}</td><td class=\"num\">{}</td><td class=\"num\">{}</td><td>{}</td></tr>"),
                Escape_Html(Position.Get_PositionId()),
                Format_Metric(Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::Frame)),
                Format_Metric(Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::GameThread)),
                Format_Metric(Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::RenderThread)),
                Format_Metric(Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::RhiThread)),
                Format_Metric(Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::Gpu)),
                Escape_Html(Get_ConfidenceWord(Position.Get_Confidence().Get_Rating()))));
        }

        Lines.Add(TEXT("</tbody></table>"));

        Lines.Add(TEXT("<h2>Findings</h2>"));

        const auto Findings = Get_FindingOrder(InAnalysis);

        if (Findings.IsEmpty())
        {
            // "Nothing to report" and "nobody looked" are different results and the report says which.
            Lines.Add(InAnalysis.Get_MeasuredClean()
                ? TEXT("<p class=\"meta\">Every measured position was inside budget.</p>")
                : TEXT("<p class=\"meta\">No findings were produced. This session measured nothing usable — it is not a clean result.</p>"));
        }
        else
        {
            Lines.Add(TEXT("<table><thead><tr><th>Severity</th><th>Check</th><th>Position</th><th>Evidence</th><th>What to do</th></tr></thead><tbody>"));

            for (const auto& Finding : Findings)
            {
                const auto SeverityWord = Get_SeverityWord(Finding.Get_Severity());

                const auto& Evidence = Finding.Get_Evidence();

                const auto EvidenceText = ck::Format_UE(TEXT("{} {} ms vs {} ms budget ({}: {})"),
                    Get_MetricWord(Evidence.Get_Metric()),
                    Format_Ms(Evidence.Get_MeasuredMs()),
                    Format_Ms(Evidence.Get_BudgetMs()),
                    Evidence.Get_Signal(),
                    Format_Ms(Evidence.Get_SignalValue()));

                const auto RecommendationText = FString::Join(
                    ck::algo::Transform<TArray<FString>>(Finding.Get_Recommendations(),
                        [](const FCk_PerfLab_Recommendation& InRecommendation)
                        {
                            return ck::Format_UE(TEXT("{} (gain {}, effort {})"),
                                InRecommendation.Get_Text(),
                                Get_BandWord(InRecommendation.Get_GainBand()),
                                Get_BandWord(InRecommendation.Get_EffortBand()));
                        }),
                    TEXT(" · "));

                Lines.Add(ck::Format_UE(
                    TEXT("<tr><td class=\"sev-{}\">{}</td><td><code>{}</code></td><td><code>{}</code></td><td>{}</td><td>{}</td></tr>"),
                    SeverityWord,
                    SeverityWord,
                    Escape_Html(Finding.Get_CheckId()),
                    Escape_Html(Finding.Get_PositionId()),
                    Escape_Html(EvidenceText),
                    Escape_Html(RecommendationText)));
            }

            Lines.Add(TEXT("</tbody></table>"));

            Lines.Add(TEXT("<h2>Likely contributors</h2>"));
            Lines.Add(ck::Format_UE(TEXT("<p class=\"meta\">Actors {}.</p>"),
                Escape_Html(FString{k_ContributorDisclaimer})));
            Lines.Add(TEXT("<table><thead><tr><th>#</th><th>Check</th><th>Actor</th><th>Class</th><th>Distance</th><th>Note</th></tr></thead><tbody>"));

            for (const auto& Finding : Findings)
            {
                for (const auto& Contributor : Finding.Get_Contributors())
                {
                    Lines.Add(ck::Format_UE(
                        TEXT("<tr><td class=\"num\">{}</td><td><code>{}</code></td><td><code>{}</code></td>")
                        TEXT("<td>{}</td><td class=\"num\">{} cm</td><td>{}</td></tr>"),
                        Contributor.Get_Rank(),
                        Escape_Html(Finding.Get_CheckId()),
                        Escape_Html(Contributor.Get_ObjectPath()),
                        Escape_Html(Contributor.Get_ClassName()),
                        Format_Score(Contributor.Get_DistanceCm()),
                        Escape_Html(Contributor.Get_Note())));
                }
            }

            Lines.Add(TEXT("</tbody></table>"));
        }

        Lines.Add(ck::Format_UE(TEXT("<p class=\"limit\">{}</p>"),
            Escape_Html(FString{k_LimitationParagraph})));

        Lines.Add(TEXT("</body></html>"));

        return FString::Join(Lines, TEXT("\n"));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_SessionCsv(
            const FCk_PerfLab_Session& InSession,
            const FCk_PerfLab_Analysis& InAnalysis,
            ECk_CsvTable InTable)
        -> FString
    {
        using namespace ck_perf_lab_export;

        auto Lines = TArray<FString>{};

        switch (InTable)
        {
            case ECk_CsvTable::SessionInfo:
            {
                Lines.Add(Build_CsvRow({TEXT("key"), TEXT("value")}));

                const auto& Environment = InSession.Get_Environment();
                const auto& Request     = InSession.Get_Request();

                const auto AppendRow = [&Lines](const TCHAR* InKey, const FString& InValue)
                {
                    Lines.Add(Build_CsvRow({InKey, InValue}));
                };

                AppendRow(TEXT("sessionId"),          InSession.Get_SessionId());
                AppendRow(TEXT("mapPath"),            Request.Get_MapPath());
                AppendRow(TEXT("budgetMs"),           Format_Ms(InAnalysis.Get_BudgetMs()));
                AppendRow(TEXT("requestedBudgetMs"),  Format_Ms(Request.Get_BudgetMs()));
                AppendRow(TEXT("seed"),               ck::Format_UE(TEXT("{}"), Request.Get_Seed()));
                AppendRow(TEXT("score"),              Format_Score(InAnalysis.Get_Score().Get_Value()));
                AppendRow(TEXT("measuredClean"),      InAnalysis.Get_MeasuredClean() ? TEXT("true") : TEXT("false"));
                AppendRow(TEXT("positionCount"),      ck::Format_UE(TEXT("{}"), InSession.Get_Positions().Num()));
                AppendRow(TEXT("findingCount"),       ck::Format_UE(TEXT("{}"), InAnalysis.Get_Findings().Num()));
                AppendRow(TEXT("machineName"),        Environment.Get_MachineName());
                AppendRow(TEXT("cpuBrand"),           Environment.Get_CpuBrand());
                AppendRow(TEXT("gpuBrand"),           Environment.Get_GpuBrand());
                AppendRow(TEXT("rhiName"),            Environment.Get_RhiName());
                AppendRow(TEXT("engineVersion"),      Environment.Get_EngineVersion());
                AppendRow(TEXT("buildConfiguration"), Environment.Get_BuildConfiguration());
                AppendRow(TEXT("instanceMode"),       Environment.Get_InstanceMode());
                AppendRow(TEXT("viewportSize"),       Format_ViewportSize(Environment.Get_ViewportSizeActual()));
                AppendRow(TEXT("startedUtc"),         Environment.Get_StartedUtc());
                AppendRow(TEXT("finishedUtc"),        Environment.Get_FinishedUtc());

                break;
            }

            case ECk_CsvTable::Positions:
            {
                Lines.Add(Build_CsvRow({
                    TEXT("positionId"), TEXT("x"), TEXT("y"), TEXT("z"), TEXT("revisited"),
                    TEXT("frameMs"), TEXT("gameThreadMs"), TEXT("renderThreadMs"), TEXT("rhiThreadMs"),
                    TEXT("gpuMs"), TEXT("frameWorstMs"), TEXT("frameP99Ms"), TEXT("frameOnePercentLowMs"),
                    TEXT("sampleCount"), TEXT("outlierCount"), TEXT("confidence")}));

                for (const auto& Position : Get_PositionOrder(InSession))
                {
                    const auto& Frame = Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::Frame);

                    Lines.Add(Build_CsvRow({
                        Position.Get_PositionId(),
                        Format_Cm(Position.Get_Location().X),
                        Format_Cm(Position.Get_Location().Y),
                        Format_Cm(Position.Get_Location().Z),
                        Position.Get_Revisited() ? TEXT("true") : TEXT("false"),
                        Format_Metric(Frame),
                        Format_Metric(Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::GameThread)),
                        Format_Metric(Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::RenderThread)),
                        Format_Metric(Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::RhiThread)),
                        Format_Metric(Position.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::Gpu)),
                        Format_MetricValue(Frame, Frame.Get_WorstMs()),
                        Format_MetricValue(Frame, Frame.Get_P99Ms()),
                        Format_MetricValue(Frame, Frame.Get_OnePercentLowMs()),
                        ck::Format_UE(TEXT("{}"), Frame.Get_SampleCount()),
                        ck::Format_UE(TEXT("{}"), Frame.Get_OutlierCount()),
                        Get_ConfidenceWord(Position.Get_Confidence().Get_Rating())}));
                }

                break;
            }

            case ECk_CsvTable::Directions:
            {
                Lines.Add(Build_CsvRow({
                    TEXT("positionId"), TEXT("yawDegrees"), TEXT("pitchDegrees"),
                    TEXT("frameMs"), TEXT("gameThreadMs"), TEXT("renderThreadMs"),
                    TEXT("rhiThreadMs"), TEXT("gpuMs")}));

                for (const auto& Position : Get_PositionOrder(InSession))
                {
                    // Within a position, by yaw: the order the camera actually swept, which is the
                    // order a reader retracing the run expects to see.
                    const auto Directions = ck::algo::Sort(Position.Get_Directions(),
                        [](const FCk_PerfLab_DirectionResult& InA, const FCk_PerfLab_DirectionResult& InB)
                        {
                            // Exact, for the strict-weak-ordering reason above.
                            if (InA.Get_YawDegrees() != InB.Get_YawDegrees())
                            { return InA.Get_YawDegrees() < InB.Get_YawDegrees(); }

                            return InA.Get_PitchDegrees() < InB.Get_PitchDegrees();
                        });

                    for (const auto& Direction : Directions)
                    {
                        Lines.Add(Build_CsvRow({
                            Position.Get_PositionId(),
                            Format_Ms(Direction.Get_YawDegrees()),
                            Format_Ms(Direction.Get_PitchDegrees()),
                            Format_Metric(Direction.Get_Metrics().Get_ByMetric(ECk_PerfLab_Metric::Frame)),
                            Format_Metric(Direction.Get_Metrics().Get_ByMetric(ECk_PerfLab_Metric::GameThread)),
                            Format_Metric(Direction.Get_Metrics().Get_ByMetric(ECk_PerfLab_Metric::RenderThread)),
                            Format_Metric(Direction.Get_Metrics().Get_ByMetric(ECk_PerfLab_Metric::RhiThread)),
                            Format_Metric(Direction.Get_Metrics().Get_ByMetric(ECk_PerfLab_Metric::Gpu))}));
                    }
                }

                break;
            }

            case ECk_CsvTable::Findings:
            {
                Lines.Add(Build_CsvRow({
                    TEXT("checkId"), TEXT("stableKey"), TEXT("severity"), TEXT("positionId"),
                    TEXT("metric"), TEXT("measuredMs"), TEXT("budgetMs"), TEXT("overBudgetRatio"),
                    TEXT("signal"), TEXT("signalValue"), TEXT("recommendations")}));

                for (const auto& Finding : Get_FindingOrder(InAnalysis))
                {
                    const auto RecommendationText = FString::Join(
                        ck::algo::Transform<TArray<FString>>(Finding.Get_Recommendations(),
                            [](const FCk_PerfLab_Recommendation& InRecommendation)
                            {
                                return ck::Format_UE(TEXT("{} (gain {}, effort {})"),
                                    InRecommendation.Get_Text(),
                                    Get_BandWord(InRecommendation.Get_GainBand()),
                                    Get_BandWord(InRecommendation.Get_EffortBand()));
                            }),
                        TEXT(" | "));

                    // Evidence lands in its own columns rather than a prose cell: this is the table
                    // people pivot on, and a spreadsheet cannot split a sentence back into numbers.
                    const auto& Evidence = Finding.Get_Evidence();

                    Lines.Add(Build_CsvRow({
                        Finding.Get_CheckId(),
                        Finding.Get_StableKey(),
                        Get_SeverityWord(Finding.Get_Severity()),
                        Finding.Get_PositionId(),
                        Get_MetricWord(Evidence.Get_Metric()),
                        Format_Ms(Evidence.Get_MeasuredMs()),
                        Format_Ms(Evidence.Get_BudgetMs()),
                        Format_Score(Evidence.Get_OverBudgetRatio()),
                        Evidence.Get_Signal(),
                        Format_Ms(Evidence.Get_SignalValue()),
                        RecommendationText}));
                }

                break;
            }

            case ECk_CsvTable::Contributors:
            {
                Lines.Add(Build_CsvRow({
                    TEXT("checkId"), TEXT("positionId"), TEXT("rank"), TEXT("objectPath"),
                    TEXT("className"), TEXT("distanceCm"), TEXT("note"), TEXT("disclaimer")}));

                for (const auto& Finding : Get_FindingOrder(InAnalysis))
                {
                    for (const auto& Contributor : Finding.Get_Contributors())
                    {
                        // The disclaimer travels in the ROW, not only in a header somebody can drop
                        // when they filter or re-sort the sheet.
                        Lines.Add(Build_CsvRow({
                            Finding.Get_CheckId(),
                            Finding.Get_PositionId(),
                            ck::Format_UE(TEXT("{}"), Contributor.Get_Rank()),
                            Contributor.Get_ObjectPath(),
                            Contributor.Get_ClassName(),
                            Format_Score(Contributor.Get_DistanceCm()),
                            Contributor.Get_Note(),
                            FString{k_ContributorDisclaimer}}));
                    }
                }

                break;
            }

            case ECk_CsvTable::ScoreComponents:
            {
                Lines.Add(Build_CsvRow({
                    TEXT("component"), TEXT("weight"), TEXT("value"), TEXT("included"),
                    TEXT("excludedReason")}));

                for (const auto& Component : InAnalysis.Get_Score().Get_Components())
                {
                    Lines.Add(Build_CsvRow({
                        Get_ComponentWord(Component.Get_Component()),
                        Format_Score(Component.Get_Weight()),
                        Format_Score(Component.Get_Value()),
                        Component.Get_Included() ? TEXT("true") : TEXT("false"),
                        Component.Get_ExcludedReason()}));
                }

                break;
            }
        }

        // A trailing newline, so appending tables or piping the file never glues two rows together.
        return FString::Join(Lines, TEXT("\n")) + TEXT("\n");
    }

    auto
        Build_SessionCsvBundle(
            const FCk_PerfLab_Session& InSession,
            const FCk_PerfLab_Analysis& InAnalysis)
        -> FString
    {
        auto Sections = TArray<FString>{};

        for (const auto Table : Get_AllCsvTables())
        {
            Sections.Add(ck::Format_UE(TEXT("# {}\n{}"),
                Get_CsvTableName(Table),
                Build_SessionCsv(InSession, InAnalysis, Table)));
        }

        return FString::Join(Sections, TEXT("\n"));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_SessionJson(
            const FCk_PerfLab_Session& InSession,
            const FCk_PerfLab_Analysis& InAnalysis,
            const FDateTime& InGeneratedAt)
        -> FString
    {
        using namespace ck_perf_lab_export;

        // The session half comes from the codec verbatim — one schema, one writer. Re-serialising it
        // here would be a second writer of the same shape, and two writers of one schema drift.
        const auto SessionJson = Write_SessionToJson(InSession);

        auto SessionObject = TSharedPtr<FJsonObject>{};
        auto Reader        = TJsonReaderFactory<>::Create(SessionJson);

        if (NOT FJsonSerializer::Deserialize(Reader, SessionObject) || ck::Is_NOT_Valid(SessionObject))
        {
            // The codec just produced this string, so failing to read it back is a defect in the
            // codec rather than bad input. Hand back the session alone instead of a broken document.
            CK_TRIGGER_ENSURE(TEXT("Could not re-read the session JSON the codec just wrote"));
            return SessionJson;
        }

        auto AnalysisObject = MakeShared<FJsonObject>();

        AnalysisObject->SetStringField(TEXT("analysedUtc"),   InGeneratedAt.ToIso8601());
        AnalysisObject->SetNumberField(TEXT("budgetMs"),      InAnalysis.Get_BudgetMs());
        AnalysisObject->SetBoolField  (TEXT("measuredClean"), InAnalysis.Get_MeasuredClean());
        AnalysisObject->SetStringField(TEXT("limitations"),   k_LimitationParagraph);

        const auto& Score = InAnalysis.Get_Score();

        auto ScoreObject     = MakeShared<FJsonObject>();
        auto ComponentValues = TArray<TSharedPtr<FJsonValue>>{};

        for (const auto& Component : Score.Get_Components())
        {
            auto ComponentObject = MakeShared<FJsonObject>();

            ComponentObject->SetStringField(TEXT("key"),      Get_JsonKey(Get_ComponentWord(Component.Get_Component())));
            ComponentObject->SetNumberField(TEXT("weight"),   Component.Get_Weight());
            ComponentObject->SetNumberField(TEXT("value"),    Component.Get_Value());
            ComponentObject->SetBoolField  (TEXT("included"), Component.Get_Included());
            ComponentObject->SetStringField(TEXT("reason"),   Component.Get_ExcludedReason());

            ComponentValues.Add(MakeShared<FJsonValueObject>(ComponentObject));
        }

        ScoreObject->SetNumberField(TEXT("value"), Score.Get_Value());
        ScoreObject->SetNumberField(TEXT("componentsUsed"),
            ck::algo::CountIf(Score.Get_Components(), [](const FCk_PerfLab_ScoreComponentResult& InComponent)
            {
                return InComponent.Get_Included();
            }));
        ScoreObject->SetArrayField (TEXT("components"), ComponentValues);
        ScoreObject->SetStringField(TEXT("formula"),    Score.Get_Formula());

        AnalysisObject->SetObjectField(TEXT("score"), ScoreObject);

        auto FindingValues = TArray<TSharedPtr<FJsonValue>>{};

        for (const auto& Finding : Get_FindingOrder(InAnalysis))
        {
            auto FindingObject = MakeShared<FJsonObject>();

            FindingObject->SetStringField(TEXT("checkId"),    Finding.Get_CheckId());
            FindingObject->SetStringField(TEXT("stableKey"),  Finding.Get_StableKey());
            FindingObject->SetStringField(TEXT("severity"),   Get_JsonKey(Get_SeverityWord(Finding.Get_Severity())));
            FindingObject->SetStringField(TEXT("positionId"), Finding.Get_PositionId());

            const auto& Evidence       = Finding.Get_Evidence();
            auto        EvidenceObject = MakeShared<FJsonObject>();

            EvidenceObject->SetStringField(TEXT("metric"),          Get_JsonKey(Get_MetricWord(Evidence.Get_Metric())));
            EvidenceObject->SetNumberField(TEXT("measuredMs"),      Evidence.Get_MeasuredMs());
            EvidenceObject->SetNumberField(TEXT("budgetMs"),        Evidence.Get_BudgetMs());
            EvidenceObject->SetNumberField(TEXT("overBudgetRatio"), Evidence.Get_OverBudgetRatio());
            EvidenceObject->SetStringField(TEXT("signal"),          Evidence.Get_Signal());
            EvidenceObject->SetNumberField(TEXT("signalValue"),     Evidence.Get_SignalValue());

            FindingObject->SetObjectField(TEXT("evidence"), EvidenceObject);

            auto ContributorValues = TArray<TSharedPtr<FJsonValue>>{};

            for (const auto& Contributor : Finding.Get_Contributors())
            {
                auto ContributorObject = MakeShared<FJsonObject>();

                ContributorObject->SetNumberField(TEXT("rank"),       Contributor.Get_Rank());
                ContributorObject->SetStringField(TEXT("objectPath"), Contributor.Get_ObjectPath());
                ContributorObject->SetStringField(TEXT("className"),  Contributor.Get_ClassName());
                // The note IS the disclaimer — the analysis authors it that way so no consumer can
                // read a contributor without it, and a separate field would be one a reader drops.
                ContributorObject->SetNumberField(TEXT("distanceCm"), Contributor.Get_DistanceCm());
                ContributorObject->SetStringField(TEXT("note"),       Contributor.Get_Note());

                ContributorValues.Add(MakeShared<FJsonValueObject>(ContributorObject));
            }

            FindingObject->SetArrayField(TEXT("contributors"), ContributorValues);

            auto RecommendationValues = TArray<TSharedPtr<FJsonValue>>{};

            for (const auto& Recommendation : Finding.Get_Recommendations())
            {
                auto RecommendationObject = MakeShared<FJsonObject>();

                RecommendationObject->SetNumberField(TEXT("order"),      Recommendation.Get_Order());
                RecommendationObject->SetStringField(TEXT("gainBand"),   Get_JsonKey(Get_BandWord(Recommendation.Get_GainBand())));
                RecommendationObject->SetStringField(TEXT("effortBand"), Get_JsonKey(Get_BandWord(Recommendation.Get_EffortBand())));
                RecommendationObject->SetStringField(TEXT("text"),       Recommendation.Get_Text());

                RecommendationValues.Add(MakeShared<FJsonValueObject>(RecommendationObject));
            }

            FindingObject->SetArrayField(TEXT("recommendations"), RecommendationValues);

            FindingValues.Add(MakeShared<FJsonValueObject>(FindingObject));
        }

        AnalysisObject->SetArrayField(TEXT("findings"), FindingValues);

        SessionObject->SetObjectField(TEXT("analysis"), AnalysisObject);

        auto Output = FString{};
        auto Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);

        FJsonSerializer::Serialize(SessionObject.ToSharedRef(), Writer);

        return Output;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_CompareHtml(
            const FCk_PerfLab_Compare& InCompare,
            const FDateTime& InGeneratedAt)
        -> FString
    {
        using namespace ck_perf_lab_export;

        auto Lines = TArray<FString>{};

        Lines.Add(TEXT("<!DOCTYPE html>"));
        Lines.Add(TEXT("<html lang=\"en\"><head><meta charset=\"utf-8\">"));
        Lines.Add(ck::Format_UE(TEXT("<title>Performance comparison — {}</title>"),
            Escape_Html(InCompare.Get_MapPath())));

        Lines.Append(Build_StyleBlock());

        Lines.Add(ck::Format_UE(TEXT("<h1>Performance comparison — {}</h1>"),
            Escape_Html(InCompare.Get_MapPath())));
        Lines.Add(ck::Format_UE(TEXT("<p class=\"meta\">Generated {}</p>"),
            Escape_Html(InGeneratedAt.ToString())));

        if (NOT InCompare.Get_IsComparable())
        {
            Lines.Add(ck::Format_UE(TEXT("<p class=\"limit\">{}</p>"),
                Escape_Html(Get_RefusalText(InCompare.Get_Refusal()))));
            Lines.Add(TEXT("</body></html>"));

            return FString::Join(Lines, TEXT("\n"));
        }

        // Banners before the table, never instead of it. Suppressing the comparison would hide the
        // very fact the reader needs in order to discount it correctly.
        for (const auto Warning : InCompare.Get_Warnings())
        {
            Lines.Add(ck::Format_UE(TEXT("<p class=\"limit\">{}</p>"),
                Escape_Html(Get_WarningText(Warning))));
        }

        Lines.Add(ck::Format_UE(
            TEXT("<p class=\"meta\">{} regressed · {} improved · {} positions compared</p>"),
            InCompare.Get_RegressedCount(),
            InCompare.Get_ImprovedCount(),
            InCompare.Get_Positions().Num()));

        // Always set by this point: the refusal returned above, and a comparison that was not
        // refused always has a delta because both scores were computed against the same budget.
        const auto ScoreDelta = InCompare.Get_ScoreDelta().Get(0.0f);

        Lines.Add(ck::Format_UE(TEXT("<p>Score {} &rarr; {} <span class=\"meta\">({}{})</span></p>"),
            Format_Score(InCompare.Get_BaselineScore()),
            Format_Score(InCompare.Get_CurrentScore()),
            ScoreDelta >= 0.0f ? TEXT("+") : TEXT(""),
            Format_Score(ScoreDelta)));

        Lines.Add(TEXT("<h2>Positions</h2>"));
        Lines.Add(TEXT("<table><thead><tr><th>Position</th><th>Verdict</th><th>Metric</th><th>Baseline</th><th>Current</th><th>Delta</th><th>Noise band</th></tr></thead><tbody>"));

        for (const auto& Position : InCompare.Get_Positions())
        {
            for (const auto& Metric : Position.Get_Metrics())
            {
                const auto VerdictWord = Get_VerdictWord(Metric.Get_Verdict());

                Lines.Add(Metric.Get_Verdict() == ECk_PerfLab_CompareVerdict::Incomparable
                    ? ck::Format_UE(
                          TEXT("<tr><td><code>{}</code></td><td class=\"v-{}\">{}</td><td>{}</td>")
                          TEXT("<td colspan=\"4\" class=\"v-Incomparable\">not measured in both sessions</td></tr>"),
                          Escape_Html(Position.Get_PositionId()),
                          VerdictWord, VerdictWord,
                          Get_MetricWord(Metric.Get_Metric()))
                    : ck::Format_UE(
                          TEXT("<tr><td><code>{}</code></td><td class=\"v-{}\">{}</td><td>{}</td>")
                          TEXT("<td class=\"num\">{}</td><td class=\"num\">{}</td><td class=\"num\">{}{}</td><td class=\"num\">&plusmn;{}</td></tr>"),
                          Escape_Html(Position.Get_PositionId()),
                          VerdictWord, VerdictWord,
                          Get_MetricWord(Metric.Get_Metric()),
                          Format_Ms(Metric.Get_BaselineMs()),
                          Format_Ms(Metric.Get_CurrentMs()),
                          Metric.Get_DeltaMs() >= 0.0f ? TEXT("+") : TEXT(""),
                          Format_Ms(Metric.Get_DeltaMs()),
                          Format_Ms(Metric.Get_NoiseBandMs())));
            }
        }

        Lines.Add(TEXT("</tbody></table>"));

        // Positions on one side only are the finding, not noise to drop: a map that changed shape
        // between two runs is exactly what a reader chasing a regression needs to know first.
        const auto AppendIdList = [&Lines](const TCHAR* InHeading, const TArray<FString>& InIds)
        {
            if (InIds.IsEmpty())
            { return; }

            Lines.Add(ck::Format_UE(TEXT("<h2>{}</h2>"), InHeading));
            Lines.Add(ck::Format_UE(TEXT("<p class=\"meta\"><code>{}</code></p>"),
                Escape_Html(FString::Join(InIds, TEXT(", ")))));
        };

        AppendIdList(TEXT("Only in the baseline"), InCompare.Get_OnlyInBaseline());
        AppendIdList(TEXT("Only in the current session"), InCompare.Get_OnlyInCurrent());

        Lines.Add(ck::Format_UE(TEXT("<p class=\"limit\">{}</p>"),
            Escape_Html(FString{k_LimitationParagraph})));

        Lines.Add(TEXT("</body></html>"));

        return FString::Join(Lines, TEXT("\n"));
    }
}

// --------------------------------------------------------------------------------------------------------------------
