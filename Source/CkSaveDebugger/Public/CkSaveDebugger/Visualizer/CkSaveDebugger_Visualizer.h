#pragma once

#include "CkSaveDebugger/Model/CkSaveDebugger_Model.h"

#include "Containers/Array.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

// --------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

/** Editor-viewport visualizer state for the CK Save Debugger — the bridge between the window (which owns the
 *  document and all interaction) and the hidden EdMode (which only draws). Function-slot shape on purpose,
 *  mirroring ck::DebugNav: the window publishes an IMMUTABLE row snapshot and registers a click handler; the
 *  EdMode pulls the snapshot per draw and pushes clicks back through the slot. Nothing here holds a world,
 *  registry or handle — rows are plain data projected from the save file. */
namespace ck::save_debugger_viz
{
    using FRowsPtr = TSharedPtr<const TArray<FCkSaveDebugger_VisualizationRow>>;
    using FOnRowClicked = TFunction<void(uint32)>;

    // ----------------------------------------------------------------------------------------------------------------

    /** Replaces the published snapshot and redraws. The array is shared immutably so a draw pass mid-replace
     *  still reads a coherent set. */
    CKSAVEDEBUGGER_API auto
    Publish_Rows(
        const FRowsPtr& InRows) -> void;

    CKSAVEDEBUGGER_API auto
    Clear_Rows() -> void;

    CKSAVEDEBUGGER_API auto
    Get_Rows() -> FRowsPtr;

    // ----------------------------------------------------------------------------------------------------------------

    CKSAVEDEBUGGER_API auto
    Set_SelectedSavedId(
        uint32 InSavedId) -> void;

    CKSAVEDEBUGGER_API auto
    Get_SelectedSavedId() -> uint32;

    // ----------------------------------------------------------------------------------------------------------------

    CKSAVEDEBUGGER_API auto
    Register_OnRowClicked(
        FOnRowClicked InHandler) -> void;

    CKSAVEDEBUGGER_API auto
    Unregister_OnRowClicked() -> void;

    /** EdMode -> window. No-op when no handler is registered (the window closed under an active mode). */
    CKSAVEDEBUGGER_API auto
    Notify_RowClicked(
        uint32 InSavedId) -> void;

    // ----------------------------------------------------------------------------------------------------------------

    /** Activates/deactivates the hidden visualizer EdMode. Returns whether the viewport now matches the request —
     *  false during PIE, where the level-editor mode stack is not available. */
    CKSAVEDEBUGGER_API auto
    Set_VisualizerEnabled(
        bool InEnabled) -> bool;

    CKSAVEDEBUGGER_API auto
    Get_IsVisualizerEnabled() -> bool;

    // ----------------------------------------------------------------------------------------------------------------

    /** Moves the level-editor cameras to the selected row's diamond. Returns false when nothing placed is
     *  selected or the mode is not active. */
    CKSAVEDEBUGGER_API auto
    Frame_SelectedRow() -> bool;
}

#endif // WITH_EDITOR

// --------------------------------------------------------------------------------------------------------------------
