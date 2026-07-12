#pragma once
#include "GameplayTagContainer.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"

// --------------------------------------------------------------------------------------------------------------------

/** Describes one field that a provider can contribute. */
struct FCk_DebugOverlay_FieldDesc
{
    FGameplayTag Tag;
    bool         DefaultEnabled = true;
};

/** Per-entity runtime configuration passed into each provider call. */
struct FCk_DebugOverlay_ProviderConfig
{
    FGameplayTagContainer EnabledFields;
    FGameplayTagQuery     EntryFilter;
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Interface for all on-screen entity debug overlay providers.
 *
 * Implement this interface and register your provider via
 * CK_REGISTER_DEBUG_OVERLAY_PROVIDER(YourClass) in your .cpp file.
 */
class ICk_DebugOverlay_Provider
{
public:
    virtual ~ICk_DebugOverlay_Provider() = default;

    /** Stable gameplay tag identifying this provider (must be unique across all providers). */
    virtual auto Get_ProviderTag()  const -> FGameplayTag = 0;

    /** All field tags this provider can emit, with their default-enabled state. */
    virtual auto Get_FieldTags()    const -> TArray<FCk_DebugOverlay_FieldDesc> = 0;

    /** Lower values appear higher in the overlay. */
    virtual auto Get_SortPriority() const -> int32 = 0;

    /** If true, the provider honours FCk_DebugOverlay_ProviderConfig::EntryFilter. */
    virtual auto Get_SupportsEntryFilter() const -> bool { return false; }

    /**
     * If false, the aggregated card collects this provider from the FOCUS entity only —
     * never from lifetime-descendant sources. For providers whose payload is identity-like
     * (EntityInfo, Transform), every unnamed sub-entity would otherwise emit one
     * boilerplate section per tick.
     */
    virtual auto Get_CollectsFromSubSources() const -> bool { return true; }

    /** Return true if this provider has data to show for the given entity. */
    virtual auto CanProvide(const FCk_Handle& Entity) const -> bool = 0;

    /**
     * Populate Out with debug rows for Entity.
     * Only called when CanProvide returned true.
     */
    virtual auto Collect(
        const FCk_Handle&                  Entity,
        const FCk_DebugOverlay_ProviderConfig& Cfg,
        FCk_DebugOverlay_Section&          Out) -> void = 0;

    /**
     * Optional: return a short inline token for compact overlay views.
     * Default returns an empty string (provider is hidden in compact mode).
     */
    virtual auto Get_CompactToken(
        const FCk_Handle&                  Entity,
        const FCk_DebugOverlay_ProviderConfig& Cfg) const -> FString { return {}; }
};

// --------------------------------------------------------------------------------------------------------------------
