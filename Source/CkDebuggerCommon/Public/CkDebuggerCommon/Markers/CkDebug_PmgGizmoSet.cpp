#include "CkDebug_PmgGizmoSet.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkPmg/CkPmg_Utils_BasicShapes.h"

#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_pmggizmoset
{
    // Shaft + tip sum to the same 100 uu the old DrawDebugTransformGizmo spanned.
    constexpr auto ShaftLengthCm    = 70.0f;
    constexpr auto ShaftRadiusCm    = 2.0f;
    constexpr auto TipLengthCm      = 30.0f;
    constexpr auto TipRadiusCm      = 7.0f;
    constexpr auto ShaftSegments    = 10;
    constexpr auto TipSegments      = 12;
    constexpr auto AxisAlpha        = 0.85f;   // fill only — baked outlines force alpha 1
    constexpr auto OutlineThickness = 1.0f;
    constexpr auto MoveToleranceCm  = 0.01f;

    const FVector AxisDirs[3]   = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };
    const FLinearColor AxisColors[3] =
    {
        FLinearColor{1.0f, 0.0f, 0.0f, AxisAlpha},   // X
        FLinearColor{0.0f, 1.0f, 0.0f, AxisAlpha},   // Y
        FLinearColor{0.0f, 0.0f, 1.0f, AxisAlpha},   // Z
    };

    // Both solids build along local +Z (cone Orientation=Up, cylinder height axis):
    // aim +Z down the world-space axis, then offset along it — the cylinder is
    // centered on its origin, the cone's base sits at its origin.
    auto PartTransform(const FTransform& InTarget, int32 InAxisIndex, bool InIsTip) -> FTransform
    {
        const auto WorldDir = InTarget.GetRotation().RotateVector(AxisDirs[InAxisIndex]);
        const auto Rotation = FRotationMatrix::MakeFromZ(WorldDir).ToQuat();
        const auto Offset   = InIsTip ? ShaftLengthCm : ShaftLengthCm * 0.5f;
        return FTransform{Rotation, InTarget.GetLocation() + WorldDir * Offset, FVector::OneVector};
    }
}

// --------------------------------------------------------------------------------------------------------------------

FCkDebug_PmgGizmoSet::~FCkDebug_PmgGizmoSet()
{
    Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_PmgGizmoSet::
    UpdateGizmo(
        UWorld* InWorld,
        const FCk_Handle& InKey,
        const FTransform& InTransform) -> void
{
    using namespace ck_debug_pmggizmoset;

    if (auto* Existing = _Gizmos.Find(InKey))
    {
        if (ck::Is_NOT_Valid(Existing->Root))
        {
            // World reset out from under us (e.g. quick PIE restart) — rebuild below.
            _Gizmos.Remove(InKey);
        }
        else
        {
            if (Existing->LastTransform.Equals(InTransform, MoveToleranceCm))
            { return; }

            for (auto Index = 0; Index < 6; ++Index)
            {
                if (ck::Is_NOT_Valid(Existing->Parts[Index]))
                { continue; }

                UCk_Utils_Transform_UE::Request_SetTransform(Existing->Parts[Index],
                    FCk_Request_Transform_SetTransform{PartTransform(InTransform, Index % 3, Index >= 3)}, {});
            }
            Existing->LastTransform = InTransform;
            return;
        }
    }

    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    auto Gizmo = FGizmo{};
    Gizmo.Root = UCk_Utils_EntityLifetime_UE::Request_CreateEntity_TransientOwner(InWorld);
    if (ck::Is_NOT_Valid(Gizmo.Root))
    { return; }

    for (auto Axis = 0; Axis < 3; ++Axis)
    {
        auto Shaft = UCk_Utils_Pmg_BasicShapes::Create_Cylinder(
            Gizmo.Root,
            PartTransform(InTransform, Axis, /*InIsTip=*/false),
            ShaftRadiusCm,
            ShaftLengthCm,
            ShaftSegments,
            ECk_Plane_Axis::XY,
            AxisColors[Axis],
            /*InDrawLines=*/true,
            OutlineThickness,
            /*InDuration=*/-1.0f);
        Gizmo.Parts[Axis] = UCk_Utils_Transform_UE::Cast(Shaft);

        auto Tip = UCk_Utils_Pmg_BasicShapes::Create_Cone(
            Gizmo.Root,
            PartTransform(InTransform, Axis, /*InIsTip=*/true),
            TipRadiusCm,
            TipLengthCm,
            TipSegments,
            ECk_Plane_Axis::XY,
            AxisColors[Axis],
            /*InDrawLines=*/true,
            OutlineThickness,
            /*InDuration=*/-1.0f,
            ECk_Pmg_ConeOrientation::Up);
        Gizmo.Parts[3 + Axis] = UCk_Utils_Transform_UE::Cast(Tip);
    }

    Gizmo.LastTransform = InTransform;
    _Gizmos.Add(InKey, MoveTemp(Gizmo));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_PmgGizmoSet::
    Remove(
        const FCk_Handle& InKey) -> void
{
    if (auto* Found = _Gizmos.Find(InKey))
    {
        DoDestroy(*Found);
        _Gizmos.Remove(InKey);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_PmgGizmoSet::
    Reset() -> void
{
    for (auto& Kvp : _Gizmos)
    {
        DoDestroy(Kvp.Value);
    }
    _Gizmos.Empty();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_PmgGizmoSet::
    DoDestroy(
        FGizmo& InGizmo) -> void
{
    if (ck::IsValid(InGizmo.Root))
    {
        // Child solids cascade-destroy with the root overlay entity.
        UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InGizmo.Root);
    }
    InGizmo.Root = FCk_Handle{};
    for (auto Index = 0; Index < 6; ++Index)
    {
        InGizmo.Parts[Index] = FCk_Handle_Transform{};
    }
}

// --------------------------------------------------------------------------------------------------------------------
