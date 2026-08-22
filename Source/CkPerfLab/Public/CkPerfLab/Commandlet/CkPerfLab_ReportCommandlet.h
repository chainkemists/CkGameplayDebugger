#pragma once

#include <CoreMinimal.h>

#include <Commandlets/Commandlet.h>

#include "CkPerfLab_ReportCommandlet.generated.h"

// --------------------------------------------------------------------------------------------------------------------

/** What the process exits with. Fixed, because a CI job's only view of this tool is the number. */
enum class ECk_PerfLab_ReportExitCode : int32
{
    // The session analysed cleanly and, if a threshold was given, met it.
    Pass = 0,

    // Something went wrong: a missing argument, an unreadable session, a directory that could not
    // be written. Distinct from Threshold on purpose — a build that cannot tell "the level is slow"
    // from "the report never ran" will eventually ship a slow level believing it was measured.
    Error = 1,

    // The session was analysed and scored below the threshold the caller demanded.
    Threshold = 2,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Headless entry: analyse an already-measured session and write its reports.
 *
 * Invocation:
 *   CkPluginsEditor-Cmd.exe <Project.uproject> -run=CkPerfLabReport -session=<dir> -output=<dir>
 *                           [-budget=<ms>] [-failbelow=<score>] [-compare=<dir>]
 *
 * Options:
 *   -session=<dir>     Session directory to analyse (required)
 *   -output=<dir>      Directory to write report.html, report.csv and report.json into (required)
 *   -budget=<ms>       Frame budget to analyse against; defaults to the session's own request
 *   -failbelow=<score> Exit 2 when the score is below this
 *   -compare=<dir>     A baseline session directory; also writes compare.html
 *
 * It only ANALYSES. Measurement stays with the `-game` child the host launches, because a
 * commandlet runs without a rendering viewport and would measure a frame nobody drew — the numbers
 * would parse, look plausible, and describe nothing.
 */
// Named without the usual `Ck_` underscores: UE resolves `-run=X` by looking up a class literally
// called `XCommandlet`, so the class name IS the command-line token. `CkInsightsAnalyzer` and
// `CkAssetExporter` drop them for the same reason.
UCLASS()
class UCkPerfLabReportCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UCkPerfLabReportCommandlet();

    //~ Begin UCommandlet Interface
    virtual int32 Main(const FString& Params) override;
    //~ End UCommandlet Interface
};

// --------------------------------------------------------------------------------------------------------------------
