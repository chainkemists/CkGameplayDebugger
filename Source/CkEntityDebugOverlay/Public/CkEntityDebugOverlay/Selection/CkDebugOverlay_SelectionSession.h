#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"

struct FCk_DebugOverlay_SelectionConfig;

namespace ck_debugoverlay::selection_session
{
    inline constexpr uint32 InvalidEntityId = MAX_uint32;

    /** A value-only snapshot of one live entity. Entity number zero is valid. */
    struct FNode
    {
        uint32    Id                        = InvalidEntityId;
        uint32    OwnerId                   = InvalidEntityId;
        bool      MeaningfulBoundary         = false;
        bool      TransparentInfrastructure  = false;
        bool      Selectable                 = true;
        bool      HasPosition                = false;
        FVector   Position                   = FVector::ZeroVector;
        bool      IsOnScreen                 = false;
        FVector2D ScreenPos                  = FVector2D::ZeroVector;
        bool      Occluded                   = false;
    };

    struct FViewpoint
    {
        FVector Location = FVector::ZeroVector;
        FVector Forward  = FVector::ForwardVector;
    };

    /** A root candidate with the spatial member that promoted it. */
    struct FCandidateSelection
    {
        uint32    Id             = InvalidEntityId;
        uint32    RootId         = InvalidEntityId;
        uint32    AnchorId       = InvalidEntityId;
        FVector   WorldLocation  = FVector::ZeroVector;
        FVector2D ScreenPos      = FVector2D::ZeroVector;
        float     Score          = 0.0f;
        float     Distance       = 0.0f;
        float     AngleRadians   = 0.0f;
        bool      IsOnScreen     = false;
    };

    /**
     * Resolve a candidate's presentation root. Returns InvalidEntityId for an invalid,
     * absent, cyclic, or transparent-only lineage. In MeaningfulRoots, the nearest explicit
     * boundary wins; otherwise the top-most non-transparent ancestor wins.
     */
    CKENTITYDEBUGOVERLAY_API auto ResolveRoot(
        const TArray<FNode>&                    InNodes,
        uint32                                  InId,
        const FCk_DebugOverlay_SelectionConfig& InConfig) -> uint32;

    /** Build one deterministic, freshly ranked world snapshot. */
    CKENTITYDEBUGOVERLAY_API auto BuildCandidates(
        const TArray<FNode>&                    InNodes,
        const FViewpoint&                       InViewpoint,
        const FCk_DebugOverlay_SelectionConfig& InConfig) -> TArray<FCandidateSelection>;

    /**
     * Build the continuously refreshed range membership. Discovery deliberately ignores the
     * aim cone and screen scope: those only decide the current aim winner, never membership.
     */
    CKENTITYDEBUGOVERLAY_API auto BuildDiscoveryCandidates(
        const TArray<FNode>&                    InNodes,
        const FViewpoint&                       InViewpoint,
        const FCk_DebugOverlay_SelectionConfig& InConfig) -> TArray<FCandidateSelection>;

    /**
     * Pick the current aim winner. World candidates already obey configured targeting bounds;
     * family inspection keeps its range/occlusion freedom but still applies visible cone aim.
     */
    CKENTITYDEBUGOVERLAY_API auto PickAimCandidate(
        const TArray<FCandidateSelection>&      InCandidates,
        const FCk_DebugOverlay_SelectionConfig& InConfig,
        bool                                     InFamily) -> uint32;

    /** Build an isolated selected-family snapshot, excluding nested explicit root families. */
    CKENTITYDEBUGOVERLAY_API auto BuildFamily(
        const TArray<FNode>&                    InNodes,
        uint32                                  InRootId,
        const FViewpoint&                       InViewpoint,
        const FCk_DebugOverlay_SelectionConfig& InConfig) -> TArray<FCandidateSelection>;

    /**
     * Return the next valid ordered-snapshot ID. If selection is a child hidden by root
     * grouping, InSelectedRootId re-enters at its root. Returns InvalidEntityId when none live.
     */
    CKENTITYDEBUGOVERLAY_API auto Cycle(
        const TArray<uint32>& InOrderedIds,
        const TSet<uint32>&   InValidIds,
        uint32                InSelectedId,
        uint32                InSelectedRootId,
        int32                 InDirection) -> uint32;

    /**
     * Reconcile continuously discovered membership with the numbered order. Stable mode keeps
     * surviving previous IDs before newly discovered current IDs; rerank mode adopts current.
     */
    CKENTITYDEBUGOVERLAY_API auto ReconcileOrder(
        const TArray<uint32>& InPreviousIds,
        const TArray<uint32>& InCurrentIds,
        bool                  InRerank) -> TArray<uint32>;

    /** Shortest signed cyclic distance from the selected entity (or its displayed root).
     * Zero marks the current selection; an exact opposite uses +/- (the same number of steps
     * either way). With no selection, offsets match Cycle's first/last entry behavior. */
    CKENTITYDEBUGOVERLAY_API auto BuildRelativeLabels(
        const TArray<uint32>& InOrderedIds,
        uint32                InSelectedId,
        uint32                InSelectedRootId) -> TMap<uint32, FString>;

    /**
     * Preserve an explicitly cycled target only while the aim winner has not changed. This is
     * selection continuity: a changed or invalid aim returns the valid new winner. A supplied
     * lock ID must already have passed full-handle lifetime validation and wins even out of range.
     */
    CKENTITYDEBUGOVERLAY_API auto ResolveAimFocus(
        uint32              InAimId,
        uint32              InPreviousAimId,
        uint32              InManualId,
        const TSet<uint32>& InValidIds,
        uint32              InValidatedLockId = InvalidEntityId) -> uint32;
}
