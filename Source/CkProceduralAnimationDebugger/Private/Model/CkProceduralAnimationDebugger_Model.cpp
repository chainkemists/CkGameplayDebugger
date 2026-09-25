#include "CkProceduralAnimationDebugger/Model/CkProceduralAnimationDebugger_Model.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkCore/Validation/CkIsValid.h"
#include <Engine/World.h>

FCkProceduralAnimationDebugger_Model::FCkProceduralAnimationDebugger_Model()
{
    _SessionHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddRaw(this, &FCkProceduralAnimationDebugger_Model::Reset);
    _SelectionHandle = ck::DebugSelectionSync::Get_OnSelection().AddRaw(this, &FCkProceduralAnimationDebugger_Model::HandleSelection);
    _WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FCkProceduralAnimationDebugger_Model::HandleWorldCleanup);
}

FCkProceduralAnimationDebugger_Model::~FCkProceduralAnimationDebugger_Model()
{ ReleaseSession(); }

auto
    FCkProceduralAnimationDebugger_Model::
    Get_World() const
    -> UWorld*

{ return _World.Get(); }

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

auto
    FCkProceduralAnimationDebugger_Model::
    Refresh()
    -> void

{
    if (_Released)
    { return; }
    _Rows = FCkProceduralAnimationDebugger_DataCollector::Collect(_World.Get());
    if (NOT _SelectedId.IsEmpty())
    {
        const auto* Selected = Get_LiveStatus();
        _SelectedGone = Selected == nullptr;
        if (Selected != nullptr)
        { _History.Push(*Selected); }
    }
    _OnChanged.Broadcast();
}

auto
    FCkProceduralAnimationDebugger_Model::
    Select(
        const FCk_Handle& InEntity,
        bool InBroadcast)
    -> bool

{
    if (_Released)
    { return false; }
    const auto Target = ck::DebugSelectionSync::Resolve_ClosestLineageMatch(InEntity,
        [](const auto& InCandidate) { return FCkProceduralAnimationDebugger_DataCollector::Is_Supported(InCandidate); });
    if (NOT ck::IsValid(Target))
    { return false; }
    Set_World(UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Target));
    Refresh();
    const auto* Row = _Rows.FindByPredicate([&](const auto& InRow) { return InRow.Entity == Target; });
    if (Row == nullptr)
    { return false; }
    if (_SelectedId != Row->Snapshot.Get_EntityId())
    {
        _History.Reset();
        _SelectedId = Row->Snapshot.Get_EntityId();
    }
    _SelectedGone = false;
    _History.Push(Row->Snapshot);
    _OnChanged.Broadcast();
    if (InBroadcast)
    { ck::DebugSelectionSync::Broadcast(Target, Get_SelectionSource()); }
    return true;
}

auto
    FCkProceduralAnimationDebugger_Model::
    Get_SelectedHandle() const
    -> FCk_Handle

{
    const auto* Row = _Rows.FindByPredicate([&](const auto& InRow) { return InRow.Snapshot.Get_EntityId() == _SelectedId; });
    return Row != nullptr ? Row->Entity : FCk_Handle{};
}

auto
    FCkProceduralAnimationDebugger_Model::
    Get_LiveStatus() const
    -> const FCk_ProceduralAnimation_DebugSnapshot*

{
    const auto* Row = _Rows.FindByPredicate([&](const auto& InRow) { return InRow.Snapshot.Get_EntityId() == _SelectedId; });
    return Row != nullptr ? &Row->Snapshot : nullptr;
}

auto
    FCkProceduralAnimationDebugger_Model::
    Reset()
    -> void

{
    _Rows.Reset();
    _History.Reset();
    _SelectedId.Reset();
    _SelectedGone = false;
    _World.Reset();
    _OnChanged.Broadcast();
}

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

auto
    FCkProceduralAnimationDebugger_Model::
    Request_Hold()
    -> void

{ if (NOT _Released) { _History.Hold(); _OnChanged.Broadcast(); } }
auto
    FCkProceduralAnimationDebugger_Model::
    Request_GoLive()
    -> void

{ if (NOT _Released) { _History.GoLive(); _OnChanged.Broadcast(); } }
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
    Select(InEntity, false);
}

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
