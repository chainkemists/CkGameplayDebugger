#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Registry::Get() -> FCk_DebugOverlay_Registry&
{
    static FCk_DebugOverlay_Registry Instance;
    return Instance;
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Registry::Register(FName InId, FFactory InFactory) -> void
{
    Entries.Add({ InId, MoveTemp(InFactory) });
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Registry::Unregister(FName InId) -> void
{
    Entries.RemoveAll([&](const FEntry& E) { return E.Id == InId; });
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Registry::CreateAll() -> TArray<TSharedPtr<ICk_DebugOverlay_Provider>>
{
    TArray<TSharedPtr<ICk_DebugOverlay_Provider>> Out;
    for (const auto& E : Entries)
    {
        if (auto P = E.Factory())
        {
            Out.Add(P);
        }
    }
    Out.Sort([](const TSharedPtr<ICk_DebugOverlay_Provider>& A,
                const TSharedPtr<ICk_DebugOverlay_Provider>& B)
    {
        return A->Get_SortPriority() < B->Get_SortPriority();
    });
    return Out;
}

// --------------------------------------------------------------------------------------------------------------------
