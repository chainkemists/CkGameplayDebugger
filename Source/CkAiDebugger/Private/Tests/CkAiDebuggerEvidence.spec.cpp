#include "CkAiDebugger/Model/CkAiDebugger_EvidenceModel.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace ck_ai_debugger_evidence_spec
{
    auto Tag(const TCHAR* InName) -> FGameplayTag
    { return FGameplayTag::RequestGameplayTag(InName, false); }

    auto Row(const TCHAR* InField, const TCHAR* InValue, ECk_DebugOverlay_Severity InSeverity = ECk_DebugOverlay_Severity::Normal) -> FCk_DebugOverlay_Row
    {
        auto Result = FCk_DebugOverlay_Row{};
        Result.FieldTag = Tag(InField);
        Result.Value = FText::FromString(InValue);
        Result.Severity = InSeverity;
        return Result;
    }

    auto Section(const TCHAR* InProvider, uint32 InSourceId, int32 InSourceOrder, const TCHAR* InSourceName) -> FCk_DebugOverlay_Section
    {
        auto Result = FCk_DebugOverlay_Section{};
        Result.ProviderTag = Tag(InProvider);
        Result.SourceEntityId = InSourceId;
        Result.SourceOrder = InSourceOrder;
        Result.SourceName = FText::FromString(InSourceName);
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkAiDebuggerEvidence_IdentityAndOccurrences,
    "Ck.AiDebugger.Evidence.IdentityAndOccurrences", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkAiDebuggerEvidence_IdentityAndOccurrences::RunTest(const FString&)
{
    FCk_DebugOverlay_EntityModel Model;
    auto First = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.StateMachine"), 101, 0, TEXT("Navigation_SM"));
    First.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.StateMachine.State"), TEXT("Approach")));
    First.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.StateMachine.State"), TEXT("Recover")));
    auto Second = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.StateMachine"), 202, 1, TEXT("Recovery_SM"));
    Second.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.StateMachine.State"), TEXT("Waiting")));
    Model.Sections = {First, Second};

    const auto Facts = ck::ai_debugger::evidence::Normalize(Model);
    TestEqual(TEXT("all repeated and sub-state facts survive"), Facts.Num(), 3);
    TestNotEqual(TEXT("same-field occurrences have unique keys"), Facts[0].StableKey, Facts[1].StableKey);
    TestTrue(TEXT("separate sub-state source identity survives"), Facts.ContainsByPredicate([](const auto& Fact) { return Fact.Key.SourceEntityId == 202; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkAiDebuggerEvidence_OrderAndVisibleDetail,
    "Ck.AiDebugger.Evidence.OrderAndVisibleDetail", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkAiDebuggerEvidence_OrderAndVisibleDetail::RunTest(const FString&)
{
    FCk_DebugOverlay_EntityModel Model;
    auto Crowd = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.Crowd"), 3, 2, TEXT("Avoidance_SM"));
    Crowd.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.Crowd.Status"), TEXT("Constrained"), ECk_DebugOverlay_Severity::Warn));
    auto Goap = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.Goap"), 2, 1, TEXT("Checkout_GOAP"));
    auto Plan = ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.Goap.Plan"), TEXT("MoveToCheckout"), ECk_DebugOverlay_Severity::Bad);
    Plan.MergedCount = 3;
    Plan.ExplicitHistory = {FText::FromString(TEXT("FindRegister")), FText::FromString(TEXT("MoveToCheckout"))};
    Goap.Rows.Add(Plan);
    Model.Sections = {Crowd, Goap};

    const auto Facts = ck::ai_debugger::evidence::Normalize(Model);
    TestEqual(TEXT("strongest severity is first"), Facts[0].Category, FString(TEXT("GOAP")));
    TestTrue(TEXT("merged count remains visible"), Facts[0].Detail.Contains(TEXT("3 merged")));
    TestTrue(TEXT("explicit history remains copyable"), Facts[0].CopyText.Contains(TEXT("FindRegister")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkAiDebuggerEvidence_TopologyPreservesSources,
    "Ck.AiDebugger.Evidence.TopologyPreservesSources", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkAiDebuggerEvidence_TopologyPreservesSources::RunTest(const FString&)
{
    FCk_DebugOverlay_EntityModel Model;
    auto Root = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.Goap"), 10, 0, TEXT("Journey_GOAP"));
    Root.SourceDepth = 0;
    Root.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.Goap.Plan"), TEXT("Checkout")));
    auto Child = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.Goap"), 11, 1, TEXT("Move_GOAP"));
    Child.ParentSourceEntityId = 10;
    Child.SourceDepth = 1;
    Child.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.Goap.Plan"), TEXT("MoveToRegister")));
    auto EmptyChild = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.Goap"), 12, 2, TEXT("Recovery_GOAP"));
    EmptyChild.ParentSourceEntityId = 11;
    EmptyChild.SourceDepth = 2;
    Model.Sections = {Root, Child, EmptyChild};

    const auto Topology = ck::ai_debugger::evidence::NormalizeTopology(Model);
    TestEqual(TEXT("every GOAP source produces a node"), Topology.Goaps.Num(), 3);
    TestEqual(TEXT("child preserves parent source identity"), Topology.Goaps[1].ParentSourceEntityId, static_cast<uint32>(10));
    TestEqual(TEXT("child preserves source depth"), Topology.Goaps[1].Depth, 1);
    TestEqual(TEXT("child preserves source order"), Topology.Goaps[1].SourceOrder, 1);
    TestTrue(TEXT("node detail preserves source rows"), Topology.Goaps[1].Detail.Contains(TEXT("MoveToRegister")));
    TestEqual(TEXT("empty child preserves its exact identity"), Topology.Goaps[2].SourceEntityId, static_cast<uint32>(12));
    TestEqual(TEXT("empty child remains explicit"), Topology.Goaps[2].Detail, FString(TEXT("No current rows")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkAiDebuggerEvidence_DeltaLifecycle,
    "Ck.AiDebugger.Evidence.DeltaLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkAiDebuggerEvidence_DeltaLifecycle::RunTest(const FString&)
{
    FCk_DebugOverlay_EntityModel Model;
    auto State = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.StateMachine"), 7, 0, TEXT("Navigation_SM"));
    State.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.StateMachine.State"), TEXT("Idle"), ECk_DebugOverlay_Severity::Warn));
    Model.Sections = {State};
    auto Facts = ck::ai_debugger::evidence::Normalize(Model);
    auto Tracker = FCkAiDebugger_EvidenceDeltaTracker{};
    TestEqual(TEXT("first snapshot seeds without flood"), Tracker.Observe(Facts, 1.0).Num(), 0);
    TestEqual(TEXT("unchanged snapshot emits none"), Tracker.Observe(Facts, 2.0).Num(), 0);

    State.Rows[0].Value = FText::FromString(TEXT("Blocked"));
    Model.Sections = {State};
    Facts = ck::ai_debugger::evidence::Normalize(Model);
    const auto Changed = Tracker.Observe(Facts, 3.0);
    TestEqual(TEXT("changed fact emits once"), Changed.Num(), 1);
    TestEqual(TEXT("changed fact preserves state category"), Changed[0].Category, FString(TEXT("STATE")));
    TestEqual(TEXT("changed fact preserves tone"), Changed[0].Tone, ECk_Tone::Warn);
    TestTrue(TEXT("changed event is concise"), Changed[0].Message.Contains(TEXT("Navigation_SM #7")));
    TestFalse(TEXT("changed event omits full evidence metadata"), Changed[0].Message.Contains(TEXT("depth 0")));

    auto AddedState = State;
    AddedState.SourceEntityId = 8;
    Model.Sections = {State, AddedState};
    const auto Added = Tracker.Observe(ck::ai_debugger::evidence::Normalize(Model), 4.0);
    TestEqual(TEXT("added fact emits once"), Added.Num(), 1);

    Model.Sections = {State};
    const auto Resolved = Tracker.Observe(ck::ai_debugger::evidence::Normalize(Model), 5.0);
    TestEqual(TEXT("resolved fact emits once"), Resolved.Num(), 1);
    TestEqual(TEXT("resolved fact is success toned"), Resolved[0].Tone, ECk_Tone::Ok);

    Tracker.Reset();
    TestEqual(TEXT("reset re-seeds without stale events"), Tracker.Observe(ck::ai_debugger::evidence::Normalize(Model), 6.0).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkAiDebuggerEvidence_TopologyExpandsStateMachineChain,
    "Ck.AiDebugger.Evidence.TopologyExpandsStateMachineChain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkAiDebuggerEvidence_TopologyExpandsStateMachineChain::RunTest(const FString&)
{
    // The provider emits one "> "-prefixed State row per nested sub-state-machine level. Each level
    // must surface as its own indented node, not as a fragment of the parent's detail string.
    FCk_DebugOverlay_EntityModel Model;
    auto Sm = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.StateMachine"), 42, 3, TEXT("Body_SM"));
    Sm.SourceDepth = 2;
    Sm.ParentSourceEntityId = 7;
    Sm.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.StateMachine.State"), TEXT("Awake"), ECk_DebugOverlay_Severity::Good));
    Sm.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.StateMachine.State"), TEXT("> Locomotion"), ECk_DebugOverlay_Severity::Good));
    Sm.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.StateMachine.State"), TEXT("> > Walking")));
    auto Trail = ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.StateMachine.History"), TEXT("(2 entries)"));
    Trail.ExplicitHistory = {FText::FromString(TEXT("Idle -> Awake")), FText::FromString(TEXT("Awake -> Locomotion"))};
    Sm.Rows.Add(Trail);
    Model.Sections = {Sm};

    const auto Topology = ck::ai_debugger::evidence::NormalizeTopology(Model);
    TestEqual(TEXT("every nested level becomes its own node"), Topology.StateMachines.Num(), 3);
    TestEqual(TEXT("top level keeps the source depth"), Topology.StateMachines[0].Depth, 2);
    TestEqual(TEXT("first nested level indents one further"), Topology.StateMachines[1].Depth, 3);
    TestEqual(TEXT("second nested level indents two further"), Topology.StateMachines[2].Depth, 4);
    TestEqual(TEXT("the current state is the node headline"), Topology.StateMachines[1].Headline, FString(TEXT("Locomotion")));
    TestFalse(TEXT("the chain prefix is consumed, not rendered"), Topology.StateMachines[2].Headline.Contains(TEXT(">")));
    TestEqual(TEXT("run status is the right-hand label"), Topology.StateMachines[0].Status, FString(TEXT("running")));
    TestEqual(TEXT("the transition trail hangs off the entity's own level"), Topology.StateMachines[0].Chain.Num(), 2);
    TestEqual(TEXT("nested levels do not repeat the trail"), Topology.StateMachines[1].Chain.Num(), 0);
    TestNotEqual(TEXT("levels of one source keep distinct keys"), Topology.StateMachines[0].StableKey, Topology.StateMachines[1].StableKey);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkAiDebuggerEvidence_TopologyNamesUnnamedSources,
    "Ck.AiDebugger.Evidence.TopologyNamesUnnamedSources", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkAiDebuggerEvidence_TopologyNamesUnnamedSources::RunTest(const FString&)
{
    // Sub-state-machine and sub-planner entities are never renamed, so they arrive carrying the
    // "NO NAME" sentinel. Two such siblings must still be distinguishable in the hierarchy.
    FCk_DebugOverlay_EntityModel Model;
    auto First = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.StateMachine"), 91, 0, TEXT("NO NAME"));
    First.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.StateMachine.State"), TEXT("Alive")));
    auto Second = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.StateMachine"), 92, 1, TEXT("NO NAME"));
    Second.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.StateMachine.State"), TEXT("Awake")));
    Model.Sections = {First, Second};

    const auto Topology = ck::ai_debugger::evidence::NormalizeTopology(Model);
    TestEqual(TEXT("both unnamed sources survive"), Topology.StateMachines.Num(), 2);
    TestFalse(TEXT("the placeholder name never reaches the row"), Topology.StateMachines[0].Name.Contains(TEXT("NO NAME")));
    TestTrue(TEXT("an unnamed source is identified by its entity"), Topology.StateMachines[0].Name.Contains(TEXT("#91")));
    TestNotEqual(TEXT("unnamed siblings are distinguishable"), Topology.StateMachines[0].Name, Topology.StateMachines[1].Name);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkAiDebuggerEvidence_TopologyKeepsFullPlan,
    "Ck.AiDebugger.Evidence.TopologyKeepsFullPlan", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkAiDebuggerEvidence_TopologyKeepsFullPlan::RunTest(const FString&)
{
    // The Plan row's VALUE is a capped focus-card summary; its explicit trail is the whole chain.
    // The hierarchy renders the plan, so it must read the trail and never the capped summary.
    FCk_DebugOverlay_EntityModel Model;
    auto Goap = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.Goap"), 20, 0, TEXT("Shop_GOAP"));
    Goap.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.Goap.Status"), TEXT("PlanFound"), ECk_DebugOverlay_Severity::Good));
    Goap.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.Goap.Active"), TEXT("PickGenreShelf")));
    Goap.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.Goap.Cost"), TEXT("4")));
    auto Plan = ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.Goap.Plan"), TEXT("A > B > C +2"));
    Plan.ExplicitHistory = {
        FText::FromString(TEXT("A")), FText::FromString(TEXT("B")), FText::FromString(TEXT("C")),
        FText::FromString(TEXT("D")), FText::FromString(TEXT("E"))};
    Goap.Rows.Add(Plan);
    Model.Sections = {Goap};

    const auto Topology = ck::ai_debugger::evidence::NormalizeTopology(Model);
    TestEqual(TEXT("one node per planner source"), Topology.Goaps.Num(), 1);
    TestEqual(TEXT("the whole plan survives the card's cap"), Topology.Goaps[0].Chain.Num(), 5);
    TestEqual(TEXT("the plan keeps its order"), Topology.Goaps[0].Chain.Last(), FString(TEXT("E")));
    TestEqual(TEXT("the executing action is the headline"), Topology.Goaps[0].Headline, FString(TEXT("PickGenreShelf")));
    TestTrue(TEXT("status carries the plan cost"), Topology.Goaps[0].Status.Contains(TEXT("PlanFound")) && Topology.Goaps[0].Status.Contains(TEXT("4")));
    TestEqual(TEXT("the chain is labelled for the reader"), Topology.Goaps[0].ChainLabel, FString(TEXT("Plan")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkAiDebuggerEvidence_TopologyIdlePlannerHasNoChain,
    "Ck.AiDebugger.Evidence.TopologyIdlePlannerHasNoChain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkAiDebuggerEvidence_TopologyIdlePlannerHasNoChain::RunTest(const FString&)
{
    // An idle planner is the noise the hierarchy exists to suppress: it must collapse to a single
    // line with no detail row at all.
    FCk_DebugOverlay_EntityModel Model;
    auto Goap = ck_ai_debugger_evidence_spec::Section(TEXT("Ck.OnScreenDebugger.Provider.Goap"), 30, 0, TEXT("Roam_GOAP"));
    Goap.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.Goap.Status"), TEXT("Idle")));
    Goap.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.Goap.Active"), TEXT("(none)")));
    Goap.Rows.Add(ck_ai_debugger_evidence_spec::Row(TEXT("Ck.OnScreenDebugger.Provider.Goap.Plan"), TEXT("(no plan)")));
    Model.Sections = {Goap};

    const auto Topology = ck::ai_debugger::evidence::NormalizeTopology(Model);
    TestEqual(TEXT("an idle planner has nothing to chain"), Topology.Goaps[0].Chain.Num(), 0);
    TestTrue(TEXT("an idle planner has no headline action"), Topology.Goaps[0].Headline.IsEmpty());
    TestEqual(TEXT("an idle planner still reports its status"), Topology.Goaps[0].Status, FString(TEXT("Idle")));
    return true;
}
#endif
