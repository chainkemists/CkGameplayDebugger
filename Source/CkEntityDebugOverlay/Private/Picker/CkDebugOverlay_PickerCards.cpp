#include "CkDebugOverlay_PickerCards.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Markers/CkDebug_EntityMarkers.h"

#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_Present.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_Selection.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

// =====================================================================================================================

FCkDebugOverlay_PickerCards::FCkDebugOverlay_PickerCards() = default;
FCkDebugOverlay_PickerCards::~FCkDebugOverlay_PickerCards() = default;

// ---------------------------------------------------------------------------------------------------------------------

auto
    FCkDebugOverlay_PickerCards::
    Activate(
        UWorld* InWorld) -> void
{
    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    // Coexistence: if the On-Screen Overlay subsystem is already running
    // (ck.DebugOverlay != 0) it is drawing its own focus card + world tags for this
    // player. Adding ours would double them up, so decline — the picker's diamonds
    // still draw.
    if (const auto* MasterCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.DebugOverlay"));
        MasterCVar != nullptr && MasterCVar->GetInt() != 0)
    { return; }

    auto* GVC = InWorld->GetGameViewport();
    if (ck::Is_NOT_Valid(GVC))
    { return; }

    if (_OverlayProviders.IsEmpty())
    {
        _OverlayProviders = FCk_DebugOverlay_Registry::Get().CreateAll();
    }
    _OverlayHistory = MakeUnique<FCk_DebugOverlay_History>();

    // v1: use the overlay's StartingLayout (the live cycle index lives in the subsystem
    // instance and isn't statically reachable). Picker and overlay can diverge if the user
    // cycles layouts; acceptable for now.
    _OverlayLayoutIndex = ck_debugoverlay::Get_StartingLayoutIndex(
        GetDefault<UCk_DebugOverlay_Settings>());

    _OverlayRoot = SNew(SCkDebugOverlay_Root);

    // Same Z-order as the overlay subsystem (above game UI, below modal dialogs).
    constexpr auto OverlayCardZOrder = 100;
    GVC->AddViewportWidgetContent(_OverlayRoot.ToSharedRef(), OverlayCardZOrder);

    _IsActive = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebugOverlay_PickerCards::
    Deactivate(
        UWorld* InWorld) -> void
{
    if (_OverlayRoot.IsValid())
    {
        if (ck::IsValid(InWorld))
        {
            if (auto* GVC = InWorld->GetGameViewport(); ck::IsValid(GVC))
            {
                GVC->RemoveViewportWidgetContent(_OverlayRoot.ToSharedRef());
            }
        }
        _OverlayRoot.Reset();
    }

    _OverlayProviders.Reset();
    _OverlayHistory.Reset();
    _IsActive = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebugOverlay_PickerCards::
    Update(
        UWorld*                       InWorld,
        APlayerController*            InPC,
        bool                          InIsEjected,
        const FCk_Handle&             InFocusEntity,
        const FCkDebug_EntityMarkers& InMarkers) -> void
{
    if (NOT _IsActive || NOT _OverlayRoot.IsValid())
    { return; }

    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    const auto* Layout   = ck_debugoverlay::Resolve_Layout(Settings, _OverlayLayoutIndex);
    if (Layout == nullptr || NOT _OverlayHistory)
    { return; }

    const auto Now = FPlatformTime::Seconds();

    // ---- Plate anchor + width + height budget (settings-driven; cheap no-op when unchanged) ----
    _OverlayRoot->Set_PlateLayout(
        Settings ? Settings->PlateAnchor : ECk_DebugOverlay_PlateAnchor::TopLeft,
        Settings ? Settings->PlateWidth  : 720.0f,
        Settings ? Settings->PlateMaxHeightFraction : 0.66f);

    // ---- Focus card for the sticky focus entity (empty model hides the card content) ----
    const auto Model = ck_debugoverlay::Build_EntityModel(
        InFocusEntity, _OverlayProviders, *Layout, _OverlayHistory.Get(), Now);

    auto CardStyle = Layout->DefaultStyle;
    CardStyle.FontScale *= Settings ? Settings->PlateFontScale : 1.0f;

    _OverlayRoot->Set_FocusCardContent(Model, CardStyle, *_OverlayHistory, Now, /*bIsLocked=*/false);

    // ---- World tags for the previewed candidates (the marker snapshot = the candidate set) ----
    auto Handles    = TArray<FCk_Handle>{};
    auto Candidates = TArray<ck_debugoverlay::FCandidate>{};
    Handles.Reserve(InMarkers.Get_Entries().Num());
    Candidates.Reserve(InMarkers.Get_Entries().Num());

    for (const auto& Entry : InMarkers.Get_Entries())
    {
        auto Cand          = ck_debugoverlay::FCandidate{};
        Cand.WorldLocation = Entry.WorldPos;
        Cand.Depth         = Entry.Depth;
        Cand.bIsOnScreen   = false;

        if (ck::IsValid(InPC) && NOT InIsEjected)
        {
            auto ScreenPos = FVector2D{};
            Cand.bIsOnScreen = UGameplayStatics::ProjectWorldToScreen(
                InPC, Entry.WorldPos, ScreenPos, /*bPlayerViewportRelative=*/false);
        }

        Handles.Add(Entry.Entity);
        Candidates.Add(Cand);
    }

    const auto WorldTags = ck_debugoverlay::Build_WorldTags(
        Handles, Candidates, _OverlayProviders, *Layout, InPC, InIsEjected);

    _OverlayRoot->Update_WorldTags(WorldTags);
}

// =====================================================================================================================
