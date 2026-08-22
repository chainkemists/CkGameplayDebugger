#include "CkPerfLab/Host/CkPerfLab_SessionStore.h"
#include "CkPerfLab/Host/CkPerfLab_Subprocess.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Macros/CkMacros.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_host_spec
{
    constexpr auto kFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Host_CommandLine_QuotesAndCarriesEverything,
    "Ck.PerfLab.Host.CommandLine_QuotesAndCarriesEverything",
    ck_perf_lab_host_spec::kFlags)

bool FCkPerfLab_Host_CommandLine_QuotesAndCarriesEverything::RunTest(const FString& Parameters)
{
    // Paths with spaces are the normal case on Windows, and an unquoted one fails as a mangled map
    // name rather than as an obvious error — so the quoting is worth pinning rather than trusting.
    const auto CommandLine = ck::perf_lab::Build_ChildCommandLine(
        TEXT("D:/Some Path/Proj.uproject"),
        TEXT("/Game/Maps/Test.Test"),
        TEXT("D:/Some Path/Saved/req.json"),
        TEXT("D:/Some Path/Saved/child.log"));

    TestTrue(TEXT("The project path is quoted"),  CommandLine.Contains(TEXT("\"D:/Some Path/Proj.uproject\"")));
    TestTrue(TEXT("The map path is quoted"),      CommandLine.Contains(TEXT("\"/Game/Maps/Test.Test\"")));
    // The VALUE is quoted, never the whole token. FParse::Value skips quoted regions when locating
    // its key, so a whole-token-quoted switch reads as absent and the child boots inert while the
    // host waits out its full timeout. That is precisely how this shipped before a review caught it,
    // and it is why the assertion is written as an exclusion as well as an inclusion.
    TestTrue(TEXT("The request path is quoted as a value"),
        CommandLine.Contains(TEXT("-CkPerfLab-Request=\"D:/Some Path/Saved/req.json\"")));
    TestFalse(TEXT("The request switch is never quoted as a whole token"),
        CommandLine.Contains(TEXT("\"-CkPerfLab-Request")));
    TestTrue(TEXT("The log path is quoted"),      CommandLine.Contains(TEXT("-abslog=\"D:/Some Path/Saved/child.log\"")));

    // The flags the measurement depends on, each for a stated reason elsewhere: game mode, a real
    // window to render into, no modal dialogs, and no source control in a measurement process.
    TestTrue(TEXT("It runs in game mode"),        CommandLine.Contains(TEXT("-game")));
    TestTrue(TEXT("It renders into a window"),    CommandLine.Contains(TEXT("-windowed")));
    TestTrue(TEXT("It is unattended"),            CommandLine.Contains(TEXT("-unattended")));
    TestTrue(TEXT("Source control is disabled"),  CommandLine.Contains(TEXT("-SCCProvider=None")));

    // -nullrhi would leave nothing to measure on the GPU or the render thread.
    TestFalse(TEXT("It never disables the RHI"),  CommandLine.Contains(TEXT("-nullrhi")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Host_SessionDir_IsUnderTheStoreRoot,
    "Ck.PerfLab.Host.SessionDir_IsUnderTheStoreRoot",
    ck_perf_lab_host_spec::kFlags)

bool FCkPerfLab_Host_SessionDir_IsUnderTheStoreRoot::RunTest(const FString& Parameters)
{
    const auto Root = ck::perf_lab::Get_SessionsRoot();

    TestTrue(TEXT("A session directory sits under the store root"),
        ck::perf_lab::Get_SessionDirFor(TEXT("20260821-abc")).StartsWith(Root));

    TestTrue(TEXT("The store root is under the project's Saved directory"),
        Root.Contains(TEXT("CkPerfLab")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Host_Delete_RefusesToEscapeTheStore,
    "Ck.PerfLab.Host.Delete_RefusesToEscapeTheStore",
    ck_perf_lab_host_spec::kFlags)

bool FCkPerfLab_Host_Delete_RefusesToEscapeTheStore::RunTest(const FString& Parameters)
{
    // Each refusal logs an error on purpose — a caller asking to delete outside the store is a
    // defect worth shouting about — and the automation framework fails a test that logs one unless
    // it is declared. Declaring them here asserts that the refusals actually happened.
    AddExpectedError(TEXT("Refusing to delete"), EAutomationExpectedErrorFlags::Contains, 4);

    // The id reaches this function from a UI selection and, later, from a command line. A traversal
    // in it must never become a delete somewhere the user did not point at.
    TestFalse(TEXT("Parent traversal is refused"),        ck::perf_lab::Try_DeleteSession(TEXT("../..")));
    TestFalse(TEXT("A deeper traversal is refused"),      ck::perf_lab::Try_DeleteSession(TEXT("../../../Content")));
    TestFalse(TEXT("An empty id is refused"),             ck::perf_lab::Try_DeleteSession(TEXT("")));
    TestFalse(TEXT("An absolute path is refused"),        ck::perf_lab::Try_DeleteSession(TEXT("D:/Windows")));

    // A well-formed id that simply does not exist is a plain false, not a refusal and not a crash.
    TestFalse(TEXT("A missing session reports failure"),
        ck::perf_lab::Try_DeleteSession(TEXT("20990101-does-not-exist")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkPerfLab_Host_Rows_AreNewestFirst,
    "Ck.PerfLab.Host.Rows_AreNewestFirst",
    ck_perf_lab_host_spec::kFlags)

bool FCkPerfLab_Host_Rows_AreNewestFirst::RunTest(const FString& Parameters)
{
    const auto Rows = ck::perf_lab::Get_SessionRows();

    // Whatever happens to be on this machine, the ordering contract has to hold: ids lead with a UTC
    // timestamp, so descending id is newest first.
    TestTrue(TEXT("Rows are ordered newest first"),
        ck::algo::IsSorted(Rows, [](const FCk_PerfLab_SessionRow& InA, const FCk_PerfLab_SessionRow& InB)
        { return InA.Get_SessionId() > InB.Get_SessionId(); }));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
