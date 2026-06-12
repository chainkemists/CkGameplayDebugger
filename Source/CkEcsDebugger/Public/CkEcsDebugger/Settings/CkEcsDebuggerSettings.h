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
