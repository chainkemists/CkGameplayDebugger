#include "CkDebug_PmgGizmoSet.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkPmg/CkPmg_Utils_DirectionalShapes.h"

#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_pmggizmoset
{
    constexpr auto AxisLengthCm   = 100.0f;
    constexpr auto ShaftWidthCm   = 1.0f;    // AxisLength-scale match of CkPmg's Pivot (ArrowSize 10 * 0.1)
    constexpr auto HeadRatio      = 0.2f;
    constexpr auto HeadWidthMult  = 5.0f;
    constexpr auto AxisAlpha      = 0.85f;
    constexpr auto MoveToleranceCm = 0.01f;

    const FVector AxisDirs[3]   = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };
    const FLinearColor AxisColors[3] =
    {
        FLinearColor{1.0f, 0.0f, 0.0f, AxisAlpha},   // X
        FLinearColor{0.0f, 1.0f, 0.0f, AxisAlpha},   // Y
        FLinearColor{0.0f, 0.0f, 1.0f, AxisAlpha},   // Z
    };

    auto AxisTransform(const FTransform& InTarget, const FVector& InLocalDir) -> FTransform
    {
        const auto WorldDir = InTarget.GetRotation().RotateVector(InLocalDir);
        return FTransform{WorldDir.Rotation(), InTarget.GetLocation(), FVector::OneVector};
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

            for (auto Index = 0; Index < 3; ++Index)
            {
                if (ck::Is_NOT_Valid(Existing->Arrows[Index]))
                { continue; }

                UCk_Utils_Transform_UE::Request_SetTransform(Existing->Arrows[Index],
                    FCk_Request_Transform_SetTransform{AxisTransform(InTransform, AxisDirs[Index])});
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

    for (auto Index = 0; Index < 3; ++Index)
    {
        auto Arrow = UCk_Utils_Pmg_DirectionalShapes::Create_Arrow(
            Gizmo.Root,
            AxisTransform(InTransform, AxisDirs[Index]),
            AxisLengthCm,
            ShaftWidthCm,
            HeadRatio,
            HeadWidthMult,
            AxisColors[Index],
            /*InDrawLines=*/false,
            /*InLineThickness=*/2.0f,
            ECk_Plane_Axis::XY,
            /*InDuration=*/-1.0f);

        Gizmo.Arrows[Index] = UCk_Utils_Transform_UE::Cast(Arrow);
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
        // Child arrows cascade-destroy with the root overlay entity.
        UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InGizmo.Root);
    }
    InGizmo.Root = FCk_Handle{};
    for (auto Index = 0; Index < 3; ++Index)
    {
        InGizmo.Arrows[Index] = FCk_Handle_Transform{};
    }
}

// --------------------------------------------------------------------------------------------------------------------
