#include "CkDebug_Focus.h"

#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkIsmRenderer/Proxy/CkIsmProxy_Utils.h"

#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "EditorViewportClient.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_focus
{
    constexpr auto FallbackHalfExtentCm = 100.0f;
    constexpr auto MinFocusRadiusCm     = 50.0f;

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
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::DebugFocus
{

auto
    Get_CanFocus() -> bool
{
    return DebugViewportView::Get_IsEjected();
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
    auto* LEVC = DebugViewportView::TryGet_LevelEditorViewport();
    if (LEVC == nullptr)
    { return false; }

    if (ck::Is_NOT_Valid(InEntity))
    { return false; }

    const auto Bounds = ck_debug_focus::Resolve_WorldBounds(InEntity);
    if (NOT Bounds.IsSet())
    { return false; }

    // Editor-F feel without the engine's per-version focus API: keep the current
    // view rotation and pull the camera back until the bounds' sphere fits the
    // vertical FOV cone.
    const auto Radius     = FMath::Max(static_cast<float>(Bounds->SphereRadius), ck_debug_focus::MinFocusRadiusCm);
    const auto HalfFOVRad = FMath::DegreesToRadians(LEVC->ViewFOV * 0.5f);
    const auto Distance   = Radius / FMath::Max(FMath::Sin(HalfFOVRad), 0.01f);

    const auto ViewDir = LEVC->GetViewRotation().Vector();
    LEVC->SetViewLocation(Bounds->Origin - ViewDir * Distance);
    LEVC->Invalidate();

    return true;
#else
    return false;
#endif
}

}

// --------------------------------------------------------------------------------------------------------------------
