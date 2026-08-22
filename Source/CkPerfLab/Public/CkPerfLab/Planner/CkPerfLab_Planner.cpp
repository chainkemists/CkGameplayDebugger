#include "CkPerfLab_Planner.h"

#include "CkCore/Algorithms/CkAlgorithms.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_planner
{
    // Locations are quantised to half a metre before hashing. Coarse enough that floating-point
    // noise and a slightly moved navmesh sample still land on the same id between runs; fine enough
    // that two genuinely different measurement spots never collide.
    constexpr auto k_PositionQuantisationCm = 50.0;

    // A candidate cell keeps its own seed location so the grid path and the navmesh path can share
    // everything downstream.
    struct FCk_Candidate
    {
        FVector _Location   = FVector::ZeroVector;
        float   _CostWeight = 0.0f;
        FString _PositionId;
    };

    auto
        Get_QuantisedKey(
            const FVector& InLocation)
        -> FIntVector
    {
        return FIntVector
        {
            static_cast<int32>(FMath::RoundToDouble(InLocation.X / k_PositionQuantisationCm)),
            static_cast<int32>(FMath::RoundToDouble(InLocation.Y / k_PositionQuantisationCm)),
            static_cast<int32>(FMath::RoundToDouble(InLocation.Z / k_PositionQuantisationCm))
        };
    }

    auto
        Get_WeightAround(
            const TArray<FCk_PerfLab_SurveyActor>& InActors,
            const FVector& InLocation,
            float InRadiusCm)
        -> float
    {
        const auto RadiusSq = static_cast<double>(InRadiusCm) * static_cast<double>(InRadiusCm);

        return static_cast<float>(ck::algo::SumBy(InActors, [&](const FCk_PerfLab_SurveyActor& InActor)
        {
            return FVector::DistSquared(InActor.Get_Location(), InLocation) <= RadiusSq
                ? InActor.Get_CostWeight()
                : 0.0f;
        }));
    }

    // Evenly spaced around the circle, with the first direction derived from the seed so a run can
    // be reproduced exactly while different seeds still look at different things.
    auto
        Get_Yaws(
            int32 InCount,
            int32 InSeed)
        -> TArray<float>
    {
        auto Yaws = TArray<float>{};

        if (InCount <= 0)
        {
            return Yaws;
        }

        const auto Step      = 360.0f / static_cast<float>(InCount);
        const auto FirstYaw  = static_cast<float>(FMath::Abs(InSeed) % 360);

        Yaws.Reserve(InCount);

        for (auto Index = 0; Index < InCount; ++Index)
        {
            Yaws.Add(FMath::Fmod(FirstYaw + (Step * static_cast<float>(Index)), 360.0f));
        }

        return Yaws;
    }

    auto
        Build_NavmeshCandidates(
            const FCk_PerfLab_WorldSurvey& InSurvey,
            float InCensusRadiusCm)
        -> TArray<FCk_Candidate>
    {
        auto Candidates = TArray<FCk_Candidate>{};
        auto SeenKeys   = TSet<FIntVector>{};

        for (const auto& Point : InSurvey.Get_NavmeshPoints())
        {
            const auto Key = Get_QuantisedKey(Point);

            // Two navmesh samples that quantise to the same id would be the same position measured
            // twice, so the first one wins and the rest are dropped.
            if (SeenKeys.Contains(Key))
            {
                continue;
            }

            SeenKeys.Add(Key);

            const auto Weight = Get_WeightAround(InSurvey.Get_Actors(), Point, InCensusRadiusCm);

            // A reachable point with nothing around it measures empty air.
            if (Weight <= 0.0f)
            {
                continue;
            }

            Candidates.Add(FCk_Candidate{Point, Weight, ck::perf_lab::Get_PositionId(Point)});
        }

        return Candidates;
    }

    auto
        Build_GridCandidates(
            const FCk_PerfLab_WorldSurvey& InSurvey,
            float InSpacingCm,
            float InCensusRadiusCm)
        -> TArray<FCk_Candidate>
    {
        auto Candidates = TArray<FCk_Candidate>{};

        const auto& Bounds = InSurvey.Get_WorldBounds();

        if (NOT Bounds.IsValid || InSpacingCm <= 0.0f)
        {
            return Candidates;
        }

        const auto Size = Bounds.GetSize();

        // A pathological bounds/spacing combination could ask for millions of cells; cap the axis
        // count so a bad request degrades into a coarse plan rather than a hang.
        constexpr auto k_MaxCellsPerAxis = 64;

        const auto CellsX = FMath::Clamp(FMath::CeilToInt32(Size.X / InSpacingCm), 1, k_MaxCellsPerAxis);
        const auto CellsY = FMath::Clamp(FMath::CeilToInt32(Size.Y / InSpacingCm), 1, k_MaxCellsPerAxis);

        const auto StepX = Size.X / static_cast<double>(CellsX);
        const auto StepY = Size.Y / static_cast<double>(CellsY);

        // Z sits at the centre of the bounds: without a navmesh there is no ground to project onto,
        // and the runner is responsible for settling the camera onto something solid.
        const auto CentreZ = Bounds.GetCenter().Z;

        for (auto IndexX = 0; IndexX < CellsX; ++IndexX)
        {
            for (auto IndexY = 0; IndexY < CellsY; ++IndexY)
            {
                const auto Location = FVector
                {
                    Bounds.Min.X + (StepX * (static_cast<double>(IndexX) + 0.5)),
                    Bounds.Min.Y + (StepY * (static_cast<double>(IndexY) + 0.5)),
                    CentreZ
                };

                const auto Weight = Get_WeightAround(InSurvey.Get_Actors(), Location, InCensusRadiusCm);

                if (Weight <= 0.0f)
                {
                    continue;
                }

                Candidates.Add(FCk_Candidate{Location, Weight, ck::perf_lab::Get_PositionId(Location)});
            }
        }

        return Candidates;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    auto
        Get_PositionId(
            const FVector& InLocation)
        -> FString
    {
        const auto Key = ck_perf_lab_planner::Get_QuantisedKey(InLocation);

        // A plain mix of the three axes. It only has to be stable and collision-resistant across one
        // map, so a cryptographic hash would be cost without benefit.
        auto Hash = static_cast<uint32>(GetTypeHash(Key));

        return FString::Printf(TEXT("p_%08x"), Hash);
    }

    auto
        Generate_Plan(
            const FCk_PerfLab_WorldSurvey& InSurvey,
            const FCk_PerfLab_Request& InRequest)
        -> FCk_PerfLab_Plan
    {
        using namespace ck_perf_lab_planner;

        const auto& ModeParams = InRequest.Get_ModeParams();

        auto Plan = FCk_PerfLab_Plan{};

        if (InSurvey.Get_Actors().IsEmpty())
        {
            // Not an error: a level with nothing in it has nothing to measure, and saying so is more
            // useful than returning positions that would all read the same empty frame.
            return Plan.Set_Outcome(ECk_PerfLab_PlanOutcome::Empty_NoContent);
        }

        const auto UseNavmesh = NOT InSurvey.Get_NavmeshPoints().IsEmpty();

        auto Candidates = UseNavmesh
            ? Build_NavmeshCandidates(InSurvey, ModeParams.Get_CensusRadiusCm())
            : Build_GridCandidates(InSurvey, ModeParams.Get_MinPositionSpacingCm(), ModeParams.Get_CensusRadiusCm());

        if (NOT UseNavmesh && NOT InSurvey.Get_WorldBounds().IsValid)
        {
            return Plan.Set_Outcome(ECk_PerfLab_PlanOutcome::Empty_NoBounds);
        }

        // Heaviest first, so a tight budget is spent where the content is. The id is the tie-break
        // rather than the array order, which is what makes the result independent of how the survey
        // happened to be gathered.
        ck::algo::Sort(Candidates, [](const FCk_Candidate& InA, const FCk_Candidate& InB)
        {
            if (InA._CostWeight != InB._CostWeight)
            {
                return InA._CostWeight > InB._CostWeight;
            }

            return InA._PositionId < InB._PositionId;
        });

        const auto SpacingSq = static_cast<double>(ModeParams.Get_MinPositionSpacingCm()) *
                               static_cast<double>(ModeParams.Get_MinPositionSpacingCm());

        auto Accepted = TArray<FCk_Candidate>{};

        for (const auto& Candidate : Candidates)
        {
            if (Accepted.Num() >= ModeParams.Get_PositionBudget())
            {
                break;
            }

            const auto IsTooClose = ck::algo::AnyOf(Accepted, [&](const FCk_Candidate& InAccepted)
            {
                return FVector::DistSquared(InAccepted._Location, Candidate._Location) < SpacingSq;
            });

            if (IsTooClose)
            {
                continue;
            }

            Accepted.Add(Candidate);
        }

        // Emitted in id order rather than in selection order: the session file is diffed, and a plan
        // whose rows moved because one candidate's weight shifted would read as a bigger change than
        // it is.
        ck::algo::Sort(Accepted, [](const FCk_Candidate& InA, const FCk_Candidate& InB)
        {
            return InA._PositionId < InB._PositionId;
        });

        const auto Yaws = Get_Yaws(ModeParams.Get_DirectionsPerPosition(), InRequest.Get_Seed());

        auto Positions = TArray<FCk_PerfLab_PlannedPosition>{};
        Positions.Reserve(Accepted.Num());

        for (const auto& Candidate : Accepted)
        {
            Positions.Add(FCk_PerfLab_PlannedPosition{}
                .Set_PositionId(Candidate._PositionId)
                .Set_Location(Candidate._Location)
                .Set_EyeHeightCm(ModeParams.Get_EyeHeightCm())
                .Set_Yaws(Yaws)
                .Set_CostWeight(Candidate._CostWeight));
        }

        return Plan.Set_Positions(Positions)
                   .Set_Outcome(Positions.IsEmpty()
                       ? ECk_PerfLab_PlanOutcome::Empty_NoContent
                       : (UseNavmesh ? ECk_PerfLab_PlanOutcome::NavmeshSeeded : ECk_PerfLab_PlanOutcome::GridFallback));
    }
}

// --------------------------------------------------------------------------------------------------------------------
