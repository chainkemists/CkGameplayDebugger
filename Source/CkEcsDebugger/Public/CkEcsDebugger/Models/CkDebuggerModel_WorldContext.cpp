#include "CkDebuggerModel_WorldContext.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include <Engine/Engine.h>
#include <Engine/World.h>

auto FCkDebuggerModel_WorldContext::Set_SelectedWorld(UWorld* InWorld) -> void
{
    if (SelectedWorld == InWorld)
    { return; }

    SelectedWorld = InWorld;
    MarkCacheDirty();

    OnWorldChanged.Broadcast(InWorld);
}

auto FCkDebuggerModel_WorldContext::Get_AvailableWorlds() const -> TArray<UWorld*>
{
    auto Worlds = TArray<UWorld*>();

    if (ck::Is_NOT_Valid(GEngine))
    { return Worlds; }

    for (const auto& WorldContexts = GEngine->GetWorldContexts();
        const auto& Context : WorldContexts)
    {
        const auto World = Context.World();

        if (ck::Is_NOT_Valid(World))
        { continue; }

        if (const auto GameInstance = World->GetGameInstance();
            ck::Is_NOT_Valid(GameInstance))
        { continue; }

        Worlds.Add(World);
    }

    return Worlds;
}

auto FCkDebuggerModel_WorldContext::Refresh_EntityCache() -> void
{
    CachedEntities.Empty();

    const auto World = SelectedWorld.Get();

    if (ck::Is_NOT_Valid(World))
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(World);

    if (ck::Is_NOT_Valid(TransientEntity))
    { return; }

    TransientEntity.View<ck::FFragment_LifetimeOwner, CK_IGNORE_PENDING_KILL>().ForEach(
    [&](FCk_Entity InEntity, const ck::FFragment_LifetimeOwner& InFragment)
    {
        const auto Handle = ck::MakeHandle(InEntity, TransientEntity);

        if (Handle == TransientEntity)
        { return; }

        CachedEntities.Add(Handle);
    });

    bIsCacheDirty = false;
    LastCacheTime = UCk_Utils_Time_UE::Get_WorldTime(FCk_Utils_Time_GetWorldTime_Params{World}).Get_WorldTime().Get_Time();
}

auto FCkDebuggerModel_WorldContext::MarkCacheDirty() -> void
{
    bIsCacheDirty = true;
}