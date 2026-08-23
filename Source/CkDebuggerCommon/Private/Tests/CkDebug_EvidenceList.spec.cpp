#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkDebuggerCommon/Widgets/SCkDebug_EvidenceList.h"

namespace ck_debug_evidence_list_tests
{
    auto MakeItem(
        const TCHAR* InKey,
        const TCHAR* InHeadline,
        int32 InSelectionId = INDEX_NONE,
        int32 InIndentLevel = 0,
        const TCHAR* InRightLabel = TEXT("")) -> FCkDebug_EvidenceItem
    {
        auto Item = FCkDebug_EvidenceItem{};
        Item.Key = InKey;
        Item.Source = FText::FromString(TEXT("GOAP"));
        Item.Headline = FText::FromString(InHeadline);
        Item.Detail = FText::FromString(TEXT("Readable secondary evidence"));
        Item.IndentLevel = InIndentLevel;
        Item.RightLabel = FText::FromString(InRightLabel);
        Item.CopyText = FString::Printf(TEXT("copy:%s"), InKey);
        Item.SelectionId = InSelectionId;
        Item.Tone = ECk_Tone::Warn;
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugEvidenceList_InvalidKeysAreAtomic,
    "Ck.DebuggerCommon.EvidenceList.InvalidKeysAreAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugEvidenceList_InvalidKeysAreAtomic::RunTest(const FString&)
{
    auto Existing = TArray<ck::debug_evidence_list::FItemPtr>{};
    Existing.Add(MakeShared<FCkDebug_EvidenceItem>(ck_debug_evidence_list_tests::MakeItem(TEXT("kept"), TEXT("Original"))));
    auto Output = Existing;
    auto Error = FString{};
    const auto Accepted = ck::debug_evidence_list::Try_Reconcile(
        Existing,
        {ck_debug_evidence_list_tests::MakeItem(TEXT(""), TEXT("Bad"))},
        10,
        Output,
        Error);

    TestFalse(TEXT("Empty evidence key is rejected"), Accepted);
    TestEqual(TEXT("Rejected reconciliation leaves output membership unchanged"), Output.Num(), 1);
    TestTrue(TEXT("Rejected reconciliation leaves output pointer unchanged"), Output[0] == Existing[0]);
    TestEqual(TEXT("Rejected reconciliation leaves row values unchanged"), Existing[0]->Headline.ToString(), FString(TEXT("Original")));

    const auto DuplicateAccepted = ck::debug_evidence_list::Try_Reconcile(
        Existing,
        {ck_debug_evidence_list_tests::MakeItem(TEXT("duplicate"), TEXT("First")),
         ck_debug_evidence_list_tests::MakeItem(TEXT("duplicate"), TEXT("Second"))},
        10,
        Output,
        Error);
    TestFalse(TEXT("Duplicate evidence key is rejected"), DuplicateAccepted);
    TestTrue(TEXT("Duplicate rejection remains atomic"), Output[0] == Existing[0]);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugEvidenceList_ReconcilesStablePointersAndOrder,
    "Ck.DebuggerCommon.EvidenceList.ReconcilesStablePointersAndOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugEvidenceList_ReconcilesStablePointersAndOrder::RunTest(const FString&)
{
    auto Existing = TArray<ck::debug_evidence_list::FItemPtr>{};
    Existing.Add(MakeShared<FCkDebug_EvidenceItem>(ck_debug_evidence_list_tests::MakeItem(TEXT("a"), TEXT("Old A"))));
    Existing.Add(MakeShared<FCkDebug_EvidenceItem>(ck_debug_evidence_list_tests::MakeItem(TEXT("b"), TEXT("Old B"))));
    const auto OldB = Existing[1];
    auto Output = TArray<ck::debug_evidence_list::FItemPtr>{};
    auto Error = FString{};

    const auto Accepted = ck::debug_evidence_list::Try_Reconcile(
        Existing,
        {ck_debug_evidence_list_tests::MakeItem(TEXT("b"), TEXT("New B"), INDEX_NONE, 2, TEXT("ACTIVE")),
         ck_debug_evidence_list_tests::MakeItem(TEXT("c"), TEXT("New C"))},
        10,
        Output,
        Error);

    TestTrue(TEXT("Valid evidence update is accepted"), Accepted);
    TestEqual(TEXT("Caller order is preserved"), Output[0]->Key, FString(TEXT("b")));
    TestEqual(TEXT("Caller order includes new rows"), Output[1]->Key, FString(TEXT("c")));
    TestTrue(TEXT("Same key preserves pointer identity"), Output[0] == OldB);
    TestEqual(TEXT("Same key updates row data in place"), Output[0]->Headline.ToString(), FString(TEXT("New B")));
    TestEqual(TEXT("Same key updates topology indentation in place"), Output[0]->IndentLevel, 2);
    TestEqual(TEXT("Same key updates trailing status label in place"), Output[0]->RightLabel.ToString(), FString(TEXT("ACTIVE")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugEvidenceList_CapsAndPreservesCopySelectionData,
    "Ck.DebuggerCommon.EvidenceList.CapsAndPreservesCopySelectionData",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugEvidenceList_CapsAndPreservesCopySelectionData::RunTest(const FString&)
{
    auto Output = TArray<ck::debug_evidence_list::FItemPtr>{};
    auto Error = FString{};
    const auto Accepted = ck::debug_evidence_list::Try_Reconcile(
        {},
        {ck_debug_evidence_list_tests::MakeItem(TEXT("a"), TEXT("A")),
         ck_debug_evidence_list_tests::MakeItem(TEXT("b"), TEXT("B"), 42, 1, TEXT("SM")),
         ck_debug_evidence_list_tests::MakeItem(TEXT("c"), TEXT("C"))},
        2,
        Output,
        Error);

    TestTrue(TEXT("Capped evidence update is accepted"), Accepted);
    TestEqual(TEXT("Cap retains two newest caller entries"), Output.Num(), 2);
    TestEqual(TEXT("Cap retains caller order among retained entries"), Output[0]->Key, FString(TEXT("b")));
    TestEqual(TEXT("Selection-facing ID survives reconciliation"), Output[0]->SelectionId, 42);
    TestEqual(TEXT("Copy-facing payload survives reconciliation"), Output[0]->CopyText, FString(TEXT("copy:b")));
    TestEqual(TEXT("Topology indentation survives reconciliation"), Output[0]->IndentLevel, 1);
    TestEqual(TEXT("Trailing label survives reconciliation"), Output[0]->RightLabel.ToString(), FString(TEXT("SM")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugEvidenceList_ClearItems,
    "Ck.DebuggerCommon.EvidenceList.ClearItems",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugEvidenceList_ClearItems::RunTest(const FString&)
{
    const auto List = SNew(SCkDebug_EvidenceList).MaxItems(2);
    TestTrue(TEXT("Widget accepts valid current evidence"), List->Set_Items({
        ck_debug_evidence_list_tests::MakeItem(TEXT("a"), TEXT("A")),
        ck_debug_evidence_list_tests::MakeItem(TEXT("b"), TEXT("B"))}));
    TestEqual(TEXT("Widget exposes capped item count before clear"), List->Get_Items().Num(), 2);
    List->Clear_Items();
    TestEqual(TEXT("Clear removes all evidence rows"), List->Get_Items().Num(), 0);
    return true;
}

#endif
