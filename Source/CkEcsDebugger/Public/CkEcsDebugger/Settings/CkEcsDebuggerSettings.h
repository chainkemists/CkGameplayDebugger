#pragma once

#include "Engine/DeveloperSettings.h"

#include "CkEcsDebuggerSettings.generated.h"

// ====================================================================================================================
// Per-user (editor-local) settings for the ECS debugger. Holds the persistent
// "always hide these entity types from the tree" list keyed by inspector ID.
// Read via GetDefault<>; write via GetMutableDefault<> + SaveConfig().
// ====================================================================================================================

UCLASS(Config = EditorPerProjectUserSettings, DefaultConfig, meta = (DisplayName = "Ck ECS Debugger"))
class CKECSDEBUGGER_API UCkEcsDebuggerSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UCkEcsDebuggerSettings()
    {
        // Seed per the redesign spec §3.1. An ini that already serializes the set
        // overrides these. CueRelay is listed for intent but has no registered feature
        // flag (it is an actor, not a fragment-marked feature) — it only participates
        // once a marker exists.
        InternalFeatureIds = {
            TEXT("Timer"), TEXT("SceneNode"), TEXT("Probe"),
            TEXT("FloatAttribute"), TEXT("ByteAttribute"), TEXT("IntegerAttribute"),
            TEXT("CueRelay"),
        };
    }

    virtual auto GetCategoryName() const -> FName override { return TEXT("CkGameplayDebugger"); }

    // Feature ids whose single-feature entities classify as INTERNAL (fold/rollup
    // candidates, redesign spec §3.1): an entity is internal iff its own features are
    // exactly one of these plus at most Transform/Label. Ids match the debugger-wide
    // feature-flag ids (ck::ecs_debugger_feature_flags::RegisterAll).
    UPROPERTY(Config, EditAnywhere, Category = "Entity Tree Classification",
        meta = (DisplayName = "Internal Feature Ids",
            ToolTip = "Entities whose only non-structural feature is one of these are classified INTERNAL — foldable under their owner with a rolled-up badge count. Extend with additional feature-flag ids to fold more entity types."))
    TSet<FName> InternalFeatureIds;

    // Substring tokens whose entities should be hidden from the ECS entity tree.
    // Each token matches PARTIALLY against (a) inspector IDs — "Transform"
    // matches "FCkInspector_Transform", hiding entities that inspector can
    // inspect — and (b) entity debug names — "Ck_CueRelay" hides
    // "Ck_CueRelay_UE_3".
    UPROPERTY(Config, EditAnywhere, Category = "Entity Tree Filtering",
        meta = (DisplayName = "Excluded Inspector IDs",
            ToolTip = "Entities matching any token in this set are hidden from the entity tree. Tokens are substrings, matched against inspector IDs (entities that inspector can inspect) AND entity debug names. Toggle per-session via the filter popover's Hide checkboxes; this list is the persistent default."))
    TSet<FName> DefaultExcludedInspectorIDs;

    static auto Get() -> const UCkEcsDebuggerSettings*
    {
        return GetDefault<UCkEcsDebuggerSettings>();
    }
};

// ====================================================================================================================
