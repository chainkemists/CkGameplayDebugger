#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class APlayerController;
class UWorld;
class FCkDebug_EntityMarkers;

// ====================================================================================================================
// Picker overlay-card hook — sibling of ck::DebugNav.
//
// The shared viewport picker (FCkDebug_ViewportPicker) reuses the On-Screen
// Overlay's focus card + world tags for the hovered/previewed entities. Those
// widgets and presenters live in CkEntityDebugOverlay, which depends on this
// module — so the picker cannot reference them directly. Instead this module
// owns a factory slot that CkEntityDebugOverlay fills on module startup: the
// standard one-way registration pattern (low-tier module owns the slot,
// high-tier module fills it). When nothing is registered (e.g. the overlay
// module is absent), the picker simply shows no cards — diamonds, links, and
// picking still work.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API ICkDebug_PickerOverlayCards
{
public:
    virtual ~ICkDebug_PickerOverlayCards() = default;

    // Install the card widgets on InWorld's game viewport. Implementations may
    // decline (e.g. the overlay subsystem is already showing its own cards);
    // Update then no-ops.
    virtual auto Activate(
        UWorld* InWorld) -> void = 0;

    // Remove the card widgets. InWorld may be null during teardown.
    virtual auto Deactivate(
        UWorld* InWorld) -> void = 0;

    // Push the focus card for InFocusEntity and world tags for the current
    // marker snapshot. Called from the picker's per-frame Tick.
    virtual auto Update(
        UWorld*                       InWorld,
        APlayerController*            InPC,
        bool                          InIsEjected,
        const FCk_Handle&             InFocusEntity,
        const FCkDebug_EntityMarkers& InMarkers) -> void = 0;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::DebugPickerCards
{
    using FFactoryFn = TFunction<TSharedPtr<ICkDebug_PickerOverlayCards>()>;

    // Called by FCkEntityDebugOverlayModule::StartupModule to install the factory.
    // Passing an unbound TFunction clears the registration (used in ShutdownModule).
    CKDEBUGGERCOMMON_API auto Register_Factory(FFactoryFn InFactory) -> void;

    // One presenter per picker activation. Null when no factory is registered.
    CKDEBUGGERCOMMON_API auto Create() -> TSharedPtr<ICkDebug_PickerOverlayCards>;
}

// ====================================================================================================================
