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
// (UCk_DebugOverlay_Subsystem) and the shared viewport picker
// (FCkDebug_ViewportPicker):
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

    // Opt-in session switch for expanding the per-gather FullDepthRoots below.
    // Consumers own those roots and must pass them on every Gather call; markers
    // never retain handles across frames.
    CKDEBUGGERCOMMON_API auto Get_FocusFullDepth() -> bool;

    // Console-variable name of the focus full-depth switch (for UI bindings).
    CKDEBUGGERCOMMON_API auto Get_FocusFullDepthCVarName() -> const TCHAR*;
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

        // Optional TARGET mode (feature-specialized pickers, e.g. "only GOAP
        // agents"): when bound, the snapshot becomes every entity accepted by
        // this predicate PLUS its lifetime-owner chain up to the representative
        // root — the top-most ancestor that is neither the registry transient
        // nor an ActorRelay entity (the raw relay check, independent of the
        // depth-transparency user setting). The matched entity may be a
        // transform-less sub-entity; its transform-bearing ancestors still
        // surface, which is how "GOAP lives on a sub-entity but the NPC root is
        // what you pick" works. MaxDepth and FullDepthRoots do NOT apply in
        // this mode (the predicate is the gate); CullOrigin/CullRadius and
        // Filter still do. Evaluated over ALL live entities, so keep it cheap
        // (fragment Has checks).
        TFunction<bool(const FCk_Handle&)> TargetMatch;

        // Optional distance cull: entities farther than CullRadius from
        // CullOrigin are excluded entirely (markers, links, and the entry list).
        TOptional<FVector> CullOrigin;
        float              CullRadius = 0.0f;

        // Optional call-scoped roots whose lifetime subtrees bypass MaxDepth when
        // ck.Debug.EntityMarkers.FocusFullDepth is enabled. Each root and every
        // descendant is included; sibling branches still obey MaxDepth. Gather
        // validates and uses these synchronously only -- it stores no handles.
        TArray<FCk_Handle> FullDepthRoots;
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
    // Target-mode gather (FGatherParams::TargetMatch bound) — matches + their
    // owner chains up to the representative root. See FGatherParams::TargetMatch.
    auto DoGather_Targeted(
        const FCk_Handle&    InTransientEntity,
        const FGatherParams& InParams) -> int32;

    TArray<FEntry> _Entries;
    TSet<uint32>   _EntryNums;

    TStrongObjectPtr<UTexture2D> _MarkerTexture;
    TStrongObjectPtr<UTexture2D> _MarkerHoverTexture;
};

// --------------------------------------------------------------------------------------------------------------------
