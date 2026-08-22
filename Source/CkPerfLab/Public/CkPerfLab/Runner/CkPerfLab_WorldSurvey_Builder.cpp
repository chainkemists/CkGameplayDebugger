#include "CkPerfLab_WorldSurvey_Builder.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

#include <Components/LightComponent.h>
#include <Components/PrimitiveComponent.h>
#include <Components/StaticMeshComponent.h>

#include <Engine/StaticMesh.h>
#include <Engine/World.h>
#include <EngineUtils.h>

#include <GameFramework/Actor.h>
#include <NavigationSystem.h>
#include <Particles/ParticleSystemComponent.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_survey
{
    // Navmesh candidates come from projecting a fixed lattice onto the navigation data, never from
    // random sampling. Two sessions of an unchanged map must produce the same position ids, because
    // compare matches BY position id — a sampler that lands differently each run silently turns
    // "this position regressed" into "these are different positions", which is worse than no compare
    // at all. A lattice is also better coverage than random sampling at equal cost: random points
    // clump and leave holes.
    constexpr auto k_NavmeshLatticeCellsPerAxis = 24;

    // Vertical reach of the projection query. Generous on Z so a lattice point hovering over a floor
    // or buried under a ramp still finds the surface beneath it, tight on XY so a point does not
    // snap sideways into a different room and misrepresent where it was asked about.
    constexpr auto k_NavmeshProjectExtentXYCm = 150.0f;
    constexpr auto k_NavmeshProjectExtentZCm  = 2000.0f;

    // Triangle counts dominate the ranking if taken raw — a single dense mesh would outrank a room
    // full of lights and particles. Scaling them down keeps the axes comparable without pretending
    // the weight means anything in absolute terms.
    constexpr auto k_TrianglesPerWeightUnit = 5000.0f;

    constexpr auto k_WeightPerLight         = 4.0f;
    constexpr auto k_WeightPerEffect        = 6.0f;
    constexpr auto k_WeightPerTickingActor  = 1.0f;
    constexpr auto k_WeightPerShadowCaster  = 2.0f;

    /**
     * Walks a fixed lattice over the level's XY footprint and keeps every cell centre that projects
     * onto navigation data. The result depends only on the world bounds and the navmesh itself, so
     * an unchanged map yields an identical point set on every run — which is the whole contract that
     * makes position ids, and therefore session compare, mean anything.
     *
     * Projected points are deduplicated: neighbouring lattice cells over a flat floor frequently
     * snap to the same navmesh vertex, and a duplicate would bias the planner's ranking toward that
     * spot for no reason other than lattice spacing.
     */
    auto
        Project_LatticeOntoNavmesh(
            const UNavigationSystemV1& InNavSystem,
            const FBox& InBounds)
        -> TArray<FVector>
    {
        const auto Min    = InBounds.Min;
        const auto Size   = InBounds.GetSize();
        const auto Centre = InBounds.GetCenter();
        const auto Extent = FVector
        {
            static_cast<double>(k_NavmeshProjectExtentXYCm),
            static_cast<double>(k_NavmeshProjectExtentXYCm),
            static_cast<double>(k_NavmeshProjectExtentZCm)
        };

        const auto StepX = Size.X / static_cast<double>(k_NavmeshLatticeCellsPerAxis);
        const auto StepY = Size.Y / static_cast<double>(k_NavmeshLatticeCellsPerAxis);

        auto Points = TArray<FVector>{};
        auto Seen   = TSet<FString>{};

        for (auto CellX = 0; CellX < k_NavmeshLatticeCellsPerAxis; ++CellX)
        {
            for (auto CellY = 0; CellY < k_NavmeshLatticeCellsPerAxis; ++CellY)
            {
                const auto Query = FVector
                {
                    Min.X + (StepX * (static_cast<double>(CellX) + 0.5)),
                    Min.Y + (StepY * (static_cast<double>(CellY) + 0.5)),
                    Centre.Z
                };

                auto Projected = FNavLocation{};

                if (NOT InNavSystem.ProjectPointToNavigation(Query, Projected, Extent))
                {
                    continue;
                }

                // Deduplicated on the same 50 cm quantisation the planner uses for position ids, so
                // two lattice cells that resolve to the same measurable spot count once.
                const auto Key = ck::perf_lab::Get_PositionId(Projected.Location);

                if (Seen.Contains(Key))
                {
                    continue;
                }

                Seen.Add(Key);
                Points.Add(Projected.Location);
            }
        }

        return Points;
    }

    auto
        Get_TriangleCount(
            const AActor* InActor)
        -> int64
    {
        auto Triangles = int64{0};

        for (const auto* Component : TInlineComponentArray<UStaticMeshComponent*>{InActor})
        {
            if (ck::Is_NOT_Valid(Component))
            {
                continue;
            }

            const UStaticMesh* Mesh = Component->GetStaticMesh();

            if (ck::Is_NOT_Valid(Mesh))
            {
                continue;
            }

            // LOD 0 is the worst case, which is the one worth ranking by: it is what a camera close
            // enough to care will actually draw.
            if (Mesh->GetNumLODs() > 0)
            {
                Triangles += static_cast<int64>(Mesh->GetNumTriangles(0));
            }
        }

        return Triangles;
    }

    auto
        Build_CensusRow(
            const AActor* InActor)
        -> FCk_PerfLab_ActorCensusRow
    {
        const auto Primitives = TInlineComponentArray<UPrimitiveComponent*>{InActor};

        const auto Is_Usable = [](const UPrimitiveComponent* InComponent)
        { return ck::IsValid(InComponent); };

        // Three separate questions rather than three accumulators sharing a loop: each one reads as
        // what it asks, each result is const, and the two predicates short-circuit.
        const auto MaterialSlots = static_cast<int32>(ck::algo::SumBy(Primitives,
            [&](const UPrimitiveComponent* InComponent)
            { return Is_Usable(InComponent) ? InComponent->GetNumMaterials() : 0; }));

        const auto CastsShadow = ck::algo::AnyOf(Primitives, [&](const UPrimitiveComponent* InComponent)
        {
            return Is_Usable(InComponent) && InComponent->CastShadow
                && InComponent->Mobility == EComponentMobility::Movable;
        });

        const auto HasCollision = ck::algo::AnyOf(Primitives, [&](const UPrimitiveComponent* InComponent)
        { return Is_Usable(InComponent) && InComponent->IsCollisionEnabled(); });

        const auto Lights = TInlineComponentArray<ULightComponent*>{InActor}.Num();

        // Counted through the engine's own FX component base so this module does not have to take a
        // Niagara dependency to notice that effects are present.
        const auto Effects = TInlineComponentArray<UFXSystemComponent*>{InActor}.Num();

        return FCk_PerfLab_ActorCensusRow{}
            .Set_ObjectPath(InActor->GetPathName())
            .Set_ClassName(InActor->GetClass()->GetName())
            .Set_TriangleCount(Get_TriangleCount(InActor))
            .Set_MaterialSlotCount(MaterialSlots)
            .Set_LightCount(Lights)
            .Set_NiagaraComponentCount(Effects)
            .Set_TickEnabled(InActor->IsActorTickEnabled())
            .Set_CastsDynamicShadow(CastsShadow)
            .Set_HasCollision(HasCollision);
    }

    auto
        Get_CostWeight(
            const FCk_PerfLab_ActorCensusRow& InRow)
        -> float
    {
        return (static_cast<float>(InRow.Get_TriangleCount()) / k_TrianglesPerWeightUnit)
             + (static_cast<float>(InRow.Get_LightCount())            * k_WeightPerLight)
             + (static_cast<float>(InRow.Get_NiagaraComponentCount()) * k_WeightPerEffect)
             + (InRow.Get_TickEnabled()        ? k_WeightPerTickingActor : 0.0f)
             + (InRow.Get_CastsDynamicShadow() ? k_WeightPerShadowCaster : 0.0f);
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    auto
        Build_WorldSurvey(
            const UWorld* InWorld,
            const FCk_PerfLab_ModeParams& InModeParams)
        -> FCk_PerfLab_WorldSurvey
    {
        auto Survey = FCk_PerfLab_WorldSurvey{};

        if (ck::Is_NOT_Valid(InWorld))
        {
            return Survey;
        }

        auto Actors = TArray<FCk_PerfLab_SurveyActor>{};
        auto Bounds = FBox{ForceInit};

        for (auto Iterator = TActorIterator<AActor>{const_cast<UWorld*>(InWorld)}; Iterator; ++Iterator)
        {
            const auto* Actor = *Iterator;

            if (ck::Is_NOT_Valid(Actor))
            {
                continue;
            }

            const auto Row    = ck_perf_lab_survey::Build_CensusRow(Actor);
            const auto Weight = ck_perf_lab_survey::Get_CostWeight(Row);

            Bounds += Actor->GetActorLocation();

            // A weightless actor still bounds the level but never attracts a measurement position:
            // a camera pointed at a trigger volume measures nothing.
            if (Weight <= 0.0f)
            {
                continue;
            }

            Actors.Add(FCk_PerfLab_SurveyActor{}
                .Set_Location(Actor->GetActorLocation())
                .Set_CostWeight(Weight)
                .Set_Census(Row));
        }

        // Actor iteration order is not a contract, and the planner's determinism spec pins that it
        // does not matter — sorting here makes that true at the source rather than relying on it.
        ck::algo::Sort(Actors, [](const FCk_PerfLab_SurveyActor& InA, const FCk_PerfLab_SurveyActor& InB)
        {
            return InA.Get_Census().Get_ObjectPath() < InB.Get_Census().Get_ObjectPath();
        });

        auto NavmeshPoints = TArray<FVector>{};

        if (const auto* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(InWorld))
        {
            // A map with no navigation data is a normal case; the planner falls back to a grid and
            // says so, so absence here is a quiet branch rather than an error.
            if (ck::IsValid(NavSystem->GetDefaultNavDataInstance()))
            {
                // The navigation system's OWN bounds, not the actor bounds accumulated above. Those
                // are a box over actor PIVOTS, and a level built as one landscape proxy with its
                // pivot at the origin produces a box a few centimetres across over a kilometre of
                // navigable space — the lattice would collapse into a single cell and return one
                // point for the whole map. Actor bounds remain the fallback for a navigation system
                // that reports nothing.
                const auto NavBounds = NavSystem->GetWorldBounds();
                const auto LatticeBounds = NavBounds.IsValid && NavBounds.GetSize().Size2D() > 0.0
                    ? NavBounds
                    : Bounds;

                if (LatticeBounds.IsValid)
                {
                    NavmeshPoints = ck_perf_lab_survey::Project_LatticeOntoNavmesh(*NavSystem, LatticeBounds);
                }
            }
        }

        return Survey.Set_Actors(Actors)
                     .Set_NavmeshPoints(NavmeshPoints)
                     .Set_WorldBounds(Bounds);
    }

    auto
        Build_CensusForPosition(
            const FCk_PerfLab_WorldSurvey& InSurvey,
            const FVector& InLocation,
            float InRadiusCm)
        -> TArray<FCk_PerfLab_ActorCensusRow>
    {
        const auto RadiusSq = static_cast<double>(InRadiusCm) * static_cast<double>(InRadiusCm);

        auto Rows = ck::algo::TransformIf<TArray<FCk_PerfLab_ActorCensusRow>>(InSurvey.Get_Actors(),
            [&](const FCk_PerfLab_SurveyActor& InActor)
            { return FVector::DistSquared(InActor.Get_Location(), InLocation) <= RadiusSq; },
            [&](const FCk_PerfLab_SurveyActor& InActor)
            {
                return FCk_PerfLab_ActorCensusRow{InActor.Get_Census()}.Set_DistanceCm(
                    static_cast<float>(FVector::Dist(InActor.Get_Location(), InLocation)));
            });

        // Nearest first, object path as the tie-break: this order reaches the session file and the
        // contributor list, so it has to be reproducible rather than incidental.
        ck::algo::Sort(Rows, [](const FCk_PerfLab_ActorCensusRow& InA, const FCk_PerfLab_ActorCensusRow& InB)
        {
            if (InA.Get_DistanceCm() != InB.Get_DistanceCm())
            {
                return InA.Get_DistanceCm() < InB.Get_DistanceCm();
            }

            return InA.Get_ObjectPath() < InB.Get_ObjectPath();
        });

        return Rows;
    }
}

// --------------------------------------------------------------------------------------------------------------------
