#include "CkPerfLab_ReportCommandlet.h"

#include "CkPerfLab/Analysis/CkPerfLab_Analysis.h"
#include "CkPerfLab/Analysis/CkPerfLab_SessionCompare.h"
#include "CkPerfLab/Export/CkPerfLab_Export.h"
#include "CkPerfLab/Session/CkPerfLab_SessionCodec.h"
#include "CkPerfLab_Log.h"

#include "CkCore/Macros/CkMacros.h"

#include <Misc/CommandLine.h>
#include <Misc/DateTime.h>
#include <Misc/FileHelper.h>
#include <Misc/Parse.h>
#include <Misc/Paths.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_report_commandlet
{
    /**
     * Reads a session from a directory.
     *
     * Takes the directory rather than the file so a CI job can point at what the tool produced
     * without having to know the file's name inside it.
     */
    auto
        TryRead_Session(
            const FString& InSessionDir,
            FCk_PerfLab_Session& OutSession)
        -> bool
    {
        const auto SessionFile = ck::perf_lab::Get_SessionFilePath(InSessionDir);

        auto Json = FString{};

        if (NOT FFileHelper::LoadFileToString(Json, *SessionFile))
        {
            ck::perf_lab::Error(TEXT("Could not read a session file at [{}]"), SessionFile);
            return false;
        }

        if (NOT ck::perf_lab::TryRead_SessionFromJson(Json, OutSession))
        {
            ck::perf_lab::Error(TEXT("The file at [{}] is not a session this build can read"), SessionFile);
            return false;
        }

        return true;
    }

    auto
        TryWrite_Report(
            const FString& InOutputDir,
            const FString& InFileName,
            const FString& InContent)
        -> bool
    {
        const auto Path = FPaths::Combine(InOutputDir, InFileName);

        // UTF-8 without a BOM: the CSV half is read by tools that treat a BOM as part of the first
        // header cell, and a header nobody can match on is a CSV nobody can pivot.
        if (NOT FFileHelper::SaveStringToFile(InContent, *Path,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            ck::perf_lab::Error(TEXT("Could not write [{}]"), Path);
            return false;
        }

        ck::perf_lab::Display(TEXT("Wrote [{}]"), Path);

        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

UCkPerfLabReportCommandlet::UCkPerfLabReportCommandlet()
{
    LogToConsole = true;
    IsEditor     = true;
    IsClient     = false;
    IsServer     = false;

    HelpDescription = TEXT("Analyses a measured PerfLab session and writes HTML, CSV and JSON reports.");
    HelpUsage       = TEXT("CkPerfLabReport -session=<dir> -output=<dir> [-budget=<ms>] [-failbelow=<score>] [-compare=<dir>]");

    HelpParamNames.Add(TEXT("session"));
    HelpParamDescriptions.Add(TEXT("Session directory to analyse (required)"));

    HelpParamNames.Add(TEXT("output"));
    HelpParamDescriptions.Add(TEXT("Directory to write the reports into (required)"));

    HelpParamNames.Add(TEXT("budget"));
    HelpParamDescriptions.Add(TEXT("Frame budget in ms; defaults to the session's own request"));

    HelpParamNames.Add(TEXT("failbelow"));
    HelpParamDescriptions.Add(TEXT("Exit 2 when the score is below this"));

    HelpParamNames.Add(TEXT("compare"));
    HelpParamDescriptions.Add(TEXT("A baseline session directory; also writes compare.html"));
}

// --------------------------------------------------------------------------------------------------------------------

int32 UCkPerfLabReportCommandlet::Main(const FString& Params)
{
    using namespace ck_perf_lab_report_commandlet;

    const auto Fail = static_cast<int32>(ECk_PerfLab_ReportExitCode::Error);

    auto SessionDir = FString{};

    if (NOT FParse::Value(*Params, TEXT("session="), SessionDir) || SessionDir.IsEmpty())
    {
        ck::perf_lab::Error(TEXT("-session=<dir> is required. {}"), HelpUsage);
        return Fail;
    }

    auto OutputDir = FString{};

    if (NOT FParse::Value(*Params, TEXT("output="), OutputDir) || OutputDir.IsEmpty())
    {
        ck::perf_lab::Error(TEXT("-output=<dir> is required. {}"), HelpUsage);
        return Fail;
    }

    auto Session = FCk_PerfLab_Session{};

    if (NOT TryRead_Session(SessionDir, Session))
    {
        return Fail;
    }

    // The session's own requested budget unless the caller overrides it, so a CI job that only wants
    // a report does not have to restate a number the session already carries.
    auto BudgetMs = Session.Get_Request().Get_BudgetMs();

    FParse::Value(*Params, TEXT("budget="), BudgetMs);

    // IsFinite FIRST: every comparison against a NaN is false, so `<= 0.0f` alone waves one through
    // — and `-budget=nan` parses, because the CRT's strtof accepts it. A NaN budget reaches
    // SetNumberField and emits a bare `nan` token, which is not parseable JSON at all.
    if (NOT FMath::IsFinite(BudgetMs) || BudgetMs <= 0.0f)
    {
        ck::perf_lab::Error(TEXT("The budget resolved to [{}] ms, which cannot be scored against. "
                                 "Pass -budget=<ms>."), BudgetMs);
        return Fail;
    }

    const auto Analysis = ck::perf_lab::Analyse_Session(Session, BudgetMs);

    // The clock is read HERE and handed to every builder, rather than each of them reading it: the
    // builders stay pure and one run stamps one time across all its files.
    const auto GeneratedAt = FDateTime::UtcNow();

    const auto Wrote =
        TryWrite_Report(OutputDir, TEXT("report.html"),
            ck::perf_lab::exporter::Build_SessionHtml(Session, Analysis, GeneratedAt)) &&
        TryWrite_Report(OutputDir, TEXT("report.csv"),
            ck::perf_lab::exporter::Build_SessionCsvBundle(Session, Analysis)) &&
        TryWrite_Report(OutputDir, TEXT("report.json"),
            ck::perf_lab::exporter::Build_SessionJson(Session, Analysis, GeneratedAt));

    if (NOT Wrote)
    {
        return Fail;
    }

    auto CompareDir = FString{};

    if (FParse::Value(*Params, TEXT("compare="), CompareDir) && NOT CompareDir.IsEmpty())
    {
        auto Baseline = FCk_PerfLab_Session{};

        if (NOT TryRead_Session(CompareDir, Baseline))
        {
            return Fail;
        }

        const auto Compare = ck::perf_lab::Compare_Sessions(Baseline, Session, BudgetMs);

        if (NOT TryWrite_Report(OutputDir, TEXT("compare.html"),
                ck::perf_lab::exporter::Build_CompareHtml(Compare, GeneratedAt)))
        {
            return Fail;
        }

        // A refusal is reported but does not fail the job: the caller asked for a comparison and got
        // an explanation of why there isn't one, which is a successful answer to the question.
        if (Compare.Get_IsComparable())
        {
            ck::perf_lab::Display(TEXT("Compare: {} regressed, {} improved, {} positions"),
                Compare.Get_RegressedCount(), Compare.Get_ImprovedCount(),
                Compare.Get_Positions().Num());
        }
        else
        {
            ck::perf_lab::Warning(TEXT("Compare refused: {}"),
                ck::perf_lab::Get_RefusalText(Compare.Get_Refusal()));
        }
    }

    const auto Score = Analysis.Get_Score().Get_Value();

    // Display, not Log. A commandlet's Log-verbosity output goes to the log file and never reaches
    // the console, so the one line a CI job actually shows its reader would be invisible — verified
    // by running this commandlet and finding only the Error lines in captured stdout.
    ck::perf_lab::Display(TEXT("Score {} / 100 against a {} ms budget, {} finding(s)"),
        Score, BudgetMs, Analysis.Get_Findings().Num());

    auto FailBelow = 0.0f;

    if (FParse::Value(*Params, TEXT("failbelow="), FailBelow) && Score < FailBelow)
    {
        ck::perf_lab::Error(TEXT("Score {} is below the required {}"), Score, FailBelow);
        return static_cast<int32>(ECk_PerfLab_ReportExitCode::Threshold);
    }

    return static_cast<int32>(ECk_PerfLab_ReportExitCode::Pass);
}

// --------------------------------------------------------------------------------------------------------------------
