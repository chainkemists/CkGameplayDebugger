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

    // ---- Predicate -------------------------------------------------------

    /**
     * Returns true if the entity passes the active filter (or there is no active filter).
     * "Empty selection" intentionally returns true — zero selected = filter off.
     */
    auto
    Test_Entity(
        const FCk_Handle& InEntity) const -> bool;

    // ---- Multicast -------------------------------------------------------

    FCkDebugger_OnInspectorFilterChanged OnFilterChanged;

private:
    auto
    DoBroadcast() -> void;

    TArray<FInspectorEntry>          _AllEntries;
    TSet<FName>                      _SelectedIDs;
    ECk_InspectorFilter_MatchMode    _MatchMode  = ECk_InspectorFilter_MatchMode::All;
    ECk_InspectorFilter_BadgeStyle   _BadgeStyle = ECk_InspectorFilter_BadgeStyle::ColorAndText;
};
