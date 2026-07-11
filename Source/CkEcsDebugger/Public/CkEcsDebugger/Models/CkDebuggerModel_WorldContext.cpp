#include "CkDebuggerModel_WorldContext.h"

#include "Engine/Engine.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"
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
    CachedTransientEntity = FCk_Handle{};
    LastAdded.Reset();
    LastRemoved.Reset();

    const auto World = Get_SelectedWorld();
    if (ck::Is_NOT_Valid(World))
    {
        // Dead world (PIE ended): drop every cached handle without broadcasting a diff —
        // the handles are dying and no consumer should touch them as "removals".
        CachedEntitySet.Reset();
        CacheDirty = false;
        return;
    }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(World);
    if (ck::Is_NOT_Valid(TransientEntity))
    {
        CachedEntitySet.Reset();
        CacheDirty = false;
        return;
    }

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

    // Diff against the previous membership so consumers can apply deltas instead of
    // rebuilding. A world switch naturally lands every old entity in Removed and every
    // new one in Added.
    auto NewSet = TSet<FCk_Handle>{};
    NewSet.Reserve(CachedEntities.Num());
    for (const auto& Entity : CachedEntities)
    {
        NewSet.Add(Entity);
        if (NOT CachedEntitySet.Contains(Entity))
        { LastAdded.Add(Entity); }
    }
    for (const auto& Previous : CachedEntitySet)
    {
        if (NOT NewSet.Contains(Previous))
        { LastRemoved.Add(Previous); }
    }
    CachedEntitySet = MoveTemp(NewSet);

    CachedTransientEntity = TransientEntity;
    LastSeenFlagRevision = ck::debug_feature_flags::Get_Revision(TransientEntity.Get_RegistryView());

    CacheDirty = false;
    LastCacheTime = UCk_Utils_Time_UE::Get_WorldTime(FCk_Utils_Time_GetWorldTime_Params{World}).Get_WorldTime().Get_Time();

    if (NOT LastAdded.IsEmpty() || NOT LastRemoved.IsEmpty())
    { OnCacheDiff.Broadcast(LastAdded, LastRemoved); }
}

auto FCkDebuggerModel_WorldContext::Get_LastAdded() const -> const TArray<FCk_Handle>&
{
    return LastAdded;
}

auto FCkDebuggerModel_WorldContext::Get_LastRemoved() const -> const TArray<FCk_Handle>&
{
    return LastRemoved;
}

auto FCkDebuggerModel_WorldContext::Get_HasStructuralChanges() const -> bool
{
    if (ck::Is_NOT_Valid(CachedTransientEntity))
    { return false; }

    return ck::debug_feature_flags::Get_Revision(CachedTransientEntity.Get_RegistryView()) != LastSeenFlagRevision;
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

    // Live churn: the flag-cache revision moved since the last refresh (O(1) probe;
    // steady state stays clean without any scan).
    if (Get_HasStructuralChanges())
    { return true; }

    return false;
}

auto FCkDebuggerModel_WorldContext::OnSelectorWorldChanged(UWorld* InWorld) -> void
{
    MarkCacheDirty();
    OnWorldChanged.Broadcast(InWorld);
}
