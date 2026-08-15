#pragma once

#include "CoreMinimal.h"

#include "Camera/CameraTypes.h"

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
    FrameAll
};

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
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    /** The world every registered target must bind to. Valid for the widget's lifetime. */
    auto Get_PreviewWorld() const -> UWorld*;

    /** Framing source for FrameAll. Held weakly — the window owns the target. */
    auto Set_Target(TSharedPtr<FCk_Jolt_DebugDrawTarget> InTarget) -> void;

    auto ApplyPreset(ECkJoltDebugger_CameraPreset InPreset) -> void;

    auto Get_ProjectionMode() const -> ECameraProjectionMode::Type;
    auto Get_ViewRotation() const -> FRotator;
    auto Get_ViewLocation() const -> FVector;

    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
    TSharedPtr<FPreviewScene> _PreviewScene;
    TSharedPtr<FCkJoltDebugger_3dViewportClient> _ViewportClient;
    TSharedPtr<FSceneViewport> _SceneViewport;
};

// --------------------------------------------------------------------------------------------------------------------
