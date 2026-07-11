#include "Misc/AutomationTest.h"

#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"
#include "CkEcsDebugger/Classification/CkEcsDebugger_Classification.h"

// --------------------------------------------------------------------------------------------------------------------
// Pure-logic specs over the REAL flag registry: RegisterAll ran at module startup, and
// registration is global + idempotent, so real feature ids/bits are stable here. The
// rollup forest is synthetic — no world or PIE needed.

namespace ck_ecs_debugger_classification_spec
{
    static auto Bit(const TCHAR* InFeatureId) -> uint64
    {
        const auto Index = ck::debug_feature_flags::Get_BitIndex(InFeatureId);
        return Index == INDEX_NONE ? uint64{0} : uint64{1} << Index;
    }

    static auto DefaultInternalSet() -> TSet<FName>
    {
        return { TEXT("Timer"), TEXT("SceneNode"), TEXT("Probe"),
                 TEXT("FloatAttribute"), TEXT("ByteAttribute"), TEXT("IntegerAttribute") };
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkEcsDebuggerClassification_Signature_Test,
    "Ck.EcsDebugger.Classification.SignatureAndInternalSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerClassification_Signature_Test::RunTest(const FString&)
{
    using namespace ck::ecs_debugger_classification;
    namespace spec = ck_ecs_debugger_classification_spec;

    // Guard: the ids this spec leans on must have registered bits — a rename in
    // RegisterAll should fail HERE, not as a confusing classification assert below.
    for (const auto& RequiredId : { TEXT("Timer"), TEXT("Probe"), TEXT("SceneNode"),
                                    TEXT("StateMachine"), TEXT("Transform"), TEXT("Label") })
    {
        if (spec::Bit(RequiredId) == 0)
        {
            AddError(FString::Printf(TEXT("feature id '%s' has no registered bit — RegisterAll changed?"), RequiredId));
            return false;
        }
    }

    const auto Table = BuildTable(spec::DefaultInternalSet());

    // Timer + structural carriers → internal, X = Timer.
    {
        const auto Result = Classify(
            spec::Bit(TEXT("Timer")) | spec::Bit(TEXT("Transform")) | spec::Bit(TEXT("Label")), Table);
        TestTrue(TEXT("timer+transform+label is internal"), Result.IsInternal);
        TestEqual(TEXT("X is Timer"), Result.InternalFeatureId, FName{TEXT("Timer")});
    }

    // Bare single feature (no structural carriers) → still internal.
    TestTrue(TEXT("bare timer is internal"),
        Classify(spec::Bit(TEXT("Timer")), Table).IsInternal);

    // Two non-structural features → primary.
    TestFalse(TEXT("timer+probe is primary"),
        Classify(spec::Bit(TEXT("Timer")) | spec::Bit(TEXT("Probe")) | spec::Bit(TEXT("Transform")), Table).IsInternal);

    // Structural-only signature (no X) → primary per spec §3.1.
    TestFalse(TEXT("transform+label only is primary"),
        Classify(spec::Bit(TEXT("Transform")) | spec::Bit(TEXT("Label")), Table).IsInternal);

    // Single feature with a registered bit but NOT in the internal set → primary.
    TestFalse(TEXT("statemachine-only is primary"),
        Classify(spec::Bit(TEXT("StateMachine")) | spec::Bit(TEXT("Label")), Table).IsInternal);

    // Empty internal set → nothing classifies internal.
    TestFalse(TEXT("empty internal set means no internals"),
        Classify(spec::Bit(TEXT("Timer")), BuildTable(TSet<FName>{})).IsInternal);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkEcsDebuggerClassification_Rollup_Test,
    "Ck.EcsDebugger.Classification.RollupStopsAtPrimaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerClassification_Rollup_Test::RunTest(const FString&)
{
    using namespace ck::ecs_debugger_classification;
    namespace spec = ck_ecs_debugger_classification_spec;

    const auto Table = BuildTable(spec::DefaultInternalSet());

    const auto Timer     = spec::Bit(TEXT("Timer"));
    const auto SceneNode = spec::Bit(TEXT("SceneNode"));
    const auto Primary   = spec::Bit(TEXT("Timer")) | spec::Bit(TEXT("Probe"));   // two features → primary

    // Forest (spec §3.2's Station/Display example plus the defensive cases):
    // [0] primary root (Station)
    // [1] timer internal, owner 0
    // [2] scenenode internal, owner 1 — internal chain walks through to 0
    // [3] primary (Display), owner 0
    // [4] timer internal, owner 3 — rolls to Display, NOT the Station
    // [5] timer internal, owner 6 — owner cycle with [6]
    // [6] scenenode internal, owner 5
    // [7] timer internal, no owner — internal root, contributes nowhere
    const auto OwnBits = TArray<uint64>{ Primary, Timer, SceneNode, Primary, Timer, Timer, SceneNode, Timer };
    const auto Owners  = TArray<int32>{ INDEX_NONE, 0, 1, 0, 3, 6, 5, INDEX_NONE };

    const auto Rollups = ComputeRollups(OwnBits, Owners, Table);

    TestEqual(TEXT("rollup count matches entity count"), Rollups.Num(), OwnBits.Num());
    TestEqual(TEXT("station rolls up one timer"),        Rollups[0].FindRef(TEXT("Timer")), 1);
    TestEqual(TEXT("station rolls up one scenenode"),    Rollups[0].FindRef(TEXT("SceneNode")), 1);
    TestEqual(TEXT("display rolls up its own timer"),    Rollups[3].FindRef(TEXT("Timer")), 1);
    TestEqual(TEXT("display claims no scenenode"),       Rollups[3].FindRef(TEXT("SceneNode")), 0);
    TestTrue(TEXT("internals have empty rollups"),       Rollups[1].IsEmpty() && Rollups[2].IsEmpty());
    TestTrue(TEXT("owner cycle contributes nothing"),    Rollups[5].IsEmpty() && Rollups[6].IsEmpty());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
