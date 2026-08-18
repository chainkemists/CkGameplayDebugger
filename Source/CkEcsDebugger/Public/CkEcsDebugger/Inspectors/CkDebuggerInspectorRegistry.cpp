#include "CkDebuggerInspectorRegistry.h"

#include "CkCore/Validation/CkIsValid.h"

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

auto FCkDebuggerInspectorRegistry::Get_AllMetadata() const -> TArray<FCkDebuggerInspectorMetadata>
{
    auto Result = TArray<FCkDebuggerInspectorMetadata>{};
    Result.Reserve(Entries.Num());

    // Build sorted (ID, sort-priority, display-name) triples by instantiating each factory
    // once. Instances go out of scope at the end of this function — no prototypes pinned.
    struct FSortable
    {
        FName ID;
        FText DisplayName;
        int32 SortPriority;
        ECk_Icon Icon = ECk_Icon::None;
        TOptional<FLinearColor> Color;
        FName FeatureFlagId;
    };

    auto Sortable = TArray<FSortable>{};
    Sortable.Reserve(Entries.Num());

    for (const auto& Entry : Entries)
    {
        const auto Inspector = Entry.Factory();
        if (NOT Inspector.IsValid())
        { continue; }

        Sortable.Add(FSortable{
            Entry.ID,
            Inspector->Get_ComponentName(),
            Inspector->Get_SortPriority(),
            Inspector->Get_Icon(),
            Inspector->Get_FeatureColor(),
            Inspector->Get_FeatureFlagId()
        });
    }

    Sortable.Sort([](const FSortable& A, const FSortable& B)
    {
        return A.SortPriority < B.SortPriority;
    });

    for (const auto& Item : Sortable)
    {
        Result.Add(FCkDebuggerInspectorMetadata{ Item.ID, Item.DisplayName, Item.Icon, Item.Color, Item.FeatureFlagId });
    }

    return Result;
}

auto FCkDebuggerInspectorRegistry::Test(
    FName             InInspectorID,
    const FCk_Handle& InEntity) const -> bool
{
    for (const auto& Entry : Entries)
    {
        if (Entry.ID != InInspectorID)
        { continue; }

        const auto Inspector = Entry.Factory();
        if (NOT Inspector.IsValid())
        { return false; }

        return Inspector->CanInspect(InEntity);
    }

    return false;
}
