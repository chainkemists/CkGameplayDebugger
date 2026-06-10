#include "CkDebugOverlay_Subsystem.h"

#if WITH_CK_DEBUG_OVERLAY

#include "CkEntityDebugOverlay/CkEntityDebugOverlay_Log.h"

// Already included via the header (History, Layout, Model, Provider, Selection).
// Only add the extras the header omits:
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Resolve.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Style/CkDebugOverlay_RenderStyle.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

// CkPmg fragment — debug-draw shape entities are excluded from candidacy in Gather.
#include "CkPmg/CkPmg_Fragment.h"

// Parent→child dotted marker links (one-frame dashed lines, re-drawn per tick).
#include "CkCore/Debug/CkDebugDraw_Utils.h"

// B2 — marker billboards (UDebugDrawService canvas tiles, ECS-picker textures).
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "CanvasItem.h"
#include "GameFramework/Pawn.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
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

    // Hierarchy-depth gate for overlay candidates. -1 = unlimited; 0 = top-level
    // entities only; N = include entities up to N lifetime-owner hops below the
    // registry transient. Tune live: `ck.DebugOverlay.MaxDepth 1`.
    TAutoConsoleVariable<int32> CVar_DebugOverlay_MaxDepth(
        TEXT("ck.DebugOverlay.MaxDepth"),
        -1,
        TEXT("Max hierarchy depth for overlay candidates. -1 = unlimited, 0 = top-level only, N = up to N levels deep."),
        ECVF_Cheat);

    // Ultra-condensed multi-line plates for candidates within NearDist of the camera.
    TAutoConsoleVariable<int32> CVar_DebugOverlay_NearPlates(
        TEXT("ck.DebugOverlay.NearPlates"),
        1,
        TEXT("1 = show ultra-condensed multi-line plates for candidates within NearDist; 0 = single-line pills only."),
        ECVF_Cheat);

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

    // B2 — marker billboards: same textures as the ECS Debugger's viewport picker.
    if (NOT _MarkerTexture.IsValid())
    {
        _MarkerTexture.Reset(LoadObject<UTexture2D>(
            nullptr, TEXT("/CkDebugger/Textures/T_ECSPicker_Marker.T_ECSPicker_Marker")));
    }
    if (NOT _MarkerHoverTexture.IsValid())
    {
        _MarkerHoverTexture.Reset(LoadObject<UTexture2D>(
            nullptr, TEXT("/CkDebugger/Textures/T_ECSPicker_Marker_Hover.T_ECSPicker_Marker_Hover")));
    }

#if WITH_EDITOR
    // Freshly-imported textures compile platform data asynchronously; force completion so
    // GetResource() never returns a half-initialized placeholder (same guard as the picker).
    auto TexturesToFinish = TArray<UTexture*>{};
    if (_MarkerTexture.IsValid())      { TexturesToFinish.Add(_MarkerTexture.Get()); }
    if (_MarkerHoverTexture.IsValid()) { TexturesToFinish.Add(_MarkerHoverTexture.Get()); }
    if (TexturesToFinish.Num() > 0)
    {
        FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);
    }
#endif

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
    _MarkerDraws.Empty();

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
    // Markers are screen-space ECS-diamond icons drawn by DoDrawMarkers (registered
    // with UDebugDrawService, same approach + textures as the ECS Debugger's viewport
    // picker). Here we only snapshot what to draw and emit the dashed hierarchy links.
    if (ck::IsValid(PC))
    {
        const auto FocusedEntityNum = ck::IsValid(FocusEntity)
            ? static_cast<uint32>(FocusEntity.Get_Entity().Get_EntityNumber())
            : static_cast<uint32>(0);

        // Per-tick scratch for the parent→child dotted links.
        auto PosByEntity     = TMap<uint32, FVector>{};
        auto OwnerEntityNums = TArray<uint32>{};
        OwnerEntityNums.Init(MAX_uint32, CandidateHandles.Num());

        // The possessed pawn's entities get no marker — it would sit permanently at
        // screen center. They remain candidates (focusable/cyclable/plated) on purpose.
        const auto IgnoredActors = Get_LocalIgnoredActors(World, _ViewpointIsEjected);

        _MarkerDraws.Reset(CandidateHandles.Num());

        for (auto CandIdx = 0; CandIdx < CandidateHandles.Num(); ++CandIdx)
        {
            auto CandHandle = CandidateHandles[CandIdx];
            const auto EntityNum =
                static_cast<uint32>(CandHandle.Get_Entity().Get_EntityNumber());

            const auto bIsFocused = (EntityNum == FocusedEntityNum && ck::IsValid(FocusEntity));

            PosByEntity.Add(EntityNum, Candidates[CandIdx].WorldLocation);
            {
                const auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(CandHandle);
                if (ck::IsValid(LifetimeOwner))
                {
                    OwnerEntityNums[CandIdx] =
                        static_cast<uint32>(LifetimeOwner.Get_Entity().Get_EntityNumber());
                }
            }

            if (Is_EntityOwnedByIgnoredActor(CandHandle, IgnoredActors))
            { continue; }

            auto Draw       = FMarkerDraw{};
            Draw.WorldPos   = Candidates[CandIdx].WorldLocation;
            Draw.Depth      = Candidates[CandIdx].Depth;
            Draw.bIsFocused = bIsFocused;
            _MarkerDraws.Add(Draw);
        }

        // Dotted parent→child links: for every candidate whose lifetime owner is also a
        // candidate, draw a one-frame dashed line between the two (re-issued every tick,
        // so it tracks both endpoints with no cached state). Line color = the CHILD's
        // depth tint at full alpha.
        for (auto CandIdx = 0; CandIdx < CandidateHandles.Num(); ++CandIdx)
        {
            const auto OwnerNum = OwnerEntityNums[CandIdx];
            if (OwnerNum == MAX_uint32)
            { continue; }

            const auto* ParentPos = PosByEntity.Find(OwnerNum);
            if (ParentPos == nullptr)
            { continue; }

            auto LinkColor = ck_debugoverlay::Get_MarkerDepthTint(Candidates[CandIdx].Depth);
            LinkColor.A = 0.9f;

            UCk_Utils_DebugDraw_UE::DrawDebugDashedLine(
                World,
                *ParentPos,
                Candidates[CandIdx].WorldLocation,
                /*InDashSize=*/14.0f,
                LinkColor,
                /*InDuration=*/0.0f,
                /*InThickness=*/1.5f);
        }

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
                _MarkerDraws.Num(),
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
        TArray<ck_debugoverlay::FCandidate>&                 OutCandidates) const
    -> void
{
    OutHandles.Empty();
    OutCandidates.Empty();

    // BATCH-VERIFY: Mirrors CkDebuggerModel_WorldContext::Refresh_EntityCache exactly.
    // UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity returns the world's
    // transient-entity handle which is the root of the registry view.
    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
    if (ck::Is_NOT_Valid(TransientEntity))
    { return; }

    // Enumerate all live entities that have at least one provider willing to serve them
    // AND a resolvable world location (via FFragment_Transform).
    //
    // Pattern mirrors CkDebuggerModel_ViewportPicker::DoPickAtRay / DoDrawBillboards:
    // view over FFragment_Transform with CK_IGNORE_PENDING_KILL to skip entities in the
    // destroy pipeline.
    //
    // BATCH-VERIFY: confirm CK_IGNORE_PENDING_KILL is the correct exclude tag here
    // (mirrors exact ViewportPicker pattern).
    const auto MaxDepth = CVar_DebugOverlay_MaxDepth.GetValueOnGameThread();

    TransientEntity.View<ck::FFragment_Transform, CK_IGNORE_PENDING_KILL>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_Transform& InTransform)
        {
            const auto Handle = ck::MakeHandle(InEntity, TransientEntity);
            if (ck::Is_NOT_Valid(Handle))
            { return; }

            // Every transform-bearing entity is a candidate (the old "top-level only" gate
            // missed real inspectables, e.g. crowd agents owned by a player-controller entity).
            //
            // The ONE mandatory exclusion: debug-draw shape entities. Without it, each of our
            // own diamond markers (which carry FFragment_Transform) would become a candidate
            // and receive its own marker next tick — unbounded entity growth. This also keeps
            // EQS/nav debug shapes out of the candidate list, which is never useful.
            if (Handle.Has<ck::FFragment_Pmg_DebugShape_Common>())
            { return; }

            // Hierarchy depth = lifetime-owner hops to the transient (0 = top-level).
            // Gated by ck.DebugOverlay.MaxDepth; hard cap guards against ownership cycles.
            auto Depth = 0;
            {
                auto Owner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Handle);
                while (ck::IsValid(Owner) && NOT (Owner == TransientEntity) && Depth < 32)
                {
                    ++Depth;
                    Owner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Owner);
                }
            }
            if (MaxDepth >= 0 && Depth > MaxDepth)
            { return; }

            // Check if any provider can serve this entity.
            auto bAnyProvider = false;
            for (const auto& Provider : InProviders)
            {
                if (Provider && Provider->CanProvide(Handle))
                {
                    bAnyProvider = true;
                    break;
                }
            }
            if (NOT bAnyProvider)
            { return; }

            auto Candidate          = ck_debugoverlay::FCandidate{};
            Candidate.WorldLocation = InTransform.Get_Transform().GetLocation();
            // bIsOnScreen filled in DoTick after projection.
            Candidate.bIsOnScreen   = false;
            Candidate.Depth         = Depth;

            OutHandles.Add(Handle);
            OutCandidates.Add(Candidate);
        });
}

// ====================================================================================================================
// Always-on provider tags — force-included regardless of the active layout,
// as long as CanProvide() returns true for the focused entity.
// Avoids losing critical behavioral state (e.g. SM) when a layout doesn't list them.
// ====================================================================================================================

namespace
{
    // Returns true if InProviderTag is in the always-on set.
    // Currently: StateMachine is always shown when present.
    auto Is_AlwaysOnProvider(const FGameplayTag& InProviderTag) -> bool
    {
        // Leaf-name compare avoids a hard dep on the SM provider's tag declaration here.
        return ck_debugoverlay::Get_LeafName(InProviderTag) == TEXT("StateMachine");
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
    OutModel = FCk_DebugOverlay_EntityModel{};
    OutModel.Entity = InFocusEntity;

    // Header is now rendered by SCkDebug_EntityRef in the FocusCard; OutModel.Header kept as
    // a fallback / for callers that may read it, but the card no longer uses it as primary display.
    {
        const auto DebugName   = UCk_Utils_Handle_UE::Get_DebugName(InFocusEntity);
        const auto CleanName   = ck::DebugNameClean::Get_CleanName(DebugName.ToString());
        const auto EntityNum   = static_cast<int32>(InFocusEntity.Get_Entity().Get_EntityNumber());
        OutModel.Header = FText::FromString(
            FString::Printf(TEXT("%s [%d]"), *CleanName, EntityNum));
    }

    const auto EntityId = static_cast<uint32>(InFocusEntity.Get_Entity().Get_EntityNumber());

    // Track which provider tags we've already emitted so always-on providers don't double-add.
    auto EmittedProviderTags = FGameplayTagContainer{};

    // ---- Layout-driven providers ----
    for (const auto& Provider : InProviders)
    {
        if (NOT Provider || NOT Provider->CanProvide(InFocusEntity))
        { continue; }

        const auto& ProviderTag   = Provider->Get_ProviderTag();
        const auto  EnabledFields = ck_debugoverlay::Resolve_EnabledFields(
            InLayout, ProviderTag, Provider->Get_FieldTags());

        if (EnabledFields.IsEmpty())
        { continue; }

        // Find the per-provider entry filter (if any).
        auto EntryFilter = FGameplayTagQuery{};
        for (const auto& Entry : InLayout.Entries)
        {
            if (Entry.ProviderTag == ProviderTag)
            {
                EntryFilter = Entry.EntryFilter;
                break;
            }
        }

        auto Config          = FCk_DebugOverlay_ProviderConfig{};
        Config.EnabledFields = EnabledFields;
        Config.EntryFilter   = EntryFilter;

        auto Section = FCk_DebugOverlay_Section{};
        Section.ProviderTag   = ProviderTag;
        Section.SortPriority  = Provider->Get_SortPriority();

        Provider->Collect(InFocusEntity, Config, Section);

        // Record history for each row.
        for (const auto& Row : Section.Rows)
        {
            const auto Key = FCk_DebugOverlay_HistoryKey{ EntityId, Row.FieldTag };
            if (_History) { _History->Observe(Key, Row.Value.ToString(), InNow); }
        }

        EmittedProviderTags.AddTag(ProviderTag);
        OutModel.Sections.Add(MoveTemp(Section));
    }

    // ---- Always-on providers (force-include if CanProvide and not already emitted) ----
    for (const auto& Provider : InProviders)
    {
        if (NOT Provider)
        { continue; }

        const auto& ProviderTag = Provider->Get_ProviderTag();

        if (NOT Is_AlwaysOnProvider(ProviderTag))
        { continue; }

        // Skip if already emitted via the layout pass above.
        if (EmittedProviderTags.HasTagExact(ProviderTag))
        { continue; }

        if (NOT Provider->CanProvide(InFocusEntity))
        { continue; }

        // Use the provider's own default field set (all DefaultEnabled fields).
        auto EnabledFields = FGameplayTagContainer{};
        for (const auto& FieldDesc : Provider->Get_FieldTags())
        {
            if (FieldDesc.DefaultEnabled)
            {
                EnabledFields.AddTag(FieldDesc.Tag);
            }
        }

        if (EnabledFields.IsEmpty())
        { continue; }

        auto Config          = FCk_DebugOverlay_ProviderConfig{};
        Config.EnabledFields = EnabledFields;
        // No entry filter for force-included always-on providers.

        auto Section = FCk_DebugOverlay_Section{};
        Section.ProviderTag   = ProviderTag;
        Section.SortPriority  = Provider->Get_SortPriority();

        Provider->Collect(InFocusEntity, Config, Section);

        for (const auto& Row : Section.Rows)
        {
            const auto Key = FCk_DebugOverlay_HistoryKey{ EntityId, Row.FieldTag };
            if (_History) { _History->Observe(Key, Row.Value.ToString(), InNow); }
        }

        OutModel.Sections.Add(MoveTemp(Section));
    }

    // TODO(batch-C): parent-entity-summarizes-sub-entities aggregation.
    // When a top-level entity owns sub-entities (e.g. scene-node children, SM states),
    // collect their provider output and fold it into a synthesized summary section here,
    // so the focus card shows a single rolled-up view instead of showing only the root
    // entity's own data. This requires walking the lifetime-owner tree from InFocusEntity
    // and calling Build_Model recursively (or a lightweight variant) for each sub-entity,
    // then merging the resulting sections under a "Children" header.
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

    // ---- World tags: one per on-screen candidate (B1 — distance-scaled / faded / culled) ----
    // Skipped while ejected: tag positions come from PC-based screen projection, which
    // reflects the frozen player camera, not the editor camera actually rendering.
    auto WorldTags = TArray<FCk_DebugOverlay_WorldTagInfo>{};

    if (ck::IsValid(InPC) && NOT _ViewpointIsEjected)
    {
        const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();

        // Read distance-scaling params from settings with safe defaults.
        const auto MaxDist       = Settings ? Settings->MaxDist       : 5000.0f;
        const auto NearDist      = Settings ? Settings->NearDist      : 600.0f;
        const auto FarDist       = Settings ? Settings->FarDist       : 4000.0f;
        const auto MinScale      = Settings ? Settings->MinScale      : 0.5f;
        const auto FadeStartDist = Settings ? Settings->FadeStartDist : 3000.0f;

        // Retrieve the camera viewpoint (already computed above — re-query here because
        // Push_ToRoot does not receive the Viewpoint struct directly).
        auto CamLoc = FVector::ZeroVector;
        {
            auto CamRot = FRotator::ZeroRotator;
            InPC->GetPlayerViewPoint(CamLoc, CamRot);
        }

        for (auto CandIdx = 0; CandIdx < InCandidates.Num(); ++CandIdx)
        {
            if (NOT InCandidates[CandIdx].bIsOnScreen)
            { continue; }

            const auto& Handle = InCandidateHandles[CandIdx];

            // B1 — distance cull.
            const auto Dist = static_cast<float>(
                FVector::Dist(CamLoc, InCandidates[CandIdx].WorldLocation));
            if (Dist > MaxDist)
            { continue; }

            // Build compact token: concatenate top providers' tokens (skip empty).
            // Alongside, collect feature badges (abbrev + provider color) for the
            // near-plate rendering — these reflect what the entity HAS, independent
            // of the active layout's enabled-field selection.
            auto TokenParts = TArray<FString>{};
            auto Badges     = TArray<FCk_DebugOverlay_WorldTagBadge>{};
            for (const auto& Provider : InProviders)
            {
                if (NOT Provider || NOT Provider->CanProvide(Handle))
                { continue; }

                const auto& ProviderTag = Provider->Get_ProviderTag();

                // World tags survey *behavioral* state. Skip the core identity
                // providers (EntityInfo / Transform) so tags aren't just
                // "Info:<name> | Loc:(x,y,z)" spam on every transform-bearing entity.
                const auto ProviderLeaf = ck_debugoverlay::Get_LeafName(ProviderTag);
                if (ProviderLeaf == TEXT("EntityInfo") || ProviderLeaf == TEXT("Transform"))
                { continue; }

                Badges.Add(FCk_DebugOverlay_WorldTagBadge{
                    ck_debugoverlay::Get_ProviderAbbrev(ProviderLeaf),
                    SCkDebugOverlay_FocusCard::Get_ProviderColor(ProviderTag) });

                const auto EnabledFields = ck_debugoverlay::Resolve_EnabledFields(
                    InLayout, ProviderTag, Provider->Get_FieldTags());
                if (EnabledFields.IsEmpty())
                { continue; }

                auto EntryFilter = FGameplayTagQuery{};
                for (const auto& Entry : InLayout.Entries)
                {
                    if (Entry.ProviderTag == ProviderTag) { EntryFilter = Entry.EntryFilter; break; }
                }

                auto Config          = FCk_DebugOverlay_ProviderConfig{};
                Config.EnabledFields = EnabledFields;
                Config.EntryFilter   = EntryFilter;

                const auto Token = Provider->Get_CompactToken(Handle, Config);
                if (NOT Token.IsEmpty())
                {
                    TokenParts.Add(Token);
                }
            }

            // Ultra-condensed near plate: candidates within NearDist get a compact
            // plate — cleaned debug-name header with a row of colored feature badges
            // (SM / GOAP / …) under it. Toggle with `ck.DebugOverlay.NearPlates 0`.
            const auto NearPlatesEnabled =
                CVar_DebugOverlay_NearPlates.GetValueOnGameThread() != 0;
            const auto IsNearPlate = NearPlatesEnabled && Dist <= NearDist;

            const auto DebugName   = UCk_Utils_Handle_UE::Get_DebugName(Handle);
            const auto HasRealName = DebugName.IsNone() == false;

            // Far pills require behavioral tokens (identity-only pills are spam at range).
            // Near plates show when the entity has feature badges OR an explicit name —
            // close-up, identity alone is worth a plate. Unnamed, feature-less entities
            // (scene-node children etc.) stay hidden.
            if (IsNearPlate)
            {
                if (Badges.IsEmpty() && NOT HasRealName)
                { continue; }
            }
            else if (TokenParts.IsEmpty())
            { continue; }

            auto ScreenPos = FVector2D{};
            if (NOT UGameplayStatics::ProjectWorldToScreen(
                InPC, InCandidates[CandIdx].WorldLocation, ScreenPos,
                /*bPlayerViewportRelative=*/false))
            { continue; }

            // B1 — compute scale and opacity from distance.
            const auto Scale = FMath::GetMappedRangeValueClamped(
                FVector2D{ NearDist, FarDist },
                FVector2D{ 1.0f, MinScale },
                Dist);
            const auto Opacity = FMath::GetMappedRangeValueClamped(
                FVector2D{ FadeStartDist, MaxDist },
                FVector2D{ 1.0f, 0.15f },
                Dist);

            auto TagInfo      = FCk_DebugOverlay_WorldTagInfo{};
            TagInfo.ScreenPos = ScreenPos;
            TagInfo.Scale     = Scale;
            TagInfo.Opacity   = Opacity;

            if (IsNearPlate)
            {
                const auto Header = HasRealName
                    ? ck::DebugNameClean::Get_CleanName(DebugName.ToString())
                    : FString::Printf(TEXT("#%u"),
                        static_cast<uint32>(Handle.Get_Entity().Get_EntityNumber()));

                TagInfo.bIsPlate = true;
                TagInfo.Header   = FText::FromString(Header);
                TagInfo.Badges   = MoveTemp(Badges);
            }
            else
            {
                TagInfo.Text = FText::FromString(FString::Join(TokenParts, TEXT(" | ")));
            }

            WorldTags.Add(MoveTemp(TagInfo));

            // Hard cap to avoid clutter in dense scenes (e.g. crowds).
            if (WorldTags.Num() >= 16)
            { break; }
        }

        // De-overlap co-located tags: entities at the same world position project to
        // the same screen point and their plates hide each other. Stack subsequent
        // plates upward (anchor is bottom-center, so -Y stacks above).
        {
            constexpr auto CellW = 48.0f;
            constexpr auto CellH = 32.0f;
            constexpr auto StackStep = 34.0f;

            auto CountByCell = TMap<FIntPoint, int32>{};
            for (auto& Tag : WorldTags)
            {
                const auto Cell = FIntPoint{
                    FMath::RoundToInt32(Tag.ScreenPos.X / CellW),
                    FMath::RoundToInt32(Tag.ScreenPos.Y / CellH) };
                auto& CountInCell = CountByCell.FindOrAdd(Cell);
                Tag.ScreenPos.Y -= CountInCell * StackStep;
                ++CountInCell;
            }
        }
    }

    _RootWidget->Update_WorldTags(WorldTags);
}

// ====================================================================================================================
// Marker billboards — UDebugDrawService callback (fires per viewport, after the world renders).
// Mirrors FCkDebuggerModel_ViewportPicker::DoDrawBillboards: constant screen-size canvas
// tiles using the ECS-picker diamond textures. Tint encodes hierarchy depth; non-focused
// markers are semi-transparent, the focused one is opaque + hover texture + 1.25×.
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

    auto* MarkerTex = _MarkerTexture.Get();
    auto* HoverTex  = _MarkerHoverTexture.IsValid() ? _MarkerHoverTexture.Get() : MarkerTex;
    if (MarkerTex == nullptr || HoverTex == nullptr)
    { return; }

    const auto* MarkerResource = MarkerTex->GetResource();
    const auto* HoverResource  = HoverTex->GetResource();
    if (MarkerResource == nullptr || HoverResource == nullptr)
    { return; }

    const auto* Settings    = GetDefault<UCk_DebugOverlay_Settings>();
    const auto  TileSizePx  = 28.0f * (Settings ? Settings->DiamondScale : 1.0f);
    const auto  CanvasSizeX = static_cast<float>(InCanvas->SizeX);
    const auto  CanvasSizeY = static_cast<float>(InCanvas->SizeY);

    constexpr auto FocusedScale = 1.25f;
    constexpr auto DefaultAlpha = 0.45f;

    // Defer the focused marker so it renders on top of any cluster.
    auto Focused = TOptional<FMarkerDraw>{};

    for (const auto& Marker : _MarkerDraws)
    {
        if (Marker.bIsFocused)
        {
            Focused = Marker;
            continue;
        }

        const auto Projected = InCanvas->Project(Marker.WorldPos);
        if (Projected.Z <= 0.0f)
        { continue; }
        if (Projected.X < 0.0f || Projected.Y < 0.0f || Projected.X >= CanvasSizeX || Projected.Y >= CanvasSizeY)
        { continue; }

        auto Tint = ck_debugoverlay::Get_MarkerDepthTint(Marker.Depth);
        Tint.A = DefaultAlpha;

        const auto TopLeft = FVector2D(
            static_cast<float>(Projected.X) - TileSizePx * 0.5f,
            static_cast<float>(Projected.Y) - TileSizePx * 0.5f);

        auto Tile = FCanvasTileItem(TopLeft, MarkerResource, FVector2D(TileSizePx, TileSizePx), Tint);
        Tile.BlendMode = SE_BLEND_Translucent;
        InCanvas->DrawItem(Tile);
    }

    if (Focused.IsSet())
    {
        const auto Projected = InCanvas->Project(Focused->WorldPos);
        if (Projected.Z > 0.0f)
        {
            const auto FocusSize = TileSizePx * FocusedScale;

            auto Tint = ck_debugoverlay::Get_MarkerDepthTint(Focused->Depth);
            Tint.A = 1.0f;

            const auto TopLeft = FVector2D(
                static_cast<float>(Projected.X) - FocusSize * 0.5f,
                static_cast<float>(Projected.Y) - FocusSize * 0.5f);

            auto Tile = FCanvasTileItem(TopLeft, HoverResource, FVector2D(FocusSize, FocusSize), Tint);
            Tile.BlendMode = SE_BLEND_Translucent;
            InCanvas->DrawItem(Tile);
        }
    }
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

#endif // WITH_CK_DEBUG_OVERLAY
