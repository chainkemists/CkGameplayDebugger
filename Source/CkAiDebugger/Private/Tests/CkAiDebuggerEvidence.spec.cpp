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
#endif
