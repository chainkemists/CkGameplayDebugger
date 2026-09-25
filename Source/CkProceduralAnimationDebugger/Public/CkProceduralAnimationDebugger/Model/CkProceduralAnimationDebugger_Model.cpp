#include "CkProceduralAnimationDebugger/Model/CkProceduralAnimationDebugger_Model.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkProceduralAnimation/Leg/CkProceduralLeg_Utils.h"

#include <Engine/World.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_debug_model
{
    auto
        Get_HasRosterChanged(
            const TArray<FCkProceduralAnimationDebugger_Row>& InBefore,
            const TArray<FCkProceduralAnimationDebugger_Row>& InAfter)
        -> bool
    {
        if (InBefore.Num() != InAfter.Num())
        { return true; }

        for (auto Index = 0; Index < InBefore.Num(); ++Index)
        {
            const auto& Before = InBefore[Index];
            const auto& After = InAfter[Index];
            if (NOT (Before.Entity == After.Entity) || Before.Label != After.Label || NOT (Before.Summary == After.Summary))
            { return true; }
        }
        return false;
    }
}

// --------------------------------------------------------------------------------------------------------------------

FCkProceduralAnimationDebugger_Model::
    FCkProceduralAnimationDebugger_Model()
{
    _SessionHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddRaw(this, &FCkProceduralAnimationDebugger_Model::Reset);
    _SelectionHandle = ck::DebugSelectionSync::Get_OnSelection().AddRaw(this, &FCkProceduralAnimationDebugger_Model::HandleSelection);
    _WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FCkProceduralAnimationDebugger_Model::HandleWorldCleanup);
}

// --------------------------------------------------------------------------------------------------------------------

FCkProceduralAnimationDebugger_Model::
    ~FCkProceduralAnimationDebugger_Model()
{
    ReleaseSession();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Get_World() const
    -> UWorld*
{
    return _World.Get();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Set_World(
        UWorld* InWorld)
    -> void
{
    if (_Released || _World.Get() == InWorld)
    { return; }

    Reset();
    _World = InWorld;
    Refresh();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Refresh()
    -> void
{
    if (_Released)
    { return; }

    auto Rows = FCkProceduralAnimationDebugger_DataCollector::Collect(_World.Get());
    const auto RosterChanged = ck_procedural_debug_model::Get_HasRosterChanged(_Rows, Rows);
    _Rows = MoveTemp(Rows);

    const auto RevisionBefore = _HistoryRevision;
    const auto GoneBefore = _SelectedGone;
    if (NOT _SelectedId.IsEmpty())
    { DoCapture_Selected(); }

    if (RosterChanged || _HistoryRevision != RevisionBefore || _SelectedGone != GoneBefore)
    { _OnChanged.Broadcast(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Select(
        const FCk_Handle& InEntity,
        ECkProceduralAnimationDebugger_SelectionSync InSync)
    -> bool
{
    if (_Released)
    { return false; }

    const auto Target = ck::DebugSelectionSync::Resolve_ClosestLineageMatch(InEntity,
        [](const FCk_Handle& InCandidate) -> bool
        {
            return FCkProceduralAnimationDebugger_DataCollector::Is_Supported(InCandidate);
        });

    if (ck::Is_NOT_Valid(Target))
    { return false; }

    auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Target);
    if (_World.Get() != World)
    {
        DoClear();
        _World = World;
    }

    _Rows = FCkProceduralAnimationDebugger_DataCollector::Collect(World);
    const auto* Row = _Rows.FindByPredicate([&](const FCkProceduralAnimationDebugger_Row& InRow)
    {
        return InRow.Entity == Target;
    });

    if (Row == nullptr)
    { return false; }

    if (_SelectedId != Row->Summary.Get_EntityId())
    {
        _History.Reset();
        ++_HistoryRevision;
        _SelectedId = Row->Summary.Get_EntityId();
        _SelectedLegId.Reset();
    }

    if (UCk_Utils_ProceduralLeg_UE::Has(InEntity))
    { _SelectedLegId = InEntity.Get_Entity().ToString(); }

    _SelectedGone = false;
    DoCapture_Selected();
    _OnChanged.Broadcast();

    if (InSync == ECkProceduralAnimationDebugger_SelectionSync::Broadcast)
    { ck::DebugSelectionSync::Broadcast(Target, Get_SelectionSource()); }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_SelectLeg(
        const FString& InLegEntityId)
    -> void
{
    if (_Released || _SelectedLegId == InLegEntityId)
    { return; }

    _SelectedLegId = InLegEntityId;
    _OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Get_SelectedHandle() const
    -> FCk_Handle
{
    const auto* Row = _Rows.FindByPredicate([&](const FCkProceduralAnimationDebugger_Row& InRow)
    {
        return InRow.Summary.Get_EntityId() == _SelectedId;
    });

    return Row != nullptr ? Row->Entity : FCk_Handle{};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Get_LiveStatus() const
    -> const FCk_ProceduralAnimation_DebugSnapshot*
{
    return _LiveSnapshot.IsSet() ? &_LiveSnapshot.GetValue() : nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Reset()
    -> void
{
    DoClear();
    _OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    ReleaseSession()
    -> void
{
    if (_Released)
    { return; }

    _Released = true;
    ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionHandle);
    ck::DebugSelectionSync::Get_OnSelection().Remove(_SelectionHandle);
    FWorldDelegates::OnWorldCleanup.Remove(_WorldCleanupHandle);
    Reset();
    _OnChanged.Clear();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_Hold()
    -> void
{
    if (_Released)
    { return; }

    _History.Hold();
    _OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_GoLive()
    -> void
{
    if (_Released)
    { return; }

    _History.GoLive();
    _OnChanged.Broadcast();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_Scrub(
        int32 InIndex)
    -> bool
{
    if (_Released || NOT _History.Scrub(InIndex))
    { return false; }

    _OnChanged.Broadcast();
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    Request_ScrubBySequence(
        uint64 InSequence)
    -> bool
{
    if (_Released || NOT _History.Scrub_BySequence(InSequence))
    { return false; }

    _OnChanged.Broadcast();
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    DoClear()
    -> void
{
    _Rows.Reset();
    _LiveSnapshot.Reset();
    _History.Reset();
    ++_HistoryRevision;
    _SelectedId.Reset();
    _SelectedLegId.Reset();
    _SelectedGone = false;
    _World.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    DoCapture_Selected()
    -> void
{
    const auto* Row = _Rows.FindByPredicate([&](const FCkProceduralAnimationDebugger_Row& InRow)
    {
        return InRow.Summary.Get_EntityId() == _SelectedId;
    });

    _SelectedGone = Row == nullptr;
    if (_SelectedGone)
    {
        _LiveSnapshot.Reset();
        return;
    }

    auto Snapshot = UCk_Utils_ProceduralAnimation_Debug_UE::Get_Snapshot(Row->Entity);
    if (_History.Push(Snapshot))
    { ++_HistoryRevision; }

    _LiveSnapshot = MoveTemp(Snapshot);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    HandleSelection(
        const FCk_Handle& InEntity,
        FName InSource)
    -> void
{
    if (_Released || InSource == Get_SelectionSource())
    { return; }

    const auto Guard = ck::DebugSelectionSync::FApplyGuard{};
    Select(InEntity, ECkProceduralAnimationDebugger_SelectionSync::Silent);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkProceduralAnimationDebugger_Model::
    HandleWorldCleanup(
        UWorld* InWorld,
        bool,
        bool)
    -> void
{
    if (_World.Get() == InWorld)
    { Reset(); }
}

// --------------------------------------------------------------------------------------------------------------------
