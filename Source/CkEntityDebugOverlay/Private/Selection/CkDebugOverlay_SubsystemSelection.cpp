#include "CkEntityDebugOverlay/Subsystem/CkDebugOverlay_Subsystem.h"

#if WITH_CK_DEBUG_OVERLAY

#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_SelectionSettings.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_SelectionHud.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_SelectionPanel.h"
#include "CkDebuggerCommon/Classification/CkDebug_DepthTransparency.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkCore/Diagnostics/CkDiagnosticVisibility.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkLabel/CkLabel_Utils.h"
#include "CkPmg/CkPmg_Fragment.h"
#include "Engine/Console.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(CkSelectionRootTag, "Ck.Debug.Selection.Root");
UE_DEFINE_GAMEPLAY_TAG_STATIC(CkSelectionCounterTag, "Ck.Debug.Selection.Composite.Counter");

namespace
{
    auto SelectionId(const FCk_Handle& InHandle) -> uint32
    {
        return ck::IsValid(InHandle)
            ? static_cast<uint32>(InHandle.Get_Entity().Get_EntityNumber()) : MAX_uint32;
    }

    auto SelectionName(const FCk_Handle& InHandle) -> FString
    {
        if (ck::Is_NOT_Valid(InHandle))
        { return TEXT("removed"); }
        const auto Name = UCk_Utils_Handle_UE::Get_DebugName(InHandle);
        return Name.IsNone() ? FString::Printf(TEXT("Entity %u"), SelectionId(InHandle))
            : ck::DebugNameClean::Get_CleanName(Name.ToString());
    }
}

auto UCk_DebugOverlay_Subsystem::Reset_SelectionSession() -> void
{
    _SelectionNodes.Reset();
    _SelectionHandles.Reset();
    _WorldSelection.Reset();
    _FamilySelection.Reset();
    _SelectionRoot = FCk_Handle{};
    _SelectionAimTarget = FCk_Handle{};
    _SelectionCycleTarget = FCk_Handle{};
    _SelectionLockedEntity = FCk_Handle{};
    _FocusedEntity = FCk_Handle{};
    _LastSyncedEntity = FCk_Handle{};
    _LastFrameCandidates.Reset();
    _PinnedEntities.Reset();
    _SelectionWorld.Reset();
    _FamilyVisible = false;
    _SelectionProjectionValid = false;
    _FocusLocked = false;
    if (_InputProcessor.IsValid())
    { _InputProcessor->ClearSelectionState(); }
    if (_SelectionHud.IsValid())
    { _SelectionHud->SetSnapshot({}, {}, 1.0f); }
}

auto UCk_DebugOverlay_Subsystem::CanHandle_SelectionInput() const -> bool
{
    if (NOT _RootWidget.IsValid() || _SelectionPanel.IsValid())
    { return false; }
    return CanHandle_GlobalInput();
}

auto UCk_DebugOverlay_Subsystem::CanHandle_GlobalInput() const -> bool
{
    if (NOT FSlateApplication::IsInitialized() || ck::diagnostic_visibility::Is_HiddenForStreamerMode())
    { return false; }
    const auto& App = FSlateApplication::Get();
    const auto Window = App.GetActiveTopLevelWindow();
    if (NOT App.IsActive() || NOT Window.IsValid() || NOT Window->IsActive())
    { return false; }
    auto* World = Resolve_ActiveWorld();
    if (ck::Is_NOT_Valid(World))
    { return false; }
    auto* Viewport = World->GetGameViewport();
    if (ck::Is_NOT_Valid(Viewport))
    { return false; }
    if (Viewport->ViewportConsole && Viewport->ViewportConsole->ConsoleActive())
    { return false; }
    // Only a focused viewport may own shortcuts. Editable text, console, chat, tool windows,
    // and the settings drawer never satisfy this; ejected SViewport is handled identically.
    const auto Focused = App.GetKeyboardFocusedWidget();
    if (NOT Focused.IsValid() || Focused->GetType() != FName(TEXT("SViewport")))
    { return false; }
    return ck::DebugViewportView::Get_IsEjected() || Focused == Viewport->GetGameViewportWidget();
}

auto UCk_DebugOverlay_Subsystem::Update_Selection(UWorld* InWorld,
    const ck_debugoverlay::FViewpoint& InViewpoint, TArray<FCk_Handle>& OutHandles,
    TArray<ck_debugoverlay::FCandidate>& OutCandidates) -> void
{
    using namespace ck_debugoverlay::selection_session;
    if (ck::diagnostic_visibility::Is_HiddenForStreamerMode())
    { Close_SelectionSettings(); }
    if (_SelectionWorld.Get() != InWorld)
    {
        Reset_SelectionSession();
        _SelectionWorld = InWorld;
    }
    _SelectionView = {InViewpoint.Location, InViewpoint.Forward};
    _SelectionProjectionValid = ck::DebugViewportView::TryGet_Projection(InWorld, _SelectionProjection);
    _SelectionNodes.Reset();
    _SelectionHandles.Reset();
    auto Transient = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
    if (ck::Is_NOT_Valid(Transient))
    { return; }
    const auto* Preferences = UCk_DebugOverlay_SelectionSettings::Get();
    const auto& Config = Preferences->Get_Config();
    const auto AddNode = [&](const FCk_Handle& InHandle)
    {
        if (ck::Is_NOT_Valid(InHandle))
        { return; }
        auto Node = FNode{};
        Node.Id = SelectionId(InHandle);
        Node.TransparentInfrastructure = UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(InHandle) ||
            ck::DebugDepthTransparency::Get_IsRelayEntity(InHandle);
        Node.Selectable = NOT Node.TransparentInfrastructure &&
            NOT InHandle.Has<ck::FFragment_Pmg_DebugShape_Common>() &&
            NOT ck::DebugViewportView::Get_IsLocalPlayerSelf(InHandle);
        const auto IsContextRoot = UCk_Utils_ContextOwner_UE::Has(InHandle) &&
            UCk_Utils_ContextOwner_UE::Get_ContextOwner(InHandle) == InHandle;
        Node.MeaningfulBoundary = NOT Node.TransparentInfrastructure && (IsContextRoot ||
            (UCk_Utils_GameplayLabel_UE::Has(InHandle) &&
             UCk_Utils_GameplayLabel_UE::MatchesExact(InHandle, CkSelectionRootTag)));
        if (InHandle != Transient)
        { Node.OwnerId = SelectionId(UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InHandle)); }
        if (InHandle.Has<ck::FFragment_Transform>())
        {
            Node.Position = InHandle.Get<ck::FFragment_Transform>().Get_Transform().GetLocation();
            Node.HasPosition = NOT Node.Position.ContainsNaN();
            if (Node.HasPosition && _SelectionProjectionValid)
            { _SelectionProjection.Project(Node.Position, Node.ScreenPos, Node.IsOnScreen); }
            if (Node.Selectable && Node.HasPosition && NOT Config.IncludeOccluded &&
                FVector::DistSquared(Node.Position, InViewpoint.Location) <= FMath::Square(Config.SearchRadius))
            {
                auto Params = FCollisionQueryParams(SCENE_QUERY_STAT(CkSelectionOcclusion), false);
                if (auto* PC = InWorld->GetFirstPlayerController(); ck::IsValid(PC))
                { Params.AddIgnoredActor(PC->GetPawn().Get()); }
                // An entity's own actor is the target, not an occluder.
                if (auto* Actor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor_Recursive(InHandle); ck::IsValid(Actor))
                { Params.AddIgnoredActor(Actor); }
                Node.Occluded = InWorld->LineTraceTestByChannel(InViewpoint.Location, Node.Position,
                    ECC_Visibility, Params);
            }
        }
        _SelectionNodes.Add(Node);
        _SelectionHandles.Add(Node.Id, InHandle);
    };
    AddNode(Transient);
    Transient.View<ck::FFragment_LifetimeOwner, CK_IGNORE_PENDING_KILL>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_LifetimeOwner&)
        {
            const auto Handle = ck::MakeHandle(InEntity, Transient);
            if (Handle != Transient)
            { AddNode(Handle); }
        });

    // Remove dead/reused slots without reordering surviving entries. Cached identity includes
    // generation; a newly spawned entity at the same numeric slot is never adopted silently.
    const auto Prune = [this](TArray<FSelectionEntry>& InEntries)
    {
        InEntries.RemoveAll([this](const FSelectionEntry& InEntry)
        {
            const auto* Live = _SelectionHandles.Find(InEntry.Candidate.Id);
            return Live == nullptr || NOT (*Live == InEntry.Entity);
        });
    };
    Prune(_WorldSelection);
    Prune(_FamilySelection);
    if (ck::Is_NOT_Valid(_FocusedEntity))
    { _FocusedEntity = FCk_Handle{}; }
    if (_FamilyVisible && ck::Is_NOT_Valid(_SelectionRoot))
    { Set_FamilyVisible(false); }
    if (_SelectionRevision != Preferences->Get_Revision())
    {
        _SelectionRevision = Preferences->Get_Revision();
        _SelectionCycleTarget = FCk_Handle{};
    }
    // Membership is live even when numbering/order is stable. Late-spawned gym fixtures and
    // entities entering range cannot be stranded outside an activation-time snapshot.
    Refresh_SelectionSnapshot(false);

    if (_InputProcessor.IsValid())
    {
        _InputProcessor->SetSelectionBindings(*GetDefault<UCk_DebugOverlay_InputSettings>());
        if (NOT CanHandle_SelectionInput())
        {
            _InputProcessor->ClearSelectionState();
            if (Config.Family == ECk_DebugOverlay_SelectionFamily::Hold && NOT _SelectionPanel.IsValid())
            { Set_FamilyVisible(false); }
        }
        for (const auto Action : _InputProcessor->ConsumeSelectionActions())
        {
            switch (Action)
            {
                case ECkDebugOverlaySelectionInputAction::Select: DoCmd_Select(); break;
                case ECkDebugOverlaySelectionInputAction::ToggleSelectionLock: DoCmd_Lock(); break;
                case ECkDebugOverlaySelectionInputAction::Next: DoCmd_Next(); break;
                case ECkDebugOverlaySelectionInputAction::Previous: DoCmd_Prev(); break;
                case ECkDebugOverlaySelectionInputAction::Settings: DoCmd_Settings(); break;
                case ECkDebugOverlaySelectionInputAction::FamilyPressed:
                    Set_FamilyVisible(Config.Family == ECk_DebugOverlay_SelectionFamily::Toggle ? NOT _FamilyVisible : true);
                    break;
                case ECkDebugOverlaySelectionInputAction::FamilyReleased:
                    if (Config.Family == ECk_DebugOverlay_SelectionFamily::Hold)
                    { Set_FamilyVisible(false); }
                    break;
            }
        }
    }
    const auto& Entries = _FamilyVisible ? _FamilySelection : _WorldSelection;
    for (const auto& Entry : Entries)
    {
        const auto Position = TryGet_SelectionPosition(Entry);
        if (NOT Position.IsSet())
        { continue; }
        auto Candidate = ck_debugoverlay::FCandidate{};
        Candidate.WorldLocation = Position.GetValue();
        if (_SelectionProjectionValid)
        { _SelectionProjection.Project(Candidate.WorldLocation, Candidate.ScreenPos, Candidate.bIsOnScreen); }
        OutHandles.Add(Entry.Entity);
        OutCandidates.Add(Candidate);
    }
    _LastFrameCandidates = OutHandles;
    _FocusLocked = ck::IsValid(_SelectionLockedEntity);
}

auto UCk_DebugOverlay_Subsystem::Refresh_SelectionSnapshot(bool InSelectBest) -> void
{
    using namespace ck_debugoverlay::selection_session;
    const auto& Config = UCk_DebugOverlay_SelectionSettings::Get()->Get_Config();
    const auto Candidates = _FamilyVisible
        ? BuildFamily(_SelectionNodes, SelectionId(_SelectionRoot), _SelectionView, Config)
        : BuildDiscoveryCandidates(_SelectionNodes, _SelectionView, Config);
    auto& Entries = _FamilyVisible ? _FamilySelection : _WorldSelection;
    auto PreviousIds = TArray<uint32>{};
    for (const auto& Entry : Entries)
    {
        const auto* Live = _SelectionHandles.Find(Entry.Candidate.Id);
        if (Live != nullptr && ck::IsValid(Entry.Entity) && *Live == Entry.Entity)
        {
            PreviousIds.Add(Entry.Candidate.Id);
        }
    }
    auto CurrentIds = TArray<uint32>{};
    auto FreshEntries = TMap<uint32, FSelectionEntry>{};
    for (const auto& Candidate : Candidates)
    {
        const auto* Handle = _SelectionHandles.Find(Candidate.Id);
        if (Handle != nullptr)
        {
            const auto* Anchor = _SelectionHandles.Find(Candidate.AnchorId);
            FreshEntries.Add(Candidate.Id, {*Handle, Candidate, Anchor != nullptr ? *Anchor : FCk_Handle{}});
            CurrentIds.Add(Candidate.Id);
        }
    }
    const auto Rerank = Config.Stability == ECk_DebugOverlay_SelectionStability::Live;
    const auto OrderedIds = ReconcileOrder(PreviousIds, CurrentIds, Rerank);
    Entries.Reset();
    auto ValidIds = TSet<uint32>{};
    for (const auto Id : OrderedIds)
    {
        auto Entry = FreshEntries.FindChecked(Id);
        ValidIds.Add(Id);
        Entries.Add(MoveTemp(Entry));
    }

    // The cone/view scope limits aim selection, not discovery. All in-range roots can still
    // display diamonds and be cycled. Only an explicit selection lock suppresses aim following.
    const auto AimCandidates = _FamilyVisible ? Candidates : BuildCandidates(_SelectionNodes, _SelectionView, Config);
    const auto EligibleAim = AimCandidates.FilterByPredicate([&ValidIds](const FCandidateSelection& InCandidate)
        { return ValidIds.Contains(InCandidate.Id); });
    const auto AimId = PickAimCandidate(EligibleAim, Config, _FamilyVisible);
    if (ck::Is_NOT_Valid(_SelectionLockedEntity))
    { _SelectionLockedEntity = FCk_Handle{}; }
    const auto FocusId = ResolveAimFocus(AimId, SelectionId(_SelectionAimTarget),
        InSelectBest ? InvalidEntityId : SelectionId(_SelectionCycleTarget), ValidIds,
        SelectionId(_SelectionLockedEntity));
    if (InSelectBest || AimId != SelectionId(_SelectionAimTarget) ||
        NOT ValidIds.Contains(SelectionId(_SelectionCycleTarget)))
    { _SelectionCycleTarget = FCk_Handle{}; }
    const auto* AimHandle = _SelectionHandles.Find(AimId);
    _SelectionAimTarget = AimHandle != nullptr ? *AimHandle : FCk_Handle{};
    const auto* FocusHandle = _SelectionHandles.Find(FocusId);
    _FocusedEntity = FocusHandle != nullptr ? *FocusHandle : FCk_Handle{};
    if (NOT _FamilyVisible)
    {
        const auto RootId = ResolveRoot(_SelectionNodes, SelectionId(_FocusedEntity), Config);
        const auto* Root = _SelectionHandles.Find(RootId);
        _SelectionRoot = Root != nullptr ? *Root : FCk_Handle{};
    }
    _FocusLocked = ck::IsValid(_SelectionLockedEntity);
}

auto UCk_DebugOverlay_Subsystem::Build_SelectionNavigationOrder() const -> TArray<uint32>
{
    const auto& Entries = _FamilyVisible ? _FamilySelection : _WorldSelection;
    auto Candidates = TArray<ck_debugoverlay::selection_session::FCandidateSelection>{};
    Candidates.Reserve(Entries.Num());
    for (const auto& Entry : Entries)
    { Candidates.Add(Entry.Candidate); }
    return ck_debugoverlay::selection_session::BuildSpatialOrder(
        Candidates, SelectionId(_FocusedEntity), SelectionId(_SelectionRoot));
}

auto UCk_DebugOverlay_Subsystem::Cycle_Selection(int32 InDirection) -> void
{
    using namespace ck_debugoverlay::selection_session;
    const auto& Entries = _FamilyVisible ? _FamilySelection : _WorldSelection;
    const auto Ids = Build_SelectionNavigationOrder();
    auto Live = TSet<uint32>{};
    for (const auto& Entry : Entries)
    {
        if (ck::IsValid(Entry.Entity))
        { Live.Add(Entry.Candidate.Id); }
    }
    const auto Target = Cycle(Ids, Live, SelectionId(_FocusedEntity), SelectionId(_SelectionRoot), InDirection);
    const auto* Entry = Entries.FindByPredicate([Target](const FSelectionEntry& InEntry)
        { return InEntry.Candidate.Id == Target; });
    if (Entry == nullptr)
    { return; }
    _FocusedEntity = Entry->Entity;
    _SelectionCycleTarget = Entry->Entity;
    // Explicit navigation while locked moves the lock; camera motion alone never does.
    if (ck::IsValid(_SelectionLockedEntity))
    { _SelectionLockedEntity = Entry->Entity; }
    if (NOT _FamilyVisible)
    { _SelectionRoot = _FocusedEntity; }
}

auto UCk_DebugOverlay_Subsystem::Set_FamilyVisible(bool InVisible) -> void
{
    if (_FamilyVisible == InVisible)
    { return; }
    if (InVisible && ck::Is_NOT_Valid(_SelectionRoot))
    { return; }
    _FamilyVisible = InVisible;
    _FamilySelection.Reset();
    _SelectionCycleTarget = FCk_Handle{};
    Refresh_SelectionSnapshot(true);
}

auto UCk_DebugOverlay_Subsystem::DoCmd_Select() -> void
{
    _SelectionLockedEntity = FCk_Handle{};
    _FocusLocked = false;
    Refresh_SelectionSnapshot(true);
}

auto UCk_DebugOverlay_Subsystem::DoCmd_Family() -> void
{
    Set_FamilyVisible(NOT _FamilyVisible);
}

auto UCk_DebugOverlay_Subsystem::Get_SelectionStatus(bool InCompact) const -> FText
{
    using namespace ck_debugoverlay::selection_session;
    const auto& Entries = _FamilyVisible ? _FamilySelection : _WorldSelection;
    const auto Ids = Build_SelectionNavigationOrder();
    auto Live = TSet<uint32>{};
    for (const auto& Entry : Entries)
    {
        if (ck::IsValid(Entry.Entity))
        { Live.Add(Entry.Candidate.Id); }
    }
    const auto RelativeLabels = BuildRelativeLabels(Ids, SelectionId(_FocusedEntity), SelectionId(_SelectionRoot));
    const auto DescribeNumber = [&RelativeLabels](uint32 InId) -> FString
    {
        const auto* Label = RelativeLabels.Find(InId);
        return Label != nullptr ? *Label : TEXT("none");
    };
    const auto Describe = [&Entries, &DescribeNumber](uint32 InId) -> FString
    {
        const auto* Entry = Entries.FindByPredicate([InId](const FSelectionEntry& InEntry)
            { return InEntry.Candidate.Id == InId; });
        return Entry == nullptr ? TEXT("none") : DescribeNumber(InId) + TEXT(" ") + SelectionName(Entry->Entity);
    };
    const auto Next = Cycle(Ids, Live, SelectionId(_FocusedEntity), SelectionId(_SelectionRoot), 1);
    const auto Previous = Cycle(Ids, Live, SelectionId(_FocusedEntity), SelectionId(_SelectionRoot), -1);
    const auto* Keys = GetDefault<UCk_DebugOverlay_InputSettings>();
    const auto& Config = UCk_DebugOverlay_SelectionSettings::Get()->Get_Config();
    if (InCompact)
    {
        const auto CountText = _FamilyVisible ? FString::Printf(TEXT("%d members"), Entries.Num())
            : FString::Printf(TEXT("%d in range (%.0f)"), Entries.Num(), Config.SearchRadius);
        return FText::FromString(FString::Printf(
            TEXT("%s | %s | %s Prev %s   %s Next %s | hold %s %s | %s Family   %s%s Settings"),
            _FocusLocked ? TEXT("LOCKED") : _FamilyVisible ? TEXT("FAMILY") : TEXT("AIM"),
            *CountText,
            *Keys->PreviousKey.GetDisplayName().ToString(), *DescribeNumber(Previous),
            *Keys->NextKey.GetDisplayName().ToString(), *DescribeNumber(Next),
            *Keys->SelectKey.GetDisplayName().ToString(), _FocusLocked ? TEXT("unlock") : TEXT("lock"),
            *Keys->FamilyKey.GetDisplayName().ToString(),
            Keys->SettingsRequireShift ? TEXT("Shift+") : TEXT(""), *Keys->SettingsKey.GetDisplayName().ToString()));
    }
    const auto SelectedPath = ck::IsValid(_SelectionRoot) && NOT (_SelectionRoot == _FocusedEntity)
        ? SelectionName(_SelectionRoot) + TEXT(" / ") + SelectionName(_FocusedEntity) : SelectionName(_FocusedEntity);
    auto Status = FString::Printf(TEXT("%s %s | %d candidates | %s\n%s Prev %s    %s Next %s\nTap %s select best; hold to lock/unlock   %s Family   %s%s Settings"),
        _FocusLocked ? TEXT("LOCKED") : _FamilyVisible ? TEXT("FAMILY") : TEXT("AIM"),
        Config.Stability == ECk_DebugOverlay_SelectionStability::Frozen ? TEXT("STABLE ORDER") : TEXT("RERANKED"),
        Entries.Num(), *SelectedPath,
        *Keys->PreviousKey.GetDisplayName().ToString(), *Describe(Previous),
        *Keys->NextKey.GetDisplayName().ToString(), *Describe(Next),
        *Keys->SelectKey.GetDisplayName().ToString(), *Keys->FamilyKey.GetDisplayName().ToString(),
        Keys->SettingsRequireShift ? TEXT("Shift+") : TEXT(""), *Keys->SettingsKey.GetDisplayName().ToString());
    if (Config.Labels == ECk_DebugOverlay_SelectionLabels::Shortlist)
    {
        for (auto Index = 0; Index < FMath::Min(Entries.Num(), 8); ++Index)
        { Status += TEXT("\n") + Describe(Entries[Index].Candidate.Id); }
        if (Entries.Num() > 8)
        { Status += FString::Printf(TEXT("\n+%d more; Next/Prev traverse the complete list"), Entries.Num() - 8); }
    }
    return FText::FromString(Status);
}

auto UCk_DebugOverlay_Subsystem::TryGet_SelectionPosition(const FSelectionEntry& InEntry) const -> TOptional<FVector>
{
    const auto* LiveAnchor = _SelectionHandles.Find(InEntry.Candidate.AnchorId);
    if (LiveAnchor == nullptr || ck::Is_NOT_Valid(InEntry.AnchorEntity) || NOT (*LiveAnchor == InEntry.AnchorEntity))
    { return {}; }
    const auto* Node = _SelectionNodes.FindByPredicate([&InEntry](const auto& InNode)
        { return InNode.Id == InEntry.Candidate.AnchorId; });
    return Node != nullptr && Node->HasPosition ? TOptional<FVector>{Node->Position} : TOptional<FVector>{};
}

auto UCk_DebugOverlay_Subsystem::Update_SelectionHud() -> void
{
    if (NOT _SelectionHud.IsValid())
    { return; }
    const auto Hidden = ck::diagnostic_visibility::Is_HiddenForStreamerMode();
    _SelectionHud->SetVisibility(Hidden ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
    if (Hidden)
    { return; }
    const auto& Config = UCk_DebugOverlay_SelectionSettings::Get()->Get_Config();
    const auto& Entries = _FamilyVisible ? _FamilySelection : _WorldSelection;
    auto Markers = TArray<SCkDebugOverlay_SelectionHud::FMarker>{};
    auto ConePoints = TArray<FVector2D>{};
    const auto OrderedIds = Build_SelectionNavigationOrder();
    const auto RelativeLabels = ck_debugoverlay::selection_session::BuildRelativeLabels(
        OrderedIds, SelectionId(_FocusedEntity), SelectionId(_SelectionRoot));
    const auto LocalSize = _SelectionHud->GetCachedGeometry().GetLocalSize();
    const auto PixelToLocal = FVector2D(
        _SelectionProjection.ViewSize.X > 0 ? LocalSize.X / _SelectionProjection.ViewSize.X : 1.0,
        _SelectionProjection.ViewSize.Y > 0 ? LocalSize.Y / _SelectionProjection.ViewSize.Y : 1.0);
    for (const auto& Entry : Entries)
    {
        const auto Position = TryGet_SelectionPosition(Entry);
        if (NOT Position.IsSet())
        { continue; }
        auto Pixel = FVector2D{};
        auto Inside = false;
        if (NOT _SelectionProjectionValid || NOT _SelectionProjection.Project(Position.GetValue(), Pixel, Inside) || NOT Inside)
        { continue; }
        auto Marker = SCkDebugOverlay_SelectionHud::FMarker{};
        Marker.Position = Pixel * PixelToLocal;
        Marker.Selected = Entry.Entity == _FocusedEntity ||
            (NOT _FamilyVisible && ck::IsValid(_FocusedEntity) && Entry.Entity == _SelectionRoot);
        Marker.Locked = Marker.Selected && _FocusLocked;
        Marker.RelativeLabel = RelativeLabels.FindRef(Entry.Candidate.Id);
        Markers.Add(MoveTemp(Marker));
    }
    if (Config.ShowAimCone && _SelectionProjectionValid)
    {
        auto Right = FVector{};
        auto Up = FVector{};
        const auto Forward = _SelectionView.Forward.GetSafeNormal();
        Forward.FindBestAxisVectors(Right, Up);
        const auto HalfAngle = FMath::DegreesToRadians(Config.ConeHalfAngle);
        for (auto Index = 0; Index <= 96; ++Index)
        {
            const auto Phase = 2.0 * PI * Index / 96.0;
            const auto Direction = Forward * FMath::Cos(HalfAngle) +
                (Right * FMath::Cos(Phase) + Up * FMath::Sin(Phase)) * FMath::Sin(HalfAngle);
            auto Pixel = FVector2D{};
            auto Inside = false;
            if (NOT _SelectionProjection.Project(_SelectionView.Location + Direction * 1000.0, Pixel, Inside))
            { ConePoints.Reset(); break; }
            ConePoints.Add(Pixel * PixelToLocal);
        }
    }
    _SelectionHud->SetSnapshot(MoveTemp(Markers), MoveTemp(ConePoints), Config.DiamondScale);
}

auto UCk_DebugOverlay_Subsystem::DoCmd_Settings() -> void
{
    if (_SelectionPanel.IsValid())
    { Close_SelectionSettings(); return; }
    if (NOT FSlateApplication::IsInitialized())
    { return; }
    const auto* LocalPlayer = GetLocalPlayer();
    if (ck::Is_NOT_Valid(LocalPlayer) || ck::Is_NOT_Valid(LocalPlayer->ViewportClient.Get()))
    { return; }
    auto* Viewport = LocalPlayer->ViewportClient.Get();
    Set_FamilyVisible(false);
    _SelectionPriorFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
    const auto WeakSubsystem = TWeakObjectPtr<UCk_DebugOverlay_Subsystem>(this);
    _SelectionPanel = SNew(SCkDebugOverlay_SelectionPanel)
        .OnClose(FSimpleDelegate::CreateUObject(this, &UCk_DebugOverlay_Subsystem::Close_SelectionSettings))
        .OnSelect([WeakSubsystem]() { if (auto* Self = WeakSubsystem.Get()) { Self->DoCmd_Select(); } })
        .OnPrevious([WeakSubsystem]() { if (auto* Self = WeakSubsystem.Get()) { Self->DoCmd_Prev(); } })
        .OnNext([WeakSubsystem]() { if (auto* Self = WeakSubsystem.Get()) { Self->DoCmd_Next(); } })
        .OnFamily([WeakSubsystem]() { if (auto* Self = WeakSubsystem.Get()) { Self->DoCmd_Family(); } })
        .StatusText_Lambda([WeakSubsystem]() { const auto* Self = WeakSubsystem.Get();
            return Self != nullptr ? Self->Get_SelectionStatus() : FText::GetEmpty(); })
        .ExplanationText(FText::FromString(TEXT("Runtime preferences persist in GameUserSettings. Discovery updates continuously within range; cone and view scope govern aim selection. Badges rank the nearest screen-left (-) and screen-right (+) entities relative to selection (0). Tap a separately bound Select key for best aim; hold it to lock/unlock selection. Double-Shift independently pins a data card. Roots skip Transient/ActorRelay. Shipping debugger is disabled.")));
    _SelectionPanelHost = SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Fill)
        [ SNew(SBox).WidthOverride(480.0f)[_SelectionPanel.ToSharedRef()] ];
    Viewport->AddViewportWidgetContent(_SelectionPanelHost.ToSharedRef(), 120);
    if (auto* PC = LocalPlayer->GetPlayerController(GetWorld()); ck::IsValid(PC))
    {
        _SelectionInputController = PC;
        _SelectionPriorCursor = PC->bShowMouseCursor;
        PC->bShowMouseCursor = true;
        PC->SetIgnoreLookInput(true);
        PC->SetIgnoreMoveInput(true);
    }
    FSlateApplication::Get().ReleaseAllPointerCapture();
    FSlateApplication::Get().SetKeyboardFocus(_SelectionPanel, EFocusCause::SetDirectly);
}

auto UCk_DebugOverlay_Subsystem::Close_SelectionSettings() -> void
{
    if (NOT _SelectionPanelHost.IsValid())
    { return; }
    if (const auto* LocalPlayer = GetLocalPlayer(); ck::IsValid(LocalPlayer))
    {
        if (auto* Viewport = LocalPlayer->ViewportClient.Get(); ck::IsValid(Viewport))
        { Viewport->RemoveViewportWidgetContent(_SelectionPanelHost.ToSharedRef()); }
    }
    if (auto* PC = _SelectionInputController.Get())
    {
        PC->bShowMouseCursor = _SelectionPriorCursor;
        PC->SetIgnoreLookInput(false);
        PC->SetIgnoreMoveInput(false);
    }
    _SelectionInputController.Reset();
    _SelectionPanel.Reset();
    _SelectionPanelHost.Reset();
    if (FSlateApplication::IsInitialized())
    {
        if (const auto Prior = _SelectionPriorFocus.Pin())
        { FSlateApplication::Get().SetKeyboardFocus(Prior, EFocusCause::SetDirectly); }
    }
    _SelectionPriorFocus.Reset();
}

#endif
