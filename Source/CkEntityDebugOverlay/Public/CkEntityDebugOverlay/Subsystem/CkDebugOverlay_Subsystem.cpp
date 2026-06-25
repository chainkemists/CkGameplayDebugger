#include "CkDebugOverlay_Subsystem.h"

#if WITH_CK_DEBUG_OVERLAY

#include "CkEntityDebugOverlay/CkEntityDebugOverlay_Log.h"

// Already included via the header (History, Layout, Model, Provider, Selection).
// Only add the extras the header omits:
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Resolve.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_Present.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

// B2 — marker billboards (shared FCkDebug_EntityMarkers preview, UDebugDrawService callback).
#include "Debug/DebugDrawService.h"
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

#if WITH_EDITOR
    // Ejected-PIE discriminator — mirrors FCkDebuggerModel_ViewportPicker:
    // GEditor->GetActiveViewport() is the PIE game viewport while possessed and the
    // level editor viewport itself while ejected. Other signals don't reliably flip.
    auto TryGet_LevelEditorViewport() -> FEditorViewportClient*
    {
        if (GEditor == nullptr || GEditor->PlayWorld == nullptr)
        { return nullptr; }

        auto* LEVC = GCurrentLevelEditingViewportClient;
        if (LEVC == nullptr || LEVC->Viewport == nullptr)
        { return nullptr; }

        if (GEditor->GetActiveViewport() != LEVC->Viewport)
        { return nullptr; }

        return LEVC;
    }
#endif

    // Locally possessed pawn (+ controller + attached actors) — their entities get no
    // marker billboard, otherwise a marker sits permanently at screen center. While
    // ejected the pawn is just another world object, so nothing is ignored (mirrors
    // the ECS picker's Ignore Self behavior).
    auto Get_LocalIgnoredActors(UWorld* InWorld, bool InIsEjected) -> TArray<TWeakObjectPtr<const AActor>>
    {
        auto IgnoredActors = TArray<TWeakObjectPtr<const AActor>>{};

        if (InIsEjected || ck::Is_NOT_Valid(InWorld))
        { return IgnoredActors; }

        auto* PC = InWorld->GetFirstPlayerController();
        if (ck::Is_NOT_Valid(PC))
        { return IgnoredActors; }

        auto* LocalPawn = PC->GetPawn().Get();
        if (ck::Is_NOT_Valid(LocalPawn))
        { return IgnoredActors; }

        IgnoredActors.Add(LocalPawn);
        IgnoredActors.Add(PC);

        constexpr auto ResetArray      = true;
        constexpr auto RecurseAttached = true;
        auto AttachedChildren = TArray<AActor*>{};
        LocalPawn->GetAttachedActors(AttachedChildren, ResetArray, RecurseAttached);
        for (auto* AttachedActor : AttachedChildren)
        {
            if (ck::IsValid(AttachedActor))
            {
                IgnoredActors.Add(AttachedActor);
            }
        }

        return IgnoredActors;
    }

    auto Is_EntityOwnedByIgnoredActor(
        const FCk_Handle& InEntity,
        const TArray<TWeakObjectPtr<const AActor>>& InIgnoredActors) -> bool
    {
        if (InIgnoredActors.IsEmpty() || ck::Is_NOT_Valid(InEntity))
        { return false; }

        auto* OwningActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor_Recursive(InEntity);
        if (ck::Is_NOT_Valid(OwningActor))
        { return false; }

        for (const auto& IgnoredActorWeak : InIgnoredActors)
        {
            if (OwningActor == IgnoredActorWeak.Get())
            { return true; }
        }

        return false;
    }
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

        _Cmd_Next = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.Next"),
            TEXT("Cycle focus to the next candidate entity (enables lock)."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Next));

        _Cmd_Prev = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.Prev"),
            TEXT("Cycle focus to the previous candidate entity (enables lock)."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Prev));

        _Cmd_Lock = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.Lock"),
            TEXT("Toggle focus lock on the current entity."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Lock));

        _Cmd_Layout_Next = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.Layout.Next"),
            TEXT("Cycle to the next debug overlay layout."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Layout_Next));

        _Cmd_Layout_Prev = MakeUnique<FAutoConsoleCommand>(
            TEXT("ck.DebugOverlay.Layout.Prev"),
            TEXT("Cycle to the previous debug overlay layout."),
            FConsoleCommandDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::DoCmd_Layout_Prev));

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

    // ---- Seed the active layout index from settings ----
    if (const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>())
    {
        const auto& StartingTag = Settings->StartingLayout;
        for (auto Idx = 0; Idx < Settings->Layouts.Num(); ++Idx)
        {
            if (Settings->Layouts[Idx].LayoutTag == StartingTag)
            {
                _ActiveLayoutIndex = Idx;
                break;
            }
        }
        // Fall back to first layout if starting tag not found.
        if (_ActiveLayoutIndex == INDEX_NONE && !Settings->Layouts.IsEmpty())
        {
            _ActiveLayoutIndex = 0;
        }
    }

    ck::debug_overlay::Log(TEXT("UCk_DebugOverlay_Subsystem initialized"));
}

auto
    UCk_DebugOverlay_Subsystem::
    Deinitialize()
    -> void
{
    DoDeactivate();

    // Release console objects owned by this instance (only the primary instance owns them).
    // FAutoConsoleCommand destructor unregisters the command; reset in reverse-init order.
    _Cmd_World.Reset();
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

    // B2 — marker billboards: front-load the shared marker textures (load + async
    // compilation) so the first DrawMarkers call never sees a placeholder resource.
    _Markers.EnsureTextures();

    // Register on BOTH "Game" and "Editor" show flags so markers keep rendering after
    // the user ejects with F8 (mirrors the picker's registration).
    const auto DrawDelegate = FDebugDrawDelegate::CreateUObject(
        this, &UCk_DebugOverlay_Subsystem::DoDrawMarkers);
    _DebugDrawHandle_Game   = UDebugDrawService::Register(TEXT("Game"),   DrawDelegate);
    _DebugDrawHandle_Editor = UDebugDrawService::Register(TEXT("Editor"), DrawDelegate);

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
    if (_TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_TickerHandle);
        _TickerHandle.Reset();
    }

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

    // B2 — stop drawing marker billboards.
    if (_DebugDrawHandle_Game.IsValid())
    {
        UDebugDrawService::Unregister(_DebugDrawHandle_Game);
        _DebugDrawHandle_Game.Reset();
    }
    if (_DebugDrawHandle_Editor.IsValid())
    {
        UDebugDrawService::Unregister(_DebugDrawHandle_Editor);
        _DebugDrawHandle_Editor.Reset();
    }
    _Markers.Reset();
    _MarkerSuppressed.Empty();
    _FocusedEntityNum = MAX_uint32;

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
    if (ck::Is_NOT_Valid(World))
    { return true; }

    const auto* Layout = Resolve_ActiveLayout();
    if (Layout == nullptr)
    { return true; }

    // ---- 1. Gather candidates ----
    auto CandidateHandles = TArray<FCk_Handle>{};
    auto Candidates       = TArray<ck_debugoverlay::FCandidate>{};

    Gather_Candidates(World, _Providers, CandidateHandles, Candidates);
    _LastFrameCandidates = CandidateHandles;

    // ---- 2. Compute viewpoint ----
    // Ejected PIE (F8): the player camera freezes but the user is flying the editor
    // viewport — hover/focus selection must follow THAT camera.
    auto Viewpoint = ck_debugoverlay::FViewpoint{};
    auto* PC = World->GetFirstPlayerController();

    _ViewpointIsEjected = false;
#if WITH_EDITOR
    if (auto* LEVC = TryGet_LevelEditorViewport())
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

    // ---- 3. Resolve on-screen flags for all candidates ----
    // Ejected: PC-based screen projection reflects the frozen player camera, not the
    // view being rendered — approximate "on screen" as "in front of the editor camera".
    if (_ViewpointIsEjected)
    {
        for (auto CandIdx = 0; CandIdx < Candidates.Num(); ++CandIdx)
        {
            const auto ToCandidate =
                (Candidates[CandIdx].WorldLocation - Viewpoint.Location).GetSafeNormal();
            Candidates[CandIdx].bIsOnScreen =
                FVector::DotProduct(ToCandidate, Viewpoint.Forward) > 0.0f;
        }
    }
    else if (ck::IsValid(PC))
    {
        for (auto CandIdx = 0; CandIdx < Candidates.Num(); ++CandIdx)
        {
            auto ScreenPos = FVector2D{};
            const auto bOnScreen = UGameplayStatics::ProjectWorldToScreen(
                PC, Candidates[CandIdx].WorldLocation, ScreenPos,
                /*bPlayerViewportRelative=*/false);
            Candidates[CandIdx].bIsOnScreen = bOnScreen;
        }
    }

    // ---- 4. Pick focus entity ----
    FCk_Handle FocusEntity{};
    if (_FocusLocked && _LockedCandidateIndex != INDEX_NONE &&
        _LockedCandidateIndex < CandidateHandles.Num())
    {
        FocusEntity = CandidateHandles[_LockedCandidateIndex];
    }
    else
    {
        const auto BestIdx = ck_debugoverlay::Pick_Best(Candidates, Viewpoint);
        if (BestIdx != INDEX_NONE)
        {
            FocusEntity = CandidateHandles[BestIdx];
            // Keep the lock index in sync with best pick when not locked, so
            // Next/Prev starts from the last auto-picked position.
            _LockedCandidateIndex = BestIdx;
        }
    }

    // ---- 5. Time + B3 double-tap lock detection ----
    const auto Now = FPlatformTime::Seconds();

    if (ck::IsValid(PC))
    {
        const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
        if (Settings != nullptr)
        {
            const auto bJustPressed = PC->WasInputKeyJustPressed(Settings->LockKey);
            if (bJustPressed)
            {
                const auto TimeSinceLast = static_cast<float>(Now - _LastLockKeyPressTime);
                if (_LastLockKeyPressTime >= 0.0 &&
                    TimeSinceLast <= Settings->LockDoubleTapWindowSeconds)
                {
                    // Double-tap detected: toggle focus lock.
                    _FocusLocked = NOT _FocusLocked;
                    if (_FocusLocked)
                    {
                        // Pin to the current best-pick index (already updated in step 4).
                        // _LockedCandidateIndex was kept in sync by the unlocked path above.
                        ck::debug_overlay::Log(TEXT("Focus lock: ON (double-tap)"));
                    }
                    else
                    {
                        ck::debug_overlay::Log(TEXT("Focus lock: OFF (double-tap)"));
                    }
                    // Reset so a third tap doesn't re-toggle immediately.
                    _LastLockKeyPressTime = -1.0;
                }
                else
                {
                    // First tap: record time, wait for possible second tap.
                    _LastLockKeyPressTime = Now;
                }
            }

            // Double-tap CycleCoLocatedKey (default Left Alt): cycle focus through
            // candidates within CoLocatedRadius of the current focus and lock on —
            // the only way to reach entities stacked at the same world position.
            if (PC->WasInputKeyJustPressed(Settings->CycleCoLocatedKey))
            {
                const auto TimeSinceLast = static_cast<float>(Now - _LastCycleKeyPressTime);
                if (_LastCycleKeyPressTime >= 0.0 &&
                    TimeSinceLast <= Settings->LockDoubleTapWindowSeconds)
                {
                    if (ck::IsValid(FocusEntity) &&
                        Candidates.IsValidIndex(_LockedCandidateIndex))
                    {
                        const auto  FocusPos  = Candidates[_LockedCandidateIndex].WorldLocation;
                        const auto  RadiusSq  = FMath::Square(Settings->CoLocatedRadius);

                        auto CoLocated = TArray<int32>{};
                        for (auto CandIdx = 0; CandIdx < Candidates.Num(); ++CandIdx)
                        {
                            if (FVector::DistSquared(Candidates[CandIdx].WorldLocation, FocusPos) <= RadiusSq)
                            {
                                CoLocated.Add(CandIdx);
                            }
                        }

                        if (CoLocated.Num() > 1)
                        {
                            const auto CurPos = CoLocated.IndexOfByKey(_LockedCandidateIndex);
                            _LockedCandidateIndex = CoLocated[(CurPos + 1) % CoLocated.Num()];
                            _FocusLocked = true;
                            ck::debug_overlay::Log(
                                TEXT("Cycled co-located focus ({} candidates within {}cm)"),
                                CoLocated.Num(), Settings->CoLocatedRadius);
                        }
                    }
                    _LastCycleKeyPressTime = -1.0;
                }
                else
                {
                    _LastCycleKeyPressTime = Now;
                }
            }

            // Double-tap EcsDebuggerFocusKey (default Left Ctrl): open/select the
            // focused entity in the CK ECS Debugger via the cross-debugger navigator
            // (no-op if the CkEcsDebugger module isn't loaded).
            if (PC->WasInputKeyJustPressed(Settings->EcsDebuggerFocusKey))
            {
                const auto TimeSinceLast = static_cast<float>(Now - _LastEcsFocusKeyPressTime);
                if (_LastEcsFocusKeyPressTime >= 0.0 &&
                    TimeSinceLast <= Settings->LockDoubleTapWindowSeconds)
                {
                    if (ck::IsValid(FocusEntity))
                    {
                        ck::DebugNav::Goto_Entity(FocusEntity);
                        ck::debug_overlay::Log(TEXT("Focused entity sent to ECS Debugger (double-tap)"));
                    }
                    _LastEcsFocusKeyPressTime = -1.0;
                }
                else
                {
                    _LastEcsFocusKeyPressTime = Now;
                }
            }
        }
    }

    // ---- 6. B2 — marker billboards + parent→child links ----
    // The shared _Markers snapshot was rebuilt in Gather_Candidates (same set as the
    // candidates). Here we emit the dashed hierarchy links and stash the per-tick draw
    // state DoDrawMarkers needs (focused entity + pawn-marker suppression).
    if (ck::IsValid(PC))
    {
        _FocusedEntityNum = ck::IsValid(FocusEntity)
            ? static_cast<uint32>(FocusEntity.Get_Entity().Get_EntityNumber())
            : MAX_uint32;

        // The possessed pawn's entities get no marker — it would sit permanently at
        // screen center. They remain candidates (focusable/cyclable/plated) on purpose.
        const auto IgnoredActors = Get_LocalIgnoredActors(World, _ViewpointIsEjected);

        _MarkerSuppressed.Reset();
        for (const auto& CandHandle : CandidateHandles)
        {
            if (Is_EntityOwnedByIgnoredActor(CandHandle, IgnoredActors))
            {
                _MarkerSuppressed.Add(
                    static_cast<uint32>(CandHandle.Get_Entity().Get_EntityNumber()));
            }
        }

        _Markers.DrawLinks(World);

        // Throttled diagnostics (~1/sec): candidate/marker counts + first candidate location.
        if (Now - _LastMarkerLogTime >= 1.0)
        {
            _LastMarkerLogTime = Now;

            const auto FirstCandidateLoc = Candidates.Num() > 0
                ? Candidates[0].WorldLocation.ToString()
                : FString{TEXT("none")};

            ck::debug_overlay::Log(
                TEXT("Markers: Candidates=[{}] Markers=[{}] FirstCandidateLoc=[{}]"),
                CandidateHandles.Num(),
                CandidateHandles.Num() - _MarkerSuppressed.Num(),
                FirstCandidateLoc);
        }
    }

    // ---- 7. Build model ----
    auto Model = FCk_DebugOverlay_EntityModel{};

    if (ck::IsValid(FocusEntity))
    {
        Build_Model(FocusEntity, _Providers, *Layout, Now, Model);
    }

    // ---- 8. Push to root ----
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
    Gather_Candidates(
        UWorld*                                              InWorld,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        TArray<FCk_Handle>&                                  OutHandles,
        TArray<ck_debugoverlay::FCandidate>&                 OutCandidates)
    -> void
{
    OutHandles.Empty();
    OutCandidates.Empty();

    // Delegate enumeration (transform view, pending-kill + PMG debug-shape exclusion,
    // depth walk, shared `ck.Debug.EntityMarkers.MaxDepth` gate) to the shared marker
    // preview. The only overlay-specific rule is the provider filter: an entity is a
    // candidate iff at least one provider is willing to serve it. Markers, links, and
    // candidates are therefore the same set — what you see is what you can focus.
    auto GatherParams = FCkDebug_EntityMarkers::FGatherParams{};
    GatherParams.Filter = [&InProviders](const FCk_Handle& InHandle) -> bool
    {
        for (const auto& Provider : InProviders)
        {
            if (Provider && Provider->CanProvide(InHandle))
            { return true; }
        }
        return false;
    };

    _Markers.Gather(InWorld, GatherParams);

    for (const auto& Entry : _Markers.Get_Entries())
    {
        auto Candidate          = ck_debugoverlay::FCandidate{};
        Candidate.WorldLocation = Entry.WorldPos;
        // bIsOnScreen filled in DoTick after projection.
        Candidate.bIsOnScreen   = false;
        Candidate.Depth         = Entry.Depth;

        OutHandles.Add(Entry.Entity);
        OutCandidates.Add(Candidate);
    }
}

// ====================================================================================================================

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
    if (NOT _RootWidget.IsValid())
    { return; }

    const auto* OverlaySettings = GetDefault<UCk_DebugOverlay_Settings>();

    // ---- Plate anchor + width (settings-driven; cheap no-op when unchanged) ----
    _RootWidget->Set_PlateLayout(
        OverlaySettings ? OverlaySettings->PlateAnchor : ECk_DebugOverlay_PlateAnchor::TopRight,
        OverlaySettings ? OverlaySettings->PlateWidth  : 720.0f);

    // ---- Focus card ----
    // Use the layout's DefaultStyle for the card level; per-provider style applied inside.
    // The settings' PlateFontScale multiplies the layout's own FontScale.
    // Guard against a missing History (shouldn't happen if called only from DoTick while active,
    // but defensive against edge-cases around deactivation ordering).
    if (NOT _History)
    { return; }

    auto CardStyle = InLayout.DefaultStyle;
    CardStyle.FontScale *= OverlaySettings ? OverlaySettings->PlateFontScale : 1.0f;

    _RootWidget->Set_FocusCardContent(InModel, CardStyle, *_History, InNow, _FocusLocked);

    // ---- World tags (B1 — distance-scaled / faded / culled / near-plates) ----
    // Delegates to the shared builder so the ECS picker renders identical world cards.
    const auto WorldTags = ck_debugoverlay::Build_WorldTags(
        InCandidateHandles, InCandidates, InProviders, InLayout, InPC, _ViewpointIsEjected);

    _RootWidget->Update_WorldTags(WorldTags);
}

// ====================================================================================================================
// Marker billboards — UDebugDrawService callback (fires per viewport, after the world renders).
// Delegates to the shared FCkDebug_EntityMarkers preview: tint encodes hierarchy depth,
// non-focused markers are semi-transparent, the focused one is opaque + hover texture + 1.25×.
// ====================================================================================================================

auto
    UCk_DebugOverlay_Subsystem::
    DoDrawMarkers(
        UCanvas*           InCanvas,
        APlayerController* InPC)
    -> void
{
    if (NOT _RootWidget.IsValid())
    { return; }

    if (InCanvas == nullptr)
    { return; }

    auto* World = Resolve_ActiveWorld();
    if (ck::Is_NOT_Valid(World))
    { return; }

    // The draw service fires for every viewport on screen; only draw into ours.
    if (ck::IsValid(InPC) && InPC->GetWorld() != World)
    { return; }

    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();

    auto DrawParams                 = FCkDebug_EntityMarkers::FDrawParams{};
    DrawParams.TileSizePx           = 28.0f * (Settings ? Settings->DiamondScale : 1.0f);
    DrawParams.DefaultAlpha         = 0.45f;
    DrawParams.EmphasizedEntityNum  = _FocusedEntityNum;
    DrawParams.EmphasizedScale      = 1.25f;
    DrawParams.SuppressedEntityNums = &_MarkerSuppressed;

    _Markers.DrawMarkers(InCanvas, DrawParams);
}

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
    if (_LastFrameCandidates.IsEmpty())
    { return; }

    _FocusLocked = true;

    if (_LockedCandidateIndex == INDEX_NONE)
    {
        _LockedCandidateIndex = 0;
    }
    else
    {
        _LockedCandidateIndex = (_LockedCandidateIndex + 1) % _LastFrameCandidates.Num();
    }
}

auto
    UCk_DebugOverlay_Subsystem::
    DoCmd_Prev()
    -> void
{
    if (_LastFrameCandidates.IsEmpty())
    { return; }

    _FocusLocked = true;

    if (_LockedCandidateIndex == INDEX_NONE)
    {
        _LockedCandidateIndex = _LastFrameCandidates.Num() - 1;
    }
    else
    {
        _LockedCandidateIndex =
            (_LockedCandidateIndex - 1 + _LastFrameCandidates.Num()) % _LastFrameCandidates.Num();
    }
}

auto
    UCk_DebugOverlay_Subsystem::
    DoCmd_Lock()
    -> void
{
    _FocusLocked = !_FocusLocked;
    ck::debug_overlay::Log(TEXT("Focus lock: {}"), _FocusLocked ? TEXT("ON") : TEXT("OFF"));
}

auto
    UCk_DebugOverlay_Subsystem::
    DoCmd_Layout_Next()
    -> void
{
    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    if (Settings == nullptr || Settings->Layouts.IsEmpty())
    { return; }

    _ActiveLayoutIndex = (_ActiveLayoutIndex + 1) % Settings->Layouts.Num();
    ck::debug_overlay::Log(TEXT("Layout changed to index {}"), _ActiveLayoutIndex);
}

auto
    UCk_DebugOverlay_Subsystem::
    DoCmd_Layout_Prev()
    -> void
{
    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    if (Settings == nullptr || Settings->Layouts.IsEmpty())
    { return; }

    const auto Count   = Settings->Layouts.Num();
    _ActiveLayoutIndex = (_ActiveLayoutIndex - 1 + Count) % Count;
    ck::debug_overlay::Log(TEXT("Layout changed to index {}"), _ActiveLayoutIndex);
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

// The debug overlay is compiled out (e.g. Test/Shipping, or wherever the UncookedOnly
// CkDebuggerCommon dependency is absent), but the UCLASS still declares Initialize/
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

#endif // WITH_CK_DEBUG_OVERLAY
