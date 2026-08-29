#include "CkVisualLodDebugger/Markers/CkVisualLodDebugger_MarkerSet.h"

#include "CkCore/Diagnostics/CkDiagnosticVisibility.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkPmg/CkPmg_Utils_BasicShapes.h"
#include "CkPmg/CkPmg_Utils_FlatShapes.h"

#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_visuallod_debugger_markerset
{
    // Sized against a humanoid member: readable from the far band without covering the mesh it annotates.
    constexpr auto k_DiamondExtentCm  = 26.0f;
    constexpr auto k_RingOuterCm      = 20.0f;
    constexpr auto k_RingInnerCm      = 13.0f;
    constexpr auto k_RingSegments     = 20;
    constexpr auto k_DotRadiusCm      = 9.0f;
    constexpr auto k_DotSegments      = 10;
    constexpr auto k_DotRings         = 8;
    constexpr auto k_EmphasisScale    = 1.5f;
    constexpr auto k_OutlineThickness = 1.5f;
    constexpr auto k_MoveToleranceCm  = 0.5f;
    constexpr auto k_PersistDuration  = -1.0f;   // 0 destroys on the next tick — the CkPmg duration footgun

    // Flat PMG shapes are authored in a plane, so a marker left in its authored orientation is edge-on invisible from
    // half the compass. Yaw-only: pitching a marker toward an overhead camera reads as the shape changing size.
    auto Get_BillboardRotation(
        const FVector& InLocation,
        const FVector& InViewerLocation)
        -> FRotator
    {
        auto Planar = InViewerLocation - InLocation;
        Planar.Z = 0.0;

        if (Planar.IsNearlyZero())
        { return FRotator::ZeroRotator; }

        // Create_Diamond / Create_Ring on the XZ plane put the shape's normal on local +Y, so the yaw that aims the
        // NORMAL at the viewer is the direction's yaw less a quarter turn.
        return FRotator{0.0, Planar.Rotation().Yaw - 90.0, 0.0};
    }
}

// --------------------------------------------------------------------------------------------------------------------

FCkVisualLodDebugger_MarkerSet::~FCkVisualLodDebugger_MarkerSet()
{
    Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkVisualLodDebugger_MarkerSet::
    Update_Marker(
        UWorld*             InWorld,
        const FCk_Handle&   InKey,
        EShape              InShape,
        const FVector&      InLocation,
        const FLinearColor& InColor,
        bool                InEmphasized,
        const FVector&      InViewerLocation)
    -> void
{
    if (ck::diagnostic_visibility::Is_HiddenForStreamerMode())
    {
        Reset();
        return;
    }

    using namespace ck_visuallod_debugger_markerset;

    const auto Rotation = InShape == EShape::Dot
        ? FRotator::ZeroRotator
        : Get_BillboardRotation(InLocation, InViewerLocation);

    const auto Wanted = FTransform{Rotation, InLocation, FVector::OneVector};

    if (auto* Existing = _Markers.Find(InKey))
    {
        // Geometry and tint are baked at setup, so a marker that changed representation, selection emphasis or role
        // colour is a different shape — rebuild that one and leave every other marker alone.
        const auto SameAppearance = Existing->ShapeKind == InShape
            && Existing->Emphasized == InEmphasized
            && Existing->Color.Equals(InColor);

        if (SameAppearance && ck::IsValid(Existing->Root))
        {
            if (Existing->LastTransform.Equals(Wanted, k_MoveToleranceCm))
            { return; }

            if (ck::IsValid(Existing->Shape))
            {
                UCk_Utils_Transform_UE::Request_SetTransform(Existing->Shape,
                    FCk_Request_Transform_SetTransform{Wanted}, {});
            }

            Existing->LastTransform = Wanted;
            return;
        }

        DoDestroy(*Existing);
        _Markers.Remove(InKey);
    }

    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    auto Marker = DoCreate(InWorld, InShape, Wanted, InColor, InEmphasized);
    if (ck::Is_NOT_Valid(Marker.Root))
    { return; }

    _Markers.Add(InKey, MoveTemp(Marker));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkVisualLodDebugger_MarkerSet::
    Prune(
        const TSet<FCk_Handle>& InLiveKeys)
    -> void
{
    auto Stale = TArray<FCk_Handle>{};

    for (auto& Kvp : _Markers)
    {
        if (InLiveKeys.Contains(Kvp.Key))
        { continue; }

        Stale.Add(Kvp.Key);
    }

    for (const auto& Key : Stale)
    { Remove(Key); }
}

auto
    FCkVisualLodDebugger_MarkerSet::
    Remove(
        const FCk_Handle& InKey)
    -> void
{
    if (auto* Found = _Markers.Find(InKey))
    {
        DoDestroy(*Found);
        _Markers.Remove(InKey);
    }
}

auto
    FCkVisualLodDebugger_MarkerSet::
    Reset()
    -> void
{
    for (auto& Kvp : _Markers)
    { DoDestroy(Kvp.Value); }

    _Markers.Empty();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkVisualLodDebugger_MarkerSet::
    DoCreate(
        UWorld*             InWorld,
        EShape              InShape,
        const FTransform&   InTransform,
        const FLinearColor& InColor,
        bool                InEmphasized)
    -> FMarker
{
    using namespace ck_visuallod_debugger_markerset;

    auto Marker = FMarker{};
    Marker.Root = UCk_Utils_EntityLifetime_UE::Request_CreateEntity_TransientOwner(InWorld);

    if (ck::Is_NOT_Valid(Marker.Root))
    { return Marker; }

    Marker.ShapeKind     = InShape;
    Marker.Emphasized    = InEmphasized;
    Marker.Color         = InColor;
    Marker.LastTransform = InTransform;

    const auto Scale = InEmphasized ? k_EmphasisScale : 1.0f;

    constexpr auto DrawLines = true;

    auto Shape = FCk_Handle_Pmg_DebugShape{};

    switch (InShape)
    {
        case EShape::Diamond:
        {
            // Create_Diamond's "Thickness" parameter is the SECOND in-plane extent, not a depth — equal values give
            // the square-on-point silhouette the mockup draws over a promoted proxy.
            Shape = UCk_Utils_Pmg_FlatShapes::Create_Diamond(
                Marker.Root,
                InTransform,
                k_DiamondExtentCm * Scale,
                k_DiamondExtentCm * Scale,
                InColor,
                DrawLines,
                k_OutlineThickness,
                ECk_Plane_Axis::XZ,
                k_PersistDuration);
            break;
        }
        case EShape::Ring:
        {
            Shape = UCk_Utils_Pmg_FlatShapes::Create_Ring(
                Marker.Root,
                InTransform,
                k_RingOuterCm * Scale,
                k_RingInnerCm * Scale,
                k_RingSegments,
                InColor,
                DrawLines,
                k_OutlineThickness,
                ECk_Plane_Axis::XZ,
                k_PersistDuration);
            break;
        }
        default:
        {
            // A solid: a far member's marker sits at the rendered instance and must stay legible without billboarding.
            Shape = UCk_Utils_Pmg_BasicShapes::Create_Sphere(
                Marker.Root,
                InTransform,
                k_DotRadiusCm * Scale,
                k_DotSegments,
                k_DotRings,
                ECk_Plane_Axis::XY,
                InColor,
                DrawLines,
                k_OutlineThickness,
                k_PersistDuration);
            break;
        }
    }

    Marker.Shape = UCk_Utils_Transform_UE::Cast(Shape);

    return Marker;
}

auto
    FCkVisualLodDebugger_MarkerSet::
    DoDestroy(
        FMarker& InMarker)
    -> void
{
    if (ck::IsValid(InMarker.Root))
    {
        // The shape was spawned as a child of the root overlay entity, so it cascade-destroys with it.
        UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(InMarker.Root);
    }

    InMarker.Root  = FCk_Handle{};
    InMarker.Shape = FCk_Handle_Transform{};
}

// --------------------------------------------------------------------------------------------------------------------
