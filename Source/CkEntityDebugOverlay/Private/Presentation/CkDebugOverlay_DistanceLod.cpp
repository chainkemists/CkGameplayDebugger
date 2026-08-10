#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_DistanceLod.h"

#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_FocusCardBudget.h"   // Sort_SectionsForBudget

// ====================================================================================================================

auto
    ck_debugoverlay::
    Resolve_LodTier(
        float                                 InDistance,
        const FCk_DebugOverlay_LodThresholds& InThresholds,
        ECk_DebugOverlay_LodTier              InCurrentTier)
    -> ECk_DebugOverlay_LodTier
{
    // No trustworthy distance (no viewpoint yet, NaN from a degenerate transform) — show
    // everything rather than trim on a number we cannot defend.
    if (NOT FMath::IsFinite(InDistance) || InDistance < 0.0f)
    { return ECk_DebugOverlay_LodTier::Full; }

    const auto SummaryDist = FMath::Max(0.0f, InThresholds.SummaryDistance);
    const auto PillDist    = FMath::Max(SummaryDist, InThresholds.PillDistance);

    // Half the band between the thresholds is the widest gap that keeps the adjusted
    // boundaries ordered; the near side is additionally capped by SummaryDist so the Full
    // boundary never goes negative.
    const auto Gap = FMath::Clamp(
        InThresholds.Hysteresis,
        0.0f,
        FMath::Min(SummaryDist, (PillDist - SummaryDist) * 0.5f));

    // Coarsening pushes the boundary OUT, refining pulls it IN — so the tier only changes
    // once the camera has moved a real distance past the threshold it last crossed.
    const auto SummaryEdge = SummaryDist +
        (InCurrentTier == ECk_DebugOverlay_LodTier::Full ? Gap : -Gap);
    const auto PillEdge = PillDist +
        (InCurrentTier == ECk_DebugOverlay_LodTier::Pill ? -Gap : Gap);

    if (InDistance >= PillEdge)
    { return ECk_DebugOverlay_LodTier::Pill; }

    if (InDistance >= SummaryEdge)
    { return ECk_DebugOverlay_LodTier::Summary; }

    return ECk_DebugOverlay_LodTier::Full;
}

// ====================================================================================================================

auto
    ck_debugoverlay::
    Apply_DistanceLod(
        const FCk_DebugOverlay_EntityModel& InModel,
        ECk_DebugOverlay_LodTier            InTier,
        const FCk_DebugOverlay_LodTrim&     InTrim)
    -> FCk_DebugOverlay_EntityModel
{
    auto Result    = InModel;
    Result.LodTier = InTier;

    if (InTier == ECk_DebugOverlay_LodTier::Full)
    { return Result; }

    // Same ordering the budget rations against, so the sections a tier keeps are exactly the
    // ones the budget would have protected (GOAP / Crowd / PathNetwork / AStar first, generic
    // attribute floods last).
    Sort_SectionsForBudget(Result.Sections);

    if (InTier == ECk_DebugOverlay_LodTier::Summary)
    {
        const auto KeptSections = FMath::Max(0, InTrim.SummaryMaxSections);

        for (auto Idx = KeptSections; Idx < Result.Sections.Num(); ++Idx)
        {
            // Whole section leaves the model — its rows are surfaced by the card's card-wide
            // omission line instead of a per-section "+N more" that would have nowhere to live.
            Result.LodOmittedRowCount += Result.Sections[Idx].Rows.Num();
            Result.LodOmittedRowCount += Result.Sections[Idx].OmittedRowCount;
        }

        if (Result.Sections.Num() > KeptSections)
        { Result.Sections.SetNum(KeptSections); }

        return Result;
    }

    // ---- Pill: one token per provider ----
    for (auto& Section : Result.Sections)
    {
        if (Section.Rows.Num() <= 1)
        { continue; }

        // The survivor stands for the whole section, so it keeps the loudest signal — same
        // rule the duplicate-merge pass follows.
        const auto Loudest = ck_debugoverlay::Get_MaxSeverity(Section.Rows);

        Section.OmittedRowCount += Section.Rows.Num() - 1;
        Section.Rows.SetNum(1);
        Section.Rows[0].Severity = Loudest;
    }

    return Result;
}

// ====================================================================================================================
