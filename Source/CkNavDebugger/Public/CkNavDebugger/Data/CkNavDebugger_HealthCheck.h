#pragma once

#include "CkNavDebugger/Data/CkNavDebugger_Types.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------
// One-shot health check — runs a battery of system-level diagnostics that catch the
// vast majority of "why is my path failing?" causes BEFORE you have to debug them in code.
//
// Result: a list of named items (Pass/Warn/Fail + detail). A green list means the nav
// stack is set up correctly. Any Fail item is the most likely root cause of agent path
// failures.
// --------------------------------------------------------------------------------------------------------------------

class FCkNavDebugger_HealthCheck
{
public:
    static auto
    Run(
        UWorld* InWorld) -> FCkNavDebugger_HealthCheckReport;
};

// --------------------------------------------------------------------------------------------------------------------
