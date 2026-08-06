#include "Misc/AutomationTest.h"
#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_Selection.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_Selection_Test,
    "Ck.DebugOverlay.Selection.PickBest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_Selection_Test::RunTest(const FString&)
{
    ck_debugoverlay::FViewpoint VP;
    VP.Location = FVector::ZeroVector;
    VP.Forward  = FVector::ForwardVector;

    TArray<ck_debugoverlay::FCandidate> C {
        { FVector(1000,0,0), true  },    // dead ahead, near  → best
        { FVector(1000,800,0), true  },  // ahead but off-axis
        { FVector(200,0,0),  false },    // closest but OFF screen → excluded
    };

    TestEqual(TEXT("pick dead-ahead"), ck_debugoverlay::Pick_Best(C, VP), 0);

    // A full-depth hierarchy can legitimately expose parent and child markers at the
    // same transform. Their gaze scores are identical, so prefer the deeper entity;
    // otherwise registry/gather order makes the parent permanently unselectable-by-depth.
    TArray<ck_debugoverlay::FCandidate> CoLocatedHierarchy {
        { FVector(1000,0,0), true, 0 },
        { FVector(1000,0,0), true, 2 },
    };

    TestEqual(
        TEXT("co-located hierarchy prefers deepest candidate"),
        ck_debugoverlay::Pick_Best(CoLocatedHierarchy, VP),
        1);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
