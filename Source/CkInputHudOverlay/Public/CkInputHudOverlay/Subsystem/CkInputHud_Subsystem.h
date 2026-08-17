#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "CkInputHud_Subsystem.generated.h"

class APlayerController;
class FCk_InputHud_Model;
class FCkDebug_KeyActivityObserver;
class SCkInputHud_Root;
class UCommonInputSubsystem;

enum class ECommonInputType : uint8;

namespace ck::input_hud
{
    // Non-shipping builds present the keyboard overlay immediately; Shipping compiles the subsystem as a no-op.
    inline constexpr auto DefaultOverlayMode = UE_BUILD_SHIPPING ? 0 : 1;
}

// ====================================================================================================================
// UCk_InputHud_Subsystem
//
// LocalPlayer subsystem driving the QA-facing on-screen input overlay: one chip per press-lifecycle with what it
// resolved to, which layer the stack is arbitrating under, and the gamepad's stick deflection.
//
// Implementation is gated by WITH_CK_INPUT_HUD so Shipping carries no overhead; the UCLASS declaration is
// unconditional so UHT always sees it.
//
// v1 is LOCAL PLAYER 0 ONLY. Split-screen brings up one subsystem per local player, but only the first to
// initialize owns the console objects, and the collector resolves the first game player's input source. A per-player
// HUD would need a per-player corner and a per-player cvar surface, which is deliberately not built yet.
// ====================================================================================================================

UCLASS(NotBlueprintable)
class CKINPUTHUDOVERLAY_API UCk_InputHud_Subsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_InputHud_Subsystem);

public:
    // --- USubsystem ---
    virtual void Initialize(FSubsystemCollectionBase& InCollection) override;
    virtual void Deinitialize() override;
    virtual void PlayerControllerChanged(APlayerController* InNewPlayerController) override;

#if WITH_CK_INPUT_HUD

private:
    auto DoActivate()   -> void;
    auto DoDeactivate() -> void;

    auto DoTick(float InDeltaSeconds) -> bool;

    auto OnCVar_MasterChanged(IConsoleVariable* InVar) -> void;

    // Bound to UCommonInputSubsystem::OnInputMethodChangedNative (a native multicast delegate — no UFUNCTION, and
    // none is possible here: this whole block is compiled out in Shipping).
    auto OnInputMethodChanged(ECommonInputType InNewInputType) -> void;

private:
    TSharedPtr<FCk_InputHud_Model>           _Model;
    TSharedPtr<SCkInputHud_Root>             _RootWidget;

    // Read for the two stick magnitudes ONLY. Every button reading comes from the routed events, which is the one
    // place the delivery outcome and the intent that answered a press both exist.
    TSharedPtr<FCkDebug_KeyActivityObserver> _Observer;

    FTSTicker::FDelegateHandle _TickerHandle;

    TWeakObjectPtr<UCommonInputSubsystem> _CommonInput;
    FDelegateHandle                       _InputMethodChangedHandle;

    // Raw pointers to the function-local statics (not owned; they outlive the subsystem).
    TAutoConsoleVariable<int32>* _CVar_Master  = nullptr;
    TAutoConsoleVariable<float>* _CVar_Scale   = nullptr;
    TAutoConsoleVariable<int32>* _CVar_Corner  = nullptr;
    TAutoConsoleVariable<float>* _CVar_Opacity = nullptr;

    // True if this instance registered the console objects (first LP subsystem wins).
    bool _bIsPrimaryConsoleOwner = false;

#endif // WITH_CK_INPUT_HUD
};

// ====================================================================================================================
