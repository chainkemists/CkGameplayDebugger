#pragma once
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"

// --------------------------------------------------------------------------------------------------------------------

/**
 * Singleton registry for ICk_DebugOverlay_Provider factories.
 *
 * Providers register themselves at static-init time via the
 * CK_REGISTER_DEBUG_OVERLAY_PROVIDER macro. The overlay system calls
 * CreateAll() to instantiate and sort all registered providers.
 */
class CKENTITYDEBUGOVERLAY_API FCk_DebugOverlay_Registry
{
public:
    using FFactory = TFunction<TSharedPtr<ICk_DebugOverlay_Provider>()>;

    static auto Get() -> FCk_DebugOverlay_Registry&;

    /** Add a provider factory under the given unique ID. */
    auto Register(FName InId, FFactory InFactory) -> void;

    /** Remove the factory registered under InId (no-op if not found). */
    auto Unregister(FName InId) -> void;

    /**
     * Instantiate all registered providers and return them sorted ascending by
     * Get_SortPriority(). Null factories are silently skipped.
     */
    auto CreateAll() -> TArray<TSharedPtr<ICk_DebugOverlay_Provider>>;

private:
    struct FEntry
    {
        FName    Id;
        FFactory Factory;
    };

    TArray<FEntry> Entries;
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * RAII helper used by CK_REGISTER_DEBUG_OVERLAY_PROVIDER.
 * Constructed at static-init time; calls Register on the singleton.
 * Note: construction only calls Register() — no StaticStruct()/StaticClass() involved.
 */
struct FCk_DebugOverlay_AutoRegistrar
{
    FCk_DebugOverlay_AutoRegistrar(FName InId, FCk_DebugOverlay_Registry::FFactory InFactory)
    {
        FCk_DebugOverlay_Registry::Get().Register(InId, MoveTemp(InFactory));
    }
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Place this macro in the .cpp file of any ICk_DebugOverlay_Provider subclass
 * to auto-register it with the overlay registry at module load time.
 *
 * Example:
 *   CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_MyProvider)
 */
#define CK_REGISTER_DEBUG_OVERLAY_PROVIDER(ProviderClass) \
    static FCk_DebugOverlay_AutoRegistrar AutoRegister_##ProviderClass( \
        TEXT(#ProviderClass), \
        []() -> TSharedPtr<ICk_DebugOverlay_Provider> { return MakeShared<ProviderClass>(); });

// --------------------------------------------------------------------------------------------------------------------
