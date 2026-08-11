#pragma once

#include "CoreMinimal.h"

#include "CkDebuggerCommon/Picker/CkDebug_PickerOverlayCards.h"

#include "CkEcs/Handle/CkHandle.h"

// --------------------------------------------------------------------------------------------------------------------

class SCkDebugOverlay_Root;
class ICk_DebugOverlay_Provider;
class FCk_DebugOverlay_History;

// ====================================================================================================================
// The overlay-module implementation of the shared viewport picker's focus card
// + world tags: the same main card and world cards the On-Screen Overlay shows,
// built via the shared presenters. Registered into ck::DebugPickerCards by
// FCkEntityDebugOverlayModule::StartupModule.
// ====================================================================================================================

class FCkDebugOverlay_PickerCards : public ICkDebug_PickerOverlayCards
{
public:
    // Out-of-line: TUniquePtr<FCk_DebugOverlay_History> needs the complete type
    // wherever its destructor is instantiated — which the implicit inline
    // constructor does too (member-cleanup path) — and this header only
    // forward-declares it.
    FCkDebugOverlay_PickerCards();
    ~FCkDebugOverlay_PickerCards() override;

    auto
    Activate(
        UWorld* InWorld) -> void override;

    auto
    Deactivate(
        UWorld* InWorld) -> void override;

    auto
    Update(
        UWorld*                       InWorld,
        APlayerController*            InPC,
        bool                          InIsEjected,
        const FCk_Handle&             InFocusEntity,
        const FCkDebug_EntityMarkers& InMarkers) -> void override;

private:
    TSharedPtr<SCkDebugOverlay_Root>              _OverlayRoot;
    TArray<TSharedPtr<ICk_DebugOverlay_Provider>> _OverlayProviders;
    TUniquePtr<FCk_DebugOverlay_History>          _OverlayHistory;
    int32                                         _OverlayLayoutIndex = 0;
    bool                                          _IsActive           = false;
};

// ====================================================================================================================
