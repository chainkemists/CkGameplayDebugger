#include "CkInputHud_Subsystem.h"

#if WITH_CK_INPUT_HUD

#include "CkInputHudOverlay/CkInputHudOverlay_Log.h"
#include "CkInputHudOverlay/Model/CkInputHud_Model.h"
#include "CkInputHudOverlay/Producer/CkInputHud_Collector.h"
#include "CkInputHudOverlay/Widgets/SCkInputHud_Root.h"

#include "CkDebuggerCommon/Devices/CkDebug_KeyActivityObserver.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CommonInputSubsystem.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_hud_subsystem
{
    static_assert(WITH_CK_INPUT_HUD == !UE_BUILD_SHIPPING,
        "The input overlay must compile for every non-shipping configuration and compile out in Shipping");

    // Deliberately BELOW CkEntityDebugOverlay's 100: when both are on, the entity overlay owns the foreground.
    constexpr int32 HudZOrder = 90;

    // The instance currently owning the cvar change-callback. The cvars themselves are process-global statics that
    // are NEVER unregistered, so "does ck.InputOverlay exist yet" cannot discriminate the primary instance — from
    // the second PIE session onward it always exists, every instance would rank itself secondary, and the overlay
    // could never activate again. Ownership is therefore tracked per-instance and handed back on Deinitialize.
    static TWeakObjectPtr<UCk_InputHud_Subsystem> GConsoleOwner;
}

// ====================================================================================================================
// Initialize / Deinitialize
// ====================================================================================================================

auto
    UCk_InputHud_Subsystem::
    Initialize(
        FSubsystemCollectionBase& InCollection)
    -> void
{
    Super::Initialize(InCollection);

    // ECVF_Default, NOT ECVF_Cheat — DELIBERATE. Test packages disable cheat cvars, and a QA-facing input overlay
    // that cannot be turned on in the build QA actually plays is worthless. Do not "fix" this to ECVF_Cheat.
    static TAutoConsoleVariable<int32> CVar_Master(
        TEXT("ck.InputOverlay"),
        ck::input_hud::DefaultOverlayMode,
        TEXT("Ck on-screen input overlay. 0 = off, 1 = keyboard only, 2 = follow the active device."),
        ECVF_Default);

    static TAutoConsoleVariable<float> CVar_Scale(
        TEXT("ck.InputOverlay.Scale"),
        1.0f,
        TEXT("Render scale of the Ck input overlay."),
        ECVF_Default);

    static TAutoConsoleVariable<int32> CVar_Corner(
        TEXT("ck.InputOverlay.Corner"),
        1,
        TEXT("Screen corner the Ck input overlay anchors to. 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right."),
        ECVF_Default);

    static TAutoConsoleVariable<float> CVar_Opacity(
        TEXT("ck.InputOverlay.Opacity"),
        1.0f,
        TEXT("Base render opacity of the Ck input overlay panel (clamped to [0.15, 1])."),
        ECVF_Default);

    _CVar_Master  = &CVar_Master;
    _CVar_Scale   = &CVar_Scale;
    _CVar_Corner  = &CVar_Corner;
    _CVar_Opacity = &CVar_Opacity;

    // One owner per session (multi-player PIE creates one subsystem per local player); the weak pointer hands
    // ownership to the next session's first instance once the previous owner deinitializes.
    _bIsPrimaryConsoleOwner = NOT ck_input_hud_subsystem::GConsoleOwner.IsValid();

    if (_bIsPrimaryConsoleOwner)
    {
        ck_input_hud_subsystem::GConsoleOwner = this;

        _CVar_Master->AsVariable()->SetOnChangedCallback(
            FConsoleVariableDelegate::CreateUObject(this, &UCk_InputHud_Subsystem::OnCVar_MasterChanged));
    }
    else
    {
        ck::input_hud::Log(TEXT("UCk_InputHud_Subsystem: secondary instance — console objects owned by primary"));
    }

    ck::input_hud::Log(TEXT("UCk_InputHud_Subsystem initialized"));
}

auto
    UCk_InputHud_Subsystem::
    Deinitialize()
    -> void
{
    DoDeactivate();

    if (_CVar_Master != nullptr && _bIsPrimaryConsoleOwner)
    {
        _CVar_Master->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate{});
    }

    if (_bIsPrimaryConsoleOwner)
    { ck_input_hud_subsystem::GConsoleOwner.Reset(); }

    _CVar_Master  = nullptr;
    _CVar_Scale   = nullptr;
    _CVar_Corner  = nullptr;
    _CVar_Opacity = nullptr;

    ck::input_hud::Log(TEXT("UCk_InputHud_Subsystem deinitialized"));

    Super::Deinitialize();
}

auto
    UCk_InputHud_Subsystem::
    PlayerControllerChanged(
        APlayerController* InNewPlayerController)
    -> void
{
    Super::PlayerControllerChanged(InNewPlayerController);

    // The master cvar is a process-global static that SURVIVES PIE restarts, but this subsystem is per-LocalPlayer
    // and recreated each session — with the cvar already non-zero no change event ever fires, so a session started
    // with the overlay left on would sit inactive until the toggle is cycled. Level-trigger here instead:
    // Initialize is too early, since DoActivate needs the LP's viewport client and does not retry.
    if (_bIsPrimaryConsoleOwner && ck::IsValid(InNewPlayerController) &&
        _CVar_Master != nullptr && _CVar_Master->GetValueOnGameThread() != 0)
    {
        DoActivate();   // no-op when already active
    }
}

// ====================================================================================================================
// Activation
// ====================================================================================================================

auto
    UCk_InputHud_Subsystem::
    DoActivate()
    -> void
{
    using namespace ck_input_hud_subsystem;

    if (_RootWidget.IsValid())
    { return; }

    auto* LocalPlayer = GetLocalPlayer();
    if (ck::Is_NOT_Valid(LocalPlayer))
    {
        ck::input_hud::Warning(TEXT("DoActivate: no local player — input overlay not shown"));
        return;
    }

    auto* ViewportClient = LocalPlayer->ViewportClient.Get();
    if (ck::Is_NOT_Valid(ViewportClient))
    {
        ck::input_hud::Warning(TEXT("DoActivate: no viewport client — input overlay not shown"));
        return;
    }

    _Model = MakeShared<FCk_InputHud_Model>();

    // The cvars are function-local statics that outlive every subsystem instance, so the widget captures THEM
    // rather than `this` — a widget the viewport somehow outlives the subsystem with still reads a live value
    // instead of a dangling UObject.
    auto* CVarCorner  = _CVar_Corner;
    auto* CVarOpacity = _CVar_Opacity;
    auto* CVarScale  = _CVar_Scale;
    auto* CVarMaster = _CVar_Master;

    _RootWidget = SNew(SCkInputHud_Root)
        .Model(_Model)
        .Corner_Lambda([CVarCorner]() -> int32
        {
            return CVarCorner != nullptr ? CVarCorner->GetValueOnGameThread() : 1;
        })
        .Opacity_Lambda([CVarOpacity]() -> float
        {
            return CVarOpacity != nullptr ? CVarOpacity->GetValueOnGameThread() : 1.0f;
        })
        .Scale_Lambda([CVarScale]() -> float
        {
            return CVarScale != nullptr ? CVarScale->GetValueOnGameThread() : 1.0f;
        })
        .Mode_Lambda([CVarMaster]() -> int32
        {
            return CVarMaster != nullptr ? CVarMaster->GetValueOnGameThread() : 1;
        });

    ViewportClient->AddViewportWidgetContent(_RootWidget.ToSharedRef(), HudZOrder);

    if (FSlateApplication::IsInitialized())
    {
        _Observer = MakeShared<FCkDebug_KeyActivityObserver>();

        constexpr auto PreProcessorIndex = 0;
        FSlateApplication::Get().RegisterInputPreProcessor(_Observer, PreProcessorIndex);
    }

    if (auto* CommonInput = UCommonInputSubsystem::Get(LocalPlayer);
        ck::IsValid(CommonInput))
    {
        _CommonInput = CommonInput;

        // GetCurrentInputType ONLY. Get_ActiveControllerData ensures when no controller data matches the live
        // device, which a debug HUD must never provoke.
        _Model->Set_ActiveInputType(CommonInput->GetCurrentInputType());

        _InputMethodChangedHandle = CommonInput->OnInputMethodChangedNative.AddUObject(
            this, &UCk_InputHud_Subsystem::OnInputMethodChanged);
    }

    _TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UCk_InputHud_Subsystem::DoTick),
        0.0f);

    ck::input_hud::Log(TEXT("Input overlay activated"));
}

auto
    UCk_InputHud_Subsystem::
    DoDeactivate()
    -> void
{
    if (_TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_TickerHandle);
        _TickerHandle.Reset();
    }

    if (_Observer.IsValid())
    {
        if (FSlateApplication::IsInitialized())
        { FSlateApplication::Get().UnregisterInputPreProcessor(_Observer); }
    }

    if (_RootWidget.IsValid())
    {
        if (const auto* LocalPlayer = GetLocalPlayer();
            ck::IsValid(LocalPlayer))
        {
            if (auto* ViewportClient = LocalPlayer->ViewportClient.Get();
                ck::IsValid(ViewportClient))
            {
                ViewportClient->RemoveViewportWidgetContent(_RootWidget.ToSharedRef());
            }
        }

        _RootWidget.Reset();
    }

    if (_Observer.IsValid())
    {
        // Viewport focus loss (alt-tab, an editor panel click) means the release edge never reaches the observer,
        // so a key held at deactivation would read held forever on the next activation.
        _Observer->Clear();
        _Observer.Reset();
    }

    if (auto* CommonInput = _CommonInput.Get();
        ck::IsValid(CommonInput) && _InputMethodChangedHandle.IsValid())
    {
        CommonInput->OnInputMethodChangedNative.Remove(_InputMethodChangedHandle);
    }

    _InputMethodChangedHandle.Reset();
    _CommonInput.Reset();

    if (_Model.IsValid())
    { _Model->Reset(); }

    _Model.Reset();

    ck::input_hud::Log(TEXT("Input overlay deactivated"));
}

// ====================================================================================================================
// Per-frame tick
// ====================================================================================================================

auto
    UCk_InputHud_Subsystem::
    DoTick(
        float InDeltaSeconds)
    -> bool
{
    if (NOT _Model.IsValid() || NOT _RootWidget.IsValid())
    { return true; } // keep ticking; deactivation unregisters us

    const auto* LocalPlayer = GetLocalPlayer();
    if (ck::Is_NOT_Valid(LocalPlayer))
    { return true; }

    auto Params = FCk_InputHud_CollectParams{};
    Params.World      = LocalPlayer->GetWorld();
    Params.Observer   = _Observer.Get();
    Params.NowSeconds = FApp::GetCurrentTime();

    FCk_InputHud_Collector::Collect(Params, *_Model);

    return true;
}

// ====================================================================================================================
// CVar / device callbacks
// ====================================================================================================================

auto
    UCk_InputHud_Subsystem::
    OnCVar_MasterChanged(
        IConsoleVariable* InVar)
    -> void
{
    if (InVar == nullptr)
    { return; }

    if (InVar->GetInt() != 0)
    {
        DoActivate();
    }
    else
    {
        DoDeactivate();
    }
}

auto
    UCk_InputHud_Subsystem::
    OnInputMethodChanged(
        ECommonInputType InNewInputType)
    -> void
{
    if (NOT _Model.IsValid())
    { return; }

    _Model->Set_ActiveInputType(InNewInputType);
}

// --------------------------------------------------------------------------------------------------------------------

#else // WITH_CK_INPUT_HUD

// Shipping stub — the UCLASS still exists for UHT, but the subsystem does nothing at all.

auto
    UCk_InputHud_Subsystem::
    Initialize(
        FSubsystemCollectionBase& InCollection)
    -> void
{
    Super::Initialize(InCollection);
}

auto
    UCk_InputHud_Subsystem::
    Deinitialize()
    -> void
{
    Super::Deinitialize();
}

auto
    UCk_InputHud_Subsystem::
    PlayerControllerChanged(
        APlayerController* InNewPlayerController)
    -> void
{
    Super::PlayerControllerChanged(InNewPlayerController);
}

#endif // WITH_CK_INPUT_HUD

// --------------------------------------------------------------------------------------------------------------------
