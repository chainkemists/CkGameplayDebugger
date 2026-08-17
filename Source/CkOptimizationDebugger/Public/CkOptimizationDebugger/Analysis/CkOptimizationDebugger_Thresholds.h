#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class UCkOptimizationDebuggerSettings;

// --------------------------------------------------------------------------------------------------------------------

/**
 * The numbers a scan compares against, copied out of the settings object ONCE when the scan starts.
 *
 * The checks take this struct and never touch `UCkOptimizationDebuggerSettings::Get()` themselves. Two reasons, both
 * load-bearing:
 *
 *  - A check that reads the CDO is a check a spec cannot drive. Every threshold rule below is asserted against a
 *    hand-built struct, which is only possible because there is a struct to build.
 *  - A scan is one answer to one question. If a user edits a threshold in Editor Preferences half way through a
 *    walk of ten thousand actors, the second half must not be judged by a different rule than the first.
 */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_Thresholds
{
    // Defaults mirror UCkOptimizationDebuggerSettings' own defaults. `Ck.OptimizationDebugger.Thresholds.Defaults`
    // pins the two together, so a settings default that moves without this one moving fails a spec rather than
    // silently making a default-constructed scan disagree with a configured one.
    int32 MaxTriangleCountLOD0 = 100000;
    int32 MinTrianglesForNanite = 5000;
    int32 MaxTrianglesForNaniteWarning = 2000;
    int32 MaxCollisionPrimitives = 8;

    int32 MaxTextureSize = 2048;

    int32 MaxMaterialSlots = 8;
    int32 MaxTextureSamplers = 16;

    int32 MaxMovableLights = 4;
    int32 MaxLightmapResolution = 512;

    int32 MinRepeatedActorsForInstancing = 10;

    int32 MaxBlueprintDependencies = 50;
    int32 MinTexturesForStreamingWarning = 50;
};

// --------------------------------------------------------------------------------------------------------------------

/** A graded finding: how severe, and how far over. Both come off ONE (value, budget) pair on purpose — the severity
 *  is a three-bucket collapse of the ratio, so deriving them separately would be two places for the same rule to
 *  drift, and the pair is exactly what a check has in hand at the point it decides to report. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_GradedFinding
{
    ECkOptimizationDebugger_Severity Severity = ECkOptimizationDebugger_Severity::Minor;

    // Zero when the budget is non-positive — a ratio against a budget that cannot be exceeded is not a measurement.
    float BudgetRatio = 0.0f;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_thresholds
{
    /** Copies the per-user settings object into a plain struct. Returns the defaults above when the CDO cannot be
     *  reached, which is the honest answer — a scan judged by the shipped defaults beats a scan judged by zero. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_FromSettings() -> FCkOptimizationDebugger_Thresholds;

    CKOPTIMIZATIONDEBUGGER_API auto
    Build_FromSettings(
        const UCkOptimizationDebuggerSettings* InSettings) -> FCkOptimizationDebugger_Thresholds;

    // ----------------------------------------------------------------------------------------------------------------

    /** One step more severe, saturating at Critical. Severity is declared most-severe-first, so "more severe" is
     *  one step DOWN the enum. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Escalate_One(
        ECkOptimizationDebugger_Severity InSeverity) -> ECkOptimizationDebugger_Severity;

    /** How badly a measured value misses its budget.
     *
     *  At or past **twice** the budget the finding is escalated one step past `InAtBudgetSeverity`; merely past the
     *  budget it IS `InAtBudgetSeverity`. Grading exists because "3,000 triangles over" and "300,000 triangles over"
     *  are not the same finding, and a list that paints them identically is a list the reader stops sorting by.
     *
     *  A non-positive budget cannot be exceeded by a ratio, so it grades to `InAtBudgetSeverity` unchanged — the
     *  caller already decided there was something to report. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_GraduatedSeverity(
        double InValue,
        double InBudget,
        ECkOptimizationDebugger_Severity InAtBudgetSeverity) -> ECkOptimizationDebugger_Severity;

    /** How badly a measured value misses its budget, graded AND measured from one (value, budget) pair.
     *
     *  The severity is `Get_GraduatedSeverity` unchanged — this calls it rather than restating the rule. What it adds
     *  is the ratio that severity is a three-bucket collapse OF: past twice the budget the escalation saturates, so
     *  "over budget" and "five times over budget" wear the same badge, and the ratio is the only thing left that
     *  tells the reader which is which.
     *
     *  Both halves come off the same pair deliberately. A check that graded from one measurement and reported the
     *  ratio of another would emit a row whose badge and number disagree, and nothing downstream could tell which of
     *  the two was lying. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_Graded(
        double InValue,
        double InBudget,
        ECkOptimizationDebugger_Severity InAtBudgetSeverity) -> FCkOptimizationDebugger_GradedFinding;
}

// --------------------------------------------------------------------------------------------------------------------
