#pragma once

#include "CoreMinimal.h"

#include "CkEcs/Handle/CkHandle.h"

#include "UObject/StrongObjectPtr.h"

// --------------------------------------------------------------------------------------------------------------------

class UCanvas;
class UTexture2D;
class UWorld;

// ====================================================================================================================
// Shared in-world entity marker preview — the "layered connected diamonds".
//
// One implementation of the marker display used by both the On-Screen Overlay
// (UCk_DebugOverlay_Subsystem) and the ECS Debugger's viewport picker
// (FCkDebuggerModel_ViewportPicker):
//
//   - Gather:      snapshot every transform-bearing entity (pending-kill and
//                  PMG debug-shape entities excluded), compute its hierarchy
//                  depth (lifetime-owner hops to the registry transient) and
//                  gate by the shared `ck.Debug.EntityMarkers.MaxDepth` cvar.
//   - DrawLinks:   one-frame dashed parent→child lines between gathered
//                  entities (re-issued per tick so they track both endpoints).
//   - DrawMarkers: screen-space diamond billboards tinted by depth, with one
//                  optional emphasized entity (overlay focus / picker hover)
//                  drawn on top at full alpha with the hover texture.
//
// Consumers own an instance, call Gather + DrawLinks from their per-frame
// tick, and DrawMarkers from their UDebugDrawService callback. The gathered
// entries are exposed so consumers can reuse the exact visible set for their
// own logic (focus candidates, ray hit-testing) — what you see is what you
// can pick/focus.
// ====================================================================================================================

namespace ck::DebugMarkers
{
    // Marker tint per hierarchy depth: 0 = blue, then green / yellow / orange /
    // magenta (4+). Alpha is always 1 — callers apply their own alpha.
    CKDEBUGGERCOMMON_API auto Get_DepthTint(int32 InDepth) -> FLinearColor;

    // Shared hierarchy-depth gate (`ck.Debug.EntityMarkers.MaxDepth`):
    // -1 = unlimited, 0 = top-level entities only, N = up to N levels deep.
    CKDEBUGGERCOMMON_API auto Get_MaxDepth() -> int32;

    // Console-variable name of the shared depth gate (for UI bindings).
    CKDEBUGGERCOMMON_API auto Get_MaxDepthCVarName() -> const TCHAR*;
}

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API FCkDebug_EntityMarkers
{
public:
    struct FEntry
    {
        FCk_Handle Entity;
        FVector    WorldPos       = FVector::ZeroVector;
        int32      Depth          = 0;
        uint32     EntityNum      = 0;
        uint32     OwnerEntityNum = MAX_uint32;
    };

    struct FGatherParams
    {
        // Optional per-entity filter (return false to exclude). Runs after the
        // built-in exclusions (pending-kill, PMG debug shapes, depth gate).
        TFunction<bool(const FCk_Handle&)> Filter;

        // Optional distance cull: entities farther than CullRadius from
        // CullOrigin are excluded entirely (markers, links, and the entry list).
        TOptional<FVector> CullOrigin;
        float              CullRadius = 0.0f;
    };

    struct FDrawParams
    {
        float TileSizePx   = 28.0f;
        float DefaultAlpha = 0.45f;

        // Entity number drawn on top with the hover texture at full alpha
        // (overlay focus / picker hover). MAX_uint32 = none.
        uint32  EmphasizedEntityNum = MAX_uint32;
        float   EmphasizedScale     = 1.25f;
        FString EmphasizedLabel;

        // Entity numbers to skip when drawing markers (still gathered/linked) —
        // e.g. the locally possessed pawn, whose marker would sit at screen center.
        const TSet<uint32>* SuppressedEntityNums = nullptr;
    };

public:
    // Rebuild the entry snapshot for InWorld. Returns the number of entries.
    auto Gather(UWorld* InWorld, const FGatherParams& InParams) -> int32;

    // Dashed parent→child lines between gathered entries (one-frame lifetime;
    // call every tick). Line color = the CHILD's depth tint.
    auto DrawLinks(UWorld* InWorld) const -> void;

    // Depth-tinted diamond billboards for the gathered entries.
    auto DrawMarkers(UCanvas* InCanvas, const FDrawParams& InParams) -> void;

    // Load the marker textures (and finish their async compilation) up front.
    // DrawMarkers also does this lazily; call at activation to front-load the cost.
    auto EnsureTextures() -> void;

    auto Reset() -> void;

    auto Get_Entries() const -> const TArray<FEntry>& { return _Entries; }

    // True if InEntity is in the current snapshot (i.e. currently previewed).
    auto Contains(const FCk_Handle& InEntity) const -> bool;

private:
    TArray<FEntry> _Entries;
    TSet<uint32>   _EntryNums;

    TStrongObjectPtr<UTexture2D> _MarkerTexture;
    TStrongObjectPtr<UTexture2D> _MarkerHoverTexture;
};

// --------------------------------------------------------------------------------------------------------------------
