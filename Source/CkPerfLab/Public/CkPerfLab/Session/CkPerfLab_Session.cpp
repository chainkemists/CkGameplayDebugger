#include "CkPerfLab_Session.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_PerfLab_ModeParams::
    Get_ForMode(
        ECk_PerfLab_Mode InMode)
    -> FCk_PerfLab_ModeParams
{
    auto Params = FCk_PerfLab_ModeParams{};

    switch (InMode)
    {
        case ECk_PerfLab_Mode::Quick:
        {
            // One direction, no adaptive pass: answers "does this level have a problem, and roughly
            // where" and nothing more.
            Params.Set_PositionBudget(12);
            Params.Set_DirectionsPerPosition(1);
            Params.Set_Repeats(1);
            Params.Set_AdaptiveRevisit(false);
            break;
        }
        case ECk_PerfLab_Mode::Standard:
        {
            // Survey from four directions, then spend the remaining budget only where the survey
            // justifies it. The defaults on the struct already describe this mode.
            break;
        }
        case ECk_PerfLab_Mode::Deep:
        {
            // Sixteen directions and repeated measurements. The adaptive pass is off because Deep
            // already measures everything exhaustively — there is nothing left to escalate to.
            Params.Set_DirectionsPerPosition(16);
            Params.Set_Repeats(3);
            Params.Set_AdaptiveRevisit(false);
            break;
        }
    }

    return Params;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_PerfLab_MetricSet::
    Get_ByMetric(
        ECk_PerfLab_Metric InMetric) const
    -> const FCk_PerfLab_MetricStats&
{
    switch (InMetric)
    {
        case ECk_PerfLab_Metric::Frame:        return _Frame;
        case ECk_PerfLab_Metric::GameThread:   return _GameThread;
        case ECk_PerfLab_Metric::RenderThread: return _RenderThread;
        case ECk_PerfLab_Metric::RhiThread:    return _RhiThread;
        case ECk_PerfLab_Metric::Gpu:          return _Gpu;
    }

    return _Frame;
}

// --------------------------------------------------------------------------------------------------------------------
