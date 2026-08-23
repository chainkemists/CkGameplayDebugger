#pragma once

#include "CoreMinimal.h"

class FEditorViewportClient;
class UWorld;
struct FCk_Handle;

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
    /** Converts normalized viewport coordinates to pixels, rejecting points outside the viewport. */
    CKDEBUGGERCOMMON_API auto
    TryGet_ViewportPixel(
        const FVector2D& InNormalizedViewportPosition,
        const FIntPoint& InViewportSize) -> TOptional<FVector2D>;

#if WITH_EDITOR
    // The level-editor viewport client currently driving input/rendering, or
    // nullptr when the player is possessed (or no PIE). Non-null == "ejected".
    CKDEBUGGERCOMMON_API auto TryGet_LevelEditorViewport() -> FEditorViewportClient*;
#endif

    // True when an ejected/simulate editor camera is the active view.
    CKDEBUGGERCOMMON_API auto Get_IsEjected() -> bool;

    // True while POSSESSED and InEntity resolves (via owning actor, recursively) to
    // the locally possessed pawn / its controller / anything attached to the pawn —
    // in-world self-decoration (gizmos, floating labels) would sit inside the
    // first-person camera. Always false while ejected: the pawn is then just
    // another world object.
    CKDEBUGGERCOMMON_API auto Get_IsLocalPlayerSelf(const FCk_Handle& InEntity) -> bool;

    // Location of the camera the user is looking through — the level-editor
    // camera while ejected, else the first player controller's view point.
    CKDEBUGGERCOMMON_API auto Get_ViewCameraLocation(UWorld* InWorld) -> TOptional<FVector>;

    // Deproject a Slate absolute screen position through the active ordinary-editor,
    // ejected-PIE, or game viewport. Returns false outside the viewport.
    CKDEBUGGERCOMMON_API auto Deproject(
        UWorld*          InWorld,
        const FVector2D& InAbsolutePos,
        FVector&         OutOrigin,
        FVector&         OutDirection) -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
