#pragma once

#include <CoreMinimal.h>

// --------------------------------------------------------------------------------------------------------------------
// Entity classification + HAS rollup (redesign spec §3.1/§3.2).
//
// Pure logic over the CkEcs debug feature-flag bits — no registry access, no Slate.
// The tree (Phase 2) builds the per-entity bit/owner arrays from its cached entities
// and feeds them here; specs feed synthetic forests.
//
// An entity is INTERNAL iff its own feature signature is minimal — own bits ⊆
// { X, Transform, Label } for exactly one non-structural feature X — and X is in the
// settings-driven internal-feature set (UCkEcsDebuggerSettings::InternalFeatureIds).
// Everything else is PRIMARY.
//
// Known blind spots (tracked in PROGRESS.md): features without a registered flag are
// invisible to the signature, and CueRelay entities carry no marker fragment — both
// make an entity look MORE minimal than it is. The internal set is data, so
// misclassifications are user-fixable without a plugin rebuild.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::ecs_debugger_classification
{
    struct FClassificationTable
    {
        // Transform + Label — ubiquitous carriers that never disqualify an entity from
        // being internal (spec §3.1).
        uint64 StructuralMask = 0;

        // OR of the bits of every internal-set feature id that has a registered flag.
        uint64 InternalMask = 0;

        // Bit index → feature id, for naming rollup counts (NAME_None = unregistered bit).
        TArray<FName> BitToFeatureId;
    };

    // Snapshot the flag registry + internal-feature set. Build once per tree rebuild —
    // flag registration is startup-only, but the internal set is user-editable data.
    CKECSDEBUGGER_API auto BuildTable(
        const TSet<FName>& InInternalFeatureIds) -> FClassificationTable;

    struct FClassification
    {
        bool IsInternal = false;
        FName InternalFeatureId;   // the single X when internal, NAME_None otherwise
    };

    CKECSDEBUGGER_API auto Classify(
        uint64 InOwnBits,
        const FClassificationTable& InTable) -> FClassification;

    // HAS rollup (spec §3.2): every internal entity contributes its X to its NEAREST
    // PRIMARY ancestor — the walk passes through internal ancestors and stops at the
    // first primary, so a Station never claims its (primary) Display child's timers.
    //
    // InOwnerIndex holds each entity's lifetime-owner index, INDEX_NONE for roots.
    // Returns one count-map per entity (empty for internals and rollup-less primaries).
    // Owner cycles contribute nothing instead of hanging (defensive — the lifetime
    // graph is a forest, but restored snapshots have historically produced cycles).
    CKECSDEBUGGER_API auto ComputeRollups(
        const TArray<uint64>& InOwnBits,
        const TArray<int32>& InOwnerIndex,
        const FClassificationTable& InTable) -> TArray<TMap<FName, int32>>;

    // Depth transparency (design 2026-08-09): rewrite a LITERAL owner-index array so every
    // owner flagged transparent is looked THROUGH — each entry becomes its nearest
    // non-transparent owner, INDEX_NONE when the walk runs off the top. Feeding the result
    // to ComputeRollups makes a relay child's effective owner the relay's own owner, so
    // internals beneath a relay child roll up to that child instead of vanishing behind the
    // relay. Pure: the tree supplies the flags from the shared relay predicate, specs supply
    // synthetic ones. Cycles through transparent owners resolve to INDEX_NONE, never hang.
    CKECSDEBUGGER_API auto Apply_TransparentOwnerPassThrough(
        const TArray<int32>& InOwnerIndex,
        const TArray<bool>& InIsTransparentOwner) -> TArray<int32>;
}
