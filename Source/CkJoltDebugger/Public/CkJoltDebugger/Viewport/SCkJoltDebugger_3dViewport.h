#pragma once

#include "CoreMinimal.h"

#include "Camera/CameraTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "InputCoreTypes.h"

#include "Widgets/SViewport.h"

// --------------------------------------------------------------------------------------------------------------------

class FPreviewScene;
class FSceneViewport;
class FCkJoltDebugger_3dViewportClient;
class FCk_Jolt_DebugDrawTarget;
class UWorld;

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

/** A plain left-click in the viewport, resolved against the target's live instances. Unset = empty space. */
DECLARE_DELEGATE_OneParam(FOnCkJoltDebugger_BodyPicked, TOptional<uint64>);

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
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** The world every registered target must bind to. Valid for the widget's lifetime. */
    auto Get_PreviewWorld() const -> UWorld*;

    /** Framing source for FrameAll, and the ray-pick source for a viewport click. Held weakly — the window owns the target. */
    auto Set_Target(TSharedPtr<FCk_Jolt_DebugDrawTarget> InTarget) -> void;

    /** Framing source for FrameSelection. Unset makes the preset (and the F hotkey) inert. */
    auto Set_SelectionBounds(TOptional<FBox> InBounds) -> void;

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

    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
    TSharedPtr<FPreviewScene> _PreviewScene;
    TSharedPtr<FCkJoltDebugger_3dViewportClient> _ViewportClient;
    TSharedPtr<FSceneViewport> _SceneViewport;
};

// --------------------------------------------------------------------------------------------------------------------
