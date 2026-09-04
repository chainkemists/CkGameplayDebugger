#include "CkDebuggerCommon/Viewport/CkDebug3dWorldPing.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkPmg/CkPmg_Utils_FlatShapes.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::debug_3d
{
    auto
        Draw_WorldCommandPing(
            UWorld* InWorld,
            const FVector& InLocation)
        -> void
    {
        // Not a soft guard: every caller reaches this only after its command succeeded against a
        // live entity, so a null world here is a bug worth seeing rather than a silent no-draw.
        const auto WorldIsValid = ck::IsValid(InWorld);
        CK_ENSURE_IF_NOT(WorldIsValid, TEXT("Cannot draw a world command ping: no valid world"))
        { return; }

        UCk_Utils_Pmg_FlatShapes::DrawFilledRing(
            InWorld,
            // Lifted so the ring does not z-fight the surface it was placed on.
            InLocation + FVector{0.0f, 0.0f, 4.0f},
            /*InOuterRadius=*/60.0f,
            /*InInnerRadius=*/45.0f,
            /*InSegments=*/32,
            // KEPT LOCAL -- in-world PMG paint, not a Slate surface: the ping is read against
            // arbitrary level art, not against the debugger palette.
            FLinearColor{0.15f, 1.0f, 0.35f, 0.85f},
            /*InDrawLines=*/false,
            /*InLineThickness=*/2.0f,
            ECk_Plane_Axis::XY,
            /*InDuration=*/1.5f);
    }
}
