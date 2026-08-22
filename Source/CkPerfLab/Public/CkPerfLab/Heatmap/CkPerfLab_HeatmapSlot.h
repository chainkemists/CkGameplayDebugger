#pragma once

#include "CkPerfLab/Analysis/CkPerfLab_Analysis.h"

// --------------------------------------------------------------------------------------------------------------------
// The seam between the page that knows what was measured and the EdMode that draws it.
//
// A slot rather than a direct call, for the same reason the save visualizer uses one: the EdMode is stateless and
// runs on the editor's schedule, not the window's. It pulls an immutable snapshot whenever it draws and pushes a
// click back the same way, so neither side holds a reference to the other and neither can outlive the other.
//
// Carries NORMALISED values and no colours. What a marker looks like is the EdMode's business; what it measured is
// this module's, and keeping the two apart means a restyle never touches the analysis.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab::heatmap
{
    /** One measured position, reduced to what it takes to draw and identify it. */
    struct CKPERFLAB_API FCk_Marker
    {
        FVector _Location = FVector::ZeroVector;

        /** Where this position sits between "at budget" and "the worst in the level", in [0, 1]. */
        float _Normalised = 0.0f;

        /** Over-budget multiple, which the marker's size reads. 1.0 is exactly at budget. */
        float _OverBudgetRatio = 1.0f;

        ECk_PerfLab_Severity _Severity = ECk_PerfLab_Severity::Minor;

        FString _PositionId;

        /** Shown on the marker so a reader never has to hover to learn what it is worth. */
        float _AvgMs = 0.0f;
    };

    // ----------------------------------------------------------------------------------------------------------------

    /**
     * Everything the EdMode needs for one draw.
     *
     * The map path is carried because a session measured elsewhere must NOT be drawn over the level currently open:
     * the marker positions would be meaningless and would look exactly as authoritative as real ones.
     */
    struct CKPERFLAB_API FCk_Snapshot
    {
        TArray<FCk_Marker> _Markers;

        FString _MapPath;

        /** What colour, shape and size each mean. A lens without a legend is a picture nobody can read. */
        FString _Legend;

        bool _IsEnabled = false;
    };

    // ----------------------------------------------------------------------------------------------------------------

    /** Replace what the EdMode draws. Thread-safe enough for the game thread, which is the only caller. */
    CKPERFLAB_API auto
    Publish(
        const FCk_Snapshot& InSnapshot) -> void;

    /** Stop drawing, without disturbing anything else. */
    CKPERFLAB_API auto
    Clear() -> void;

    CKPERFLAB_API auto
    Get_Published() -> FCk_Snapshot;

    /** Pushed back by a viewport click so the page can select the row the reader clicked. */
    CKPERFLAB_API auto
    Set_SelectedPositionId(
        const FString& InPositionId) -> void;

    CKPERFLAB_API auto
    Get_SelectedPositionId() -> FString;

    // ----------------------------------------------------------------------------------------------------------------

    /**
     * Build a snapshot from an analysed session.
     *
     * Normalisation is against the WORST position in this session rather than an absolute scale, because the score
     * is already the absolute statement and the heatmap's job is to say where within one level to look first.
     */
    CKPERFLAB_API auto
    Build_Snapshot(
        const FCk_PerfLab_Session& InSession,
        const FCk_PerfLab_Analysis& InAnalysis) -> FCk_Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
