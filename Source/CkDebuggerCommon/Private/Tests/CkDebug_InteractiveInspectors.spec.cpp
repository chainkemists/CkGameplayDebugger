#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------
// The foundation the interactive inspector rows stand on, tested where it is pure: the edit guard's
// defer-don't-drop bookkeeping, the request gate's net-mode truth table, and the numeric editor's
// parse/format/clamp/commit-type policy. Composing the actual Slate rows stays [EDITOR-VERIFY].
// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugInspector_EditGuardDefersRebuild,
    "Ck.DebuggerCommon.Inspector.EditGuardDefersRebuild",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugInspector_EditGuardDefersRebuild::RunTest(const FString& Parameters)
{
    auto Guard = MakeShared<FCkInspectorEditGuard>();

    TestFalse(TEXT("a fresh guard has no active edit"), Guard->Get_HasActiveEdit());
    TestFalse(TEXT("a fresh guard has nothing pending"), Guard->Get_HasPendingRebuild());
    TestFalse(TEXT("consuming nothing yields nothing"), Guard->Consume_PendingRebuild());

    // With no edit in flight, a request is answered immediately and exactly once.
    Guard->Request_Rebuild();
    TestTrue(TEXT("an unblocked request is consumable"), Guard->Consume_PendingRebuild());
    TestFalse(TEXT("a consumed request does not come back"), Guard->Consume_PendingRebuild());

    // The contract: while an edit is in flight the request is held, NOT dropped.
    {
        auto Scope = FCkInspectorEditScope{Guard};
        Scope.Set_Active(true);

        TestTrue(TEXT("an active scope is an active edit"), Guard->Get_HasActiveEdit());

        Guard->Request_Rebuild();

        TestFalse(TEXT("a rebuild is not performed while an edit is in flight"), Guard->Consume_PendingRebuild());
        TestTrue(TEXT("...and the request is still pending — deferred, not dropped"), Guard->Get_HasPendingRebuild());

        // Repeated requests during the edit coalesce into the one pending rebuild.
        Guard->Request_Rebuild();
        Guard->Request_Rebuild();
        TestFalse(TEXT("still deferred after further requests"), Guard->Consume_PendingRebuild());

        Scope.Set_Active(false);

        TestFalse(TEXT("ending the edit clears the active state"), Guard->Get_HasActiveEdit());
        TestTrue(TEXT("the deferred rebuild fires once the edit ends"), Guard->Consume_PendingRebuild());
        TestFalse(TEXT("and only once"), Guard->Consume_PendingRebuild());
    }

    // Two rows editing at once: the last one to finish releases the rebuild.
    // TSharedPtr, not the TSharedRef MakeShared hands back — the point of the second scope here is
    // that it can be DROPPED, which is what a widget torn down mid-type does.
    {
        const TSharedPtr<FCkInspectorEditScope> First  = MakeShared<FCkInspectorEditScope>(Guard);
        TSharedPtr<FCkInspectorEditScope>       Second = MakeShared<FCkInspectorEditScope>(Guard);

        First->Set_Active(true);
        Second->Set_Active(true);
        TestEqual(TEXT("two concurrent edits are counted separately"), Guard->Get_ActiveEditCount(), 2);

        Guard->Request_Rebuild();

        First->Set_Active(false);
        TestFalse(TEXT("one edit ending is not enough"), Guard->Consume_PendingRebuild());

        // Destruction is a release: a widget torn down mid-type must not wedge the guard.
        Second.Reset();
        TestFalse(TEXT("a destroyed scope releases its edit"), Guard->Get_HasActiveEdit());
        TestTrue(TEXT("the deferred rebuild survives the teardown"), Guard->Consume_PendingRebuild());
    }

    // Idempotence: a double-begin / double-end from quirky focus events cannot wedge the guard.
    {
        auto Scope = FCkInspectorEditScope{Guard};
        Scope.Set_Active(true);
        Scope.Set_Active(true);
        TestEqual(TEXT("a double-begin is one edit"), Guard->Get_ActiveEditCount(), 1);

        Scope.Set_Active(false);
        Scope.Set_Active(false);
        TestEqual(TEXT("a double-end leaves no edit"), Guard->Get_ActiveEditCount(), 0);
    }

    // Clear_AllEdits is the panel's rebuild-time reset; a scope released afterwards is inert.
    {
        TSharedPtr<FCkInspectorEditScope> Scope = MakeShared<FCkInspectorEditScope>(Guard);
        Scope->Set_Active(true);

        Guard->Clear_AllEdits();
        TestFalse(TEXT("clearing drops every active edit"), Guard->Get_HasActiveEdit());

        auto Other = FCkInspectorEditScope{Guard};
        Other.Set_Active(true);

        Scope.Reset();
        TestTrue(TEXT("a stale release cannot cancel somebody else's edit"), Guard->Get_HasActiveEdit());
    }

    // A null guard is a valid, inert scope — builders outside an inspector panel need no wiring.
    {
        auto Orphan = FCkInspectorEditScope{nullptr};
        Orphan.Set_Active(true);
        TestTrue(TEXT("a guardless scope still tracks its own state"), Orphan.Get_IsActive());
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugInspector_RequestGateTruthTable,
    "Ck.DebuggerCommon.Inspector.RequestGateTruthTable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugInspector_RequestGateTruthTable::RunTest(const FString& Parameters)
{
    const auto Evaluate = [](TOptional<ENetMode> InNetMode, ECk_DebugRequest_Requirement InRequirement)
    {
        return ck::DebugRequestGate::Evaluate(InNetMode, InRequirement);
    };

    const auto NetModes = TArray<ENetMode>{NM_Standalone, NM_ListenServer, NM_DedicatedServer, NM_Client};
    const auto Requirements = TArray<ECk_DebugRequest_Requirement>{
        ECk_DebugRequest_Requirement::AuthorityOnly,
        ECk_DebugRequest_Requirement::CosmeticOnly,
        ECk_DebugRequest_Requirement::LocalOk};

    // AuthorityOnly: everything but a pure client has authority.
    for (const auto NetMode : NetModes)
    {
        const auto Verdict = Evaluate(NetMode, ECk_DebugRequest_Requirement::AuthorityOnly);

        TestTrue(
            *ck::Format_UE(TEXT("AuthorityOnly enabled-ness for net mode {}"), static_cast<int32>(NetMode)),
            Verdict.IsEnabled == (NetMode != NM_Client));
    }

    // CosmeticOnly: only a dedicated server has nothing to play it on.
    for (const auto NetMode : NetModes)
    {
        const auto Verdict = Evaluate(NetMode, ECk_DebugRequest_Requirement::CosmeticOnly);

        TestTrue(
            *ck::Format_UE(TEXT("CosmeticOnly enabled-ness for net mode {}"), static_cast<int32>(NetMode)),
            Verdict.IsEnabled == (NetMode != NM_DedicatedServer));
    }

    // LocalOk never blocks, but warns where a server projection will stomp the local write.
    for (const auto NetMode : NetModes)
    {
        const auto Verdict = Evaluate(NetMode, ECk_DebugRequest_Requirement::LocalOk);

        TestTrue(
            *ck::Format_UE(TEXT("LocalOk stays enabled for net mode {}"), static_cast<int32>(NetMode)),
            Verdict.IsEnabled);

        TestTrue(
            *ck::Format_UE(TEXT("LocalOk advisory presence for net mode {}"), static_cast<int32>(NetMode)),
            (NOT Verdict.Reason.IsEmpty()) == (NetMode == NM_Client));
    }

    // A blocked control MUST explain itself — a greyed button with no reason is the failure mode
    // this helper exists to prevent.
    for (const auto NetMode : NetModes)
    {
        for (const auto Requirement : Requirements)
        {
            const auto Verdict = Evaluate(NetMode, Requirement);

            if (Verdict.IsEnabled)
            { continue; }

            TestFalse(
                *ck::Format_UE(TEXT("a disabled verdict carries a reason (net mode {}, requirement {})"),
                    static_cast<int32>(NetMode), static_cast<int32>(Requirement)),
                Verdict.Reason.IsEmpty());
        }
    }

    // No world: nothing can be said about authority, so the two net-sensitive requirements refuse.
    TestFalse(TEXT("AuthorityOnly refuses without a world"),
        Evaluate({}, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled);

    TestFalse(TEXT("CosmeticOnly refuses without a world"),
        Evaluate({}, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled);

    TestTrue(TEXT("LocalOk survives without a world"),
        Evaluate({}, ECk_DebugRequest_Requirement::LocalOk).IsEnabled);

    // An invalid handle has no world, so the handle overload lands on exactly those rows.
    const auto InvalidHandle = FCk_Handle{};

    TestFalse(TEXT("an invalid handle gates AuthorityOnly off"),
        ck::DebugRequestGate::Evaluate(InvalidHandle, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled);

    TestFalse(TEXT("an invalid handle has no net mode"),
        ck::DebugRequestGate::Get_NetMode(InvalidHandle).IsSet());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugInspector_NumericEditorValuePolicy,
    "Ck.DebuggerCommon.Inspector.NumericEditorValuePolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugInspector_NumericEditorValuePolicy::RunTest(const FString& Parameters)
{
    using FEditor = SCkDebug_NumericEditor;

    // Commit policy — the reason this widget exists instead of a raw SEditableTextBox.
    TestTrue(TEXT("enter commits"),            FEditor::Should_Commit(ETextCommit::OnEnter));
    TestTrue(TEXT("moving focus away commits"), FEditor::Should_Commit(ETextCommit::OnUserMovedFocus));
    TestFalse(TEXT("a keystroke does not commit"), FEditor::Should_Commit(ETextCommit::Default));
    TestFalse(TEXT("escape does not commit"),      FEditor::Should_Commit(ETextCommit::OnCleared));

    // Formatting.
    TestEqual(TEXT("float formatting honours the digit count"),
        FEditor::Format_Value(1.23456, ECkDebug_NumericKind::Float, 2), FString{TEXT("1.23")});

    TestEqual(TEXT("integer formatting has no fractional part"),
        FEditor::Format_Value(1.6, ECkDebug_NumericKind::Integer, 2), FString{TEXT("2")});

    TestEqual(TEXT("integer formatting rounds negatives away from zero"),
        FEditor::Format_Value(-1.6, ECkDebug_NumericKind::Integer, 2), FString{TEXT("-2")});

    // Parsing. Non-numeric text is a typo, not an instruction to zero the value.
    TestFalse(TEXT("empty text parses to nothing"),
        FEditor::Parse_Value(FString{}, ECkDebug_NumericKind::Float).IsSet());

    TestFalse(TEXT("whitespace parses to nothing"),
        FEditor::Parse_Value(TEXT("   "), ECkDebug_NumericKind::Float).IsSet());

    TestFalse(TEXT("garbage parses to nothing rather than 0"),
        FEditor::Parse_Value(TEXT("twelve"), ECkDebug_NumericKind::Float).IsSet());

    const auto Parsed = FEditor::Parse_Value(TEXT(" -4.5 "), ECkDebug_NumericKind::Float);
    TestTrue(TEXT("a padded negative parses"), Parsed.IsSet());
    TestEqual(TEXT("...to its value"), Parsed.Get(0.0), -4.5, 0.0001);

    const auto ParsedInt = FEditor::Parse_Value(TEXT("4.7"), ECkDebug_NumericKind::Integer);
    TestTrue(TEXT("an integer field accepts a typed decimal"), ParsedInt.IsSet());
    TestEqual(TEXT("...and rounds it"), ParsedInt.Get(0.0), 5.0, 0.0001);

    // Clamping applies to the COMMITTED value only.
    const auto Min = TOptional<double>{0.0};
    const auto Max = TOptional<double>{10.0};

    TestEqual(TEXT("below the floor clamps up"),
        FEditor::Clamp_Value(-3.0, Min, Max, ECkDebug_NumericKind::Float), 0.0, 0.0001);

    TestEqual(TEXT("above the ceiling clamps down"),
        FEditor::Clamp_Value(31.0, Min, Max, ECkDebug_NumericKind::Float), 10.0, 0.0001);

    TestEqual(TEXT("in range passes through"),
        FEditor::Clamp_Value(4.25, Min, Max, ECkDebug_NumericKind::Float), 4.25, 0.0001);

    TestEqual(TEXT("an unbounded value is untouched"),
        FEditor::Clamp_Value(-9999.0, TOptional<double>{}, TOptional<double>{}, ECkDebug_NumericKind::Float),
        -9999.0, 0.0001);

    TestEqual(TEXT("a floor alone still applies"),
        FEditor::Clamp_Value(-1.0, Min, TOptional<double>{}, ECkDebug_NumericKind::Float), 0.0, 0.0001);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
