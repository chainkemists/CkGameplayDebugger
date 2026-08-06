#pragma once

#include "CoreMinimal.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkCore/Time/CkTime.h"

// Declares FCkDebugger_OnWorldChanged + the shared world-selection model this
// context composes its ECS entity cache on top of.
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

// Cache-diff broadcast: entities that appeared / disappeared in the latest
// Refresh_EntityCache. Feeds the incremental tree apply and the Activity feed.
DECLARE_MULTICAST_DELEGATE_TwoParams(FCkDebugger_OnEntityCacheDiff,
    const TArray<FCk_Handle>& /*Added*/, const TArray<FCk_Handle>& /*Removed*/);

// ====================================================================================================================
// ECS-flavoured world context: the shared world-selection state
// (FCkDebuggerModel_WorldSelector) plus an ECS entity cache for the selected
// world. World selection forwards to the shared selector; cache invalidation
// rides the selector's OnWorldChanged.
// ====================================================================================================================

class FCkDebuggerModel_WorldContext
{
public:
    FCkDebuggerModel_WorldContext();

    auto Set_SelectedWorld(UWorld* InWorld) -> void;
    auto Get_SelectedWorld() const -> UWorld*;
    auto Get_AvailableWorlds() const -> TArray<UWorld*>;

    // The shared world-selection model — hand this to SCkDebug_WorldSelector.
    auto Get_SelectorModel() const -> TSharedRef<FCkDebuggerModel_WorldSelector>;

    auto Get_CachedEntities() const -> const TArray<FCk_Handle>&;
    auto Refresh_EntityCache() -> void;
    auto Reset_ForWorldChange() -> void;
    auto MarkCacheDirty() -> void;
    auto IsCacheDirty() const -> bool;

    /**
     * Diff of the latest Refresh_EntityCache against the previous cache. Consumers that
     * keep per-entity state (tree nodes, activity feed) apply these instead of rebuilding.
     * Both empty on a refresh that found no membership change.
     */
    auto Get_LastAdded() const -> const TArray<FCk_Handle>&;
    auto Get_LastRemoved() const -> const TArray<FCk_Handle>&;

    FCkDebugger_OnWorldChanged OnWorldChanged;
    FCkDebugger_OnEntityCacheDiff OnCacheDiff;

private:
    auto OnSelectorWorldChanged(UWorld* InWorld) -> void;

    /**
     * O(1) churn probe: the CkEcs debug flag cache bumps a revision counter on every
     * marker add/remove (the "_TreeEntity" flag rides FFragment_LifetimeOwner, so entity
     * spawn/destroy always bumps it). Unchanged revision = provably no membership change,
     * so IsCacheDirty can stay false without any O(n) scan.
     */
    auto Get_HasStructuralChanges() const -> bool;

    TSharedRef<FCkDebuggerModel_WorldSelector> _Selector;

    TArray<FCk_Handle> CachedEntities;
    TSet<FCk_Handle> CachedEntitySet;
    TArray<FCk_Handle> LastAdded;
    TArray<FCk_Handle> LastRemoved;
    FCk_Handle CachedTransientEntity;
    uint64 LastSeenFlagRevision = 0;
    bool CacheDirty = true;
    FCk_Time LastCacheTime;
};
