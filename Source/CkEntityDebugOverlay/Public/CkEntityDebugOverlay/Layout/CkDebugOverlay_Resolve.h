#pragma once
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugoverlay
{
    /**
     * Returns the set of field tags that are enabled for a given provider within a layout.
     *
     * Resolution rules (in priority order):
     *   1. Provider listed in Layout.EnabledProviders  → all DefaultEnabled fields are on.
     *   2. Provider has an Entry with no explicit EnabledFields → all DefaultEnabled fields are on.
     *   3. Provider has an Entry with explicit EnabledFields    → only those exact field tags are on.
     *   If the provider is not referenced at all, the result is empty.
     */
    CKENTITYDEBUGOVERLAY_API auto Resolve_EnabledFields(
        const FCk_DebugOverlay_Layout&           Layout,
        const FGameplayTag&                      ProviderTag,
        const TArray<FCk_DebugOverlay_FieldDesc>& AllFields) -> FGameplayTagContainer;

    /**
     * Returns the effective render style for a given provider.
     * Starts from Layout.DefaultStyle; if the provider's Entry has bOverrideStyle set,
     * the Entry's StyleOverride is returned instead.
     */
    CKENTITYDEBUGOVERLAY_API auto Resolve_Style(
        const FCk_DebugOverlay_Layout& Layout,
        const FGameplayTag&            ProviderTag) -> FCk_DebugOverlay_RenderStyle;

    /**
     * Validates that all provider tags referenced by the layout (EnabledProviders tags and
     * each Entries[].ProviderTag) are present in KnownProviderTags.
     * Returns one problem string per unknown tag containing the offending tag's string.
     * Never crashes — safe on a default-constructed Layout or empty KnownProviderTags.
     */
    CKENTITYDEBUGOVERLAY_API auto Validate_Layout(
        const FCk_DebugOverlay_Layout&   Layout,
        const FGameplayTagContainer&     KnownProviderTags) -> TArray<FString>;
}

// --------------------------------------------------------------------------------------------------------------------
