#pragma once
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Containers/Array.h"

namespace ck_debugoverlay
{
    struct FViewpoint
    {
        FVector Location = FVector::ZeroVector;
        FVector Forward  = FVector::ForwardVector;
    };

    struct FCandidate
    {
        FVector WorldLocation = FVector::ZeroVector;
        bool    bIsOnScreen   = false;

        // Lifetime-owner hops to the registry transient: 0 = top-level entity.
        int32   Depth         = 0;

        // Viewport screen position (px), filled during the on-screen projection pass.
        // Valid only when bIsOnScreen and not ejected; used for co-located clustering.
        FVector2D ScreenPos   = FVector2D::ZeroVector;
    };

    /** Score a single candidate relative to the given viewpoint.
     *  Returns negative (-1) if the candidate is off-screen.
     *  Higher is better; range (0, 1] when on-screen.
     */
    CKENTITYDEBUGOVERLAY_API auto Score_Candidate(const FCandidate& C, const FViewpoint& VP) -> float;

    /** Return the index of the highest-scoring on-screen candidate, or INDEX_NONE. */
    CKENTITYDEBUGOVERLAY_API auto Pick_Best(const TArray<FCandidate>& Cands, const FViewpoint& VP) -> int32;
}
