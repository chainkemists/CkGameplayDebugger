#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

/** Lightweight description of a registered inspector — does not pin a live instance. */
struct FCkDebuggerInspectorMetadata
{
    FName ID;
    FText DisplayName;
};

class CKECSDEBUGGER_API FCkDebuggerInspectorRegistry
{
public:
    using FInspectorFactory = TFunction<TSharedPtr<ICkDebuggerComponentInspector_Base>()>;

    static auto Get() -> FCkDebuggerInspectorRegistry&;

    auto Register(FName InInspectorID, FInspectorFactory InFactory) -> void;
    auto Unregister(FName InInspectorID) -> void;
    auto CreateAll() -> TArray<TSharedPtr<ICkDebuggerComponentInspector_Base>>;

    /**
     * Snapshot of registered inspectors as (ID, DisplayName) pairs.
     * Each entry is built by instantiating the factory once and calling Get_ComponentName(),
     * then discarding the instance — no prototypes are pinned. Sorted by sort priority for
     * stable presentation in UI.
     */
    auto Get_AllMetadata() const -> TArray<FCkDebuggerInspectorMetadata>;

    /**
     * Lifetime-safe CanInspect query: instantiates a fresh inspector for InInspectorID,
     * calls CanInspect(InEntity), then discards the instance. Returns false if the ID is
     * not registered or the factory returned null.
     */
    auto Test(
        FName             InInspectorID,
        const FCk_Handle& InEntity) const -> bool;

private:
    struct FRegistryEntry
    {
        FName ID;
        FInspectorFactory Factory;
    };

    TArray<FRegistryEntry> Entries;
};

struct FCkDebuggerInspectorAutoRegistrar
{
    FCkDebuggerInspectorAutoRegistrar(FName InID, FCkDebuggerInspectorRegistry::FInspectorFactory InFactory)
    {
        FCkDebuggerInspectorRegistry::Get().Register(InID, MoveTemp(InFactory));
    }
};

#define CK_REGISTER_DEBUGGER_INSPECTOR(InspectorClass) \
    static FCkDebuggerInspectorAutoRegistrar AutoRegister_##InspectorClass( \
        TEXT(#InspectorClass), \
        []() -> TSharedPtr<ICkDebuggerComponentInspector_Base> { return MakeShared<InspectorClass>(); });
