#pragma once
#include "GameplayTagContainer.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkDebugOverlay_Model.generated.h"

UENUM() enum class ECk_DebugOverlay_Severity : uint8 { Normal, Good, Warn, Bad };

/**
 * Focus-card detail tier, chosen from the camera distance to the focused entity.
 *
 * The tier is resolved (with hysteresis) and APPLIED to the model before Slate — the card
 * never hides rows it was handed. Opt-in: OFF unless
 * UCk_DebugOverlay_Settings::bEnableFocusCardDistanceLod is set, in which case every card is
 * Full and the pipeline is exactly the shipped one.
 */
UENUM()
enum class ECk_DebugOverlay_LodTier : uint8
{
    // Every collected section and row reaches the focus-card budget (shipped behavior).
    Full,

    // Header + the top budget-ordered sections only. Sections beyond the cap are dropped and
    // their rows counted into FCk_DebugOverlay_EntityModel::LodOmittedRowCount, which the card
    // renders as its card-wide omission line.
    Summary,

    // Header + ONE severity-carrying token per provider, all on a single flow line. The rows
    // each section shed are counted into its own OmittedRowCount ("+N more").
    Pill,
};

/**
 * How a provider's rows may be collapsed by the pre-budget merge pass.
 *
 * Subtree aggregation emits one section per (provider x source entity), so "duplicate"
 * means two different things depending on the provider: genuine redundancy inside one
 * source, versus two sub-entities that happen to read the same. Each provider declares
 * which it is via ICk_DebugOverlay_Provider::Get_MergeBehavior().
 */
UENUM()
enum class ECk_DebugOverlay_MergeBehavior : uint8
{
    // Default. Duplicate (FieldTag, Value) rows collapse only INSIDE one source's section.
    // Rows on different sources are distinct facts and all render, each under its own chip.
    MergeWithinSource,

    // The value is subtree-wide, so identical rows on different sources are true noise:
    // they collapse into a single xN row (Label, Team).
    MergeAcrossSources,

    // The provider's FIRST section — nearest source, hence the NPC's primary one — stays full;
    // every section after it collapses into ONE section holding a summary row per source
    // (Goap, StateMachine).
    CondensePerSource,
};

USTRUCT()
struct FCk_DebugOverlay_Row
{
    GENERATED_BODY()
    UPROPERTY() FGameplayTag FieldTag;
    UPROPERTY() FText        Value;
    UPROPERTY() ECk_DebugOverlay_Severity Severity = ECk_DebugOverlay_Severity::Normal;
    UPROPERTY() TArray<FText> ExplicitHistory;

    // How many identical rows (same FieldTag + Value, same provider) this row stands for
    // after the pre-budget merge pass. 1 = never merged. The card renders a muted "xN"
    // suffix when > 1 instead of repeating the row once per lifetime descendant.
    UPROPERTY() int32 MergedCount = 1;
};

USTRUCT()
struct FCk_DebugOverlay_Section
{
    GENERATED_BODY()
    UPROPERTY() FGameplayTag ProviderTag;
    UPROPERTY() int32        SortPriority = 0;
    UPROPERTY() TArray<FCk_DebugOverlay_Row> Rows;

    // Subtree aggregation: which entity this section was collected from. SourceName is
    // EMPTY when it is the focus entity itself (only sub-entity sections get the dim
    // source chip). SourceEntityId buckets history — the same field on two sub-entities
    // must not collide. SourceOrder is the stable tie-break for the card's priority sort
    // (focus first, then discovery order) — an unstable sort over equal priorities
    // reorders sections every tick.
    UPROPERTY() FText  SourceName;
    UPROPERTY() uint32 SourceEntityId = 0;
    UPROPERTY() int32  SourceOrder = 0;

    // Value-only position of this source in the focused entity's lifetime
    // subtree. The root/focus source has ParentSourceEntityId = 0 and
    // SourceDepth = 0; descendants name the exact source that discovered them.
    // This intentionally does not retain a second FCk_Handle.
    UPROPERTY() uint32 ParentSourceEntityId = 0;
    UPROPERTY() int32  SourceDepth = 0;
    // Rows removed by the pre-render focus-card budget.  Slate renders this
    // explicitly instead of leaving the user to infer that clipping hid data.
    UPROPERTY() int32  OmittedRowCount = 0;

    // Stamped from the owning provider at collection time so the merge pass stays a pure
    // model->model function (it never sees a provider pointer). Specs hand-set it.
    UPROPERTY() ECk_DebugOverlay_MergeBehavior MergeBehavior = ECk_DebugOverlay_MergeBehavior::MergeWithinSource;
};

USTRUCT()
struct FCk_DebugOverlay_EntityModel
{
    GENERATED_BODY()
    FCk_Handle   Entity;
    UPROPERTY() FText Header;
    UPROPERTY() TArray<FCk_DebugOverlay_Section> Sections;

    // Detail tier this model was trimmed to. Full = untouched by the distance-LOD pass.
    UPROPERTY() ECk_DebugOverlay_LodTier LodTier = ECk_DebugOverlay_LodTier::Full;

    // Rows the distance-LOD pass removed by dropping WHOLE sections (Summary tier). Rows it
    // trimmed inside a surviving section live on that section's OmittedRowCount instead. The
    // card adds this to its budget omission summary so an LOD trim is never silent.
    UPROPERTY() int32 LodOmittedRowCount = 0;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugoverlay
{
    // Value-only topology record used while collecting the focus entity's
    // lifetime subtree and retained on each provider/source section.
    struct FCk_DebugOverlay_SourceTopology
    {
        uint32 EntityId             = 0;
        uint32 ParentSourceEntityId = 0;
        int32  SourceDepth          = 0;
    };

    // Append unvisited lifetime dependents in their caller-provided order.
    // The caller owns handle lookup; this helper intentionally accepts IDs only
    // so topology tests need no world or ECS registry.
    inline auto Append_SourceTopology(
        uint32                         InParentEntityId,
        int32                          InParentDepth,
        const TArray<uint32>&          InDependentEntityIds,
        TSet<uint32>&                  InOutVisitedEntityIds,
        TArray<FCk_DebugOverlay_SourceTopology>& InOutTopology)
        -> TArray<uint32>
    {
        auto Added = TArray<uint32>{};
        for (const auto DependentId : InDependentEntityIds)
        {
            if (DependentId == 0 || InOutVisitedEntityIds.Contains(DependentId))
            { continue; }

            InOutVisitedEntityIds.Add(DependentId);
            InOutTopology.Add({ DependentId, InParentEntityId, InParentDepth + 1 });
            Added.Add(DependentId);
        }
        return Added;
    }

    // Pure breadth-first topology seam. Production collection uses the append
    // helper above while resolving IDs back to the current lifetime handles.
    inline auto Build_SourceTopology(
        uint32                                InRootEntityId,
        const TMap<uint32, TArray<uint32>>&   InDependentsByEntityId)
        -> TArray<FCk_DebugOverlay_SourceTopology>
    {
        auto Result = TArray<FCk_DebugOverlay_SourceTopology>{};
        if (InRootEntityId == 0)
        { return Result; }

        Result.Add({ InRootEntityId, 0, 0 });
        auto Visited = TSet<uint32>{ InRootEntityId };
        for (auto Index = 0; Index < Result.Num(); ++Index)
        {
            const auto& Source = Result[Index];
            const auto* Dependents = InDependentsByEntityId.Find(Source.EntityId);
            if (Dependents == nullptr)
            { continue; }

            Append_SourceTopology(
                Source.EntityId, Source.SourceDepth, *Dependents, Visited, Result);
        }
        return Result;
    }

    // Loudest of the two (Normal < Good < Warn < Bad). Whenever several rows collapse into
    // one, the survivor keeps the strongest signal — a Bad row must never be silenced by a
    // Normal duplicate that happened to be seen first.
    inline auto Get_MaxSeverity(
        ECk_DebugOverlay_Severity InA,
        ECk_DebugOverlay_Severity InB)
        -> ECk_DebugOverlay_Severity
    {
        return static_cast<uint8>(InA) >= static_cast<uint8>(InB) ? InA : InB;
    }

    // Loudest severity across a row set (Normal when empty).
    inline auto Get_MaxSeverity(
        const TArray<FCk_DebugOverlay_Row>& InRows)
        -> ECk_DebugOverlay_Severity
    {
        auto Severity = ECk_DebugOverlay_Severity::Normal;
        for (const auto& Row : InRows)
        { Severity = Get_MaxSeverity(Severity, Row.Severity); }
        return Severity;
    }
}
