#include "CkPerfLab_HeatmapSlot.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

#include <Misc/ScopeLock.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_heatmap
{
    // The published snapshot and the selection are read by the EdMode and written by the window. Both are on the game
    // thread today, but the lock costs nothing at this frequency and removes the question entirely.
    FCriticalSection Guard;

    ck::perf_lab::heatmap::FCk_Snapshot Published;

    FString SelectedPositionId;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab::heatmap
{
    const FName k_EdModeId = TEXT("Ck.PerfLab.Heatmap");

    auto
        Publish(
            const FCk_Snapshot& InSnapshot)
        -> void
    {
        auto Lock = FScopeLock{&ck_perf_lab_heatmap::Guard};
        ck_perf_lab_heatmap::Published = InSnapshot;
    }

    auto
        Clear()
        -> void
    {
        auto Lock = FScopeLock{&ck_perf_lab_heatmap::Guard};
        ck_perf_lab_heatmap::Published = FCk_Snapshot{};
    }

    auto
        Get_Published()
        -> FCk_Snapshot
    {
        auto Lock = FScopeLock{&ck_perf_lab_heatmap::Guard};
        return ck_perf_lab_heatmap::Published;
    }

    auto
        Set_SelectedPositionId(
            const FString& InPositionId)
        -> void
    {
        auto Lock = FScopeLock{&ck_perf_lab_heatmap::Guard};
        ck_perf_lab_heatmap::SelectedPositionId = InPositionId;
    }

    auto
        Get_SelectedPositionId()
        -> FString
    {
        auto Lock = FScopeLock{&ck_perf_lab_heatmap::Guard};
        return ck_perf_lab_heatmap::SelectedPositionId;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_Snapshot(
            const FCk_PerfLab_Session& InSession,
            const FCk_PerfLab_Analysis& InAnalysis)
        -> FCk_Snapshot
    {
        auto Snapshot = FCk_Snapshot{};
        Snapshot._MapPath   = InSession.Get_Request().Get_MapPath();
        Snapshot._IsEnabled = true;
        Snapshot._Legend    = TEXT(
            "colour: cool = inside budget, hot = worst in this level   |   "
            "size: how far over budget   |   shape: minor / major / critical");

        const auto BudgetMs = InAnalysis.Get_BudgetMs();

        // A position whose frame time was never measured is not drawn at all. Drawing it in the "fast" colour would
        // claim a reading that does not exist, so this predicate gates the ramp and the markers alike.
        const auto Is_Measured = [](const FCk_PerfLab_PositionResult& InPosition)
        { return InPosition.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::Frame).Get_IsAvailable(); };

        const auto Get_AvgMs = [](const FCk_PerfLab_PositionResult& InPosition)
        { return static_cast<double>(InPosition.Get_Aggregate().Get_ByMetric(ECk_PerfLab_Metric::Frame).Get_AvgMs()); };

        // The worst average in THIS session sets the top of the colour ramp. Normalising against an absolute scale
        // would paint every level in a project the same colour; the score is the absolute statement, and the
        // heatmap's job is only to say where inside one level to look first.
        const auto MeasuredAverages =
            ck::algo::TransformIf<TArray<double>>(InSession.Get_Positions(), Is_Measured, Get_AvgMs);

        const auto WorstMs = ck::algo::Percentile(MeasuredAverages, 1.0).Get(0.0);

        Snapshot._Markers = ck::algo::TransformIf<TArray<FCk_Marker>>(InSession.Get_Positions(), Is_Measured,
            [&](const FCk_PerfLab_PositionResult& InPosition)
            {
                const auto AvgMs = Get_AvgMs(InPosition);

                auto Marker = FCk_Marker{};
                Marker._Location   = InPosition.Get_Location();
                Marker._PositionId = InPosition.Get_PositionId();
                Marker._AvgMs      = static_cast<float>(AvgMs);
                Marker._Normalised = WorstMs > 0.0
                    ? static_cast<float>(FMath::Clamp(AvgMs / WorstMs, 0.0, 1.0))
                    : 0.0f;
                Marker._OverBudgetRatio = BudgetMs > 0.0f ? static_cast<float>(AvgMs) / BudgetMs : 1.0f;

                // Severity comes from the findings actually published at this position, not from a second threshold
                // invented here — two places deciding "how bad" is two places for them to disagree.
                const auto Worst_Finding = ck::algo::FindIf(InAnalysis.Get_Findings(),
                    [&](const FCk_PerfLab_Finding& InFinding)
                    { return InFinding.Get_PositionId() == Marker._PositionId; });

                Marker._Severity = Worst_Finding.IsSet() ? Worst_Finding->Get_Severity() : ECk_PerfLab_Severity::Minor;

                return Marker;
            });

        return Snapshot;
    }
}

// --------------------------------------------------------------------------------------------------------------------
