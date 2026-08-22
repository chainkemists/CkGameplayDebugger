#pragma once

#include "CkPerfLab/Planner/CkPerfLab_Planner.h"

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    /**
     * Read a level into the plain-data survey the planner consumes.
     *
     * Everything world-facing lives here so that planning stays pure. What comes out is
     * location + cost proxies + object paths; no UObject pointers and no handles, because the survey
     * feeds a session file that is read by another process.
     *
     * Cost proxies are for RANKING candidate positions, never for reporting. They are cheap
     * approximations read off components; the numbers a user sees always come from measurement.
     */
    CKPERFLAB_API auto
    Build_WorldSurvey(
        const UWorld* InWorld,
        const FCk_PerfLab_ModeParams& InModeParams) -> FCk_PerfLab_WorldSurvey;

    /**
     * The census rows for one measured position: everything within radius, nearest first.
     * Filtered from the survey rather than re-walking the world, so a 25-position run reads the
     * level once instead of 25 times.
     */
    CKPERFLAB_API auto
    Build_CensusForPosition(
        const FCk_PerfLab_WorldSurvey& InSurvey,
        const FVector& InLocation,
        float InRadiusCm) -> TArray<FCk_PerfLab_ActorCensusRow>;
}

// --------------------------------------------------------------------------------------------------------------------
