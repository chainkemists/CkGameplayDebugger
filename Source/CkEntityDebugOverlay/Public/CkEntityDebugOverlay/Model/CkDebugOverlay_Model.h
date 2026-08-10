#pragma once
#include "GameplayTagContainer.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkDebugOverlay_Model.generated.h"

UENUM() enum class ECk_DebugOverlay_Severity : uint8 { Normal, Good, Warn, Bad };

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
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugoverlay
{
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
