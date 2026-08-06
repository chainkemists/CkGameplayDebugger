#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Handle/CkHandle.h"

// --------------------------------------------------------------------------------------------------------------------

auto FCkAStarDebugger_ViewModel::Reset_ForWorldChange() -> void
{
    _DataCollector.Reset_ForWorldChange();
    _SelectedEntityHandle = FCk_Handle{};
    _HasSelectedEntity = false;
    _SelectedCellIndex = -1;

    OnSearchListChanged.Broadcast(_DataCollector.Get_AllSearchEntities());
    OnSelectedEntityChanged.Broadcast(_SelectedEntityHandle);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAStarDebugger_ViewModel::
    Tick(
        UWorld* InWorld,
        float InDeltaTime)
    -> void
{
    if (_IsPaused)
    { return; }

    _DataCollector.Collect(InWorld);

    const auto& Entities = _DataCollector.Get_AllSearchEntities();

    OnSearchListChanged.Broadcast(Entities);

    if (_HasSelectedEntity)
    {
        auto* Info = Get_CurrentSearchInfo();

        if (Info)
        {
            OnSearchDataRefreshed.Broadcast(*Info);
        }
    }

    if (NOT _HasSelectedEntity && Entities.Num() > 0)
    {
        Set_SelectedEntityHandle(Entities[0].EntityHandle);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAStarDebugger_ViewModel::
    Set_SelectedEntityHandle(
        FCk_Handle InHandle)
    -> void
{
    _SelectedEntityHandle = InHandle;
    _HasSelectedEntity = ck::IsValid(InHandle);
    _SelectedCellIndex = -1;
    OnSelectedEntityChanged.Broadcast(InHandle);
}

auto
    FCkAStarDebugger_ViewModel::
    Get_SelectedEntityHandle() const
    -> FCk_Handle
{
    return _SelectedEntityHandle;
}

auto
    FCkAStarDebugger_ViewModel::
    Has_SelectedEntity() const
    -> bool
{
    return _HasSelectedEntity;
}

auto
    FCkAStarDebugger_ViewModel::
    Get_AllSearchEntities() const
    -> const TArray<FCkAStarDebugger_SearchInfo>&
{
    return _DataCollector.Get_AllSearchEntities();
}

auto
    FCkAStarDebugger_ViewModel::
    Get_CurrentSearchInfo() const
    -> const FCkAStarDebugger_SearchInfo*
{
    if (NOT _HasSelectedEntity)
    { return nullptr; }

    for (const auto& Info : _DataCollector.Get_AllSearchEntities())
    {
        if (Info.EntityHandle == _SelectedEntityHandle)
        {
            return &Info;
        }
    }

    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAStarDebugger_ViewModel::
    Get_SearchHistory(
        FCk_Handle InHandle) const
    -> const TArray<FCkAStarDebugger_HistoryEntry>*
{
    auto HandleHash = GetTypeHash(InHandle);
    return _DataCollector.Get_SearchHistory().Find(HandleHash);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAStarDebugger_ViewModel::
    Set_Paused(
        bool InPaused)
    -> void
{
    if (_IsPaused == InPaused)
    { return; }

    _IsPaused = InPaused;
    OnPausedChanged.Broadcast(_IsPaused);
}

auto
    FCkAStarDebugger_ViewModel::
    Get_Paused() const
    -> bool
{
    return _IsPaused;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkAStarDebugger_ViewModel::
    Set_SelectedCellIndex(
        int32 InIndex)
    -> void
{
    _SelectedCellIndex = InIndex;
}

auto
    FCkAStarDebugger_ViewModel::
    Get_SelectedCellIndex() const
    -> int32
{
    return _SelectedCellIndex;
}

// --------------------------------------------------------------------------------------------------------------------
