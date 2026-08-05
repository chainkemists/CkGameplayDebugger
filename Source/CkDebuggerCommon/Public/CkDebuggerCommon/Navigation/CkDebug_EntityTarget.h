#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API FCkDebug_EntityTargetRoute
{
public:
    using FCanTarget = TFunction<bool(const FCk_Handle&)>;
    using FOpenAndTarget = TFunction<void(const FCk_Handle&)>;

    FCkDebug_EntityTargetRoute() = default;

    FCkDebug_EntityTargetRoute(
        FName InOwnerModule,
        FName InTabId,
        FCanTarget InCanTarget,
        FOpenAndTarget InOpenAndTarget)
        : _OwnerModule(InOwnerModule)
        , _TabId(InTabId)
        , _CanTarget(MoveTemp(InCanTarget))
        , _OpenAndTarget(MoveTemp(InOpenAndTarget))
    {
    }

    auto Get_OwnerModule() const -> FName { return _OwnerModule; }
    auto Get_TabId() const -> FName { return _TabId; }
    auto Has_CanTarget() const -> bool { return static_cast<bool>(_CanTarget); }
    auto Has_OpenAndTarget() const -> bool { return static_cast<bool>(_OpenAndTarget); }
    auto CanTarget(const FCk_Handle& InEntity) const -> bool;
    auto OpenAndTarget(const FCk_Handle& InEntity) const -> bool;

private:
    FName _OwnerModule;
    FName _TabId;
    FCanTarget _CanTarget;
    FOpenAndTarget _OpenAndTarget;
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Reload-safe registry for debugger tabs that can open and target an ECS entity.
 *
 * Feature modules own exact entity resolution and register after their tab spawner. CkDebuggerCommon and
 * CkEcsDebugger can discover and invoke those routes without depending on sibling debugger modules.
 */
class CKDEBUGGERCOMMON_API FCkDebug_EntityTargetRegistry
{
public:
    FCkDebug_EntityTargetRegistry() = default;

    static auto Get() -> FCkDebug_EntityTargetRegistry&;

    auto Register(FCkDebug_EntityTargetRoute InRoute) -> uint64;
    auto Unregister(FName InTabId, uint64 InRegistrationId) -> void;

    auto Has_Route(FName InTabId) const -> bool;
    auto Get_RegisteredTabs() const -> TArray<FName>;
    auto Get_TargetableTabs(const FCk_Handle& InEntity) const -> TArray<FName>;
    auto CanTarget(FName InTabId, const FCk_Handle& InEntity) const -> bool;
    auto TryOpenAndTarget(FName InTabId, const FCk_Handle& InEntity) const -> bool;

    auto Get_OnChanged() -> FSimpleMulticastDelegate& { return _OnChanged; }

private:
    struct FRegisteredRoute
    {
        FCkDebug_EntityTargetRoute Route;
        uint64 RegistrationId = 0;
    };

    TMap<FName, FRegisteredRoute> _Routes;
    FSimpleMulticastDelegate _OnChanged;
    uint64 _NextRegistrationId = 1;
};

// --------------------------------------------------------------------------------------------------------------------
