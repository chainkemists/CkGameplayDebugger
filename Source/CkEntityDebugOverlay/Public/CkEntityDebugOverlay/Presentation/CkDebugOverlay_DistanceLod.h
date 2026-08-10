#pragma once

#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"

// ====================================================================================================================
// Distance LOD for the focus card — a PURE model->model trim applied before Slate, for the same
// reason the row budget is: capacity is never solved by hiding overflow. Everything the tier
// drops is counted back into an existing omission affordance (the section's "+N more", or the
// card-wide omission line via FCk_DebugOverlay_EntityModel::LodOmittedRowCount).
//
// Opt-in — UCk_DebugOverlay_Settings::bEnableFocusCardDistanceLod defaults to OFF, so the card
// renders identically at every distance until the user asks for the trim.
// ====================================================================================================================

// Camera-to-entity distances (cm) at which the focus card steps down a tier.
struct FCk_DebugOverlay_LodThresholds
{
    // At/after this distance the card drops to Summary.
    float SummaryDistance = 1200.0f;

    // At/after this distance the card drops to Pill.
    float PillDistance = 2400.0f;

    // Enter/exit gap applied to BOTH boundaries so a card sitting exactly on one does not
    // flip tier every frame as the camera breathes: coarsening needs Threshold + Hysteresis,
    // refining needs Threshold - Hysteresis. Clamped so the two adjusted boundaries can never
    // cross. Not a setting — it is a stability constant, not a tuning knob.
    float Hysteresis = 150.0f;
};

// How much survives each coarser tier.
struct FCk_DebugOverlay_LodTrim
{
    // Sections kept whole at the Summary tier, taken from the FRONT of the canonical
    // budget ordering (so protected AI / navigation providers are the ones that survive).
    int32 SummaryMaxSections = 3;
};

namespace ck_debugoverlay
{
    // Tier for InDistance, given the tier the card is CURRENTLY showing (the hysteresis
    // reference). Total: a negative / non-finite distance reads as Full (never coarsen on a
    // number we do not trust), thresholds are ordered, and the gap is clamped.
    auto Resolve_LodTier(
        float                                 InDistance,
        const FCk_DebugOverlay_LodThresholds& InThresholds,
        ECk_DebugOverlay_LodTier              InCurrentTier) -> ECk_DebugOverlay_LodTier;

    // Trim InModel to InTier and stamp the tier on it (the card reads it to lay the Pill tier
    // out as one flow line).
    //   Full    — returned untouched.
    //   Summary — canonical ordering, keep the first InTrim.SummaryMaxSections sections whole;
    //             the rest are dropped and their rows added to LodOmittedRowCount.
    //   Pill    — canonical ordering, every section keeps exactly its FIRST row, raised to the
    //             loudest severity in that section (a merge must never quieten the card), with
    //             the shed rows added to that section's OmittedRowCount.
    auto Apply_DistanceLod(
        const FCk_DebugOverlay_EntityModel& InModel,
        ECk_DebugOverlay_LodTier            InTier,
        const FCk_DebugOverlay_LodTrim&     InTrim = FCk_DebugOverlay_LodTrim{}) -> FCk_DebugOverlay_EntityModel;
}

// ====================================================================================================================
