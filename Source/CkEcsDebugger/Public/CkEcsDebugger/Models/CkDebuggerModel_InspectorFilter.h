#pragma once

#include "CoreMinimal.h"

#include "CkEcs/Handle/CkHandle.h"

// --------------------------------------------------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE(FCkDebugger_OnInspectorFilterChanged);

// --------------------------------------------------------------------------------------------------------------------

UENUM()
enum class ECk_InspectorFilter_MatchMode : uint8
{
    /** AND — entity must pass every selected inspector. */
    All,
    /** OR — entity must pass at least one selected inspector. */
    Any,
};

UENUM()
enum class ECk_InspectorFilter_BadgeStyle : uint8
{
    ColorOnly,
    TextOnly,
    ColorAndText,
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * State + policy for the debugger's "filter entities by inspector type" navigation aid.
 *
 * Held as a peer to FCkDebuggerModel_EntitySelection / WorldContext / ViewportPicker by
 * SCkDebuggerWindow_Main. Pure visual filter — entities that don't match are dimmed in
 * the entity tree and the graph view but remain fully selectable.
 *
 * Construction snapshots the inspector list via FCkDebuggerInspectorRegistry::Get_AllMetadata()
 * and assigns each entry a badge color via the curated map (with hash fallback for the tail).
 * No live inspector instances are pinned; Test_Entity instantiates fresh inspectors per call
 * via the registry's Test() helper to keep lifetime semantics simple and safe.
 */
class FCkDebuggerModel_InspectorFilter
{
public:
    struct FInspectorEntry
    {
        FName        ID;
        FText        DisplayName;
        FLinearColor BadgeColor = FLinearColor::White;

        /**
         * Bit index in the CkEcs debug flag cache when the inspector declared a
         * parity-verified Get_FeatureFlagId (see CkDebuggerInspector_Base.h). INDEX_NONE
         * = no bit; Test_* falls back to instantiating the inspector via the registry.
         */
        FName FeatureFlagId;
        int32 FeatureFlagBit = INDEX_NONE;
    };

    FCkDebuggerModel_InspectorFilter();
    ~FCkDebuggerModel_InspectorFilter() = default;

    // ---- Inspector list --------------------------------------------------

    auto
    Get_AllEntries() const -> const TArray<FInspectorEntry>&;

    auto
    Find_Entry(
        FName InID) const -> const FInspectorEntry*;

    // ---- Selection -------------------------------------------------------

    auto
    Get_SelectedIDs() const -> const TSet<FName>&;

    auto
    Get_HasActiveFilter() const -> bool;

    auto
    Get_NumSelected() const -> int32;

    auto
    Get_IsSelected(
        FName InID) const -> bool;

    auto
    Toggle_Selection(
        FName InID) -> void;

    auto
    Set_Selection(
        FName InID,
        bool  InSelected) -> void;

    auto
    Clear_Selection() -> void;

    // ---- Match mode ------------------------------------------------------

    auto
    Get_MatchMode() const -> ECk_InspectorFilter_MatchMode;

    auto
    Set_MatchMode(
        ECk_InspectorFilter_MatchMode InMode) -> void;

    // ---- Badge style -----------------------------------------------------

    auto
    Get_BadgeStyle() const -> ECk_InspectorFilter_BadgeStyle;

    auto
    Set_BadgeStyle(
        ECk_InspectorFilter_BadgeStyle InStyle) -> void;

    // ---- Exclusion (persistent hide list) --------------------------------

    /**
     * Separate from the include-selection above. Entities matched by ANY excluded inspector
     * are fully hidden from the entity tree (IsVisible=false), not just dimmed. Persisted
     * via UCkEcsDebuggerSettings so the list survives editor restarts.
     */

    auto
    Get_ExcludedIDs() const -> const TSet<FName>&;

    auto
    Get_HasActiveExclusion() const -> bool;

    auto
    Get_IsExcluded(
        FName InID) const -> bool;

    auto
    Toggle_Exclusion(
        FName InID) -> void;

    auto
    Set_Exclusion(
        FName InID,
        bool  InExcluded) -> void;

    auto
    Clear_Exclusions() -> void;

    /**
     * Reload the persistent exclusion list from project settings. Lets edits made directly in
     * Project Settings (Ck ECS Debugger → Excluded Inspector IDs) take effect at runtime without
     * reopening the debugger — the constructor's one-shot load can't see post-open edits, and
     * name-pattern tokens (e.g. "CueRelay") can ONLY be added via Project Settings (the popover
     * toggles registered inspector IDs only). Cheap; safe to call each refresh. No broadcast —
     * the caller re-applies the filter immediately after. Returns true when the set actually
     * changed, so a periodic caller can re-filter only when needed.
     */
    auto
    Sync_ExclusionsFromSettings() -> bool;

    // ---- Predicate -------------------------------------------------------

    /**
     * Returns true if the entity passes the active filter (or there is no active filter).
     * "Empty selection" intentionally returns true — zero selected = filter off.
     */
    auto
    Test_Entity(
        const FCk_Handle& InEntity) const -> bool;

    /**
     * Returns true if the entity matches ANY excluded inspector. Caller is expected to
     * force IsVisible=false on the entity's tree node when this returns true.
     */
    auto
    Test_Entity_IsExcluded(
        const FCk_Handle& InEntity) const -> bool;

    // ---- Multicast -------------------------------------------------------

    FCkDebugger_OnInspectorFilterChanged OnFilterChanged;

private:
    auto
    DoBroadcast() -> void;

    auto
    LoadExclusionsFromSettings() -> void;

    auto
    SaveExclusionsToSettings() const -> void;

    /**
     * Token → entry matching is entity-independent, so it is precomputed here whenever
     * the exclusion set changes instead of re-running FName→FString substring scans per
     * entity per tree pass. _ExclusionMatchedEntries indexes into _AllEntries;
     * _ExclusionNameTokens holds every non-empty token for the debug-name match.
     */
    auto
    DoRebuildExclusionMatches() -> void;

    /**
     * Entity's feature bits from the CkEcs debug flag cache, unset when the cache is
     * not enabled on the entity's registry (bits would read all-zero and lie).
     */
    static auto
    DoGet_FlagsIfEnabled(
        const FCk_Handle& InEntity) -> TOptional<uint64>;

    TArray<FInspectorEntry>          _AllEntries;
    TSet<FName>                      _SelectedIDs;
    TSet<FName>                      _ExcludedIDs;
    TArray<int32>                    _ExclusionMatchedEntries;
    TArray<FString>                  _ExclusionNameTokens;
    ECk_InspectorFilter_MatchMode    _MatchMode  = ECk_InspectorFilter_MatchMode::All;
    ECk_InspectorFilter_BadgeStyle   _BadgeStyle = ECk_InspectorFilter_BadgeStyle::ColorAndText;
};
