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
    virtual auto GetCategoryName() const -> FName override { return TEXT("CkGameplayDebugger"); }

    // Inspector IDs whose entities should be hidden from the ECS entity tree.
    // An entity is hidden if ANY inspector in this set can inspect it.
    // IDs match FCkDebuggerInspectorRegistry entry IDs (e.g. "FCkInspector_SceneNode").
    UPROPERTY(Config, EditAnywhere, Category = "Entity Tree Filtering",
        meta = (DisplayName = "Excluded Inspector IDs",
            ToolTip = "Entities matched by any inspector in this set are hidden from the entity tree. Toggle per-session via the filter popover's Hide checkboxes; this list is the persistent default."))
    TSet<FName> DefaultExcludedInspectorIDs;

    static auto Get() -> const UCkEcsDebuggerSettings*
    {
        return GetDefault<UCkEcsDebuggerSettings>();
    }
};

// ====================================================================================================================
