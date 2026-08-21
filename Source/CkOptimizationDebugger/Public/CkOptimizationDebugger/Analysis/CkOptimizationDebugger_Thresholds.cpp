#include "CkOptimizationDebugger/Analysis/CkOptimizationDebugger_Thresholds.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkOptimizationDebugger/Settings/CkOptimizationDebuggerSettings.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_thresholds
{
    auto
        Build_FromSettings()
        -> FCkOptimizationDebugger_Thresholds
    {
        return Build_FromSettings(UCkOptimizationDebuggerSettings::Get());
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_FromSettings(
            const UCkOptimizationDebuggerSettings* InSettings)
        -> FCkOptimizationDebugger_Thresholds
    {
        auto Thresholds = FCkOptimizationDebugger_Thresholds{};

        if (ck::Is_NOT_Valid(InSettings))
        { return Thresholds; }

        Thresholds.MaxTriangleCountLOD0 = InSettings->MaxTriangleCountLOD0;
        Thresholds.MinTrianglesForNanite = InSettings->MinTrianglesForNanite;
        Thresholds.MaxTrianglesForNaniteWarning = InSettings->MaxTrianglesForNaniteWarning;
        Thresholds.MaxCollisionPrimitives = InSettings->MaxCollisionPrimitives;

        Thresholds.MaxTextureSize = InSettings->MaxTextureSize;

        Thresholds.MaxMaterialSlots = InSettings->MaxMaterialSlots;
        Thresholds.MaxTextureSamplers = InSettings->MaxTextureSamplers;

        Thresholds.MaxMovableLights = InSettings->MaxMovableLights;
        Thresholds.MaxLightmapResolution = InSettings->MaxLightmapResolution;

        Thresholds.MinRepeatedActorsForInstancing = InSettings->MinRepeatedActorsForInstancing;

        Thresholds.MaxBlueprintDependencies = InSettings->MaxBlueprintDependencies;
        Thresholds.MinTexturesForStreamingWarning = InSettings->MinTexturesForStreamingWarning;
        Thresholds.MinSoundDurationForStreaming = InSettings->MinSoundDurationForStreaming;

        return Thresholds;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Escalate_One(
            ECkOptimizationDebugger_Severity InSeverity)
        -> ECkOptimizationDebugger_Severity
    {
        switch (InSeverity)
        {
            case ECkOptimizationDebugger_Severity::Minor:    return ECkOptimizationDebugger_Severity::Major;
            case ECkOptimizationDebugger_Severity::Major:    return ECkOptimizationDebugger_Severity::Critical;
            case ECkOptimizationDebugger_Severity::Critical: return ECkOptimizationDebugger_Severity::Critical;
            default:                                         return InSeverity;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_GraduatedSeverity(
            double InValue,
            double InBudget,
            ECkOptimizationDebugger_Severity InAtBudgetSeverity)
        -> ECkOptimizationDebugger_Severity
    {
        if (InBudget <= 0.0)
        { return InAtBudgetSeverity; }

        return InValue >= (InBudget * 2.0)
            ? Escalate_One(InAtBudgetSeverity)
            : InAtBudgetSeverity;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_Graded(
            double InValue,
            double InBudget,
            ECkOptimizationDebugger_Severity InAtBudgetSeverity)
        -> FCkOptimizationDebugger_GradedFinding
    {
        auto Graded = FCkOptimizationDebugger_GradedFinding{};
        Graded.Severity = Get_GraduatedSeverity(InValue, InBudget, InAtBudgetSeverity);

        // Only an ACTUAL overage is reported as a ratio; anything at or under the budget reports zero, meaning "no
        // overage to state". The field's whole claim is "how far PAST its budget", so a value of 0.2 would be
        // answering a question nobody asked with a number that reads like a small overage.
        //
        // This is not hypothetical. Several checks legitimately grade against a threshold that is not the condition
        // they fired on — `Blueprint.TickEnabled` fires on the tick flags but grades by placement count,
        // `Mesh.MissingLods` fires at the Nanite floor but grades against the triangle budget — so a real finding
        // can sit well under the number it is graded by. Those keep their severity, which was never derived from
        // the ratio's magnitude, and simply make no overage claim.
        //
        // It also closes a sentinel collision: `Texture.MissingMipmaps` does not gate on readable dimensions, so a
        // texture whose size could not be read grades from zero. Zero-because-unmeasured and zero-because-not-over
        // are now the same statement — "this row makes no overage claim" — rather than one of them meaning it.
        if (InBudget > 0.0 && InValue > InBudget)
        { Graded.BudgetRatio = static_cast<float>(InValue / InBudget); }

        return Graded;
    }
}

// --------------------------------------------------------------------------------------------------------------------
