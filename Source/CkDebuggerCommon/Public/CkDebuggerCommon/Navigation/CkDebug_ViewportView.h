#pragma once

#include "CoreMinimal.h"

class FEditorViewportClient;
class UWorld;

// ====================================================================================================================
// Shared PIE view resolution — one home for the "which camera is the user
// actually looking through" logic that the viewport picker and the on-screen
// overlay each carried a copy of.
//
// Eject model (verified against this project's setup): F8 does NOT swap the
// LocalPlayer to an ADebugCameraController — the game PC's view freezes at the
// eject point and input moves to the Level Editor viewport. The reliable
// discriminator is `GEditor->GetActiveViewport()`: the PIE game viewport while
// possessed, the level-editor viewport itself while ejected/simulating. Other
// signals (IsInGameView, ActorLock, cursor-in-widget) do not reliably flip,
// particularly in "Play in Current Viewport" mode.
// ====================================================================================================================

namespace ck::DebugViewportView
{
#if WITH_EDITOR
    // The level-editor viewport client currently driving input/rendering, or
    // nullptr when the player is possessed (or no PIE). Non-null == "ejected".
    CKDEBUGGERCOMMON_API auto TryGet_LevelEditorViewport() -> FEditorViewportClient*;
#endif

    // True when an ejected/simulate editor camera is the active view.
    CKDEBUGGERCOMMON_API auto Get_IsEjected() -> bool;

    // Location of the camera the user is looking through — the level-editor
    // camera while ejected, else the first player controller's view point.
    CKDEBUGGERCOMMON_API auto Get_ViewCameraLocation(UWorld* InWorld) -> TOptional<FVector>;

    // Deproject a Slate absolute screen position to a world ray through
    // whichever camera is active (ejected path reads the level-editor
    // viewport's own cursor, so InAbsolutePos is only used while possessed).
    // Returns false when the cursor is outside the viewport or no view exists.
    CKDEBUGGERCOMMON_API auto Deproject(
        UWorld*          InWorld,
        const FVector2D& InAbsolutePos,
        FVector&         OutOrigin,
        FVector&         OutDirection) -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
