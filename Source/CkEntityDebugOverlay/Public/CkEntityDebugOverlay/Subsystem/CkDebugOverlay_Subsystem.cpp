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

    // Global Slate input pre-processor — observes key-downs regardless of game-vs-editor
    // viewport focus, so the double-tap gestures keep working after the user ejects (F8).
    if (FSlateApplication::IsInitialized())
    {
        _InputProcessor = MakeShared<FCkDebugOverlay_InputProcessor>();
        FSlateApplication::Get().RegisterInputPreProcessor(_InputProcessor.ToSharedRef());
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
    if (_TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(_TickerHandle);
        _TickerHandle.Reset();
    }

    if (_InputProcessor.IsValid())
    {
        if (FSlateApplication::IsInitialized())
        { FSlateApplication::Get().UnregisterInputPreProcessor(_InputProcessor); }
        _InputProcessor.Reset();
    }
    _PinnedEntities.Reset();
    _PreferredCoLocated = FCk_Handle{};

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

    // ---- 1. Compute viewpoint ----
    // Computed BEFORE gather so its location drives the marker distance cull.
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

    // Cull origin for the marker/candidate distance gate — only meaningful when a real
    // viewpoint exists (ejected editor camera or a valid PC). Otherwise leave unset so
    // the gather doesn't cull around the world origin.
    auto CullOrigin = TOptional<FVector>{};
    if (_ViewpointIsEjected || ck::IsValid(PC))
    { CullOrigin = Viewpoint.Location; }

    // ---- 2. Gather candidates (distance-culled around the viewpoint) ----
    auto CandidateHandles = TArray<FCk_Handle>{};
    auto Candidates       = TArray<ck_debugoverlay::FCandidate>{};

    Gather_Candidates(World, _Providers, CullOrigin, CandidateHandles, Candidates);
    _LastFrameCandidates = CandidateHandles;

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
            Candidates[CandIdx].ScreenPos   = ScreenPos;
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

            // Soft co-located preference (cycle key): while the cycled-to entity is still
            // on-screen, prefer it — WITHOUT locking (no ring). Holding on "still on-screen"
            // (rather than "near the auto-pick") keeps the cycle progressing reliably through a
            // chain; it auto-clears once the entity leaves view (look away → resume auto-pick).
            if (ck::IsValid(_PreferredCoLocated))
            {
                const auto PrefIdx = CandidateHandles.IndexOfByPredicate(
                    [this](const FCk_Handle& InHandle){ return InHandle == _PreferredCoLocated; });

                if (Candidates.IsValidIndex(PrefIdx) && Candidates[PrefIdx].bIsOnScreen)
                {
                    FocusEntity = _PreferredCoLocated;
                    _LockedCandidateIndex = PrefIdx;
                }
                else
                {
                    _PreferredCoLocated = FCk_Handle{};
                }
            }
        }
    }

    // ---- 5. Time + double-tap gesture detection ----
    // Input is sampled from the global Slate pre-processor (not PC->WasInputKeyJustPressed)
    // so the gestures keep working while the user is ejected from PIE (F8).
    const auto Now = FPlatformTime::Seconds();

    if (const auto* Settings = GetDefault<UCk_DebugOverlay_InputSettings>();
        Settings != nullptr && _InputProcessor.IsValid())
    {
        const auto Window = Settings->LockDoubleTapWindowSeconds;

        // Edge-detect a double-tap of InKey: true on the second tap within Window. Consumes
        // the key from the pre-processor each call (one physical press = one detection).
        const auto WasDoubleTapped = [&](const FKey& InKey, double& InOutLastPress) -> bool
        {
            if (NOT _InputProcessor->Consume_WasJustPressed(InKey))
            { return false; }

            const auto TimeSinceLast = static_cast<float>(Now - InOutLastPress);
            if (InOutLastPress >= 0.0 && TimeSinceLast <= Window)
            {
                InOutLastPress = -1.0; // reset so a third tap doesn't immediately re-fire
                return true;
            }
            InOutLastPress = Now;
            return false;
        };

        // Double-tap LockKey (default Left Shift): PIN / UNPIN the focused entity. The
        // primary card keeps auto-following; each pin gets its own persistent side-by-side
        // card. (This replaces the old focus-lock toggle — lock is now driven by Next/Prev,
        // the co-located cycle, and `ck.DebugOverlay.Lock`.)
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

        // Double-tap UnpinAllKey (default unbound): release every pinned card at once.
        if (WasDoubleTapped(Settings->UnpinAllKey, _LastUnpinAllKeyPressTime))
        {
            if (_PinnedEntities.Num() > 0)
            {
                _PinnedEntities.Reset();
                ck::debug_overlay::Log(TEXT("Released all pins (double-tap)"));
            }
        }

        // Double-tap HelpKey (default unbound): toggle the full keyboard-hints legend.
        if (WasDoubleTapped(Settings->HelpKey, _LastHelpKeyPressTime))
        {
            _ShowFullLegend = NOT _ShowFullLegend;
        }

        // Double-tap CycleCoLocatedKey (default Left Alt): cycle the focus through the
        // co-located cluster. The cluster is the CONNECTED COMPONENT (flood-fill) of entities
        // linked by world-OR-screen proximity, so a chain A-B-C is ONE stable set regardless of
        // which member is focused (fixes "only 2 of 3 cycle"). Sets a soft preference (no lock).
        if (WasDoubleTapped(Settings->CycleCoLocatedKey, _LastCycleKeyPressTime))
        {
            if (ck::IsValid(FocusEntity) && Candidates.IsValidIndex(_LockedCandidateIndex))
            {
                const auto WorldRadiusSq  = FMath::Square(Settings->CoLocatedRadius);
                const auto ScreenRadiusSq = FMath::Square(Settings->CoLocatedScreenRadius * 1.5f);

                auto Cluster = TArray<int32>{ _LockedCandidateIndex };
                for (auto Front = 0; Front < Cluster.Num(); ++Front)
                {
                    const auto Cur = Cluster[Front];
                    for (auto CandIdx = 0; CandIdx < Candidates.Num(); ++CandIdx)
                    {
                        if (Cluster.Contains(CandIdx))
                        { continue; }

                        const auto WorldClose = FVector::DistSquared(
                            Candidates[CandIdx].WorldLocation, Candidates[Cur].WorldLocation) <= WorldRadiusSq;
                        const auto ScreenClose = Candidates[CandIdx].bIsOnScreen && Candidates[Cur].bIsOnScreen &&
                            FVector2D::DistSquared(Candidates[CandIdx].ScreenPos, Candidates[Cur].ScreenPos) <= ScreenRadiusSq;

                        if (WorldClose || ScreenClose)
                        { Cluster.Add(CandIdx); }
                    }
                }

                if (Cluster.Num() > 1)
                {
                    // Stable order (by entity number) so the cycle sequence is consistent.
                    Cluster.Sort([&CandidateHandles](int32 InA, int32 InB)
                    {
                        return CandidateHandles[InA].Get_Entity().Get_EntityNumber()
                             < CandidateHandles[InB].Get_Entity().Get_EntityNumber();
                    });

                    // Advance the SOFT preference CYCLICALLY through the cluster (stable
                    // entity-number order). Pure modular wrap so EVERY tap moves to a different
                    // member — a 2-cluster toggles, a 3-cluster goes 1->2->3->1. (The old
                    // "step past the last -> auto-follow" created a DEAD FIXED POINT whenever the
                    // auto-pick was the LAST sorted member: tap -> wrap -> re-pick the same best
                    // -> nothing visibly changes. That's the "cycle does not happen" symptom.)
                    // Auto-follow still resumes on its own: the soft preference auto-clears once
                    // the preferred entity leaves the screen (look away → the cluster merges).
                    const auto CurPos  = Cluster.IndexOfByKey(_LockedCandidateIndex);
                    const auto NextPos = (CurPos + 1) % Cluster.Num();

                    _PreferredCoLocated = CandidateHandles[Cluster[NextPos]];
                    ck::debug_overlay::Log(
                        TEXT("Cycled co-located preference ({}/{})"), NextPos + 1, Cluster.Num());
                }
            }
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
    Gather_Candidates(
        UWorld*                                              InWorld,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        const TOptional<FVector>&                            InCullOrigin,
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

    // Diamond/candidate distance cull (declutter). Only applied when a real viewpoint
    // origin exists and MarkerMaxDist > 0; otherwise the whole transform set is gathered.
    if (const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
        Settings != nullptr && InCullOrigin.IsSet() && Settings->MarkerMaxDist > 0.0f)
    {
        GatherParams.CullOrigin = InCullOrigin;
        GatherParams.CullRadius = Settings->MarkerMaxDist;
    }

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
    const auto* InputSettings   = GetDefault<UCk_DebugOverlay_InputSettings>();

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

    _RootWidget->Set_FocusCardContent(
        InModel, CardStyle, *_History, InNow, _FocusLocked, FocusCoLocIndex, FocusCoLocCount);

    // ---- Pinned cards (item 6): prune destroyed pins, dedupe vs the live focus, build models ----
    _PinnedEntities.RemoveAll([](const FCk_Handle& InPinned){ return ck::Is_NOT_Valid(InPinned); });

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
    // its diamond marker. The focus entity's plate is highlighted; co-located clusters fan out
    // gradually with camera proximity.
    auto DpiScale = 1.0f;
    if (ck::IsValid(InPC))
    {
        if (auto* PCWorld = InPC->GetWorld())
        { DpiScale = UWidgetLayoutLibrary::GetViewportScale(PCWorld); }
    }
    DpiScale = FMath::Max(0.01f, DpiScale);

    const auto WorldTags = ck_debugoverlay::Build_WorldTags(
        InCandidateHandles, InCandidates, InProviders, InLayout, InPC, _ViewpointIsEjected,
        DpiScale, InModel.Entity);

    _RootWidget->Update_WorldTags(WorldTags);

    // ---- Keyboard-hints strip (item 9): built from the live (per-user) key bindings ----
    if (OverlaySettings != nullptr && InputSettings != nullptr)
    {
        const auto KeyName = [](const FKey& InKey) -> FString
        {
            return InKey.IsValid() ? InKey.GetDisplayName().ToString() : FString(TEXT("(unbound)"));
        };

        const auto Compact = FString::Printf(
            TEXT("%s x2 pin   %s x2 cycle/unlock   %s x2 unpin-all   %s x2 ECS   %s x2 help"),
            *KeyName(InputSettings->LockKey),
            *KeyName(InputSettings->CycleCoLocatedKey),
            *KeyName(InputSettings->UnpinAllKey),
            *KeyName(InputSettings->EcsDebuggerFocusKey),
            *KeyName(InputSettings->HelpKey));

        const auto Full = FString::Printf(
            TEXT("CK ON-SCREEN DEBUGGER\n")
            TEXT("%s x2   pin / unpin focused entity (side-by-side card)\n")
            TEXT("%s x2   cycle co-located entities (one tap past the last UNLOCKS / auto-follows)\n")
            TEXT("%s x2   release ALL pins\n")
            TEXT("%s x2   open focused entity in ECS Debugger\n")
            TEXT("%s x2   toggle this help\n")
            TEXT("console: ck.DebugOverlay .Next .Prev .Lock .Layout.Next/.Prev .UnpinAll .Help"),
            *KeyName(InputSettings->LockKey),
            *KeyName(InputSettings->CycleCoLocatedKey),
            *KeyName(InputSettings->UnpinAllKey),
            *KeyName(InputSettings->EcsDebuggerFocusKey),
            *KeyName(InputSettings->HelpKey));

        _RootWidget->Update_KeyHints(Compact, Full, _ShowFullLegend, OverlaySettings->ShowKeyHints);
    }
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
// World plates — drawn on the marker canvas (UCanvas::Project), so they project with the exact
// rendered view and stay aligned with the diamonds in possessed / ejected / simulate. Co-located
// clusters fan out gradually as the camera nears them (collapsed at range → look like one entity);
// the focus plate is drawn highlighted to match the emphasized diamond.
// ====================================================================================================================

#if 0 // Retired: world plates are now rich Slate cards (see Build_WorldTags / Update_WorldTags).
auto
    UCk_DebugOverlay_Subsystem::
    DoDrawPlates(
        UCanvas*           InCanvas,
        APlayerController* /*InPC*/)
    -> void
{
    if (InCanvas == nullptr || _CanvasPlates.IsEmpty())
    { return; }

    auto* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
    if (Font == nullptr)
    { return; }

    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    const auto FanFull     = Settings ? Settings->FanFullDist          : 500.0f;
    const auto FanFade     = Settings ? Settings->FanFadeDist          : 2000.0f;
    const auto FanMaxSpace = Settings ? Settings->FanMaxSpacing        : 110.0f;
    const auto CellPx      = Settings ? Settings->CoLocatedScreenRadius : 36.0f;

    const auto CanvasW = static_cast<float>(InCanvas->SizeX);
    const auto CanvasH = static_cast<float>(InCanvas->SizeY);

    // --- Project every plate onto this viewport (same projection as the diamonds) ---
    struct FProj { int32 PlateIdx = 0; FVector2D Pos = FVector2D::ZeroVector; int32 FanIdx = INDEX_NONE; int32 FanCount = 0; };
    auto Projected = TArray<FProj>{};
    Projected.Reserve(_CanvasPlates.Num());
    for (auto Idx = 0; Idx < _CanvasPlates.Num(); ++Idx)
    {
        const auto P = InCanvas->Project(_CanvasPlates[Idx].WorldLocation);
        if (P.Z <= 0.0f)
        { continue; }
        if (P.X < 0.0f || P.Y < 0.0f || P.X >= CanvasW || P.Y >= CanvasH)
        { continue; }
        Projected.Add(FProj{ Idx, FVector2D{ static_cast<float>(P.X), static_cast<float>(P.Y) }, INDEX_NONE, 0 });
    }

    // --- Group co-located projected plates; fan each cluster gradually by camera distance ---
    const auto CellW = FMath::Max(8.0f, CellPx * 1.5f);
    const auto CellH = FMath::Max(8.0f, CellPx);

    auto CellMembers = TMap<FIntPoint, TArray<int32>>{}; // values = indices into Projected
    for (auto P = 0; P < Projected.Num(); ++P)
    {
        const auto Cell = FIntPoint{
            FMath::RoundToInt32(Projected[P].Pos.X / CellW),
            FMath::RoundToInt32(Projected[P].Pos.Y / CellH) };
        CellMembers.FindOrAdd(Cell).Add(P);
    }

    for (auto& Pair : CellMembers)
    {
        auto& Members = Pair.Value;
        if (Members.Num() <= 1)
        { continue; }

        // Fan factor from the CLOSEST member: 1 (full fan) near, 0 (collapsed) far.
        auto MinDist = TNumericLimits<float>::Max();
        for (const auto P : Members)
        { MinDist = FMath::Min(MinDist, _CanvasPlates[Projected[P].PlateIdx].Distance); }

        const auto FanFactor = (FanFade > FanFull)
            ? 1.0f - FMath::Clamp((MinDist - FanFull) / (FanFade - FanFull), 0.0f, 1.0f)
            : (MinDist <= FanFull ? 1.0f : 0.0f);

        Members.Sort([&Projected](int32 InA, int32 InB)
        { return Projected[InA].Pos.X < Projected[InB].Pos.X; });

        const auto Count = Members.Num();
        auto CentroidX = 0.0f;
        auto TopY      = TNumericLimits<float>::Max();
        for (const auto P : Members)
        {
            CentroidX += Projected[P].Pos.X;
            TopY = FMath::Min(TopY, Projected[P].Pos.Y);
        }
        CentroidX /= static_cast<float>(Count);

        for (auto i = 0; i < Count; ++i)
        {
            const auto P    = Members[i];
            const auto Slot = static_cast<float>(i) - (Count - 1) * 0.5f;

            const auto FannedX = CentroidX + Slot * FanMaxSpace;
            const auto FannedY = TopY - FMath::Abs(Slot) * 14.0f;

            // Lerp from collapsed (right on the marker) to fully fanned as the camera nears.
            Projected[P].Pos.X   = FMath::Lerp(Projected[P].Pos.X, FannedX, FanFactor);
            Projected[P].Pos.Y   = FMath::Lerp(Projected[P].Pos.Y, FannedY, FanFactor);
            Projected[P].FanIdx  = i;
            Projected[P].FanCount = Count;
        }
    }

    // --- Draw far→near so near/focus plates land on top ---
    Projected.Sort([this](const FProj& InA, const FProj& InB)
    { return _CanvasPlates[InA.PlateIdx].Distance > _CanvasPlates[InB.PlateIdx].Distance; });

    const auto LineH = static_cast<float>(Font->GetMaxCharHeight());

    for (const auto& Pr : Projected)
    {
        const auto& Plate = _CanvasPlates[Pr.PlateIdx];

        // Header / token line — focus is bright white, others a softer grey.
        auto HeaderStr = Plate.bIsNearPlate ? Plate.Header : Plate.FarText;
        if (Pr.FanCount > 1)
        { HeaderStr = FString::Printf(TEXT("[%d/%d] "), Pr.FanIdx + 1, Pr.FanCount) + HeaderStr; }

        const auto HeaderColor = Plate.bIsFocus
            ? FLinearColor{ 1.0f, 1.0f, 1.0f, 1.0f }
            : FLinearColor{ 0.78f, 0.85f, 0.95f, 0.9f };

        // The marker sits at Pos; stack the plate text just above it.
        const auto HeaderY = Plate.bIsNearPlate
            ? Pr.Pos.Y - 18.0f - LineH * 1.0f   // leave room for the badge row under the header
            : Pr.Pos.Y - 18.0f;

        {
            auto Item = FCanvasTextItem(
                FVector2D{ Pr.Pos.X, HeaderY }, FText::FromString(HeaderStr), Font, HeaderColor);
            Item.bCentreX = true;
            Item.EnableShadow(FLinearColor::Black);
            InCanvas->DrawItem(Item);
        }

        // Badge row (near plates only): colored abbrev chips centered under the header.
        if (Plate.bIsNearPlate && Plate.Badges.Num() > 0)
        {
            constexpr auto BadgeGap = 5.0f;

            auto TotalW = 0.0f;
            for (const auto& Badge : Plate.Badges)
            {
                TotalW += static_cast<float>(Font->GetStringSize(*Badge.Text)) + BadgeGap;
            }
            TotalW = FMath::Max(0.0f, TotalW - BadgeGap);

            auto PenX = Pr.Pos.X - TotalW * 0.5f;
            const auto BadgeY = Pr.Pos.Y - 18.0f;

            for (const auto& Badge : Plate.Badges)
            {
                const auto W = static_cast<float>(Font->GetStringSize(*Badge.Text));

                auto Color = Badge.Color;
                Color.A = Plate.bIsFocus ? 1.0f : 0.85f;

                auto Item = FCanvasTextItem(
                    FVector2D{ PenX, BadgeY }, FText::FromString(Badge.Text), Font, Color);
                Item.EnableShadow(FLinearColor::Black);
                InCanvas->DrawItem(Item);

                PenX += W + BadgeGap;
            }
        }
    }
}
#endif // Retired DoDrawPlates

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
