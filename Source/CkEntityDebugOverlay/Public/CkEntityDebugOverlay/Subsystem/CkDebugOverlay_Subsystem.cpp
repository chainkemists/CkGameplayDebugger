#include "CkDebugOverlay_Subsystem.h"

#if WITH_CK_DEBUG_OVERLAY

#include "CkEntityDebugOverlay/CkEntityDebugOverlay_Log.h"

// Already included via the header (History, Layout, Model, Provider, Selection).
// Only add the extras the header omits:
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Resolve.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_DistanceLod.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_Present.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_SelectionHud.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"

#include "CkCore/Diagnostics/CkDiagnosticVisibility.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

// B2 — marker billboards (shared FCkDebug_EntityMarkers preview, UDebugDrawService callback).
// Viewport DPI scale — world plates are positioned in DPI-scaled Slate units.
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/Pawn.h"
#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#endif

// Locally-possessed-pawn marker suppression (owning-actor chain lookup).
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

// Double-tap EcsDebuggerFocusKey → open the focused entity in the CK ECS Debugger.
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"

// ====================================================================================================================

namespace
{
    // Overlay widget Z-order — sits above game UI but below modal dialogs.
    constexpr int32 OverlayZOrder = 100;

    // NOTE: the `ck.DebugOverlay.NearPlates` cvar now lives in CkDebugOverlay_Present.cpp
    // (alongside Build_WorldTags, its only reader). The overlay popover reads it by name.

    // Ejected-PIE discrimination lives in ck::DebugViewportView (CkDebuggerCommon),
    // shared with the ECS viewport picker and the focus-entity helper.


}

// ====================================================================================================================
// Initialize / Deinitialize
// ====================================================================================================================

auto
    UCk_DebugOverlay_Subsystem::
    Initialize(
        FSubsystemCollectionBase& InCollection)
    -> void
{
    Super::Initialize(InCollection);

    // ---- Master CVar and commands ----
    // Console objects must be registered only once across all local-player subsystem instances
    // (multi-player PIE creates one subsystem per local player). We use a function-local static
    // for the CVar (safe: static initializes only once) and only create the command TUniquePtr
    // objects for the first registered instance (the "primary" overlay). In practice the dev
    // overlay is only useful for the first local player.
    //
    // BATCH-VERIFY: if multi-player overlay support is needed, factor the console commands
    // into a separate game-instance subsystem.
    static TAutoConsoleVariable<int32> CVar_Master(
        TEXT("ck.DebugOverlay"),
        0,
        TEXT("Enable (1) / disable (0) the Ck on-screen entity debug overlay."),
        ECVF_Cheat);

    _CVar_Master = &CVar_Master;

    // Only the first instance owns the commands.  IConsoleManager::FindConsoleObject lets us
    // detect whether they already exist.
    const bool bFirstInstance =
        (IConsoleManager::Get().FindConsoleObject(TEXT("ck.DebugOverlay.Next")) == nullptr);
    _bIsPrimaryConsoleOwner = bFirstInstance;

    if (bFirstInstance)
    {
        _CVar_Master->AsVariable()->SetOnChangedCallback(
            FConsoleVariableDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::OnCVar_MasterChanged));

        _Cmd_Select = MakeUnique<FAutoConsoleCommand>(TEXT("ck.DebugOverlay.Select"),
            TEXT("Refresh candidates and select the best target."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Select));
        _Cmd_Settings = MakeUnique<FAutoConsoleCommand>(TEXT("ck.DebugOverlay.Settings"),
            TEXT("Open/close runtime Overlay settings."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Settings));
        _Cmd_Family = MakeUnique<FAutoConsoleCommand>(TEXT("ck.DebugOverlay.Family"),
            TEXT("Toggle the selected family."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Family));
        _Cmd_Next = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.Next"),
            TEXT("Select the next screen-right numbered candidate until the aim target changes."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Next));

        _Cmd_Prev = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.Prev"),
            TEXT("Select the previous screen-left numbered candidate until the aim target changes."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Prev));

        _Cmd_Lock = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.Lock"),
            TEXT("Toggle the current selection lock. Data-card pinning remains independent."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Lock));

        _Cmd_Layout_Next = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.Layout.Next"),
            TEXT("Cycle to the next debug overlay layout."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Layout_Next));

        _Cmd_Layout_Prev = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.Layout.Prev"),
            TEXT("Cycle to the previous debug overlay layout."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Layout_Prev));

        _Cmd_UnpinAll = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.UnpinAll"),
            TEXT("Release all pinned overlay cards."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_UnpinAll));

        _Cmd_Help = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.Help"),
            TEXT("Toggle the full keyboard-hints legend."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Help));

        _Cmd_World = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
            TEXT("ck.DebugOverlay.World"),
            TEXT("Override which PIE world the overlay targets. Args: 'next' or <integer index>."),
            FConsoleCommandWithWorldAndArgsDelegate::CreateUObject(
                this, &UCk_DebugOverlay_Subsystem::DoCmd_World));
    }
    else
    {
        ck::debug_overlay::Log(TEXT("UCk_DebugOverlay_Subsystem: secondary instance — console commands owned by primary"));
    }

    Register_InputProcessor();

    _SelectionSessionInvalidated = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddUObject(
        this, &UCk_DebugOverlay_Subsystem::DoDeactivate);
    _SelectionWorldInvalidated = ck::DebugSessionLifecycle::Get_OnWorldInvalidated().AddWeakLambda(this,
        [this](UWorld* InWorld)
        {
            if (_SelectionWorld.Get() == InWorld || GetWorld() == InWorld)
            { DoDeactivate(); }
        });

    // ---- Seed the active layout index ----
    // The per-USER layout the quick-switcher last selected wins over the project's
    // StartingLayout (a developer's live switch should survive their next PIE session without
    // touching shared config); both fall back to the first configured layout.
    if (const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>())
    {
        const auto Find_LayoutIndex = [Settings](const FGameplayTag& InTag) -> int32
        {
            if (NOT InTag.IsValid())
            { return INDEX_NONE; }

            for (auto Idx = 0; Idx < Settings->Layouts.Num(); ++Idx)
            {
                if (Settings->Layouts[Idx].LayoutTag == InTag)
                { return Idx; }
            }
            return INDEX_NONE;
        };

        const auto* InputSettings = GetDefault<UCk_DebugOverlay_InputSettings>();

        _ActiveLayoutIndex = InputSettings != nullptr
            ? Find_LayoutIndex(InputSettings->LastActiveLayout)
            : INDEX_NONE;

        if (_ActiveLayoutIndex == INDEX_NONE)
        { _ActiveLayoutIndex = Find_LayoutIndex(Settings->StartingLayout); }

        // Fall back to first layout if neither tag resolves.
        if (_ActiveLayoutIndex == INDEX_NONE && NOT Settings->Layouts.IsEmpty())
        { _ActiveLayoutIndex = 0; }
    }

    ck::debug_overlay::Log(TEXT("UCk_DebugOverlay_Subsystem initialized"));
}

auto
    UCk_DebugOverlay_Subsystem::
    Deinitialize()
    -> void
{
    DoDeactivate();
    if (_InputProcessor.IsValid())
    {
        if (FSlateApplication::IsInitialized())
        { FSlateApplication::Get().UnregisterInputPreProcessor(_InputProcessor); }
        _InputProcessor.Reset();
    }
    ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SelectionSessionInvalidated);
    ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Remove(_SelectionWorldInvalidated);
    _Cmd_Select.Reset();
    _Cmd_Settings.Reset();
    _Cmd_Family.Reset();

    // Release console objects owned by this instance (only the primary instance owns them).
    // FAutoConsoleCommand destructor unregisters the command; reset in reverse-init order.
    _Cmd_World.Reset();
    _Cmd_Help.Reset();
    _Cmd_UnpinAll.Reset();
    _Cmd_Layout_Prev.Reset();
    _Cmd_Layout_Next.Reset();
    _Cmd_Lock.Reset();
    _Cmd_Prev.Reset();
    _Cmd_Next.Reset();

    // If we owned the CVar callback, remove it to avoid a dangling UObject delegate.
    // _CVar_Master is a static; it outlives the subsystem.
    if (_CVar_Master && _bIsPrimaryConsoleOwner)
    {
        _CVar_Master->AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate{});
    }
    _CVar_Master = nullptr;

    ck::debug_overlay::Log(TEXT("UCk_DebugOverlay_Subsystem deinitialized"));

    Super::Deinitialize();
}

auto
    UCk_DebugOverlay_Subsystem::
    PlayerControllerChanged(
        APlayerController* InNewPlayerController)
    -> void
{
    Super::PlayerControllerChanged(InNewPlayerController);
    Reset_SelectionSession();
    Register_InputProcessor();

    // The master cvar is a process-global static that SURVIVES PIE restarts, but this
    // subsystem is per-LocalPlayer and recreated each session — with the cvar already
    // at 1 no change event ever fires, so a session started with the overlay left on
    // sits inactive until the toggle is cycled. Level-trigger here instead: this hook
    // fires once the world/viewport are up (Initialize is too early — DoActivate needs
    // the LP's viewport client and does not retry). Primary-gated to match the toggle
    // path (only the primary instance owns the cvar callback).
    if (_bIsPrimaryConsoleOwner && ck::IsValid(InNewPlayerController) &&
        _CVar_Master != nullptr && _CVar_Master->GetValueOnGameThread() != 0)
    {
        DoActivate();   // no-op when already active
    }
}

auto UCk_DebugOverlay_Subsystem::Register_InputProcessor() -> void
{
    if (NOT _bIsPrimaryConsoleOwner || _InputProcessor.IsValid() || NOT FSlateApplication::IsInitialized())
    { return; }

    const auto WeakSubsystem = TWeakObjectPtr<UCk_DebugOverlay_Subsystem>(this);
    _InputProcessor = MakeShared<FCkDebugOverlay_InputProcessor>(
        [WeakSubsystem]()
        {
            const auto* Subsystem = WeakSubsystem.Get();
            return Subsystem != nullptr && Subsystem->CanHandle_SelectionInput();
        },
        [WeakSubsystem]()
        {
            const auto* Subsystem = WeakSubsystem.Get();
            return Subsystem != nullptr && Subsystem->CanHandle_GlobalInput();
        },
        [WeakSubsystem]()
        {
            const auto* Subsystem = WeakSubsystem.Get();
            return Subsystem != nullptr && Subsystem->_RootWidget.IsValid();
        },
        [WeakSubsystem](const ECkDebugOverlayGlobalInputAction InAction)
        {
            auto* Subsystem = WeakSubsystem.Get();
            if (Subsystem == nullptr || Subsystem->_CVar_Master == nullptr)
            { return; }

            if (InAction == ECkDebugOverlayGlobalInputAction::Deactivate)
            {
                if (Subsystem->_CVar_Master->GetValueOnGameThread() != 0)
                { Subsystem->_CVar_Master->AsVariable()->Set(0, ECVF_SetByConsole); }
                Subsystem->DoDeactivate();
                return;
            }
            if (Subsystem->_CVar_Master->GetValueOnGameThread() == 0)
            { Subsystem->_CVar_Master->AsVariable()->Set(1, ECVF_SetByConsole); }
            Subsystem->DoActivate();
            if (InAction == ECkDebugOverlayGlobalInputAction::Settings)
            { Subsystem->DoCmd_Settings(); }
        });
    _InputProcessor->SetSelectionBindings(*GetDefault<UCk_DebugOverlay_InputSettings>());
    FSlateApplication::Get().RegisterInputPreProcessor(_InputProcessor.ToSharedRef());
}

// ====================================================================================================================
// Activation
// ====================================================================================================================

auto
    UCk_DebugOverlay_Subsystem::
    DoActivate()
    -> void
{
    if (_RootWidget.IsValid())
    { return; }

    const auto* LocalPlayer = GetLocalPlayer();
    if (ck::Is_NOT_Valid(LocalPlayer))
    {
        ck::debug_overlay::Warning(TEXT("DoActivate: no local player — overlay not shown"));
        return;
    }

    auto* ViewportClient = LocalPlayer->ViewportClient.Get();
    if (ck::Is_NOT_Valid(ViewportClient))
    {
        ck::debug_overlay::Warning(TEXT("DoActivate: no viewport client — overlay not shown"));
        return;
    }

    // Instantiate the root Slate widget.
    _RootWidget = SNew(SCkDebugOverlay_Root);

    // BATCH-VERIFY: SNew returns TSharedRef; TSharedPtr assignment compiles but confirm
    // that SCkDebugOverlay_Root::Construct takes FArguments correctly.
    ViewportClient->AddViewportWidgetContent(_RootWidget.ToSharedRef(), OverlayZOrder);
    _SelectionHud = SNew(SCkDebugOverlay_SelectionHud).Visibility(EVisibility::HitTestInvisible);
    ViewportClient->AddViewportWidgetContent(_SelectionHud.ToSharedRef(), OverlayZOrder + 1);
    Reset_SelectionSession();

    // Lazy-create providers (once per subsystem lifetime after the first activation).
    if (_Providers.IsEmpty())
    {
        _Providers = FCk_DebugOverlay_Registry::Get().CreateAll();
    }

    // One-time layout validation: warn about any provider tags in the active layout
    // that have no matching registered provider.
    if (const auto* Layout = Resolve_ActiveLayout())
    {
        FGameplayTagContainer KnownTags;
        for (const auto& Provider : _Providers)
        {
            if (Provider) { KnownTags.AddTag(Provider->Get_ProviderTag()); }
        }
        for (const auto& Problem : ck_debugoverlay::Validate_Layout(*Layout, KnownTags))
        {
            ck::debug_overlay::Warning(TEXT("{}"), *Problem);
        }
    }

    // (Re-)allocate History. Reset on each activation so stale entity records don't persist
    // across PIE stop/start cycles.
    _History = MakeUnique<FCk_DebugOverlay_History>();

    // The process-lifetime input pre-processor is registered before activation so plain comma
    // can open the overlay. Refresh its live per-user bindings on every activation.
    Register_InputProcessor();
    if (_InputProcessor.IsValid())
    { _InputProcessor->SetSelectionBindings(*GetDefault<UCk_DebugOverlay_InputSettings>()); }

    // Suppress engine on-screen debug text (AddOnScreenDebugMessage / ck::Trace — same
    // channel) while the overlay is active: the plate now owns the top-left corner and
    // the tall height budget. Prior value restored on deactivate. Primary-owner-gated
    // so split-screen LP subsystems don't double-save (the second save would capture
    // the already-suppressed 'false' and the restore would stick).
    if (_bIsPrimaryConsoleOwner && ck::IsValid(GEngine))
    {
        _PriorOnScreenMessagesEnabled = GEngine->bEnableOnScreenDebugMessages;
        GEngine->bEnableOnScreenDebugMessages = false;
    }

    // Register the per-frame ticker.
    _TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoTick),
        0.0f);

    ck::debug_overlay::Log(TEXT("Overlay activated"));
}

auto
    UCk_DebugOverlay_Subsystem::
    DoDeactivate()
    -> void
{
    Close_SelectionSettings();
    if (_SelectionHud.IsValid())
    {
        if (const auto* LocalPlayer = GetLocalPlayer(); ck::IsValid(LocalPlayer))
        {
            if (auto* Viewport = LocalPlayer->ViewportClient.Get(); ck::IsValid(Viewport))
            { Viewport->RemoveViewportWidgetContent(_SelectionHud.ToSharedRef()); }
        }
        _SelectionHud.Reset();
    }
    Reset_SelectionSession();

    // Restore engine on-screen debug text to its pre-activation state. Guarded on the
    // ticker being live so a redundant DoDeactivate (Deinitialize after a cvar-off)
    // doesn't restore twice.
    if (_bIsPrimaryConsoleOwner && _TickerHandle.IsValid() && ck::IsValid(GEngine))
    {
        GEngine->bEnableOnScreenDebugMessages = _PriorOnScreenMessagesEnabled;
    }

    if (_TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_TickerHandle);
        _TickerHandle.Reset();
    }

    _PinnedEntities.Reset();

    if (_RootWidget.IsValid())
    {
        const auto* LocalPlayer = GetLocalPlayer();
        if (ck::IsValid(LocalPlayer))
        {
            if (auto* ViewportClient = LocalPlayer->ViewportClient.Get();
                ck::IsValid(ViewportClient))
            {
                ViewportClient->RemoveViewportWidgetContent(_RootWidget.ToSharedRef());
            }
        }
        _RootWidget.Reset();
    }

    _History.Reset();

    _FocusedEntity = FCk_Handle{};
    _LastSyncedEntity = FCk_Handle{};
    _LastLockKeyPressTime = -1.0;
    _LastEcsFocusKeyPressTime = -1.0;
    _LastUnpinAllKeyPressTime = -1.0;
    _LastHelpKeyPressTime = -1.0;
    _LastCycleLayoutKeyPressTime = -1.0;
    _LodTier = ECk_DebugOverlay_LodTier::Full;

    ck::debug_overlay::Log(TEXT("Overlay deactivated"));
}

// ====================================================================================================================
// Per-frame tick
// ====================================================================================================================

auto
    UCk_DebugOverlay_Subsystem::
    DoTick(
        float InDeltaSeconds)
    -> bool
{
    if (NOT _RootWidget.IsValid())
    { return true; } // keep ticking; deactivation will unregister us

    auto* World = Resolve_ActiveWorld();
    if (ck::Is_NOT_Valid(World) || NOT World->HasBegunPlay())
    { return true; }

    const auto* Layout = Resolve_ActiveLayout();
    if (Layout == nullptr)
    { return true; }

    // ---- 1. Compute viewpoint ----
    // Computed BEFORE gather so its location drives the marker distance cull.
    // Ejected PIE (F8): the player camera freezes but the user is flying the editor
    // viewport — hover/focus selection must follow THAT camera.
    auto Viewpoint = ck_debugoverlay::FViewpoint{};
    auto* PC = World->GetFirstPlayerController();

    _ViewpointIsEjected = false;
#if WITH_EDITOR
    if (auto* LEVC = ck::DebugViewportView::TryGet_LevelEditorViewport())
    {
        Viewpoint.Location  = LEVC->GetViewLocation();
        Viewpoint.Forward   = LEVC->GetViewRotation().Vector();
        _ViewpointIsEjected = true;
    }
#endif

    if (NOT _ViewpointIsEjected && ck::IsValid(PC))
    {
        auto CamLoc = FVector::ZeroVector;
        auto CamRot = FRotator::ZeroRotator;
        PC->GetPlayerViewPoint(CamLoc, CamRot);
        Viewpoint.Location = CamLoc;
        Viewpoint.Forward  = CamRot.Vector();
    }

    // Cull origin for the marker/candidate distance gate — only meaningful when a real
    // viewpoint exists (ejected editor camera or a valid PC). Otherwise leave unset so
    // the gather doesn't cull around the world origin.
    auto CullOrigin = TOptional<FVector>{};
    if (_ViewpointIsEjected || ck::IsValid(PC))
    { CullOrigin = Viewpoint.Location; }

    auto CandidateHandles = TArray<FCk_Handle>{};
    auto Candidates = TArray<ck_debugoverlay::FCandidate>{};
    Update_Selection(World, Viewpoint, CandidateHandles, Candidates);
    auto FocusEntity = _FocusedEntity;

    // ---- 5. Time + double-tap gesture detection ----
    // Input is sampled from the global Slate pre-processor (not PC->WasInputKeyJustPressed)
    // so the gestures keep working while the user is ejected from PIE (F8).
    const auto Now = FPlatformTime::Seconds();

    if (const auto* Settings = GetDefault<UCk_DebugOverlay_InputSettings>();
        Settings != nullptr && _InputProcessor.IsValid())
    {
        const auto Window = Settings->LockDoubleTapWindowSeconds;

        // Evaluate every press captured since the previous overlay tick. Using the
        // input-event timestamps preserves rapid duplicate presses even when a slow
        // frame queues both taps before the subsystem polls the pre-processor.
        const auto WasDoubleTapped = [&](const FKey& InKey, double& InOutLastPress) -> bool
        {
            auto WasDoubleTap = false;
            for (const auto PressTime : _InputProcessor->Consume_PressTimes(InKey))
            {
                const auto TimeSinceLast = static_cast<float>(PressTime - InOutLastPress);
                if (InOutLastPress >= 0.0 && TimeSinceLast >= 0.0f && TimeSinceLast <= Window)
                {
                    InOutLastPress = -1.0;
                    WasDoubleTap = true;
                    continue;
                }

                InOutLastPress = PressTime;
            }
            return WasDoubleTap;
        };

        // Double-tap LockKey (default Left Shift): PIN / UNPIN the focused entity. The
        // primary card keeps auto-following unless separately selection-locked with held Select;
        // each data pin gets its own persistent side-by-side card.
        if (WasDoubleTapped(Settings->LockKey, _LastLockKeyPressTime))
        {
            if (ck::IsValid(FocusEntity))
            {
                const auto ExistingIdx = _PinnedEntities.IndexOfByPredicate(
                    [&FocusEntity](const FCk_Handle& InPinned){ return InPinned == FocusEntity; });

                if (ExistingIdx != INDEX_NONE)
                {
                    _PinnedEntities.RemoveAt(ExistingIdx);
                    ck::debug_overlay::Log(TEXT("Unpinned entity (double-tap) — {} pinned"), _PinnedEntities.Num());
                }
                else
                {
                    _PinnedEntities.Add(FocusEntity);
                    ck::debug_overlay::Log(TEXT("Pinned entity (double-tap) — {} pinned"), _PinnedEntities.Num());
                }
            }
        }

        // Double-tap UnpinAllKey (default Backspace): release every pinned card at once.
        if (WasDoubleTapped(Settings->UnpinAllKey, _LastUnpinAllKeyPressTime))
        {
            if (_PinnedEntities.Num() > 0)
            {
                _PinnedEntities.Reset();
                ck::debug_overlay::Log(TEXT("Released all pins (double-tap)"));
            }
        }

        // Double-tap CycleLayoutKey (default L): step the active layout — same step as
        // `ck.DebugOverlay.Layout.Next`, and the card's corner chip shows where you landed.
        if (WasDoubleTapped(Settings->CycleLayoutKey, _LastCycleLayoutKeyPressTime))
        {
            DoCmd_Layout_Next();
        }

        // Double-tap HelpKey (default unbound): toggle the full keyboard-hints legend.
        if (WasDoubleTapped(Settings->HelpKey, _LastHelpKeyPressTime))
        {
            _ShowFullLegend = NOT _ShowFullLegend;
        }

        // Double-tap EcsDebuggerFocusKey (default Left Ctrl): open the focused entity in the
        // CK ECS Debugger (no-op if the CkEcsDebugger module isn't loaded).
        if (WasDoubleTapped(Settings->EcsDebuggerFocusKey, _LastEcsFocusKeyPressTime))
        {
            if (ck::IsValid(FocusEntity))
            {
                ck::DebugNav::Goto_Entity(FocusEntity);
                ck::debug_overlay::Log(TEXT("Focused entity sent to ECS Debugger (double-tap)"));
            }
        }

        // Drop any presses we didn't consume so they don't leak into the next tick.
        _InputProcessor->Clear();
    }

    _FocusedEntity = FocusEntity;
    if (ck::DebugSelectionSync::Get_IsOverlayFocusSyncEnabled() && ck::IsValid(FocusEntity))
    {
        if (NOT (_LastSyncedEntity == FocusEntity))
        {
            ck::DebugSelectionSync::Broadcast(FocusEntity, TEXT("EntityDebugOverlay"));
            _LastSyncedEntity = FocusEntity;
        }
    }
    else
    {
        _LastSyncedEntity = FCk_Handle{};
    }

    Update_SelectionHud();

    // ---- 7. Build model ----
    auto Model = FCk_DebugOverlay_EntityModel{};

    if (ck::IsValid(FocusEntity))
    {
        Build_Model(FocusEntity, _Providers, *Layout, Now, Model);
    }

    // ---- 7b. Distance LOD (opt-in) ----
    // Trims the MODEL, before Slate and before the card's row budget, so a step-down is
    // rationed against the same protected ordering and reported through the same omission
    // affordances. Needs a real viewpoint AND the focus entity's world position; without
    // either the card stays Full.
    {
        const auto* LodSettings = GetDefault<UCk_DebugOverlay_Settings>();
        const auto  FocusIdx    = ck::IsValid(FocusEntity)
            ? CandidateHandles.IndexOfByPredicate(
                [&FocusEntity](const FCk_Handle& InHandle){ return InHandle == FocusEntity; })
            : INDEX_NONE;

        if (LodSettings != nullptr && LodSettings->bEnableFocusCardDistanceLod &&
            CullOrigin.IsSet() && Candidates.IsValidIndex(FocusIdx))
        {
            auto Thresholds = FCk_DebugOverlay_LodThresholds{};
            Thresholds.SummaryDistance = LodSettings->FocusCardSummaryDist;
            Thresholds.PillDistance    = LodSettings->FocusCardPillDist;

            const auto FocusDist = static_cast<float>(
                FVector::Dist(CullOrigin.GetValue(), Candidates[FocusIdx].WorldLocation));

            _LodTier = ck_debugoverlay::Resolve_LodTier(FocusDist, Thresholds, _LodTier);
            Model    = ck_debugoverlay::Apply_DistanceLod(Model, _LodTier);
        }
        else
        {
            _LodTier = ECk_DebugOverlay_LodTier::Full;
        }
    }

    // ---- 8. Push to root (focus card + pinned cards + world tags + key hints) ----
    Push_ToRoot(Model, *Layout, CandidateHandles, Candidates, _Providers, PC, Now);

    return true; // keep ticking
}

// ====================================================================================================================
// Helpers
// ====================================================================================================================

auto
    UCk_DebugOverlay_Subsystem::
    Resolve_ActiveWorld() const
    -> UWorld*
{
    // PIE world override via console command.
    if (_WorldOverrideIndex != INDEX_NONE && ck::IsValid(GEngine))
    {
        const auto& Contexts = GEngine->GetWorldContexts();
        if (_WorldOverrideIndex < Contexts.Num())
        {
            auto* W = Contexts[_WorldOverrideIndex].World();
            if (ck::IsValid(W))
            { return W; }
        }
    }

    // Default: the local player's own world.
    const auto* LocalPlayer = GetLocalPlayer();
    if (ck::Is_NOT_Valid(LocalPlayer))
    { return nullptr; }

    return LocalPlayer->GetWorld();
}

auto
    UCk_DebugOverlay_Subsystem::
    Resolve_ActiveLayout() const
    -> const FCk_DebugOverlay_Layout*
{
    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    if (Settings == nullptr)
    { return nullptr; }

    if (Settings->Layouts.IsEmpty())
    { return nullptr; }

    const auto ClampedIdx = FMath::Clamp(
        _ActiveLayoutIndex, 0, Settings->Layouts.Num() - 1);

    return &Settings->Layouts[ClampedIdx];
}

auto
    UCk_DebugOverlay_Subsystem::
    Set_ActiveLayoutIndex(
        int32 InIndex)
    -> void
{
    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    if (Settings == nullptr || Settings->Layouts.IsEmpty())
    { return; }

    _ActiveLayoutIndex = FMath::Clamp(InIndex, 0, Settings->Layouts.Num() - 1);

    // Per-USER persistence. EditorPerProjectUserSettings is the overlay's only per-user config
    // slot, so the remembered layout rides along with the keybinds; the project settings class
    // is Config=Game/DefaultConfig and a runtime gesture must never write shared config.
    if (auto* InputSettings = GetMutableDefault<UCk_DebugOverlay_InputSettings>();
        InputSettings != nullptr)
    {
        InputSettings->LastActiveLayout = Settings->Layouts[_ActiveLayoutIndex].LayoutTag;
        InputSettings->SaveConfig();
    }

    ck::debug_overlay::Log(TEXT("Layout changed to index {}"), _ActiveLayoutIndex);
}

auto
    UCk_DebugOverlay_Subsystem::
    Get_LayoutSwitcherLabel() const
    -> FText
{
    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    if (Settings == nullptr || Settings->Layouts.IsEmpty())
    { return FText::GetEmpty(); }

    const auto Index      = FMath::Clamp(_ActiveLayoutIndex, 0, Settings->Layouts.Num() - 1);
    const auto LayoutName = ck_debugoverlay::Get_LeafName(Settings->Layouts[Index].LayoutTag).ToUpper();

    const auto* InputSettings = GetDefault<UCk_DebugOverlay_InputSettings>();
    const auto  CycleKey      = InputSettings != nullptr && InputSettings->CycleLayoutKey.IsValid()
        ? InputSettings->CycleLayoutKey.GetDisplayName().ToString()
        : FString{ TEXT("(unbound)") };

    // The chip doubles as its own affordance: the overlay root is hit-test invisible, so the
    // switcher can never be a clickable dropdown — it names the gesture that drives it.
    return FText::FromString(ck::Format_UE(TEXT("LAYOUT {} {}/{}  {} x2"),
        LayoutName, Index + 1, Settings->Layouts.Num(), CycleKey));
}

auto
    UCk_DebugOverlay_Subsystem::
    Build_Model(
        const FCk_Handle&                                    InFocusEntity,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        const FCk_DebugOverlay_Layout&                       InLayout,
        double                                               InNow,
        FCk_DebugOverlay_EntityModel&                        OutModel)
    -> void
{
    // Delegates to the shared builder (CkDebugOverlay_Present) so the ECS picker and the
    // overlay produce identical focus cards. History observation is threaded through.
    OutModel = ck_debugoverlay::Build_EntityModel(
        InFocusEntity, InProviders, InLayout, _History.Get(), InNow);
}

auto
    UCk_DebugOverlay_Subsystem::
    Push_ToRoot(
        const FCk_DebugOverlay_EntityModel&                  InModel,
        const FCk_DebugOverlay_Layout&                       InLayout,
        const TArray<FCk_Handle>&                            InCandidateHandles,
        const TArray<ck_debugoverlay::FCandidate>&           InCandidates,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        APlayerController*                                   InPC,
        double                                               InNow)
    -> void
{
    if (ck::diagnostic_visibility::Is_HiddenForStreamerMode())
    { return; }

    if (NOT _RootWidget.IsValid())
    { return; }

    const auto* OverlaySettings = GetDefault<UCk_DebugOverlay_Settings>();
    const auto* InputSettings   = GetDefault<UCk_DebugOverlay_InputSettings>();

    // ---- Plate anchor + width + height budget (settings-driven; cheap no-op when unchanged) ----
    _RootWidget->Set_PlateLayout(
        OverlaySettings ? OverlaySettings->PlateAnchor : ECk_DebugOverlay_PlateAnchor::TopLeft,
        OverlaySettings ? OverlaySettings->PlateWidth  : 720.0f,
        OverlaySettings ? OverlaySettings->PlateMaxHeightFraction : 0.66f);

    // ---- Focus card ----
    // Use the layout's DefaultStyle for the card level; per-provider style applied inside.
    // The settings' PlateFontScale multiplies the layout's own FontScale.
    // Guard against a missing History (shouldn't happen if called only from DoTick while active,
    // but defensive against edge-cases around deactivation ordering).
    if (NOT _History)
    { return; }

    auto CardStyle = InLayout.DefaultStyle;
    CardStyle.FontScale *= OverlaySettings ? OverlaySettings->PlateFontScale : 1.0f;

    // ---- Co-located i/N for the primary focus (screen-space cluster; non-ejected only) ----
    auto FocusCoLocIndex = int32{ INDEX_NONE };
    auto FocusCoLocCount = int32{ 0 };
    if (NOT _ViewpointIsEjected && ck::IsValid(InModel.Entity))
    {
        const auto FocusIdx = InCandidateHandles.IndexOfByPredicate(
            [&InModel](const FCk_Handle& InHandle){ return InHandle == InModel.Entity; });

        if (InCandidates.IsValidIndex(FocusIdx) && InCandidates[FocusIdx].bIsOnScreen)
        {
            const auto ScreenRadius = InputSettings ? InputSettings->CoLocatedScreenRadius : 36.0f;
            const auto RadiusSq     = FMath::Square(ScreenRadius);
            const auto FocusScreen  = InCandidates[FocusIdx].ScreenPos;

            auto Cluster = TArray<int32>{};
            for (auto CandIdx = 0; CandIdx < InCandidates.Num(); ++CandIdx)
            {
                if (InCandidates[CandIdx].bIsOnScreen &&
                    FVector2D::DistSquared(InCandidates[CandIdx].ScreenPos, FocusScreen) <= RadiusSq)
                { Cluster.Add(CandIdx); }
            }

            if (Cluster.Num() > 1)
            {
                FocusCoLocCount = Cluster.Num();
                FocusCoLocIndex = Cluster.IndexOfByKey(FocusIdx);
            }
        }
    }

    // Prune destroyed pins before deriving the primary card's visual state. When the live
    // focus is itself pinned, the duplicate pinned card below is intentionally omitted, so
    // the primary card must carry the cyan pinned ring instead.
    _PinnedEntities.RemoveAll([](const FCk_Handle& InPinned){ return ck::Is_NOT_Valid(InPinned); });
    const auto FocusIsPinned = ck::IsValid(InModel.Entity) && _PinnedEntities.ContainsByPredicate(
        [&InModel](const FCk_Handle& InPinned){ return InPinned == InModel.Entity; });

    // Layout quick-switcher chip — primary card only (pinned cards share the same layout).
    _RootWidget->Set_FocusCardContent(
        InModel, CardStyle, *_History, InNow, _FocusLocked, FocusIsPinned,
        FocusCoLocIndex, FocusCoLocCount, Get_LayoutSwitcherLabel(), Get_SelectionStatus(true));

    // ---- Pinned cards (item 6): dedupe vs the live focus, build models ----
    auto PinnedModels = TArray<FCk_DebugOverlay_EntityModel>{};
    PinnedModels.Reserve(_PinnedEntities.Num());
    for (const auto& Pinned : _PinnedEntities)
    {
        if (ck::IsValid(InModel.Entity) && Pinned == InModel.Entity)
        { continue; }
        PinnedModels.Add(ck_debugoverlay::Build_EntityModel(
            Pinned, InProviders, InLayout, _History.Get(), InNow));
    }
    _RootWidget->Set_PinnedCards(PinnedModels, CardStyle, *_History, InNow);

    // ---- World tags (Slate plates anchored at the entity's screen position) ----
    // ScreenPos comes from ProjectWorldToScreen (raw pixels) divided by the viewport DPI scale
    // (Slate viewport overlays position children in DPI-scaled units) so each plate lands on
    // its diamond marker. The focus entity's plate is highlighted; co-located entities retain
    // their truthful projected anchors and are disambiguated by the selection HUD.
    auto DpiScale = 1.0f;
    if (ck::IsValid(InPC))
    {
        if (auto* PCWorld = InPC->GetWorld())
        { DpiScale = UWidgetLayoutLibrary::GetViewportScale(PCWorld); }
    }
    DpiScale = FMath::Max(0.01f, DpiScale);

    const auto WorldFocus = NOT _FamilyVisible && ck::IsValid(_SelectionRoot) ? _SelectionRoot : InModel.Entity;
    const auto RetainedWorldTagKeys = _RootWidget->Get_AdmittedWorldTagKeys();
    const auto WorldTags = ck_debugoverlay::Build_WorldTags(
        InCandidateHandles, InCandidates, InProviders, InLayout, InPC, _ViewpointIsEjected,
        DpiScale, WorldFocus, &RetainedWorldTagKeys);

    _RootWidget->Update_WorldTags(WorldTags, InNow);

    // ---- Keyboard-hints strip (item 9): built from the live (per-user) key bindings ----
    if (OverlaySettings != nullptr && InputSettings != nullptr)
    {
        const auto KeyName = [](const FKey& InKey) -> FString
        {
            return InKey.IsValid() ? InKey.GetDisplayName().ToString() : FString(TEXT("(unbound)"));
        };

        const auto Compact = FString::Printf(
            TEXT("%s / %s prev / next   hold %s lock   %s x2 pin card   %s x2 unpin-all   %s x2 ECS   %s x2 layout"),
            *KeyName(InputSettings->PreviousKey),
            *KeyName(InputSettings->NextKey),
            *KeyName(InputSettings->SelectKey),
            *KeyName(InputSettings->LockKey),
            *KeyName(InputSettings->UnpinAllKey),
            *KeyName(InputSettings->EcsDebuggerFocusKey),
            *KeyName(InputSettings->CycleLayoutKey));

        const auto Full = FString::Printf(
            TEXT("CK ON-SCREEN DEBUGGER\n")
            TEXT("%s / %s   previous / next entity (relative badges)\n")
            TEXT("tap %s   select best; hold to lock / unlock selection\n")
            TEXT("%s x2   pin / unpin focused entity data card\n")
            TEXT("%s x2   release ALL pins\n")
            TEXT("%s x2   open focused entity in ECS Debugger\n")
            TEXT("%s x2   cycle the active LAYOUT (remembered per user; chip in the card corner)\n")
            TEXT("%s x2   toggle this help\n")
            TEXT("console: ck.DebugOverlay .Select .Lock .Next .Prev .Family .Settings .Layout.Next/.Prev .UnpinAll .Help"),
            *KeyName(InputSettings->PreviousKey),
            *KeyName(InputSettings->NextKey),
            *KeyName(InputSettings->SelectKey),
            *KeyName(InputSettings->LockKey),
            *KeyName(InputSettings->UnpinAllKey),
            *KeyName(InputSettings->EcsDebuggerFocusKey),
            *KeyName(InputSettings->CycleLayoutKey),
            *KeyName(InputSettings->HelpKey));

        _RootWidget->Update_KeyHints(Compact, Full, _ShowFullLegend, OverlaySettings->ShowKeyHints);
    }
}

// ====================================================================================================================
// Marker billboards — UDebugDrawService callback (fires per viewport, after the world renders).
// Delegates to the shared FCkDebug_EntityMarkers preview: tint encodes hierarchy depth,
// non-focused markers are semi-transparent, the focused one is opaque + hover texture + 1.25×.
// ====================================================================================================================

// ====================================================================================================================
// CVar / command callbacks
// ====================================================================================================================

auto
    UCk_DebugOverlay_Subsystem::
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
    UCk_DebugOverlay_Subsystem::
    DoCmd_Next()
    -> void
{
    Cycle_Selection(1);
}

auto
    UCk_DebugOverlay_Subsystem::
    DoCmd_Prev()
    -> void
{
    Cycle_Selection(-1);
}

auto
    UCk_DebugOverlay_Subsystem::
    DoCmd_Lock()
    -> void
{
    if (ck::IsValid(_SelectionLockedEntity))
    {
        _SelectionLockedEntity = FCk_Handle{};
        _SelectionCycleTarget = FCk_Handle{};
        Refresh_SelectionSnapshot(true);
        return;
    }
    if (ck::Is_NOT_Valid(_FocusedEntity))
    { Refresh_SelectionSnapshot(true); }
    _SelectionLockedEntity = _FocusedEntity;
    _FocusLocked = ck::IsValid(_SelectionLockedEntity);
}

auto
    UCk_DebugOverlay_Subsystem::
    DoCmd_UnpinAll()
    -> void
{
    if (_PinnedEntities.Num() > 0)
    {
        _PinnedEntities.Reset();
        ck::debug_overlay::Log(TEXT("Released all pins (console)"));
    }
}

auto
    UCk_DebugOverlay_Subsystem::
    DoCmd_Help()
    -> void
{
    _ShowFullLegend = NOT _ShowFullLegend;
    ck::debug_overlay::Log(TEXT("Key-hints legend: {}"), _ShowFullLegend ? TEXT("FULL") : TEXT("compact"));
}

auto
    UCk_DebugOverlay_Subsystem::
    DoCmd_Layout_Next()
    -> void
{
    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    if (Settings == nullptr || Settings->Layouts.IsEmpty())
    { return; }

    const auto Count = Settings->Layouts.Num();
    Set_ActiveLayoutIndex((FMath::Max(0, _ActiveLayoutIndex) + 1) % Count);
}

auto
    UCk_DebugOverlay_Subsystem::
    DoCmd_Layout_Prev()
    -> void
{
    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    if (Settings == nullptr || Settings->Layouts.IsEmpty())
    { return; }

    const auto Count = Settings->Layouts.Num();
    Set_ActiveLayoutIndex((FMath::Max(0, _ActiveLayoutIndex) - 1 + Count) % Count);
}

auto
    UCk_DebugOverlay_Subsystem::
    DoCmd_World(
        const TArray<FString>& InArgs,
        UWorld*                /*InWorld*/)
    -> void
{
    if (InArgs.IsEmpty())
    {
        ck::debug_overlay::Warning(TEXT("ck.DebugOverlay.World: expected 'next' or an integer index"));
        return;
    }

    if (NOT ck::IsValid(GEngine))
    { return; }

    const auto& Contexts = GEngine->GetWorldContexts();
    const auto  NumWorlds = Contexts.Num();

    if (NumWorlds == 0)
    { return; }

    if (InArgs[0].Equals(TEXT("next"), ESearchCase::IgnoreCase))
    {
        const auto CurrentIdx = (_WorldOverrideIndex == INDEX_NONE) ? -1 : _WorldOverrideIndex;
        _WorldOverrideIndex   = (CurrentIdx + 1) % NumWorlds;
    }
    else
    {
        const auto Idx = FCString::Atoi(*InArgs[0]);
        if (Idx < 0 || Idx >= NumWorlds)
        {
            ck::debug_overlay::Warning(
                TEXT("ck.DebugOverlay.World: index {} out of range [0, {})"), Idx, NumWorlds);
            return;
        }
        _WorldOverrideIndex = Idx;
    }

    ck::debug_overlay::Log(TEXT("World override set to index {}"), _WorldOverrideIndex);
}

#else // WITH_CK_DEBUG_OVERLAY

// The debug overlay is compiled out in Shipping, but the UCLASS still declares Initialize/
// Deinitialize unconditionally so UHT always sees the subsystem. Provide trivial bodies
// that just forward to the base subsystem so the linker is satisfied — the subsystem
// does nothing in this configuration.

void UCk_DebugOverlay_Subsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
    Super::Initialize(InCollection);
}

void UCk_DebugOverlay_Subsystem::Deinitialize()
{
    Super::Deinitialize();
}

void UCk_DebugOverlay_Subsystem::PlayerControllerChanged(APlayerController* InNewPlayerController)
{
    Super::PlayerControllerChanged(InNewPlayerController);
}

#endif // WITH_CK_DEBUG_OVERLAY
