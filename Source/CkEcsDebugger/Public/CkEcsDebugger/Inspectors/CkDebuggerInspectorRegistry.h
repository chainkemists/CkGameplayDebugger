#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

class FCkDebuggerInspectorRegistry
{
public:
    using FInspectorFactory = TFunction<TSharedPtr<ICkDebuggerComponentInspector_Base>()>;

    static auto Get() -> FCkDebuggerInspectorRegistry&;

    auto Register(FName InInspectorID, FInspectorFactory InFactory) -> void;
    auto Unregister(FName InInspectorID) -> void;
    auto CreateAll() -> TArray<TSharedPtr<ICkDebuggerComponentInspector_Base>>;

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
