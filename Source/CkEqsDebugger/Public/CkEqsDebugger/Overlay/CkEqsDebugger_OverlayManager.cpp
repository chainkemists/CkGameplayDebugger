#include "CkEqsDebugger/Overlay/CkEqsDebugger_OverlayManager.h"

#include "CkEqsDebugger/CkEqsDebuggerStyle.h"
#include "CkEqsDebugger/Settings/CkEqsDebuggerSettings.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkPmg/CkPmg_Utils_BasicShapes.h"
#include "CkPmg/CkPmg_Utils_DebugLines.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkGrid/2dGridSystem/Grid/Ck2dGridSystem_Utils.h"

#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    // Looks up the querier's world location from its FFragment_Transform — used for the querier marker + best-line.
    // Returns ZeroVector if the querier doesn't have a transform fragment (rare; the EQS Generate processor fails
    // queries lacking one, so a Complete query always has a valid querier transform).
    auto Get_QuerierLocation(const FCk_Handle& InQuerier) -> FVector
    {
        if (ck::Is_NOT_Valid(InQuerier))
        { return FVector::ZeroVector; }
        if (NOT InQuerier.Has<ck::FFragment_Transform>())
        { return FVector::ZeroVector; }
        return InQuerier.Get<ck::FFragment_Transform>().Get_Transform().GetLocation();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_OverlayManager::
    Update(
        UWorld*                                 InWorld,
        const TArray<FCkEqsDebugger_QueryInfo>* InAllQueries,
        const FCkEqsDebugger_QueryInfo*         InSelectedQuery,
        const UCkEqsDebuggerSettings*           InSettings)
    -> void
{
    // Master toggle off OR no settings OR no world: clear and bail.
    if (NOT IsValid(InWorld) || InSettings == nullptr || NOT InSettings->Show_Overlay)
    {
        Reset();
        return;
    }

    const auto ShowAll = InSettings->Show_AllQueriesAlways;

    // In selection mode: nothing to draw if no query is selected. In all-queries mode: nothing to draw if
    // the source list is empty / null.
    if (NOT ShowAll && InSelectedQuery == nullptr)
    {
        Reset();
        return;
    }
    if (ShowAll && (InAllQueries == nullptr || InAllQueries->IsEmpty()))
    {
        Reset();
        return;
    }

    // Skip rebuild if nothing the user can see has changed since last Update. Cache key differs between modes:
    // in selection mode we key on the selected query's identity + candidate count + best location. In all-queries
    // mode the "selected handle" slot holds a special sentinel and the candidate-count slot holds the SUM of
    // candidate counts across all queries (cheap mode-summary that catches add / remove / candidate-set changes).
    const auto SettingsHash = ComputeSettingsHash(*InSettings);

    auto KeyHandle           = FCk_Handle_EqsQuery{};
    auto KeyCandidateCount   = 0;
    auto KeyHasResults       = false;
    auto KeyBestLocation     = FVector::ZeroVector;

    if (ShowAll)
    {
        // Sentinel: in all-mode, KeyHandle is "Get_InvalidHandle"-style — i.e. always different from any real
        // handle, so the equality check below distinguishes selection-mode caches from all-mode caches and
        // forces a rebuild on mode switch.
        KeyHandle = FCk_Handle_EqsQuery{};
        for (const auto& Q : *InAllQueries)
        {
            KeyCandidateCount += Q.Candidates.Num();
            KeyHasResults     = KeyHasResults || Q.HasResults;
            // Cheap "best-position fingerprint" so a moving querier triggers rebuild — XOR int truncations.
            KeyBestLocation += Q.BestLocation;
        }
    }
    else
    {
        KeyHandle         = InSelectedQuery->QueryHandle;
        KeyCandidateCount = InSelectedQuery->Candidates.Num();
        KeyHasResults     = InSelectedQuery->HasResults;
        KeyBestLocation   = InSelectedQuery->BestLocation;
    }

    const auto CacheValid =
        ck::IsValid(_OverlayParent) &&
        _LastShownAllMode        == ShowAll &&
        _LastShownHandle         == KeyHandle &&
        _LastShownCandidateCount == KeyCandidateCount &&
        _LastShownHasResults     == KeyHasResults &&
        _LastShownBestLocation.Equals(KeyBestLocation) &&
        _LastShownSettingsHash   == SettingsHash;

    if (NOT CacheValid)
    {
        Reset();
        if (NOT EnsureParentEntity(InWorld))
        { return; }

        if (ShowAll)
        {
            for (const auto& Q : *InAllQueries)
            { Rebuild(Q, *InSettings); }
        }
        else
        {
            Rebuild(*InSelectedQuery, *InSettings);
        }

        _LastShownAllMode        = ShowAll;
        _LastShownHandle         = KeyHandle;
        _LastShownCandidateCount = KeyCandidateCount;
        _LastShownHasResults     = KeyHasResults;
        _LastShownBestLocation   = KeyBestLocation;
        _LastShownSettingsHash   = SettingsHash;
    }

    // CkGrid::DebugDraw_Grid uses UE one-shot debug-draw lines (Duration=0 → one frame), so it has to be
    // re-issued every tick to stay visible. Walk the cached grid entities even when the rebuild path was
    // skipped — this is the difference between "grid lines flash for one frame after a state change" and
    // "grid lines persistently visible while the panel is open".
    if (InSettings->Show_GridLines && _GridsPerQuery.Num() > 0)
    {
        auto DrawOptions = FCk_2dGridSystem_DebugDraw_Options{};
        DrawOptions.Set_CellVisualization(ECk_2dGridSystem_DebugDraw_CellVisualization::OBB);
        DrawOptions.Set_ShowCoordinates(false);
        DrawOptions.Set_ShowPivot(false);
        DrawOptions.Set_ShowCellSizeInfo(false);
        DrawOptions.Set_EnabledCellColor(FCkEqsDebuggerStyle::Color_Text_Muted);   // muted so spheres dominate
        DrawOptions.Set_CellThickness(InSettings->GridLineThickness);
        DrawOptions.Set_Duration(0.0f);

        for (const auto& Pair : _GridsPerQuery)
        {
            if (ck::IsValid(Pair.Value))
            { UCk_Utils_2dGridSystem_UE::DebugDraw_Grid(InWorld, Pair.Value, DrawOptions); }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_OverlayManager::
    Reset()
    -> void
{
    if (ck::IsValid(_OverlayParent))
    {
        UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(_OverlayParent);
    }
    _OverlayParent = FCk_Handle{};

    // Grid entities were children of _OverlayParent and cascade-destroyed with it; just drop the lookup map.
    _GridsPerQuery.Reset();

    // Drop the cache so the next Update() with the same selection rebuilds (otherwise we'd skip-rebuild against
    // a destroyed parent and show nothing).
    _LastShownAllMode        = false;
    _LastShownHandle         = FCk_Handle_EqsQuery{};
    _LastShownCandidateCount = -1;
    _LastShownHasResults     = false;
    _LastShownBestLocation   = FVector::ZeroVector;
    _LastShownSettingsHash   = 0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_OverlayManager::
    EnsureParentEntity(
        UWorld* InWorld)
    -> bool
{
    if (ck::IsValid(_OverlayParent))
    { return true; }

    if (NOT IsValid(InWorld))
    { return false; }

    _OverlayParent = UCk_Utils_EntityLifetime_UE::Request_CreateEntity_TransientOwner(InWorld);
    return ck::IsValid(_OverlayParent);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_OverlayManager::
    ComputeSettingsHash(
        const UCkEqsDebuggerSettings& InSettings) const
    -> uint32
{
    // Pack visibility bits + quantised float sizes (cm precision is fine — we only care whether the user changed
    // something the overlay would reflect). Matches the data flowing into Rebuild's branches.
    auto Hash = uint32{0};
    Hash |= (InSettings.Show_Overlay                 ? 1u : 0u) << 0;
    Hash |= (InSettings.Show_AllCandidateSpheres     ? 1u : 0u) << 1;
    Hash |= (InSettings.Show_BestCandidateHighlight  ? 1u : 0u) << 2;
    Hash |= (InSettings.Show_FailedCandidates        ? 1u : 0u) << 3;
    Hash |= (InSettings.Show_QuerierMarker           ? 1u : 0u) << 4;
    Hash |= (InSettings.Show_BestLocationLine        ? 1u : 0u) << 5;
    Hash |= (InSettings.Show_GridLines               ? 1u : 0u) << 6;
    Hash ^= static_cast<uint32>(InSettings.CandidateSphereRadius)       << 8;
    Hash ^= static_cast<uint32>(InSettings.BestPickSphereRadius)        << 12;
    Hash ^= static_cast<uint32>(InSettings.QuerierMarkerRadius)         << 16;
    Hash ^= static_cast<uint32>(InSettings.BestLocationLineThickness)   << 20;
    Hash ^= static_cast<uint32>(InSettings.GridLineThickness * 10.0f)   << 24;  // 0.1cm precision
    return Hash;
}

auto
    FCkEqsDebugger_OverlayManager::
    Rebuild(
        const FCkEqsDebugger_QueryInfo& InQuery,
        const UCkEqsDebuggerSettings&   InSettings)
    -> void
{
    constexpr auto Segments = 12;
    constexpr auto Rings    = 8;

    const auto QuerierLoc = Get_QuerierLocation(InQuery.Querier);

    // ---- Querier marker ------------------------------------------------------------------------------------------
    if (InSettings.Show_QuerierMarker && NOT QuerierLoc.IsZero())
    {
        UCk_Utils_Pmg_BasicShapes::Create_Sphere(
            _OverlayParent,
            FTransform{QuerierLoc},
            InSettings.QuerierMarkerRadius,
            Segments, Rings,
            ECk_Plane_Axis::XY,
            FCkEqsDebuggerStyle::Color_Status_InProgress,
            /*InDrawLines=*/true,
            /*InLineThickness=*/2.0f,
            /*InDuration=*/-1.0f);
    }

    // ---- Per-candidate spheres -----------------------------------------------------------------------------------
    if (InSettings.Show_AllCandidateSpheres)
    {
        for (const auto& Cand : InQuery.Candidates)
        {
            const auto IsBestPick = Cand.IsBestPick && InSettings.Show_BestCandidateHighlight;

            // Skip filter-failed candidates if the user hasn't asked to see them.
            if (NOT Cand.Passed && NOT InSettings.Show_FailedCandidates && NOT IsBestPick)
            { continue; }

            const auto Color = IsBestPick
                ? FCkEqsDebuggerStyle::Color_Score_Best
                : CkEqsDebugger::GetScoreColor(FMath::Clamp(Cand.FinalScore, 0.0f, 1.0f), Cand.Passed);

            const auto Radius = IsBestPick
                ? InSettings.BestPickSphereRadius
                : InSettings.CandidateSphereRadius;

            UCk_Utils_Pmg_BasicShapes::Create_Sphere(
                _OverlayParent,
                FTransform{Cand.Location},
                Radius,
                Segments, Rings,
                ECk_Plane_Axis::XY,
                Color,
                /*InDrawLines=*/true,
                /*InLineThickness=*/IsBestPick ? 3.0f : 1.5f,
                /*InDuration=*/-1.0f);
        }
    }

    // ---- Querier-to-best line ------------------------------------------------------------------------------------
    if (InSettings.Show_BestLocationLine && InQuery.HasResults && NOT QuerierLoc.IsZero())
    {
        ck::pmg::Append_DebugLine_World(
            _OverlayParent,
            QuerierLoc,
            InQuery.BestLocation,
            FCkEqsDebuggerStyle::Color_Score_Best,
            InSettings.BestLocationLineThickness);
    }

    // ---- Grid lattice (SimpleGrid / Grid only) -------------------------------------------------------------------
    // Spawns a CkGrid 2dGridSystem entity matching the EQS lattice. The entity's cell children persist for the
    // lifetime of the overlay parent (cascade-destroyed when we Reset). Update() iterates _GridsPerQuery every
    // tick and calls DebugDraw_Grid — that's where the lines actually appear in-world.
    const auto IsGridGenerator =
        InQuery.GeneratorType == ECk_Eqs_GeneratorType::SimpleGrid ||
        InQuery.GeneratorType == ECk_Eqs_GeneratorType::Grid;

    if (InSettings.Show_GridLines && IsGridGenerator &&
        InQuery.GridSpaceBetween > 0.0f && InQuery.GridHalfSize > 0.0f &&
        NOT QuerierLoc.IsZero())
    {
        // EQS SimpleGrid places candidates at querier + (x, y, 0) for x,y in [-HalfSize, +HalfSize] step Spacing.
        // Number of cells per axis = (2*HalfSize / Spacing) + 1. Square grid.
        const auto NumCellsPerAxis = FMath::TruncToInt(2.0f * InQuery.GridHalfSize / InQuery.GridSpaceBetween) + 1;

        auto GridParams = FCk_2dGridSystem_Spec{
            FIntPoint{NumCellsPerAxis, NumCellsPerAxis},
            FVector2D{InQuery.GridSpaceBetween, InQuery.GridSpaceBetween}};

        // Pivot is local to the entity's Transform (SceneNode child — Ck2dGridSystem_Utils.cpp:42). Leaving
        // identity here; the entity Transform below provides the world position.
        GridParams.Set_Pivot(FTransform::Identity);

        // 2dGridSystem treats cell (0,0) as the bottom-left corner of the grid (cell center = pivot +
        // (cellSize/2, cellSize/2), and the grid extends +X +Y). To CENTER the grid on the querier — which
        // is what EQS SimpleGrid does — the entity Transform has to be at the bottom-left corner of the
        // intended footprint, not at the querier itself. GridSize = NumCells × Spacing = footprint side.
        const auto GridSize_uu  = static_cast<float>(NumCellsPerAxis) * InQuery.GridSpaceBetween;
        const auto HalfGrid_uu  = GridSize_uu * 0.5f;
        const auto GridOriginLoc = QuerierLoc - FVector{HalfGrid_uu, HalfGrid_uu, 0.0f};

        auto GridEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(_OverlayParent);
        if (ck::IsValid(GridEntity))
        {
            auto TransformHandle = UCk_Utils_Transform_UE::Add(
                GridEntity, FTransform{GridOriginLoc}, ECk_Replication::DoesNotReplicate);

            auto GridSystem = UCk_Utils_2dGridSystem_UE::Add(TransformHandle, GridParams);
            if (ck::IsValid(GridSystem))
            { _GridsPerQuery.Add(InQuery.QueryHandle, GridSystem); }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
