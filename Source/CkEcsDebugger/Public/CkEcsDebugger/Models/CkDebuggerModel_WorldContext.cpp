#include "CkDebuggerModel_WorldContext.h"

#include "Engine/Engine.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

auto FCkDebuggerModel_WorldContext::Set_SelectedWorld(UWorld* InWorld) -> void
{
    if (SelectedWorld.Get() == InWorld)
    { return; }

    SelectedWorld = InWorld;
    MarkCacheDirty();
    BroadcastWorldChanged();
}

auto FCkDebuggerModel_WorldContext::Get_SelectedWorld() const -> UWorld*
{
    return SelectedWorld.Get();
}

auto FCkDebuggerModel_WorldContext::Get_AvailableWorlds() const -> TArray<UWorld*>
{
    auto Worlds = TArray<UWorld*>{};

    if (ck::Is_NOT_Valid(GEngine))
    { return Worlds; }

    auto WorldContexts = GEngine->GetWorldContexts();

    for (auto Index = 0; Index < WorldContexts.Num(); ++Index)
    {
        const auto& ContextWorld = WorldContexts[Index].World();

        if (ck::Is_NOT_Valid(ContextWorld))
        { continue; }

        if (auto GameInstance = ContextWorld->GetGameInstance();
            ck::IsValid(GameInstance))
        {
            Worlds.Emplace(ContextWorld);
        }
    }

    return Worlds;
}

auto FCkDebuggerModel_WorldContext::Get_CachedEntities() const -> const TArray<FCk_Handle>&
{
    return CachedEntities;
}

auto FCkDebuggerModel_WorldContext::Refresh_EntityCache() -> void
{
    CachedEntities.Empty();

    const auto World = Get_SelectedWorld();
    if (ck::Is_NOT_Valid(World))
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(World);
    if (ck::Is_NOT_Valid(TransientEntity))
    { return; }

    TransientEntity.View<ck::FFragment_LifetimeOwner, CK_IGNORE_PENDING_KILL>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_LifetimeOwner& InFragment)
        {
            const auto Handle = ck::MakeHandle(InEntity, TransientEntity);
            CachedEntities.Add(Handle);
        });

    CacheDirty = false;
    LastCacheTime = UCk_Utils_Time_UE::Get_WorldTime(FCk_Utils_Time_GetWorldTime_Params{World}).Get_WorldTime().Get_Time();;
}

auto FCkDebuggerModel_WorldContext::MarkCacheDirty() -> void
{
    CacheDirty = true;
}

auto FCkDebuggerModel_WorldContext::IsCacheDirty() const -> bool
{
    return CacheDirty;
}

auto FCkDebuggerModel_WorldContext::BroadcastWorldChanged() -> void
{
    OnWorldChanged.Broadcast(Get_SelectedWorld());
}