#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_Selection.h"
#include "Math/UnrealMathUtility.h"

namespace ck_debugoverlay
{
    auto Score_Candidate(const FCandidate& C, const FViewpoint& VP) -> float
    {
        if (!C.bIsOnScreen) { return -1.f; }

        constexpr float W_Ang  = 0.7f;
        constexpr float W_Dist = 0.3f;
        constexpr float MaxDist = 5000.f;

        const FVector To   = C.WorldLocation - VP.Location;
        const float   Dist = To.Size();

        if (Dist <= KINDA_SMALL_NUMBER) { return 1.f; }

        const float Ang  = FVector::DotProduct(To / Dist, VP.Forward.GetSafeNormal());
        const float AngS = FMath::Clamp((Ang + 1.f) * 0.5f, 0.f, 1.f);
        const float DistS = FMath::Clamp(1.f - Dist / MaxDist, 0.f, 1.f);

        return W_Ang * AngS + W_Dist * DistS;
    }

    auto Pick_Best(const TArray<FCandidate>& Cands, const FViewpoint& VP) -> int32
    {
        int32 Best      = INDEX_NONE;
        float BestScore = 0.f;

        for (int32 i = 0; i < Cands.Num(); ++i)
        {
            const float S = Score_Candidate(Cands[i], VP);
            const auto IsScoreTie = Best != INDEX_NONE && FMath::IsNearlyEqual(S, BestScore);
            const auto PreferDeeperHierarchyCandidate =
                IsScoreTie && Cands[i].Depth > Cands[Best].Depth;

            if ((S > BestScore && NOT IsScoreTie) || PreferDeeperHierarchyCandidate)
            {
                BestScore = S;
                Best = i;
            }
        }

        return Best;
    }
}
