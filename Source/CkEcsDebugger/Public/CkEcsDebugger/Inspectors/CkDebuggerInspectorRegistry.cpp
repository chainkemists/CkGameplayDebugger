#include "CkDebuggerInspectorRegistry.h"

auto FCkDebuggerInspectorRegistry::Get() -> FCkDebuggerInspectorRegistry&
{
    static FCkDebuggerInspectorRegistry Instance;
    return Instance;
}

auto FCkDebuggerInspectorRegistry::Register(FName InInspectorID, FInspectorFactory InFactory) -> void
{
    for (const auto& Entry : Entries)
    {
        if (Entry.ID == InInspectorID)
        { return; }
    }

    Entries.Add(FRegistryEntry{ InInspectorID, MoveTemp(InFactory) });
}

auto FCkDebuggerInspectorRegistry::Unregister(FName InInspectorID) -> void
{
    Entries.RemoveAll([&InInspectorID](const FRegistryEntry& Entry)
    {
        return Entry.ID == InInspectorID;
    });
}

auto FCkDebuggerInspectorRegistry::CreateAll() -> TArray<TSharedPtr<ICkDebuggerComponentInspector_Base>>
{
    TArray<TSharedPtr<ICkDebuggerComponentInspector_Base>> Result;
    Result.Reserve(Entries.Num());

    for (const auto& Entry : Entries)
    {
        if (auto Inspector = Entry.Factory())
        {
            Result.Add(MoveTemp(Inspector));
        }
    }

    Result.Sort([](const TSharedPtr<ICkDebuggerComponentInspector_Base>& A, const TSharedPtr<ICkDebuggerComponentInspector_Base>& B)
    {
        return A->Get_SortPriority() < B->Get_SortPriority();
    });

    return Result;
}
