#pragma once

#include "CoreMinimal.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkCore/Time/CkTime.h"

// Declares FCkDebugger_OnWorldChanged + the shared world-selection model this
// context composes its ECS entity cache on top of.
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

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
    auto MarkCacheDirty() -> void;
    auto IsCacheDirty() const -> bool;

    FCkDebugger_OnWorldChanged OnWorldChanged;

private:
    auto OnSelectorWorldChanged(UWorld* InWorld) -> void;

    TSharedRef<FCkDebuggerModel_WorldSelector> _Selector;

    TArray<FCk_Handle> CachedEntities;
    bool CacheDirty = true;
    FCk_Time LastCacheTime;
};
