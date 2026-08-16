#pragma once

#include "CoreMinimal.h"

#include "Camera/CameraTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "InputCoreTypes.h"

#include "CkJolt/Subsystem/CkJolt_DebugDrawTarget.h"

#include "Fonts/SlateFontInfo.h"
#include "Widgets/SViewport.h"

// --------------------------------------------------------------------------------------------------------------------

class FPreviewScene;
class FSceneViewport;
class FCkJoltDebugger_3dViewportClient;
class UWorld;

// --------------------------------------------------------------------------------------------------------------------

namespace ck_jolt_debugger_viewport
{
    /*
     * The hard cap on how many of the capture's labels one paint may draw (P8-D58). A cap rather than a budget:
     * at the campaign's 100k bar the Labels flag produces a label per dynamic body, and a paint that tried to
     * project all of them would cost more than the draw it is annotating.
     */
    constexpr int32 MaxPaintedLabels = 500;

    /*
     * Which labels a paint keeps when there are more than the cap: the NEAREST ones to the eye, nearest first.
     * Pure, and public, because the paint itself cannot be driven headlessly — this is the half of the labelling
     * that a spec can pin (the projection below it is engine math with no branch of our own in it).
     *
     * Returns INDICES into the source array rather than copies: a label carries an FString, and the caller is
     * about to read them in order anyway.
     */
    auto Select_NearestLabels(
        const TArray<FCk_Jolt_DebugDrawLabel>& InLabels,
        const FVector& InViewLocation,
        int32 InCap) -> TArray<int32>;
}

// --------------------------------------------------------------------------------------------------------------------

/*
 * A label the WINDOW owns rather than the capture: the primary selection is labelled whether or not the Labels
 * draw flag is on, because a selection the user made is not something they should have to turn a flag on to name.
 */
struct FCkJoltDebugger_ViewportLabel
{
    FVector      WorldPosition = FVector::ZeroVector;
    FString      Text;
    FLinearColor Color = FLinearColor::White;
};

// --------------------------------------------------------------------------------------------------------------------

enum class ECkJoltDebugger_CameraPreset : uint8
{
    Perspective,
    Top,
    Bottom,
    Left,
    Right,
    Front,
    Back,
    FrameAll,
    FrameSelection
};

// --------------------------------------------------------------------------------------------------------------------

/*
 * A left-click in the viewport, resolved against the target's live instances. Unset = empty space. The bool
 * is whether the click ADDS to the selection rather than replacing it — the modifier is read in InputKey and
 * carried here, never re-read from Slate state by the handler, because by the time the handler runs the key
 * may already be up.
 */
DECLARE_DELEGATE_TwoParams(FOnCkJoltDebugger_BodyPicked, TOptional<uint64>, bool);

/*
 * The cursor ray, for the drag (P7-D54). The viewport owns the deprojection; the WINDOW owns the world, the
 * subsystem and the drag plane — this widget never touches either.
 */
DECLARE_DELEGATE_TwoParams(FOnCkJoltDebugger_DragRay, FVector, FVector);

/** Ctrl+wheel during a drag: +1 pushes the drag plane away along the view, -1 pulls it in. */
DECLARE_DELEGATE_OneParam(FOnCkJoltDebugger_DragPlaneShift, float);

/*
 * The body under the cursor, throttled (P8-D58). Fires only when the answer CHANGES, and unset means the cursor
 * left every body — which is also what a lost focus reports, so a hover overlay can never latch on.
 */
DECLARE_DELEGATE_OneParam(FOnCkJoltDebugger_BodyHovered, TOptional<uint64>);

// --------------------------------------------------------------------------------------------------------------------
// Debugger-owned inspection surface for the Jolt physics world. The widget draws NOTHING itself: it hosts an
// FPreviewScene whose UWorld is the render target of an FCk_Jolt_DebugDrawTarget, and the facility's instanced
// static meshes live in that world. Everything here is camera and input.
//
// The scene is deliberately unlit and physics-free — the debug-draw materials are unlit, and a preview world that
// simulated would fight the world being inspected.
// --------------------------------------------------------------------------------------------------------------------

class SCkJoltDebugger_3dViewport final : public SViewport
{
public:
    SLATE_BEGIN_ARGS(SCkJoltDebugger_3dViewport) {}
        SLATE_EVENT(FOnCkJoltDebugger_BodyPicked, OnBodyPicked)
        SLATE_EVENT(FSimpleDelegate, OnTogglePause)
        SLATE_EVENT(FSimpleDelegate, OnStepOnce)
        SLATE_EVENT(FSimpleDelegate, OnToggleIsolate)
        SLATE_EVENT(FSimpleDelegate, OnDragArm)
        SLATE_EVENT(FOnCkJoltDebugger_DragRay, OnDragRay)
        SLATE_EVENT(FOnCkJoltDebugger_DragPlaneShift, OnDragPlaneShift)
        SLATE_EVENT(FSimpleDelegate, OnDragRelease)
        SLATE_EVENT(FOnCkJoltDebugger_BodyHovered, OnBodyHovered)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** The world every registered target must bind to. Valid for the widget's lifetime. */
    auto Get_PreviewWorld() const -> UWorld*;

    /** Framing source for FrameAll, and the ray-pick source for a viewport click. Held weakly — the window owns the target. */
    auto Set_Target(TSharedPtr<FCk_Jolt_DebugDrawTarget> InTarget) -> void;

    /** Framing source for FrameSelection. Unset makes the preset (and the F hotkey) inert. */
    auto Set_SelectionBounds(TOptional<FBox> InBounds) -> void;

    /*
     * Whether the Ctrl+LMB drag gesture is live. Computed ONCE in the window from the selected world's net
     * mode and pushed down — a drag on a client moves a body the server corrects on the next replication,
     * so on a client the gesture is inert and the toolbar says why (P7-D54).
     */
    auto Set_DragEnabled(bool InIsEnabled) -> void;

    /*
     * Keep the camera's offset to the selection as it moves: every tick the selection bounds' centre shifts,
     * the eye and the look-at shift with it. Rotation, distance and projection are untouched — this follows,
     * it does not frame.
     */
    auto Set_FollowSelection(bool InIsEnabled) -> void;

    auto ApplyPreset(ECkJoltDebugger_CameraPreset InPreset) -> void;

    auto Get_ProjectionMode() const -> ECameraProjectionMode::Type;
    auto Get_ViewRotation() const -> FRotator;
    auto Get_ViewLocation() const -> FVector;

    /*
     * The pivot every framing, orbit and dolly path maintains: eye + forward * orbit distance. It is a real
     * camera state rather than a derived one, because look-in-place moves it while orbit must leave it alone,
     * and a look-at recomputed from the eye each time could not tell those two gestures apart.
     */
    auto Get_LookAtLocation() const -> FVector;

    /*
     * The viewport client's input entry points, reachable without the client itself — it is a private type in
     * the .cpp, and a camera scheme that cannot be driven from a spec is a camera scheme nothing pins.
     * Both take the same keys FSceneViewport would deliver; the viewport pointer they would carry is only
     * needed by the cursor-position paths (picking), which no-op without one.
     */
    auto Input_Key(const FKey& InKey, EInputEvent InEvent) -> bool;
    auto Input_MouseAxis(const FKey& InAxisKey, float InDelta) -> bool;

    /*
     * The label on the PRIMARY selection. Painted whatever the Labels draw flag says, because it names something
     * the user selected rather than something the capture happened to emit.
     */
    auto Set_PrimaryLabel(TOptional<FCkJoltDebugger_ViewportLabel> InLabel) -> void;

    /** The hovered body's name, shown as this viewport's tooltip. Empty text shows none. */
    auto Set_HoverLabel(FText InText) -> void;

    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto OnPaint(
        const FPaintArgs&        InArgs,
        const FGeometry&         InAllottedGeometry,
        const FSlateRect&        InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32                    InLayerId,
        const FWidgetStyle&      InWidgetStyle,
        bool                     InParentEnabled) const -> int32 override;

private:
    auto Get_HoverTooltip() const -> FText;

    TSharedPtr<FPreviewScene> _PreviewScene;
    TSharedPtr<FCkJoltDebugger_3dViewportClient> _ViewportClient;
    TSharedPtr<FSceneViewport> _SceneViewport;

    // Held here as well as on the client: OnPaint reads the capture's labels, and the client is a private type
    // this widget cannot hand a paint pass through.
    TWeakPtr<FCk_Jolt_DebugDrawTarget> _Target;

    TOptional<FCkJoltDebugger_ViewportLabel> _PrimaryLabel;

    FText _HoverText;

    // Built ONCE. OnPaint must never allocate a font or a brush (CkDebuggerCommon/CLAUDE.md § OnPaint), and a
    // font built per paint is exactly that.
    FSlateFontInfo _LabelFont;

    // Whether the 500-label cap has already been reported. The cap is a hard truncation, and a truncation the
    // user is never told about reads as the facility losing labels — but once per session is enough.
    mutable bool _LabelCapLogged = false;
};

// --------------------------------------------------------------------------------------------------------------------
