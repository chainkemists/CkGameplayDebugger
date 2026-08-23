#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"

#include "Misc/AutomationTest.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugViewportPicker_AvailabilityClassifiesEmptyStates,
    "Ck.DebuggerCommon.ViewportPicker.AvailabilityClassifiesEmptyStates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugViewportPicker_AvailabilityClassifiesEmptyStates::RunTest(const FString& Parameters)
{
    using namespace ck::DebugViewportPicker;

    auto Counts = FAvailabilityCounts{};
    TestEqual(TEXT("Missing target world"),
        Classify_Availability(false, false, Counts), EAvailability::NoTargetWorld);
    TestEqual(TEXT("Unsupported world mode"),
        Classify_Availability(true, false, Counts), EAvailability::UnsupportedWorld);
    TestEqual(TEXT("No target-filter matches"),
        Classify_Availability(true, true, Counts), EAvailability::NoMatchingEntities);

    Counts.MatchingEntities = 1;
    TestEqual(TEXT("Transformless match"),
        Classify_Availability(true, true, Counts), EAvailability::NoTransformRepresentation);

    Counts.TransformRepresentations = 2;
    Counts.IgnoredLocalPawnCandidates = 2;
    TestEqual(TEXT("All representations belong to ignored local pawn"),
        Classify_Availability(true, true, Counts), EAvailability::IgnoredLocalPawn);

    Counts.IgnoredLocalPawnCandidates = 1;
    Counts.CulledOrFilteredCandidates = 2;
    TestEqual(TEXT("Mixed ignored/cull set reports remaining filter failure"),
        Classify_Availability(true, true, Counts), EAvailability::AllCandidatesCulledOrFiltered);

    Counts.ViableCandidates = 1;
    TestEqual(TEXT("Visible marker snapshot supplies viable candidates"),
        Classify_Availability(true, true, Counts), EAvailability::ViableCandidates);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
