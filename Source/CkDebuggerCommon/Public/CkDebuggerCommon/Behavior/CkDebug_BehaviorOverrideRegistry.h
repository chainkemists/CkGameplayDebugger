#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

struct CKDEBUGGERCOMMON_API FCkDebug_BehaviorOverrideState
{
    bool IsAvailable = false;
    bool IsActive = false;
    FText Reason;
};

struct CKDEBUGGERCOMMON_API FCkDebug_BehaviorOverrideSetResult
{
    bool Succeeded = false;
    FText Reason;
};

DECLARE_DELEGATE_RetVal(FCkDebug_BehaviorOverrideState, FCkDebug_QueryBehaviorOverride);
DECLARE_DELEGATE_RetVal_OneParam(
    FCkDebug_BehaviorOverrideSetResult,
    FCkDebug_SetBehaviorOverride,
    bool /* InShouldActivate */);

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API FCkDebug_BehaviorOverrideDescriptor
{
public:
    FCkDebug_BehaviorOverrideDescriptor() = default;

    FCkDebug_BehaviorOverrideDescriptor(
        FName InOwnerModule,
        FName InId,
        FText InLabel,
        FText InDescription,
        int32 InSortOrder,
        FCkDebug_QueryBehaviorOverride InQuery,
        FCkDebug_SetBehaviorOverride InSet)
        : _OwnerModule(InOwnerModule)
        , _Id(InId)
        , _Label(MoveTemp(InLabel))
        , _Description(MoveTemp(InDescription))
        , _SortOrder(InSortOrder)
        , _Query(MoveTemp(InQuery))
        , _Set(MoveTemp(InSet))
    {}

    auto IsValid() const -> bool;
    auto Get_OwnerModule() const -> FName { return _OwnerModule; }
    auto Get_Id() const -> FName { return _Id; }
    auto Get_Label() const -> const FText& { return _Label; }
    auto Get_Description() const -> const FText& { return _Description; }
    auto Get_SortOrder() const -> int32 { return _SortOrder; }
    auto Query() const -> FCkDebug_BehaviorOverrideState;
    auto Set(bool InShouldActivate) const -> FCkDebug_BehaviorOverrideSetResult;

private:
    FName _OwnerModule;
    FName _Id;
    FText _Label;
    FText _Description;
    int32 _SortOrder = 0;
    FCkDebug_QueryBehaviorOverride _Query;
    FCkDebug_SetBehaviorOverride _Set;
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Session-only debugger behavior catalog. Feature modules own policy and unregister by generation before unloading;
 * Common owns only discoverability and presentation.
 */
class CKDEBUGGERCOMMON_API FCkDebug_BehaviorOverrideRegistry
{
public:
    static auto Get() -> FCkDebug_BehaviorOverrideRegistry&;

    auto Register(FCkDebug_BehaviorOverrideDescriptor InDescriptor) -> uint64;
    auto Unregister(FName InId, uint64 InRegistrationId) -> void;

    auto Get_Descriptors() const -> TArray<FCkDebug_BehaviorOverrideDescriptor>;
    auto Find(FName InId) const -> TOptional<FCkDebug_BehaviorOverrideDescriptor>;
    auto Get_OnChanged() -> FSimpleMulticastDelegate& { return _OnChanged; }

private:
    struct FRegisteredOverride
    {
        FCkDebug_BehaviorOverrideDescriptor Descriptor;
        uint64 RegistrationId = 0;
    };

    TMap<FName, FRegisteredOverride> _Overrides;
    FSimpleMulticastDelegate _OnChanged;
    uint64 _NextRegistrationId = 1;
};

// --------------------------------------------------------------------------------------------------------------------
