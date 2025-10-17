#pragma once

#include "CkCore/Time/CkTime.h"
#include "CkEcs/Handle/CkHandle.h"

class FCkDebuggerModel_WorldContext
{
public:
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldChanged, UWorld*);

    auto Set_SelectedWorld(UWorld* InWorld) -> void;

    auto Get_SelectedWorld() const -> UWorld*
    {
        return SelectedWorld.Get();
    }

    auto Get_AvailableWorlds() const -> TArray<UWorld*>;

    auto Get_CachedEntities() const -> const TArray<FCk_Handle>&
    {
        return CachedEntities;
    }

    auto Refresh_EntityCache() -> void;
    auto MarkCacheDirty() -> void;

    auto IsCacheDirty() const -> bool
    {
        return bIsCacheDirty;
    }

    FOnWorldChanged OnWorldChanged;

private:
    TWeakObjectPtr<UWorld> SelectedWorld;
    TArray<FCk_Handle> CachedEntities;
    bool bIsCacheDirty = true;
    FCk_Time LastCacheTime;
};