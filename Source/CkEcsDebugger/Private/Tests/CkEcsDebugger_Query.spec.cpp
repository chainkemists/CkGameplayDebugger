#include "Misc/AutomationTest.h"

#include "CkEcsDebugger/Query/CkEcsDebugger_Query.h"

// --------------------------------------------------------------------------------------------------------------------
// Tokenizer + evaluation specs (redesign spec §3.5) — fully synthetic contexts, no world.

namespace ck_ecs_debugger_query_spec
{
    static auto MakeTable() -> ck::ecs_debugger_query::FFeatureTokenTable
    {
        auto Table = ck::ecs_debugger_query::FFeatureTokenTable{};
        Table.Add(TEXT("Timer"), uint64{1} << 0);
        Table.Add(TEXT("Transform"), uint64{1} << 1);
        Table.Add(TEXT("SceneNode"), uint64{1} << 2);
        Table.Add(TEXT("Scene Node"), uint64{1} << 2);   // display name → same bit
        Table.Add(TEXT("StateMachine"), uint64{1} << 3);
        return Table;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkEcsDebuggerQuery_Parse_Test,
    "Ck.EcsDebugger.Query.ParseGrammar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerQuery_Parse_Test::RunTest(const FString&)
{
    using namespace ck::ecs_debugger_query;

    // Every grammar production in one query.
    {
        const auto Query = Parse(TEXT("has:Timer is:primary net:auth id:42 arch:shelf loose"));
        TestEqual(TEXT("term count"), Query.Terms.Num(), 6);
        TestEqual(TEXT("has"), static_cast<int32>(Query.Terms[0].Type), static_cast<int32>(ETermType::Has));
        TestEqual(TEXT("is:primary"), static_cast<int32>(Query.Terms[1].Type), static_cast<int32>(ETermType::IsPrimary));
        TestEqual(TEXT("net"), static_cast<int32>(Query.Terms[2].Type), static_cast<int32>(ETermType::Net));
        TestEqual(TEXT("id"), static_cast<int32>(Query.Terms[3].Type), static_cast<int32>(ETermType::Id));
        TestEqual(TEXT("id value"), static_cast<int32>(Query.Terms[3].IdValue), 42);
        TestEqual(TEXT("arch"), static_cast<int32>(Query.Terms[4].Type), static_cast<int32>(ETermType::Arch));
        TestEqual(TEXT("fuzzy"), static_cast<int32>(Query.Terms[5].Type), static_cast<int32>(ETermType::Fuzzy));
    }

    // is:aux, is:<feat>.
    {
        const auto Query = Parse(TEXT("is:aux is:timer"));
        TestEqual(TEXT("aux"), static_cast<int32>(Query.Terms[0].Type), static_cast<int32>(ETermType::IsAux));
        TestEqual(TEXT("is-feature"), static_cast<int32>(Query.Terms[1].Type), static_cast<int32>(ETermType::Is));
    }

    // Incomplete tokens drop; unknown keys and malformed ids fall back to fuzzy.
    {
        const auto Query = Parse(TEXT("has: net: foo:bar id:abc"));
        TestEqual(TEXT("incomplete dropped, rest fuzzy"), Query.Terms.Num(), 2);
        TestEqual(TEXT("unknown key fuzzy"), static_cast<int32>(Query.Terms[0].Type), static_cast<int32>(ETermType::Fuzzy));
        TestEqual(TEXT("bad id fuzzy"), static_cast<int32>(Query.Terms[1].Type), static_cast<int32>(ETermType::Fuzzy));
    }

    TestTrue(TEXT("empty query is empty"), Parse(TEXT("   ")).IsEmpty());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkEcsDebuggerQuery_Matches_Test,
    "Ck.EcsDebugger.Query.MatchSemantics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerQuery_Matches_Test::RunTest(const FString&)
{
    using namespace ck::ecs_debugger_query;
    namespace spec = ck_ecs_debugger_query_spec;

    const auto Table = spec::MakeTable();

    auto Station = FEntityQueryContext{};
    Station.OwnBits = (uint64{1} << 1) | (uint64{1} << 3);   // Transform + StateMachine
    Station.RollupBits = uint64{1} << 0;                     // rolled-up Timer
    Station.IsInternal = false;
    Station.NetMode = EQueryNetMode::Authority;
    Station.EntityId = 7;
    Station.ArchetypeName = TEXT("rewind99.shelf");
    Station.DisplayName = TEXT("Station_Register");

    // has: matches own ∪ rollup; is: matches own only (spec §3.2).
    TestTrue(TEXT("has:timer via rollup"), Matches(Station, Parse(TEXT("has:timer")), Table));
    TestFalse(TEXT("is:timer own-only"), Matches(Station, Parse(TEXT("is:timer")), Table));
    TestTrue(TEXT("is:statemachine own"), Matches(Station, Parse(TEXT("is:statemachine")), Table));

    // Feature tokens PREFIX-match ids and display names.
    TestTrue(TEXT("prefix 'scene' resolves"), Matches(
        FEntityQueryContext{ uint64{1} << 2 }, Parse(TEXT("is:scene")), Table));

    // Unresolvable feature token → term fails (not vacuously true).
    TestFalse(TEXT("unknown feature fails"), Matches(Station, Parse(TEXT("has:zzz")), Table));

    // is:primary / is:aux.
    TestTrue(TEXT("is:primary"), Matches(Station, Parse(TEXT("is:primary")), Table));
    TestFalse(TEXT("is:aux"), Matches(Station, Parse(TEXT("is:aux")), Table));

    // net: with prefix forgiveness.
    TestTrue(TEXT("net:auth"), Matches(Station, Parse(TEXT("net:auth")), Table));
    TestTrue(TEXT("net:a"), Matches(Station, Parse(TEXT("net:a")), Table));
    TestFalse(TEXT("net:proxy"), Matches(Station, Parse(TEXT("net:proxy")), Table));

    // id: exact; arch: substring.
    TestTrue(TEXT("id:7"), Matches(Station, Parse(TEXT("id:7")), Table));
    TestFalse(TEXT("id:8"), Matches(Station, Parse(TEXT("id:8")), Table));
    TestTrue(TEXT("arch:shelf"), Matches(Station, Parse(TEXT("arch:shelf")), Table));
    TestFalse(TEXT("arch:crate"), Matches(Station, Parse(TEXT("arch:crate")), Table));

    // Terms AND-compose.
    TestTrue(TEXT("AND all pass"), Matches(Station, Parse(TEXT("has:timer net:auth id:7")), Table));
    TestFalse(TEXT("AND one fails"), Matches(Station, Parse(TEXT("has:timer net:proxy")), Table));

    // Empty query matches everything.
    TestTrue(TEXT("empty query"), Matches(Station, Parse(TEXT("")), Table));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkEcsDebuggerQuery_InferredKey_Test,
    "Ck.EcsDebugger.Query.InferredArchetypeKey",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerQuery_InferredKey_Test::RunTest(const FString&)
{
    using namespace ck::ecs_debugger_query;

    // Instance suffixes strip; same base + same signature → same key.
    TestEqual(TEXT("suffix strips"),
        Get_InferredArchetypeKey(TEXT("Enemy_12"), 5), Get_InferredArchetypeKey(TEXT("Enemy_7"), 5));

    // Different signature → different key even with the same base name.
    TestNotEqual(TEXT("signature splits"),
        Get_InferredArchetypeKey(TEXT("Enemy_12"), 5), Get_InferredArchetypeKey(TEXT("Enemy_7"), 9));

    // Base survives left of '#'.
    {
        const auto Key = Get_InferredArchetypeKey(TEXT("Ck_CueRelay_UE_3"), 0);
        auto Base = FString{};
        auto Signature = FString{};
        TestTrue(TEXT("key splits on #"), Key.Split(TEXT("#"), &Base, &Signature));
        TestEqual(TEXT("base"), Base, FString(TEXT("Ck_CueRelay_UE")));
    }

    // All-digit names don't strip to nothing.
    TestTrue(TEXT("digit-only name keeps base"),
        Get_InferredArchetypeKey(TEXT("123"), 0).StartsWith(TEXT("123")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
