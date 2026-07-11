#include "CkDebuggerModel_WorldContext.h"

#include "Engine/Engine.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcsDebugger/FeatureFlags/CkEcsDebugger_FeatureFlags.h"

// ====================================================================================================================

FCkDebuggerModel_WorldContext::FCkDebuggerModel_WorldContext()
    : _Selector(MakeShared<FCkDebuggerModel_WorldSelector>())
{
    _Selector->OnWorldChanged.AddRaw(this, &FCkDebuggerModel_WorldContext::OnSelectorWorldChanged);
}

auto FCkDebuggerModel_WorldContext::Set_SelectedWorld(UWorld* InWorld) -> void
{
    // Cache invalidation + OnWorldChanged re-broadcast happen in OnSelectorWorldChanged.
    _Selector->Set_SelectedWorld(InWorld);
}

auto FCkDebuggerModel_WorldContext::Get_SelectedWorld() const -> UWorld*
{
    return _Selector->Get_SelectedWorld();
}

auto FCkDebuggerModel_WorldContext::Get_AvailableWorlds() const -> TArray<UWorld*>
{
    return _Selector->Get_AvailableWorlds();
}

auto FCkDebuggerModel_WorldContext::Get_SelectorModel() const -> TSharedRef<FCkDebuggerModel_WorldSelector>
{
    return _Selector;
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

    // Connect the CkEcs debug feature-flag cache to the observed world's registry.
    // Idempotent; the ctx payload (and its sink connections) dies with the registry
    // on PIE end, so no explicit teardown is required here.
    ck::ecs_debugger_feature_flags::EnableFor(TransientEntity.Get_RegistryView());

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
    if (CacheDirty)
    { return true; }

    // If the selected world was destroyed (PIE ended), the cache is stale
    if (NOT CachedEntities.IsEmpty() && ck::Is_NOT_Valid(Get_SelectedWorld()))
    { return true; }

    return false;
}

auto FCkDebuggerModel_WorldContext::OnSelectorWorldChanged(UWorld* InWorld) -> void
{
    MarkCacheDirty();
    OnWorldChanged.Broadcast(InWorld);
}
