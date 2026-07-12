#include "CkDebug_ViewportView.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck::DebugViewportView
{

#if WITH_EDITOR
auto
    TryGet_LevelEditorViewport() -> FEditorViewportClient*
{
    if (GEditor == nullptr || GEditor->PlayWorld == nullptr)
    { return nullptr; }

    auto* LEVC = GCurrentLevelEditingViewportClient;
    if (LEVC == nullptr || LEVC->Viewport == nullptr)
    { return nullptr; }

    if (GEditor->GetActiveViewport() != LEVC->Viewport)
    { return nullptr; }

    return LEVC;
}
#endif

// --------------------------------------------------------------------------------------------------------------------

auto
    Get_IsEjected() -> bool
{
#if WITH_EDITOR
    return TryGet_LevelEditorViewport() != nullptr;
#else
    return false;
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    Get_IsLocalPlayerSelf(
        const FCk_Handle& InEntity) -> bool
{
    if (ck::Is_NOT_Valid(InEntity))
    { return false; }

    if (Get_IsEjected())
    { return false; }

    auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InEntity);
    if (ck::Is_NOT_Valid(World))
    { return false; }

    auto* PC = World->GetFirstPlayerController();
    if (ck::Is_NOT_Valid(PC))
    { return false; }

    auto* LocalPawn = PC->GetPawn().Get();
    if (ck::Is_NOT_Valid(LocalPawn))
    { return false; }

    auto* OwningActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor_Recursive(InEntity);
    if (ck::Is_NOT_Valid(OwningActor))
    { return false; }

    if (OwningActor == LocalPawn || OwningActor == PC)
    { return true; }

    return OwningActor->IsAttachedTo(LocalPawn);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    Get_ViewCameraLocation(
        UWorld* InWorld) -> TOptional<FVector>
{
    if (ck::Is_NOT_Valid(InWorld))
    { return {}; }

#if WITH_EDITOR
    if (auto* LEVC = TryGet_LevelEditorViewport())
    { return LEVC->GetViewLocation(); }
#endif

    auto* PC = InWorld->GetFirstPlayerController();
    if (ck::Is_NOT_Valid(PC))
    { return {}; }

    auto CameraLocation = FVector::ZeroVector;
    auto CameraRotation = FRotator::ZeroRotator;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    return CameraLocation;
}

// --------------------------------------------------------------------------------------------------------------------

namespace
{
#if WITH_EDITOR
    auto Deproject_LevelEditorViewport(
        FEditorViewportClient* InVC,
        FVector&               OutOrigin,
        FVector&               OutDirection) -> bool
    {
        auto* Viewport = InVC->Viewport;
        if (Viewport == nullptr)
        { return false; }

        const auto Size   = Viewport->GetSizeXY();
        const auto MouseX = Viewport->GetMouseX();
        const auto MouseY = Viewport->GetMouseY();
        if (Size.X <= 0 || Size.Y <= 0)
        { return false; }
        if (MouseX < 0 || MouseY < 0 || MouseX >= Size.X || MouseY >= Size.Y)
        { return false; }

        if (InVC->IsOrtho())
        { return false; }

        // Build view+projection from the viewport client's CURRENT camera state, rather than
        // calling FEditorViewportClient::CalcSceneView() — that call returns stale matrices on
        // repeat invocations outside the viewport's own Draw pass.
        const auto ViewLocation = InVC->GetViewLocation();
        const auto ViewRotation = InVC->GetViewRotation();

        const auto ViewRotationMatrix = FInverseRotationMatrix(ViewRotation) * FMatrix(
            FPlane(0.0, 0.0, 1.0, 0.0),
            FPlane(1.0, 0.0, 0.0, 0.0),
            FPlane(0.0, 1.0, 0.0, 0.0),
            FPlane(0.0, 0.0, 0.0, 1.0));
        const auto ViewMatrix = FTranslationMatrix(-ViewLocation) * ViewRotationMatrix;

        const auto AspectRatio = static_cast<float>(Size.X) / static_cast<float>(Size.Y);
        const auto HalfFOVRad  = FMath::DegreesToRadians(InVC->ViewFOV * 0.5f);

        const auto ProjectionMatrix = FReversedZPerspectiveMatrix(
            HalfFOVRad, AspectRatio, 1.0f, GNearClippingPlane);

        const auto InvViewProj = (ViewMatrix * ProjectionMatrix).InverseFast();
        const auto ViewRect    = FIntRect(0, 0, Size.X, Size.Y);

        FSceneView::DeprojectScreenToWorld(
            FVector2D(MouseX, MouseY), ViewRect, InvViewProj, OutOrigin, OutDirection);
        return true;
    }
#endif
}

auto
    Deproject(
        UWorld*          InWorld,
        const FVector2D& InAbsolutePos,
        FVector&         OutOrigin,
        FVector&         OutDirection) -> bool
{
    if (ck::Is_NOT_Valid(InWorld))
    { return false; }

#if WITH_EDITOR
    // During ejected PIE, the game PC's view freezes — deproject against the active Level Editor
    // viewport's live camera instead.
    if (auto* LEVC = TryGet_LevelEditorViewport())
    {
        return Deproject_LevelEditorViewport(LEVC, OutOrigin, OutDirection);
    }
#endif

    auto* GVC = InWorld->GetGameViewport();
    if (ck::Is_NOT_Valid(GVC))
    { return false; }

    auto* LocalPlayer = InWorld->GetFirstLocalPlayerFromController();
    if (ck::Is_NOT_Valid(LocalPlayer))
    { return false; }

    const auto ViewportWidget = GVC->GetGameViewportWidget();
    if (NOT ViewportWidget.IsValid())
    { return false; }

    const auto& Geom        = ViewportWidget->GetCachedGeometry();
    const auto  LocalSize   = Geom.GetLocalSize();
    const auto  LocalPixels = Geom.AbsoluteToLocal(InAbsolutePos);

    if (LocalPixels.X < 0.0f || LocalPixels.Y < 0.0f ||
        LocalPixels.X >= LocalSize.X || LocalPixels.Y >= LocalSize.Y)
    { return false; }

    auto* Viewport = GVC->Viewport;
    if (Viewport == nullptr)
    { return false; }

    constexpr auto RealtimeUpdate = true;
    auto ViewFamilyArgs = FSceneViewFamily::ConstructionValues(
        Viewport, InWorld->Scene, GVC->EngineShowFlags)
        .SetRealtimeUpdate(RealtimeUpdate);
    auto ViewFamily = FSceneViewFamilyContext{MoveTemp(ViewFamilyArgs)};

    auto ViewLocation = FVector::ZeroVector;
    auto ViewRotation = FRotator::ZeroRotator;
    auto* SceneView   = LocalPlayer->CalcSceneView(
        &ViewFamily, ViewLocation, ViewRotation, Viewport);

    if (SceneView == nullptr)
    { return false; }

    // CalcSceneView expects viewport pixels; Slate's local coordinates match when the viewport
    // widget has a 1:1 DPI scale in the cached geometry (the division is baked into AbsoluteToLocal).
    SceneView->DeprojectFVector2D(LocalPixels, OutOrigin, OutDirection);
    return true;
}

}

// --------------------------------------------------------------------------------------------------------------------
