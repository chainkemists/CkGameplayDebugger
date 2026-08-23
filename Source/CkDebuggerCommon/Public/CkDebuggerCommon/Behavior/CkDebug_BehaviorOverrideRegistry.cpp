#include "CkDebug_BehaviorOverrideRegistry.h"

#include "CkCore/Ensure/CkEnsure.h"

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebug_BehaviorOverrideDescriptor::IsValid() const -> bool
{
    return NOT _OwnerModule.IsNone()
        && NOT _Id.IsNone()
        && NOT _Label.IsEmpty()
        && NOT _Description.IsEmpty()
        && _Query.IsBound()
        && _Set.IsBound();
}

auto FCkDebug_BehaviorOverrideDescriptor::Query() const -> FCkDebug_BehaviorOverrideState
{
    return _Query.IsBound()
        ? _Query.Execute()
        : FCkDebug_BehaviorOverrideState{
            false, false, FText::FromString(TEXT("The behavior provider is no longer registered."))};
}

auto FCkDebug_BehaviorOverrideDescriptor::Set(bool InShouldActivate) const
    -> FCkDebug_BehaviorOverrideSetResult
{
    return _Set.IsBound()
        ? _Set.Execute(InShouldActivate)
        : FCkDebug_BehaviorOverrideSetResult{
            false, FText::FromString(TEXT("The behavior provider is no longer registered."))};
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebug_BehaviorOverrideRegistry::Get() -> FCkDebug_BehaviorOverrideRegistry&
{
    static auto Instance = FCkDebug_BehaviorOverrideRegistry{};
    return Instance;
}

auto FCkDebug_BehaviorOverrideRegistry::Register(FCkDebug_BehaviorOverrideDescriptor InDescriptor) -> uint64
{
    const auto DescriptorIsValid = InDescriptor.IsValid();
    CK_ENSURE_IF_NOT(DescriptorIsValid,
        TEXT("Cannot register invalid debugger behavior override [{}]"), InDescriptor.Get_Id())
    {}
    if (NOT DescriptorIsValid)
    { return 0; }

    const auto RegistrationId = _NextRegistrationId++;
    if (_NextRegistrationId == 0)
    { _NextRegistrationId = 1; }

    if (auto* Existing = _Overrides.Find(InDescriptor.Get_Id()))
    {
        const auto OwnerMatches = Existing->Descriptor.Get_OwnerModule() == InDescriptor.Get_OwnerModule();
        CK_ENSURE_IF_NOT(OwnerMatches,
            TEXT("Behavior override [{}] is already owned by module [{}]; module [{}] cannot replace it"),
            InDescriptor.Get_Id(),
            Existing->Descriptor.Get_OwnerModule(),
            InDescriptor.Get_OwnerModule())
        {}
        if (NOT OwnerMatches)
        { return 0; }

        Existing->Descriptor = MoveTemp(InDescriptor);
        Existing->RegistrationId = RegistrationId;
        _OnChanged.Broadcast();
        return RegistrationId;
    }

    const auto Id = InDescriptor.Get_Id();
    _Overrides.Add(Id, FRegisteredOverride{MoveTemp(InDescriptor), RegistrationId});
    _OnChanged.Broadcast();
    return RegistrationId;
}

auto FCkDebug_BehaviorOverrideRegistry::Unregister(FName InId, uint64 InRegistrationId) -> void
{
    if (InId.IsNone() || InRegistrationId == 0)
    { return; }

    const auto* Existing = _Overrides.Find(InId);
    if (Existing == nullptr || Existing->RegistrationId != InRegistrationId)
    { return; }

    _Overrides.Remove(InId);
    _OnChanged.Broadcast();
}

auto FCkDebug_BehaviorOverrideRegistry::Get_Descriptors() const
    -> TArray<FCkDebug_BehaviorOverrideDescriptor>
{
    auto Descriptors = TArray<FCkDebug_BehaviorOverrideDescriptor>{};
    Descriptors.Reserve(_Overrides.Num());
    for (const auto& Pair : _Overrides)
    { Descriptors.Add(Pair.Value.Descriptor); }

    Descriptors.Sort([](const auto& A, const auto& B)
    {
        if (A.Get_SortOrder() != B.Get_SortOrder())
        { return A.Get_SortOrder() < B.Get_SortOrder(); }
        return A.Get_Id().LexicalLess(B.Get_Id());
    });
    return Descriptors;
}

auto FCkDebug_BehaviorOverrideRegistry::Find(FName InId) const
    -> TOptional<FCkDebug_BehaviorOverrideDescriptor>
{
    const auto* Existing = _Overrides.Find(InId);
    return Existing == nullptr
        ? TOptional<FCkDebug_BehaviorOverrideDescriptor>{}
        : TOptional<FCkDebug_BehaviorOverrideDescriptor>{Existing->Descriptor};
}

// --------------------------------------------------------------------------------------------------------------------
