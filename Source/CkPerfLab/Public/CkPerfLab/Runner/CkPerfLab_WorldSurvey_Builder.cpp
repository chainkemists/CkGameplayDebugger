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
    // How many navmesh samples to ask for. Generous relative to any sane position budget, because
    // the planner decimates and content-weights them afterwards; the cost of an unused sample is a
    // vector, and the cost of too few is a level measured only where the sampler happened to land.
    constexpr auto k_NavmeshSampleCount = 512;

    // Triangle counts dominate the ranking if taken raw — a single dense mesh would outrank a room
    // full of lights and particles. Scaling them down keeps the axes comparable without pretending
    // the weight means anything in absolute terms.
    constexpr auto k_TrianglesPerWeightUnit = 5000.0f;

    constexpr auto k_WeightPerLight         = 4.0f;
    constexpr auto k_WeightPerEffect        = 6.0f;
    constexpr auto k_WeightPerTickingActor  = 1.0f;
    constexpr auto k_WeightPerShadowCaster  = 2.0f;

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
                const auto Origin = Bounds.IsValid ? Bounds.GetCenter() : FVector::ZeroVector;
                const auto Radius = Bounds.IsValid ? Bounds.GetExtent().Size2D() : 0.0;

                for (auto Sample = 0; Sample < ck_perf_lab_survey::k_NavmeshSampleCount; ++Sample)
                {
                    auto Location = FNavLocation{};

                    if (const_cast<UNavigationSystemV1*>(NavSystem)->GetRandomReachablePointInRadius(
                            Origin, static_cast<float>(Radius), Location))
                    {
                        NavmeshPoints.Add(Location.Location);
                    }
                }

                // The samples are random, so their order carries no meaning and would otherwise leak
                // into the plan through the dedup-first-wins rule.
                ck::algo::Sort(NavmeshPoints, [](const FVector& InA, const FVector& InB)
                {
                    if (InA.X != InB.X)
                    { return InA.X < InB.X; }

                    if (InA.Y != InB.Y)
                    { return InA.Y < InB.Y; }
                    return InA.Z < InB.Z;
                });
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
