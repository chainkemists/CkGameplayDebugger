#pragma once

#include "Engine/DeveloperSettings.h"

#include "CkEcsDebuggerSettings.generated.h"

// ====================================================================================================================
// Per-user settings for the ECS debugger. Holds the persistent
// "always hide these entity types from the tree" list keyed by inspector ID.
// Read via GetDefault<>; write via GetMutableDefault<> + SaveConfig().
// ====================================================================================================================

UCLASS(Config = GameUserSettings, meta = (DisplayName = "Ck ECS Debugger"))
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

    virtual auto
    GetContainerName() const -> FName override
    {
        return TEXT("Editor");
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

    // Fold single-feature internal entities (timers, scene nodes, attribute entries …)
    // under their owner, showing a per-owner "⊞ N" chip instead of N rows. Toggleable
    // live from the entity-list toolbar; this is the persisted default.
    UPROPERTY(Config, EditAnywhere, Category = "Entity Tree Presentation",
        meta = (DisplayName = "Fold Internal Entities"))
    bool FoldInternalEntities = true;

    // Coalesce runs of same-archetype siblings into a single "Name ×N" group row once
    // they exceed the threshold. Toggleable live from the entity-list toolbar.
    UPROPERTY(Config, EditAnywhere, Category = "Entity Tree Presentation",
        meta = (DisplayName = "Group Siblings By Archetype"))
    bool GroupSiblingsByArchetype = true;

    UPROPERTY(Config, EditAnywhere, Category = "Entity Tree Presentation",
        meta = (DisplayName = "Sibling Group Threshold", ClampMin = 2))
    int32 SiblingGroupThreshold = 5;

    // Archetypes-page card grid density. Adjustable live from the page header chips;
    // this is the persisted value.
    UPROPERTY(Config, EditAnywhere, Category = "Archetypes Page",
        meta = (DisplayName = "Archetype Grid Columns", ClampMin = 1, ClampMax = 8))
    int32 ArchetypeGridColumns = 4;

    // Viewport picker: hide the diamond billboards of entities that are pickable by
    // their rendered geometry (ISM-instance- or actor-backed), leaving diamonds only
    // on meshless entities. Everything stays pickable. Toggleable live from the
    // picker toolbar; this is the persisted default.
    UPROPERTY(Config, EditAnywhere, Category = "Viewport Picker",
        meta = (DisplayName = "Meshes First (hide diamonds on mesh-backed entities)"))
    bool PickerMeshesFirst = false;

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
