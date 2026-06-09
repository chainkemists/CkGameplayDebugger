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

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

// B2 — CkPmg diamond markers.
#include "CkPmg/CkPmg_Utils_FlatShapes.h"
#include "CkPmg/CkPmg_Utils.h"

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

    // Returns true if InHandle is a "top-level" entity that should appear as an overlay candidate.
    //
    // Definition: top-level = directly linked to an AActor (owning actor fragment present)
    //             OR has no entity lifetime-owner parent of its own (its owner IS the transient entity,
    //             meaning it sits at the root of the ECS hierarchy).
    //
    // Sub-entities (scene-node children, SM states, physics proxies, etc.) are excluded because
    // they create per-object clutter in the candidate list (gym station example: 31 scene nodes).
    //
    // BATCH-VERIFY:
    //   UCk_Utils_OwningActor_UE::Has(Handle)
    //       — checks for FFragment_OwningActor presence; returns bool.
    //         Confirmed in CkOwningActor_Utils.h.
    //   UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Handle)
    //       — returns an FCk_Handle to the owner; returns invalid/default if entity has no
    //         FFragment_LifetimeOwner (i.e. it is the transient entity itself).
    //         Confirmed in CkEntityLifetime_Utils.h.
    //   UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(Handle)
    //       — returns true only for the registry transient entity.
    //         Confirmed in CkEntityLifetime_Utils.h.
    //   InTransientEntity: passed in for identity comparison — compare lifetime owner
    //       against the known transient handle via == operator on FCk_Handle.
    //       BATCH-VERIFY: confirm FCk_Handle operator== compares entity+version (stable identity).
    auto Is_TopLevelCandidate(const FCk_Handle& InHandle, const FCk_Handle& InTransientEntity) -> bool
    {
        // Strategy 1 (preferred): entity has an owning actor — it's an actor-linked game object.
        if (UCk_Utils_OwningActor_UE::Has(InHandle))
        { return true; }

        // Strategy 2: entity has no lifetime-owner, or its lifetime-owner IS the transient entity.
        // This catches root-level ECS-only entities (no actor link) that live at the top of the
        // ownership hierarchy — e.g. a standalone ECS entity created directly under the transient.
        const auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InHandle);
        if (ck::Is_NOT_Valid(LifetimeOwner))
        {
            // No lifetime-owner fragment — this entity IS the transient root or an orphan.
            // The transient entity itself is never a debug candidate; orphans are treated as top-level.
            return NOT UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(InHandle);
        }

        // Owner is the transient entity → top-level.
        return LifetimeOwner == InTransientEntity;
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

    // B2 — destroy all persistent CkPmg diamond markers.
    for (auto& KV : _Markers)
    {
        auto Handle = static_cast<FCk_Handle>(KV.Value);
        if (ck::IsValid(Handle))
        {
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Handle);
        }
    }
    _Markers.Empty();

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
    auto Viewpoint = ck_debugoverlay::FViewpoint{};
    auto* PC = World->GetFirstPlayerController();
    if (ck::IsValid(PC))
    {
        auto CamLoc = FVector::ZeroVector;
        auto CamRot = FRotator::ZeroRotator;
        PC->GetPlayerViewPoint(CamLoc, CamRot);
        Viewpoint.Location = CamLoc;
        Viewpoint.Forward  = CamRot.Vector();
    }

    // ---- 3. Resolve on-screen flags for all candidates ----
    if (ck::IsValid(PC))
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
        }
    }

    // ---- 6. B2 — update CkPmg diamond markers ----
    // Build the current top-level candidate entity-number set for O(1) lookup.
    if (ck::IsValid(PC))
    {
        auto CurrentEntityNums = TSet<uint32>{};
        for (auto CandIdx = 0; CandIdx < CandidateHandles.Num(); ++CandIdx)
        {
            const auto EntityNum =
                static_cast<uint32>(CandidateHandles[CandIdx].Get_Entity().Get_EntityNumber());
            CurrentEntityNums.Add(EntityNum);
        }

        // Determine which entity is the focused/highlighted one.
        const auto FocusedEntityNum = ck::IsValid(FocusEntity)
            ? static_cast<uint32>(FocusEntity.Get_Entity().Get_EntityNumber())
            : static_cast<uint32>(0);

        // Remove markers whose entity is no longer a candidate.
        {
            auto ToRemove = TArray<uint32>{};
            for (auto& KV : _Markers)
            {
                if (NOT CurrentEntityNums.Contains(KV.Key))
                {
                    ToRemove.Add(KV.Key);
                }
            }
            for (const auto EntityNum : ToRemove)
            {
                auto Handle = static_cast<FCk_Handle>(_Markers[EntityNum]);
                if (ck::IsValid(Handle))
                {
                    UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Handle);
                }
                _Markers.Remove(EntityNum);
            }
        }

        // Create markers for new candidates; update highlight color.
        // Diamond shape: UCk_Utils_Pmg_FlatShapes::Create_Diamond.
        // We create the shape as a child of the candidate entity (InOwningEntity = candidate handle)
        // so it shares the entity's world transform via the CkPmg scene-node hierarchy.
        // Duration = -1.0f → persistent until explicitly destroyed.
        //
        // Diamond color convention:
        //   - Default (non-focused): dim teal  FLinearColor(0.2f, 0.6f, 0.6f, 0.6f)
        //   - Focused:               bright white-cyan FLinearColor(0.0f, 1.0f, 1.0f, 1.0f)
        //
        // Highlight approach: because Request_SetColor issues a deferred request processed next
        // tick, we send it every frame for the focused marker (cheap: request is small). This
        // avoids tracking last-highlighted-id and handles the common case (one focused entity per
        // frame) with minimal overhead. Non-focused markers only get Request_SetColor on the frame
        // their highlight state CHANGES — but since we can't know if a prior frame highlighted them
        // without extra state, the simpler path is to set color for all markers each tick. Each
        // marker sends at most one SetColor request per tick, which is the documented usage pattern.
        static const FLinearColor MarkerColor_Default  { 0.2f, 0.6f, 0.6f, 0.6f };
        static const FLinearColor MarkerColor_Focused  { 0.0f, 1.0f, 1.0f, 1.0f };
        static constexpr float    MarkerSize_Default   = 40.0f;
        static constexpr float    MarkerSize_Focused   = 60.0f;
        static constexpr float    MarkerZOffset        = 120.0f;

        for (auto CandIdx = 0; CandIdx < CandidateHandles.Num(); ++CandIdx)
        {
            auto CandHandle  = CandidateHandles[CandIdx];
            const auto EntityNum =
                static_cast<uint32>(CandHandle.Get_Entity().Get_EntityNumber());

            const auto bIsFocused = (EntityNum == FocusedEntityNum && ck::IsValid(FocusEntity));

            // Compute the marker world position from the candidate's world location.
            const auto MarkerWorldPos = Candidates[CandIdx].WorldLocation + FVector{ 0.0f, 0.0f, MarkerZOffset };

            if (NOT _Markers.Contains(EntityNum))
            {
                // Create a new persistent diamond marker owned by the candidate entity.
                // The shape is positioned in world space and tracked each tick via
                // Request_SetTransform (live-tracking overlay pattern).
                auto ShapeHandle = UCk_Utils_Pmg_FlatShapes::Create_Diamond(
                    CandHandle,
                    FTransform{ FRotator::ZeroRotator, MarkerWorldPos, FVector::OneVector },
                    bIsFocused ? MarkerSize_Focused : MarkerSize_Default,
                    /*InThickness=*/5.0f,
                    bIsFocused ? MarkerColor_Focused : MarkerColor_Default,
                    /*InDrawLines=*/true,
                    /*InLineThickness=*/2.0f,
                    ECk_Plane_Axis::XY,
                    /*InDuration=*/-1.0f);

                _Markers.Add(EntityNum, ShapeHandle);
            }

            // Every tick — follow the entity's world position and recolor.
            auto MarkerHandle = static_cast<FCk_Handle>(_Markers[EntityNum]);
            if (ck::IsValid(MarkerHandle))
            {
                auto TransformHandle = UCk_Utils_Transform_UE::Cast(MarkerHandle);
                UCk_Utils_Transform_UE::Request_SetTransform(
                    TransformHandle,
                    FCk_Request_Transform_SetTransform{ FTransform{ MarkerWorldPos } });
                UCk_Utils_Pmg_DebugShape_UE::Request_SetColor(
                    _Markers[EntityNum],
                    FCk_Request_Pmg_DebugShape_SetColor{ bIsFocused ? MarkerColor_Focused : MarkerColor_Default });
            }
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
    TransientEntity.View<ck::FFragment_Transform, CK_IGNORE_PENDING_KILL>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_Transform& InTransform)
        {
            const auto Handle = ck::MakeHandle(InEntity, TransientEntity);
            if (ck::Is_NOT_Valid(Handle))
            { return; }

            // ---- Sub-entity gate: only top-level entities become overlay candidates. ----
            // A "top-level" entity is one directly linked to an AActor (owning actor present)
            // OR one that has no entity lifetime-owner parent (i.e. its immediate owner is the
            // transient entity — which is the registry root).
            //
            // Rationale: gym stations, scene-node graphs, and other spawned hierarchies produce
            // 10–30 sub-entities per visible object; showing all of them as separate overlay
            // candidates creates unusable tag clutter.
            //
            // Strategy: UCk_Utils_OwningActor_UE::Has(Handle) returns true only for entities
            // with a direct actor link (the WithActor / EntityBridge component added an owning
            // actor fragment). Those are the "game object" roots we want to debug.
            // Entities that lack an actor link are checked for lifetime-owner: if the lifetime
            // owner is the transient entity (registry root), the entity is a top-level ECS entity
            // with no actor link — also a valid candidate. Entities whose lifetime owner is
            // neither themselves nor the transient entity are sub-entities (scene-node children,
            // SM states, etc.) and are excluded.
            //
            // BATCH-VERIFY: UCk_Utils_OwningActor_UE::Has(Handle) — confirmed in CkOwningActor_Utils.h.
            // BATCH-VERIFY: UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Handle) — confirmed in
            //   CkEntityLifetime_Utils.h. Returns invalid handle if entity has no lifetime-owner
            //   fragment (i.e. it IS the transient entity).
            // BATCH-VERIFY: UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(Handle) — confirmed.
            //   Returns true only for the registry transient entity.
            if (NOT Is_TopLevelCandidate(Handle, TransientEntity))
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
        const auto EntityNum   = static_cast<int32>(InFocusEntity.Get_Entity().Get_EntityNumber());
        OutModel.Header = FText::FromString(
            FString::Printf(TEXT("%s [%d]"), *DebugName.ToString(), EntityNum));
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

    // ---- Focus card ----
    // Use the layout's DefaultStyle for the card level; per-provider style applied inside.
    // Guard against a missing History (shouldn't happen if called only from DoTick while active,
    // but defensive against edge-cases around deactivation ordering).
    if (NOT _History)
    { return; }
    _RootWidget->Set_FocusCardContent(InModel, InLayout.DefaultStyle, *_History, InNow, _FocusLocked);

    // ---- World tags: one per on-screen candidate (B1 — distance-scaled / faded / culled) ----
    auto WorldTags = TArray<FCk_DebugOverlay_WorldTagInfo>{};

    if (ck::IsValid(InPC))
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
            auto TokenParts = TArray<FString>{};
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

            if (TokenParts.IsEmpty())
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
            TagInfo.Text      = FText::FromString(FString::Join(TokenParts, TEXT(" | ")));
            TagInfo.Scale     = Scale;
            TagInfo.Opacity   = Opacity;
            WorldTags.Add(MoveTemp(TagInfo));

            // Hard cap to avoid clutter in dense scenes (e.g. crowds).
            if (WorldTags.Num() >= 16)
            { break; }
        }
    }

    _RootWidget->Update_WorldTags(WorldTags);
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
