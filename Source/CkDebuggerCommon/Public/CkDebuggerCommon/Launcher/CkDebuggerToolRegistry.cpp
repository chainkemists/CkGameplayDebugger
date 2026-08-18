#include "CkDebuggerToolRegistry.h"

#include "CkCore/Ensure/CkEnsure.h"

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebuggerToolRegistry::Get() -> FCkDebuggerToolRegistry&
{
    static auto Instance = FCkDebuggerToolRegistry{};
    return Instance;
}

auto FCkDebuggerToolRegistry::Register(FCkDebuggerToolDescriptor InDescriptor) -> uint64
{
    CK_ENSURE_IF_NOT(NOT InDescriptor.Get_OwnerModule().IsNone(),
        TEXT("Cannot register debugger tool [{}] without an owning module"),
        InDescriptor.Get_TabId())
    { return 0; }

    CK_ENSURE_IF_NOT(NOT InDescriptor.Get_TabId().IsNone(),
        TEXT("Cannot register debugger tool from module [{}] without a tab id"),
        InDescriptor.Get_OwnerModule())
    { return 0; }

    CK_ENSURE_IF_NOT(NOT InDescriptor.Get_DisplayName().IsEmpty(),
        TEXT("Cannot register debugger tool [{}] without a display name"),
        InDescriptor.Get_TabId())
    { return 0; }

    CK_ENSURE_IF_NOT(NOT InDescriptor.Get_Tooltip().IsEmpty(),
        TEXT("Cannot register debugger tool [{}] without a tooltip"),
        InDescriptor.Get_TabId())
    { return 0; }

    CK_ENSURE_IF_NOT(InDescriptor.Get_IconId() != ECk_Icon::None,
        TEXT("Cannot register debugger tool [{}] without an icon id"),
        InDescriptor.Get_TabId())
    { return 0; }

    CK_ENSURE_IF_NOT(InDescriptor.Get_Category() != ECkDebuggerToolCategory::Invalid,
        TEXT("Cannot register debugger tool [{}] without a category"),
        InDescriptor.Get_TabId())
    { return 0; }

    const auto RegistrationId = _NextRegistrationId++;
    if (_NextRegistrationId == 0)
    { _NextRegistrationId = 1; }

    if (auto* Existing = _Tools.Find(InDescriptor.Get_TabId()))
    {
        CK_ENSURE_IF_NOT(Existing->Descriptor.Get_OwnerModule() == InDescriptor.Get_OwnerModule(),
            TEXT("Debugger tab id [{}] is already owned by module [{}]; module [{}] cannot replace it"),
            InDescriptor.Get_TabId(),
            Existing->Descriptor.Get_OwnerModule(),
            InDescriptor.Get_OwnerModule())
        { return 0; }

        Existing->Descriptor = MoveTemp(InDescriptor);
        Existing->RegistrationId = RegistrationId;
        _OnChanged.Broadcast();
        return RegistrationId;
    }

    const auto TabId = InDescriptor.Get_TabId();
    _Tools.Add(TabId, FRegisteredTool{MoveTemp(InDescriptor), RegistrationId});
    _OnChanged.Broadcast();
    return RegistrationId;
}

auto FCkDebuggerToolRegistry::Unregister(FName InTabId, uint64 InRegistrationId) -> void
{
    if (InTabId.IsNone() || InRegistrationId == 0)
    { return; }

    const auto* Existing = _Tools.Find(InTabId);
    if (Existing == nullptr || Existing->RegistrationId != InRegistrationId)
    { return; }

    _Tools.Remove(InTabId);
    _OnChanged.Broadcast();
}

auto FCkDebuggerToolRegistry::Get_Tools() const -> TArray<FCkDebuggerToolDescriptor>
{
    auto Tools = TArray<FCkDebuggerToolDescriptor>{};
    Tools.Reserve(_Tools.Num());

    for (const auto& Pair : _Tools)
    {
        Tools.Add(Pair.Value.Descriptor);
    }

    Tools.Sort([](const FCkDebuggerToolDescriptor& A, const FCkDebuggerToolDescriptor& B)
    {
        if (A.Get_Category() != B.Get_Category())
        { return static_cast<uint8>(A.Get_Category()) < static_cast<uint8>(B.Get_Category()); }

        if (A.Get_SortOrder() != B.Get_SortOrder())
        { return A.Get_SortOrder() < B.Get_SortOrder(); }

        return A.Get_TabId().LexicalLess(B.Get_TabId());
    });

    return Tools;
}

// --------------------------------------------------------------------------------------------------------------------
