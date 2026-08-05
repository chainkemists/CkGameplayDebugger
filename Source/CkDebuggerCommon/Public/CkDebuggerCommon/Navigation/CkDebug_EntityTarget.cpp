#include "CkDebug_EntityTarget.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebug_EntityTargetRoute::CanTarget(const FCk_Handle& InEntity) const -> bool
{
    if (ck::Is_NOT_Valid(InEntity) || NOT _CanTarget)
    { return false; }

    return _CanTarget(InEntity);
}

auto FCkDebug_EntityTargetRoute::OpenAndTarget(const FCk_Handle& InEntity) const -> bool
{
    if (NOT CanTarget(InEntity) || NOT _OpenAndTarget)
    { return false; }

    _OpenAndTarget(InEntity);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebug_EntityTargetRegistry::Get() -> FCkDebug_EntityTargetRegistry&
{
    static auto Instance = FCkDebug_EntityTargetRegistry{};
    return Instance;
}

auto FCkDebug_EntityTargetRegistry::Register(FCkDebug_EntityTargetRoute InRoute) -> uint64
{
    const auto HasOwner = NOT InRoute.Get_OwnerModule().IsNone();
    CK_ENSURE_IF_NOT(HasOwner,
        TEXT("Cannot register entity-target route [{}] without an owning module"),
        InRoute.Get_TabId())
    {}
    if (NOT HasOwner)
    { return 0; }

    const auto HasTabId = NOT InRoute.Get_TabId().IsNone();
    CK_ENSURE_IF_NOT(HasTabId,
        TEXT("Cannot register entity-target route from module [{}] without a tab id"),
        InRoute.Get_OwnerModule())
    {}
    if (NOT HasTabId)
    { return 0; }

    const auto HasCanTarget = InRoute.Has_CanTarget();
    CK_ENSURE_IF_NOT(HasCanTarget,
        TEXT("Cannot register entity-target route [{}] without a CanTarget callback"),
        InRoute.Get_TabId())
    {}
    if (NOT HasCanTarget)
    { return 0; }

    const auto HasOpenAndTarget = InRoute.Has_OpenAndTarget();
    CK_ENSURE_IF_NOT(HasOpenAndTarget,
        TEXT("Cannot register entity-target route [{}] without an OpenAndTarget callback"),
        InRoute.Get_TabId())
    {}
    if (NOT HasOpenAndTarget)
    { return 0; }

    const auto RegistrationId = _NextRegistrationId++;
    if (_NextRegistrationId == 0)
    { _NextRegistrationId = 1; }

    if (auto* Existing = _Routes.Find(InRoute.Get_TabId()))
    {
        const auto OwnerMatches = Existing->Route.Get_OwnerModule() == InRoute.Get_OwnerModule();
        CK_ENSURE_IF_NOT(OwnerMatches,
            TEXT("Entity-target tab id [{}] is already owned by module [{}]; module [{}] cannot replace it"),
            InRoute.Get_TabId(),
            Existing->Route.Get_OwnerModule(),
            InRoute.Get_OwnerModule())
        {}
        if (NOT OwnerMatches)
        { return 0; }

        Existing->Route = MoveTemp(InRoute);
        Existing->RegistrationId = RegistrationId;
        _OnChanged.Broadcast();
        return RegistrationId;
    }

    const auto TabId = InRoute.Get_TabId();
    _Routes.Add(TabId, FRegisteredRoute{MoveTemp(InRoute), RegistrationId});
    _OnChanged.Broadcast();
    return RegistrationId;
}

auto FCkDebug_EntityTargetRegistry::Unregister(FName InTabId, uint64 InRegistrationId) -> void
{
    if (InTabId.IsNone() || InRegistrationId == 0)
    { return; }

    const auto* Existing = _Routes.Find(InTabId);
    if (Existing == nullptr || Existing->RegistrationId != InRegistrationId)
    { return; }

    _Routes.Remove(InTabId);
    _OnChanged.Broadcast();
}

auto FCkDebug_EntityTargetRegistry::Has_Route(FName InTabId) const -> bool
{
    return NOT InTabId.IsNone() && _Routes.Contains(InTabId);
}

auto FCkDebug_EntityTargetRegistry::Get_RegisteredTabs() const -> TArray<FName>
{
    auto Tabs = TArray<FName>{};
    _Routes.GetKeys(Tabs);
    Tabs.Sort([](const FName InA, const FName InB) { return InA.LexicalLess(InB); });
    return Tabs;
}

auto FCkDebug_EntityTargetRegistry::Get_TargetableTabs(const FCk_Handle& InEntity) const -> TArray<FName>
{
    auto Tabs = TArray<FName>{};
    if (ck::Is_NOT_Valid(InEntity))
    { return Tabs; }

    for (const auto& [TabId, Registered] : _Routes)
    {
        if (Registered.Route.CanTarget(InEntity))
        { Tabs.Add(TabId); }
    }

    Tabs.Sort([](const FName InA, const FName InB) { return InA.LexicalLess(InB); });
    return Tabs;
}

auto FCkDebug_EntityTargetRegistry::CanTarget(FName InTabId, const FCk_Handle& InEntity) const -> bool
{
    if (InTabId.IsNone() || ck::Is_NOT_Valid(InEntity))
    { return false; }

    const auto* Registered = _Routes.Find(InTabId);
    return Registered != nullptr && Registered->Route.CanTarget(InEntity);
}

auto FCkDebug_EntityTargetRegistry::TryOpenAndTarget(FName InTabId, const FCk_Handle& InEntity) const -> bool
{
    if (InTabId.IsNone() || ck::Is_NOT_Valid(InEntity))
    { return false; }

    const auto* Registered = _Routes.Find(InTabId);
    if (Registered == nullptr)
    { return false; }

    return Registered->Route.OpenAndTarget(InEntity);
}

// --------------------------------------------------------------------------------------------------------------------
