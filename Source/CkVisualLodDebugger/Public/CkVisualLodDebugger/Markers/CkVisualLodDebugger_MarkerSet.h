#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CoreMinimal.h"

class UWorld;

// ====================================================================================================================
// Retained PMG world markers, one shape per visible VisualLod member, keyed by the member entity.
//
// Built on CkPmg directly rather than on CkDebug_PmgGizmoSet: that facility draws ONE thing — a six-solid RGB axis
// triad — and exposes no shape choice, while this overlay's whole content is the representation enum drawn as three
// distinguishable silhouettes (diamond = promoted proxy, dot = far GPU member, ring = unrendered). Generalizing the
// Common gizmo set into a shape-typed marker set is a CkDebuggerCommon-owned refactor, not a caller's to make.
//
// The lifecycle contract is the same one, for the same reason: a refresh-GATED Slate tick cannot issue one-frame
// debug draws without blinking whenever the gate caps below frame rate, so every shape is created once with
// InDuration = -1 and only MOVED afterwards. Shape kind and emphasis are baked into the geometry at setup, so a
// change in either destroys and recreates that one marker and leaves the rest untouched.
//
// Flat shapes (diamond, ring) are authored in the XZ plane and yaw-billboarded toward the viewer each update —
// a flat marker left in its authored plane reads edge-on-invisible from half the compass.
// ====================================================================================================================

class FCkVisualLodDebugger_MarkerSet
{
public:
    enum class EShape : uint8
    {
        Diamond,   // promoted proxy
        Dot,       // far GPU member
        Ring,      // managed but unrendered
    };

    ~FCkVisualLodDebugger_MarkerSet();

    // Create-on-first-sight, then move. InViewerLocation yaw-billboards the flat shapes; pass the camera the user is
    // actually looking through.
    auto
    Update_Marker(
        UWorld*             InWorld,
        const FCk_Handle&   InKey,
        EShape              InShape,
        const FVector&      InLocation,
        const FLinearColor& InColor,
        bool                InEmphasized,
        const FVector&      InViewerLocation) -> void;

    // Destroy every marker whose key is absent from InLiveKeys (member vanished, hidden, or domain switched).
    auto Prune(const TSet<FCk_Handle>& InLiveKeys) -> void;

    auto Remove(const FCk_Handle& InKey) -> void;

    // Destroy everything. Safe to call repeatedly, and MUST run before the PIE registry dies.
    auto Reset() -> void;

    auto Get_Num() const -> int32 { return _Markers.Num(); }

private:
    struct FMarker
    {
        FCk_Handle           Root;
        FCk_Handle_Transform Shape;

        EShape       ShapeKind  = EShape::Dot;
        bool         Emphasized = false;
        FLinearColor Color      = FLinearColor::White;
        FTransform   LastTransform = FTransform::Identity;
    };

    auto DoDestroy(FMarker& InMarker) -> void;

    auto
    DoCreate(
        UWorld*             InWorld,
        EShape              InShape,
        const FTransform&   InTransform,
        const FLinearColor& InColor,
        bool                InEmphasized) -> FMarker;

    TMap<FCk_Handle, FMarker> _Markers;
};

// --------------------------------------------------------------------------------------------------------------------
