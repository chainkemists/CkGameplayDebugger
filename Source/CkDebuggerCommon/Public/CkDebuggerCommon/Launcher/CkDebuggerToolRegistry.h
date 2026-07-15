#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

enum class ECkDebuggerToolCategory : uint8
{
    Invalid,
    Core,
    Ai,
    Systems,
    Interface,
};

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API FCkDebuggerToolDescriptor
{
public:
    FCkDebuggerToolDescriptor() = default;

    FCkDebuggerToolDescriptor(
        FName InOwnerModule,
        FName InTabId,
        FText InDisplayName,
        FText InTooltip,
        FName InIconId,
        ECkDebuggerToolCategory InCategory,
        int32 InSortOrder)
        : _OwnerModule(InOwnerModule)
        , _TabId(InTabId)
        , _DisplayName(MoveTemp(InDisplayName))
        , _Tooltip(MoveTemp(InTooltip))
        , _IconId(InIconId)
        , _Category(InCategory)
        , _SortOrder(InSortOrder)
    {
    }

    auto Get_OwnerModule() const -> FName { return _OwnerModule; }
    auto Get_TabId() const -> FName { return _TabId; }
    auto Get_DisplayName() const -> const FText& { return _DisplayName; }
    auto Get_Tooltip() const -> const FText& { return _Tooltip; }
    auto Get_IconId() const -> FName { return _IconId; }
    auto Get_Category() const -> ECkDebuggerToolCategory { return _Category; }
    auto Get_SortOrder() const -> int32 { return _SortOrder; }

private:
    FName _OwnerModule;
    FName _TabId;
    FText _DisplayName;
    FText _Tooltip;
    FName _IconId;
    ECkDebuggerToolCategory _Category = ECkDebuggerToolCategory::Invalid;
    int32 _SortOrder = 0;
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Plain-data catalog of standalone CK debugger tabs.
 *
 * Feature modules register after their tab spawner exists and unregister before removing it. Registration returns a
 * generation token: a stale module instance cannot remove a newer replacement registration during live module reload.
 */
class CKDEBUGGERCOMMON_API FCkDebuggerToolRegistry
{
public:
    FCkDebuggerToolRegistry() = default;

    static auto Get() -> FCkDebuggerToolRegistry&;

    auto Register(FCkDebuggerToolDescriptor InDescriptor) -> uint64;
    auto Unregister(FName InTabId, uint64 InRegistrationId) -> void;

    auto Get_Tools() const -> TArray<FCkDebuggerToolDescriptor>;
    auto Get_OnChanged() -> FSimpleMulticastDelegate& { return _OnChanged; }

private:
    struct FRegisteredTool
    {
        FCkDebuggerToolDescriptor Descriptor;
        uint64 RegistrationId = 0;
    };

    TMap<FName, FRegisteredTool> _Tools;
    FSimpleMulticastDelegate _OnChanged;
    uint64 _NextRegistrationId = 1;
};
