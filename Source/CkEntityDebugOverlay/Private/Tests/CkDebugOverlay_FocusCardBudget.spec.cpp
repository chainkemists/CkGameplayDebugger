#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_FocusCardBudget.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

UE_DEFINE_GAMEPLAY_TAG_STATIC(Budget_Goap, "Ck.OnScreenDebugger.Provider.Goap")
UE_DEFINE_GAMEPLAY_TAG_STATIC(Budget_Attr, "Ck.OnScreenDebugger.Provider.FloatAttributes")
UE_DEFINE_GAMEPLAY_TAG_STATIC(Budget_Other, "Ck.OnScreenDebugger.Provider.Other")

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_FocusCardBudget_Test,
    "Ck.DebugOverlay.Presentation.FocusCardBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_FocusCardBudget_Test::RunTest(const FString&)
{
    auto MakeSection = [](FGameplayTag Tag, int32 Priority, int32 RowCount)
    {
        FCk_DebugOverlay_Section Section; Section.ProviderTag = Tag; Section.SortPriority = Priority;
        for (int32 Index = 0; Index < RowCount; ++Index) { Section.Rows.Add({}); }
        return Section;
    };
    FCk_DebugOverlay_EntityModel Model;
    Model.Sections.Add(MakeSection(Budget_Attr, 1, 8));
    Model.Sections.Add(MakeSection(Budget_Goap, 50, 3));
    Model.Sections.Add(MakeSection(Budget_Other, 2, 3));

    const auto Result = ck_debugoverlay::Apply_FocusCardBudget(Model, { 2, 5 });
    TestEqual(TEXT("all budgeted sections retained for explicit summaries"), Result.Sections.Num(), 3);
    TestTrue(TEXT("GOAP promoted ahead of generic attributes"), Result.Sections[0].ProviderTag == Budget_Goap);
    TestEqual(TEXT("GOAP section cap"), Result.Sections[0].Rows.Num(), 2);
    TestEqual(TEXT("GOAP omission is explicit"), Result.Sections[0].OmittedRowCount, 1);
    TestEqual(TEXT("total cap respected"), Result.Sections[0].Rows.Num() + Result.Sections[1].Rows.Num() + Result.Sections[2].Rows.Num(), 5);
    TestTrue(TEXT("attribute flood reports omission"), Result.Sections[2].OmittedRowCount > 0);

    const auto Exhausted = ck_debugoverlay::Apply_FocusCardBudget(Model, { 2, 0 });
    TestTrue(TEXT("exhausted budget does not retain omission-only sections"), Exhausted.Sections.IsEmpty());
    return true;
}

#endif
