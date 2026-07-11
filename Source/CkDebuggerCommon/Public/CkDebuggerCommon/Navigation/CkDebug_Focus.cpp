#include "CkDebug_Focus.h"

#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkIsmRenderer/Proxy/CkIsmProxy_Utils.h"

#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"   // completes SEditorViewport so TSharedPtr can convert to SWidget
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_focus
{
    constexpr auto FallbackHalfExtentCm = 100.0f;
    constexpr auto MinFocusRadiusCm     = 50.0f;

#if WITH_EDITOR
    // Camera pull-back as a multiple of the exact FOV-fit distance. 1 fills the
    // view with the bounds sphere, which reads uncomfortably close for context.
    static TAutoConsoleVariable<float> CVarFocusDistanceScale{
        TEXT("ck.Debug.Focus.DistanceScale"), 2.0f,
        TEXT("Focus-in-viewport camera distance as a multiple of the exact FOV-fit distance (1 = bounds fill the view).")};
#endif

    auto Resolve_WorldBounds(const FCk_Handle& InEntity) -> TOptional<FBoxSphereBounds>
    {
        const auto HasTransform = UCk_Utils_Transform_UE::Has(InEntity);

        // 1) Own rendered mesh (ISM proxy) at the entity transform.
        if (HasTransform && UCk_Utils_IsmProxy_UE::Has(InEntity))
        {
            auto Mut = InEntity;
            const auto Proxy = UCk_Utils_IsmProxy_UE::Cast(Mut);
            if (ck::IsValid(Proxy))
            {
                const auto LocalBounds = UCk_Utils_IsmProxy_UE::Get_MeshBounds(Proxy, ECk_ScaledUnscaled::Unscaled);
                const auto WorldXform  = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(InEntity);
                return LocalBounds.TransformBy(WorldXform);
            }
        }

        // 2) Direct owning actor's bounds (NOT recursive — focusing a feature
        //    sub-entity must not frame its whole owner complex).
        if (auto* DirectActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor(InEntity);
            ck::IsValid(DirectActor))
        {
            auto Origin = FVector::ZeroVector;
            auto Extent = FVector::ZeroVector;
            DirectActor->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);
            if (NOT Extent.IsNearlyZero())
            { return FBoxSphereBounds{FBox{Origin - Extent, Origin + Extent}}; }
        }

        // 3) Fixed box at the entity's own transform.
        if (HasTransform)
        {
            const auto Location = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(InEntity).GetLocation();
            const auto Extent   = FVector{FallbackHalfExtentCm};
            return FBoxSphereBounds{FBox{Location - Extent, Location + Extent}};
        }

        // 4) No transform at all — fall back to whichever ancestor carries the actor.
        if (auto* ChainActor = UCk_Utils_OwningActor_UE::TryGet_EntityOwningActor_Recursive(InEntity);
            ck::IsValid(ChainActor))
        {
            auto Origin = FVector::ZeroVector;
            auto Extent = FVector::ZeroVector;
            ChainActor->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);
            const auto SafeExtent = Extent.IsNearlyZero() ? FVector{FallbackHalfExtentCm} : Extent;
            return FBoxSphereBounds{FBox{Origin - SafeExtent, Origin + SafeExtent}};
        }

        return {};
    }

#if WITH_EDITOR
    // The animated framing itself — assumes an ejected/editor-driven viewport.
    auto DoFocusNow(const FCk_Handle& InEntity) -> bool
    {
        auto* LEVC = ck::DebugViewportView::TryGet_LevelEditorViewport();
        if (LEVC == nullptr)
        { return false; }

        if (ck::Is_NOT_Valid(InEntity))
        { return false; }

        const auto Bounds = Resolve_WorldBounds(InEntity);
        if (NOT Bounds.IsSet())
        { return false; }

        // Editor-F feel without the engine's per-version focus API: keep the current
        // view rotation and pull the camera back until the bounds' sphere fits the
        // vertical FOV cone, scaled out by the distance cvar.
        const auto Radius     = FMath::Max(static_cast<float>(Bounds->SphereRadius), MinFocusRadiusCm);
        const auto HalfFOVRad = FMath::DegreesToRadians(LEVC->ViewFOV * 0.5f);
        const auto Scale      = FMath::Max(CVarFocusDistanceScale.GetValueOnGameThread(), 0.1f);
        const auto Distance   = (Radius / FMath::Max(FMath::Sin(HalfFOVRad), 0.01f)) * Scale;

        const auto ViewDir = LEVC->GetViewRotation().Vector();

        // Glide instead of teleport — the same transition the editor's own actor
        // focus animates with; a jump cut loses the user's spatial anchor.
        const TSharedPtr<SWidget> ViewportWidget = LEVC->GetEditorViewportWidget();
        LEVC->GetViewTransform().TransitionToLocation(
            Bounds->Origin - ViewDir * Distance,
            ViewportWidget,
            /*bInstant=*/false);
        LEVC->Invalidate();

        return true;
    }

    // Focus queued while possessed: the PIE↔SIE eject request is processed by the
    // editor on a later tick — poll until the viewport is actually ejected, then
    // finish the framing. Expiry bounds the wait; EndPIE clears the queued handle
    // (handle contract: it must not outlive its PIE registry).
    struct FPendingFocus
    {
        FCk_Handle Entity;
        double     ExpireAt = 0.0;
    };
    static TOptional<FPendingFocus> PendingFocus;

    auto ClearPendingFocus() -> void
    {
        PendingFocus.Reset();
    }

    auto QueuePendingFocus(const FCk_Handle& InEntity) -> void
    {
        static auto CleanupRegistered = false;
        if (NOT CleanupRegistered)
        {
            CleanupRegistered = true;
            FEditorDelegates::EndPIE.AddLambda([](bool) { ClearPendingFocus(); });
        }

        constexpr auto TimeoutSecs = 2.0;
        PendingFocus = FPendingFocus{InEntity, FPlatformTime::Seconds() + TimeoutSecs};

        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [](float) -> bool
            {
                if (NOT PendingFocus.IsSet())
                { return false; }   // superseded or cleared (EndPIE)

                if (FPlatformTime::Seconds() > PendingFocus->ExpireAt ||
                    ck::Is_NOT_Valid(PendingFocus->Entity))
                {
                    PendingFocus.Reset();
                    return false;
                }

                if (NOT ck::DebugViewportView::Get_IsEjected())
                { return true; }    // eject toggle not processed yet — keep waiting

                const auto Entity = PendingFocus->Entity;
                PendingFocus.Reset();
                DoFocusNow(Entity);
                return false;
            }));
    }
#endif
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::DebugFocus
{

auto
    Get_CanFocus() -> bool
{
#if WITH_EDITOR
    if (DebugViewportView::Get_IsEjected())
    { return true; }

    // Possessed PIE: Focus_Entity force-ejects first, so it can still act.
    return GEditor != nullptr && GEditor->PlayWorld != nullptr;
#else
    return false;
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    Get_EntityWorldBounds(
        const FCk_Handle& InEntity) -> TOptional<FBoxSphereBounds>
{
    if (ck::Is_NOT_Valid(InEntity))
    { return {}; }

    return ck_debug_focus::Resolve_WorldBounds(InEntity);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    Focus_Entity(
        const FCk_Handle& InEntity) -> bool
{
#if WITH_EDITOR
    if (ck::Is_NOT_Valid(InEntity))
    { return false; }

    if (NOT DebugViewportView::Get_IsEjected())
    {
        // Possessed: queue the framing and request the same PIE↔SIE toggle F8
        // performs — the editor processes it on a later tick, and the queued
        // focus completes once the viewport is actually ejected.
        if (GEditor == nullptr || GEditor->PlayWorld == nullptr)
        { return false; }

        ck_debug_focus::QueuePendingFocus(InEntity);
        GEditor->RequestToggleBetweenPIEandSIE();
        return true;
    }

    return ck_debug_focus::DoFocusNow(InEntity);
#else
    return false;
#endif
}

}

// --------------------------------------------------------------------------------------------------------------------
