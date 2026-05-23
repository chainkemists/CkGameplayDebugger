#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkGoapDebugger/Data/CkGoapDebugger_DataCollector.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkGoap/Action/CkGoap_Action_Utils.h"

// ====================================================================================================================

namespace ck_goap_debugger_viewmodel_internal
{
    // Forward decl — defined further down. Used in Tick's selection-validation
    // block to walk the recursive PlannerInfo tree (top-level + nested) so
    // promoted mid-tier Planners are recognised, not just top-level.
    static auto
    FindPlannerInfo_Recursive(
        const TArray<FCkGoapDebugger_PlannerInfo>& InPlanners,
        const FCk_Handle_Goap_Planner& InHandle) -> const FCkGoapDebugger_PlannerInfo*;

    // Cheap hash of the observable view-model state. Used to debounce
    // OnChanged broadcasts.
    static auto
    HashState(
        const TArray<FCkGoapDebugger_EntitySnapshot>& InSnapshots,
        const FCk_Handle& InSelectedEntity,
        const FCk_Handle_Goap_Planner& InSelectedActionSet,
        const FCk_Handle_Goap_Action& InSelectedAction,
        FCkGoapDebugger_ViewModel::EMode InMode,
        int32 InScrubIndex) -> uint32
    {
        auto Hash = uint32{0};

        Hash = HashCombine(Hash, ::GetTypeHash(InSelectedEntity));
        Hash = HashCombine(Hash, ::GetTypeHash(static_cast<FCk_Handle>(InSelectedActionSet)));
        Hash = HashCombine(Hash, ::GetTypeHash(static_cast<FCk_Handle>(InSelectedAction)));
        Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(InMode)));
        Hash = HashCombine(Hash, ::GetTypeHash(InScrubIndex));
        Hash = HashCombine(Hash, ::GetTypeHash(InSnapshots.Num()));

        // Per-entity coarse hash — chain length, status hash, frame number.
        // Keeps Tick cheap while still catching visible changes.
        for (const auto& Snap : InSnapshots)
        {
            Hash = HashCombine(Hash, ::GetTypeHash(Snap.EntityHandle));
            Hash = HashCombine(Hash, ::GetTypeHash(Snap.FrameNumber));
            Hash = HashCombine(Hash, ::GetTypeHash(Snap.ActionSets.Num()));

            for (const auto& As : Snap.ActionSets)
            {
                Hash = HashCombine(Hash, ::GetTypeHash(static_cast<FCk_Handle>(As.Handle)));
                Hash = HashCombine(Hash, ::GetTypeHash(As.ActiveChainHandles.Num()));
                Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(As.EnableToggle)));
                Hash = HashCombine(Hash, ::GetTypeHash(As.Catalog.Num()));

                for (const auto& Action : As.Catalog)
                {
                    Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(Action.PlanStatus)));
                    Hash = HashCombine(Hash, ::GetTypeHash(Action.PlanAttemptCount));
                }
            }
        }

        return Hash;
    }
} // namespace ck_goap_debugger_viewmodel_internal

// ====================================================================================================================
// TICK
// ====================================================================================================================

auto
    FCkGoapDebugger_ViewModel::
    Tick(
        UWorld* InWorld)
    -> void
{
    if (_Mode == EMode::Live)
    {
        _AllSnapshots = FCkGoapDebugger_DataCollector::CollectSnapshots(InWorld);
    }
    else
    {
        // Scrub mode: still pull a fresh batch in the background so the
        // entity picker stays current, but UI consumers render off the
        // selected history event's SnapshotAtEvent (see GetCurrent*).
        _AllSnapshots = FCkGoapDebugger_DataCollector::CollectSnapshots(InWorld);
    }

    // ---- Selection validation ---------------------------------------------------

    // Entity: if the selected entity is no longer in the snapshot list,
    // auto-pick the first available.
    const auto HasSelectedEntity = ck::IsValid(_SelectedEntity);
    const auto* SelectedSnapshot = HasSelectedEntity
        ? _AllSnapshots.FindByPredicate(
              [this](const FCkGoapDebugger_EntitySnapshot& In) { return In.EntityHandle == _SelectedEntity; })
        : nullptr;

    if (SelectedSnapshot == nullptr)
    {
        if (_AllSnapshots.Num() > 0)
        {
            _SelectedEntity    = _AllSnapshots[0].EntityHandle;
            _SelectedActionSet = FCk_Handle_Goap_Planner{};
            _SelectedAction    = FCk_Handle_Goap_Action{};
            SelectedSnapshot   = &_AllSnapshots[0];
        }
        else
        {
            _SelectedEntity    = FCk_Handle{};
            _SelectedActionSet = FCk_Handle_Goap_Planner{};
            _SelectedAction    = FCk_Handle_Goap_Action{};
        }
    }

    // Auto-select the first top-level Planner if none is currently selected.
    // Covers two cases:
    //   1. Entity change: the entity-validation block above cleared
    //      _SelectedActionSet; without this, PrimaryPane/GraphPane/WorldStateRail
    //      show empty placeholders because GetSelectedPlannerInfo() is nullptr.
    //   2. Same entity, but _SelectedActionSet was cleared (user action,
    //      Reset_ForWorldChange) and at least one top-level Planner exists.
    // Runs BEFORE the Planner-validation block below; the validation block
    // no-ops on a valid auto-selected handle.
    if (NOT ck::IsValid(_SelectedActionSet) && SelectedSnapshot != nullptr)
    {
        if (SelectedSnapshot->TopLevelPlanners.Num() > 0)
        {
            _SelectedActionSet = SelectedSnapshot->TopLevelPlanners[0].PlannerHandle;
        }
    }

    // Planner: if no longer in the selected entity's snapshot, clear.
    //
    // Walk TopLevelPlanners RECURSIVELY (via the same helper GetPlannerInfoByHandle
    // uses) so promoted mid-tier sub-Planners validate too. The legacy
    // ActionSets[] shim is top-level-only — searching it here would clobber any
    // child-Planner selection on the very next Tick after the user clicks it.
    if (SelectedSnapshot != nullptr && ck::IsValid(_SelectedActionSet))
    {
        const auto* Found = ck_goap_debugger_viewmodel_internal::FindPlannerInfo_Recursive(
            SelectedSnapshot->TopLevelPlanners, _SelectedActionSet);
        if (Found == nullptr)
        {
            _SelectedActionSet = FCk_Handle_Goap_Planner{};
            _SelectedAction    = FCk_Handle_Goap_Action{};
        }
    }
    else if (SelectedSnapshot == nullptr)
    {
        _SelectedActionSet = FCk_Handle_Goap_Planner{};
        _SelectedAction    = FCk_Handle_Goap_Action{};
    }

    // Action: if no longer in the selected Planner's catalog, clear.
    if (ck::IsValid(_SelectedAction))
    {
        const auto* AsInfo = GetSelectedActionSetInfo();
        if (AsInfo == nullptr)
        {
            _SelectedAction = FCk_Handle_Goap_Action{};
        }
        else
        {
            const auto* Found = AsInfo->Catalog.FindByPredicate(
                [this](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == _SelectedAction; });
            if (Found == nullptr)
            {
                _SelectedAction = FCk_Handle_Goap_Action{};
            }
        }
    }

    // Scrub index: clamp to history length for the selected entity.
    if (_Mode == EMode::Scrub && ck::IsValid(_SelectedEntity))
    {
        const auto& Hist = FCkGoapDebugger_DataCollector::GetHistory(_SelectedEntity);
        if (Hist.Num() == 0)
        {
            _ScrubEventIndex = INDEX_NONE;
        }
        else
        {
            _ScrubEventIndex = FMath::Clamp(_ScrubEventIndex, 0, Hist.Num() - 1);
        }
    }

    BroadcastIfChanged();
}

// ====================================================================================================================
// LIFECYCLE
// ====================================================================================================================

auto
    FCkGoapDebugger_ViewModel::
    Reset_ForWorldChange()
    -> void
{
    _AllSnapshots.Reset();
    _SelectedEntity    = FCk_Handle{};
    _SelectedActionSet = FCk_Handle_Goap_Planner{};
    _SelectedAction    = FCk_Handle_Goap_Action{};
    _Mode              = EMode::Live;
    _ScrubEventIndex   = INDEX_NONE;
    _LastBroadcastHash = 0;
    _HasBroadcast      = false;

    OnChanged.Broadcast();
}

// ====================================================================================================================
// SELECTION
// ====================================================================================================================

auto FCkGoapDebugger_ViewModel::SetSelectedEntity(FCk_Handle InHandle) -> void
{
    if (_SelectedEntity == InHandle) { return; }
    _SelectedEntity    = InHandle;
    _SelectedActionSet = FCk_Handle_Goap_Planner{};
    _SelectedAction    = FCk_Handle_Goap_Action{};
    BroadcastIfChanged();
}

auto FCkGoapDebugger_ViewModel::GetSelectedEntity() const -> FCk_Handle
{
    return _SelectedEntity;
}

auto FCkGoapDebugger_ViewModel::SetSelectedActionSet(FCk_Handle_Goap_Planner InHandle) -> void
{
    if (_SelectedActionSet == InHandle) { return; }
    _SelectedActionSet = InHandle;
    _SelectedAction    = FCk_Handle_Goap_Action{};
    BroadcastIfChanged();
}

auto FCkGoapDebugger_ViewModel::GetSelectedActionSet() const -> FCk_Handle_Goap_Planner
{
    return _SelectedActionSet;
}

auto FCkGoapDebugger_ViewModel::SetSelectedAction(FCk_Handle_Goap_Action InHandle) -> void
{
    if (_SelectedAction == InHandle) { return; }
    _SelectedAction = InHandle;
    BroadcastIfChanged();
}

auto FCkGoapDebugger_ViewModel::GetSelectedAction() const -> FCk_Handle_Goap_Action
{
    return _SelectedAction;
}

// ====================================================================================================================
// SNAPSHOT ACCESS
// ====================================================================================================================

auto
    FCkGoapDebugger_ViewModel::
    GetCurrentEntitySnapshot() const
    -> const FCkGoapDebugger_EntitySnapshot*
{
    if (NOT ck::IsValid(_SelectedEntity)) { return nullptr; }

    // Scrub mode: prefer the SnapshotAtEvent's parent ActionSet — but the
    // EntitySnapshot itself is always rendered from the live batch (we don't
    // freeze the entity-level frame counter; UI panels overlay scrub data).
    return _AllSnapshots.FindByPredicate(
        [this](const FCkGoapDebugger_EntitySnapshot& In) { return In.EntityHandle == _SelectedEntity; });
}

auto
    FCkGoapDebugger_ViewModel::
    GetSelectedActionSetInfo() const
    -> const FCkGoapDebugger_ActionSetInfo*
{
    if (NOT ck::IsValid(_SelectedActionSet)) { return nullptr; }

    // Scrub mode: read the SnapshotAtEvent on the selected history event
    // when it's for this ActionSet; otherwise fall back to live.
    if (_Mode == EMode::Scrub && ck::IsValid(_SelectedEntity) && _ScrubEventIndex != INDEX_NONE)
    {
        const auto& Hist = FCkGoapDebugger_DataCollector::GetHistory(_SelectedEntity);
        if (Hist.IsValidIndex(_ScrubEventIndex))
        {
            const auto& Event = Hist[_ScrubEventIndex];
            if (Event.SnapshotAtEvent.IsValid() &&
                Event.SnapshotAtEvent->Handle == _SelectedActionSet)
            {
                return Event.SnapshotAtEvent.Get();
            }
        }
    }

    const auto* Snap = GetCurrentEntitySnapshot();
    if (Snap == nullptr) { return nullptr; }

    return Snap->ActionSets.FindByPredicate(
        [this](const FCkGoapDebugger_ActionSetInfo& In) { return In.Handle == _SelectedActionSet; });
}

auto
    FCkGoapDebugger_ViewModel::
    GetSelectedActionInfo() const
    -> const FCkGoapDebugger_ActionInfo*
{
    if (NOT ck::IsValid(_SelectedAction)) { return nullptr; }

    const auto* AsInfo = GetSelectedActionSetInfo();
    if (AsInfo == nullptr) { return nullptr; }

    return AsInfo->Catalog.FindByPredicate(
        [this](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == _SelectedAction; });
}

auto
    FCkGoapDebugger_ViewModel::
    GetAllEntitySnapshots() const
    -> const TArray<FCkGoapDebugger_EntitySnapshot>&
{
    return _AllSnapshots;
}

// ====================================================================================================================
// PLANNER INFO LOOKUP (U11.7-C)
// ====================================================================================================================

namespace ck_goap_debugger_viewmodel_internal
{
    // Recursive walk over a TopLevelPlanners forest. Returns a pointer to the
    // matching PlannerInfo, or nullptr when the handle is not found. The
    // const_cast trick is intentional — the source array is held in a const
    // EntitySnapshot returned by const accessors; we want the pointer itself
    // const-but-mutable across the recursion so consumers receive a stable
    // pointer into the snapshot.
    static auto
    FindPlannerInfo_Recursive(
        const TArray<FCkGoapDebugger_PlannerInfo>& InPlanners,
        const FCk_Handle_Goap_Planner& InHandle) -> const FCkGoapDebugger_PlannerInfo*
    {
        for (const auto& P : InPlanners)
        {
            if (P.PlannerHandle == InHandle) { return &P; }
            if (auto* InChild = FindPlannerInfo_Recursive(P.ChildPlanners, InHandle))
            { return InChild; }
        }
        return nullptr;
    }
}

auto
    FCkGoapDebugger_ViewModel::
    GetPlannerInfoByHandle(
        FCk_Handle_Goap_Planner InHandle) const
    -> const FCkGoapDebugger_PlannerInfo*
{
    using namespace ck_goap_debugger_viewmodel_internal;

    if (NOT ck::IsValid(InHandle)) { return nullptr; }

    const auto* Snap = GetCurrentEntitySnapshot();
    if (Snap == nullptr) { return nullptr; }

    return FindPlannerInfo_Recursive(Snap->TopLevelPlanners, InHandle);
}

auto
    FCkGoapDebugger_ViewModel::
    GetSelectedPlannerInfo() const
    -> const FCkGoapDebugger_PlannerInfo*
{
    return GetPlannerInfoByHandle(_SelectedActionSet);
}

// ====================================================================================================================
// MODE + SCRUB
// ====================================================================================================================

auto FCkGoapDebugger_ViewModel::SetMode(EMode InMode) -> void
{
    if (_Mode == InMode) { return; }
    _Mode = InMode;
    if (_Mode == EMode::Live) { _ScrubEventIndex = INDEX_NONE; }
    BroadcastIfChanged();
}

auto FCkGoapDebugger_ViewModel::GetMode() const -> EMode
{
    return _Mode;
}

auto FCkGoapDebugger_ViewModel::SetScrubEventIndex(int32 InIndex) -> void
{
    if (_ScrubEventIndex == InIndex) { return; }
    _ScrubEventIndex = InIndex;
    BroadcastIfChanged();
}

auto FCkGoapDebugger_ViewModel::GetScrubEventIndex() const -> int32
{
    return _ScrubEventIndex;
}

// ====================================================================================================================
// COMMANDS
// ====================================================================================================================

auto
    FCkGoapDebugger_ViewModel::
    ForceReplanOnSelected()
    -> void
{
    if (NOT ck::IsValid(_SelectedAction)) { return; }

    auto MutableAction = _SelectedAction;
    UCk_Utils_Goap_Action_UE::Request_Plan(MutableAction);
}

// ====================================================================================================================
// PRIVATE
// ====================================================================================================================

auto
    FCkGoapDebugger_ViewModel::
    BroadcastIfChanged()
    -> void
{
    using namespace ck_goap_debugger_viewmodel_internal;

    const auto NewHash = HashState(
        _AllSnapshots,
        _SelectedEntity,
        _SelectedActionSet,
        _SelectedAction,
        _Mode,
        _ScrubEventIndex);

    if (_HasBroadcast && NewHash == _LastBroadcastHash) { return; }

    _LastBroadcastHash = NewHash;
    _HasBroadcast      = true;
    OnChanged.Broadcast();
}

// ====================================================================================================================
