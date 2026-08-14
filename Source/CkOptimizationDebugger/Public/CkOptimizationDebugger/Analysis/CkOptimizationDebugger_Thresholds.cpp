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
}

// --------------------------------------------------------------------------------------------------------------------
