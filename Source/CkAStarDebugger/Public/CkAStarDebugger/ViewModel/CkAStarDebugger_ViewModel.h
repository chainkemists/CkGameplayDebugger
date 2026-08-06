#pragma once

#include "CkAStarDebugger/Data/CkAStarDebugger_Types.h"
#include "CkAStarDebugger/Data/CkAStarDebugger_DataCollector.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Delegates
// --------------------------------------------------------------------------------------------------------------------

DECLARE_MULTICAST_DELEGATE_OneParam(FCkAStarDebugger_OnSearchListChanged, const TArray<FCkAStarDebugger_SearchInfo>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkAStarDebugger_OnSearchDataRefreshed, const FCkAStarDebugger_SearchInfo&);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkAStarDebugger_OnSelectedEntityChanged, FCk_Handle);
DECLARE_MULTICAST_DELEGATE_OneParam(FCkAStarDebugger_OnPausedChanged, bool);

// --------------------------------------------------------------------------------------------------------------------
// ViewModel — shared state model for all A* debugger sub-widgets.
// --------------------------------------------------------------------------------------------------------------------

class FCkAStarDebugger_ViewModel
{
public:
    // -----------------------------------------------------------------------------------------------------------------
    // Delegates
    // -----------------------------------------------------------------------------------------------------------------

    FCkAStarDebugger_OnSearchListChanged OnSearchListChanged;
    FCkAStarDebugger_OnSearchDataRefreshed OnSearchDataRefreshed;
    FCkAStarDebugger_OnSelectedEntityChanged OnSelectedEntityChanged;
    FCkAStarDebugger_OnPausedChanged OnPausedChanged;

    // -----------------------------------------------------------------------------------------------------------------
    // Tick — collects data and broadcasts refresh
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Tick(
        UWorld* InWorld,
        float InDeltaTime) -> void;

    auto Reset_ForWorldChange() -> void;

    // -----------------------------------------------------------------------------------------------------------------
    // Entity selection
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Set_SelectedEntityHandle(
        FCk_Handle InHandle) -> void;

    auto
    Get_SelectedEntityHandle() const -> FCk_Handle;

    auto
    Has_SelectedEntity() const -> bool;

    auto
    Get_AllSearchEntities() const -> const TArray<FCkAStarDebugger_SearchInfo>&;

    auto
    Get_CurrentSearchInfo() const -> const FCkAStarDebugger_SearchInfo*;

    // -----------------------------------------------------------------------------------------------------------------
    // Search history
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Get_SearchHistory(
        FCk_Handle InHandle) const -> const TArray<FCkAStarDebugger_HistoryEntry>*;

    // -----------------------------------------------------------------------------------------------------------------
    // Pause state
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Set_Paused(
        bool InPaused) -> void;

    auto
    Get_Paused() const -> bool;

    // -----------------------------------------------------------------------------------------------------------------
    // Selected cell (for grid view → stats panel communication)
    // -----------------------------------------------------------------------------------------------------------------

    auto
    Set_SelectedCellIndex(
        int32 InIndex) -> void;

    auto
    Get_SelectedCellIndex() const -> int32;

private:
    FCkAStarDebugger_DataCollector _DataCollector;
    FCk_Handle _SelectedEntityHandle;
    bool _HasSelectedEntity = false;
    bool _IsPaused = false;
    int32 _SelectedCellIndex = -1;
};

// --------------------------------------------------------------------------------------------------------------------
