#pragma once

#include "CoreMinimal.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkCore/Time/CkTime.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FCkDebugger_OnWorldChanged, UWorld*);

class FCkDebuggerModel_WorldContext
{
public:
    auto Set_SelectedWorld(UWorld* InWorld) -> void;
    auto Get_SelectedWorld() const -> UWorld*;
    auto Get_AvailableWorlds() const -> TArray<UWorld*>;

    auto Get_CachedEntities() const -> const TArray<FCk_Handle>&;
    auto Refresh_EntityCache() -> void;
    auto MarkCacheDirty() -> void;
    auto IsCacheDirty() const -> bool;

    FCkDebugger_OnWorldChanged OnWorldChanged;

private:
    auto BroadcastWorldChanged() -> void;

    TWeakObjectPtr<UWorld> SelectedWorld;
    TArray<FCk_Handle> CachedEntities;
    bool CacheDirty = true;
    FCk_Time LastCacheTime;
};