#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_FocusCardBudget.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_Present.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"

// --------------------------------------------------------------------------------------------------------------------
// Test-only tags. UE_DEFINE_GAMEPLAY_TAG_STATIC registers at DLL load, before the runner
// fires; distinct local names keep this TU self-contained (see CkDebugOverlay_Resolve.spec.cpp).

UE_DEFINE_GAMEPLAY_TAG_STATIC(CardModel_Prov_Label, "Ck.OnScreenDebugger.Provider.Label")
UE_DEFINE_GAMEPLAY_TAG_STATIC(CardModel_Prov_Team,  "Ck.OnScreenDebugger.Provider.Team")
UE_DEFINE_GAMEPLAY_TAG_STATIC(CardModel_Prov_Timer, "Ck.OnScreenDebugger.Provider.Timer")
UE_DEFINE_GAMEPLAY_TAG_STATIC(CardModel_Field_Name, "Ck.OnScreenDebugger.Provider.Label.Name")
UE_DEFINE_GAMEPLAY_TAG_STATIC(CardModel_Field_Kind, "Ck.OnScreenDebugger.Provider.Label.Kind")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    auto CardModel_MakeRow(FGameplayTag InFieldTag, const TCHAR* InValue) -> FCk_DebugOverlay_Row
    {
        auto Row     = FCk_DebugOverlay_Row{};
        Row.FieldTag = InFieldTag;
        Row.Value    = FText::FromString(FString{ InValue });
        return Row;
    }

    // One section per SOURCE, as subtree aggregation emits them (same provider, distinct
    // SourceEntityId — the history bucket a surviving merged row must keep).
    auto CardModel_MakeSection(FGameplayTag InProviderTag, uint32 InSourceEntityId, int32 InSourceOrder)
        -> FCk_DebugOverlay_Section
    {
        auto Section           = FCk_DebugOverlay_Section{};
        Section.ProviderTag    = InProviderTag;
        Section.SourceEntityId = InSourceEntityId;
        Section.SourceOrder    = InSourceOrder;
        return Section;
    }

    auto CardModel_CountRows(const FCk_DebugOverlay_EntityModel& InModel) -> int32
    {
        auto Count = 0;
        for (const auto& Section : InModel.Sections)
        { Count += Section.Rows.Num(); }
        return Count;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_FocusCardModel_Test,
    "Ck.DebugOverlay.Presentation.FocusCardModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_FocusCardModel_Test::RunTest(const FString&)
{
    // (a) Rows identical by (FieldTag, Value) inside one provider collapse to one row.
    {
        FCk_DebugOverlay_EntityModel Model;
        for (auto Idx = 0; Idx < 3; ++Idx)
        {
            auto Section = CardModel_MakeSection(CardModel_Prov_Label, 100 + Idx, Idx);
            Section.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Guard")));
            Model.Sections.Add(Section);
        }

        const auto Merged = ck_debugoverlay::Prepare_FocusCardModel(Model, /*InMergeDuplicateRows=*/true);
        TestEqual(TEXT("emptied duplicate sections dropped"), Merged.Sections.Num(), 1);
        TestEqual(TEXT("one surviving row"),                  CardModel_CountRows(Merged), 1);
        TestEqual(TEXT("merged count counts every duplicate"), Merged.Sections[0].Rows[0].MergedCount, 3);
        // The survivor must stay in the FIRST section so its history bucket is unchanged.
        TestEqual(TEXT("survivor keeps its original history bucket"),
            static_cast<int32>(Merged.Sections[0].SourceEntityId), 100);
    }

    // (b) Differing FieldTag or Value must NOT merge.
    {
        FCk_DebugOverlay_EntityModel Model;

        auto SectionA = CardModel_MakeSection(CardModel_Prov_Label, 200, 0);
        SectionA.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Guard")));
        Model.Sections.Add(SectionA);

        auto SectionB = CardModel_MakeSection(CardModel_Prov_Label, 201, 1);
        SectionB.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Sentry")));   // same field, other value
        Model.Sections.Add(SectionB);

        auto SectionC = CardModel_MakeSection(CardModel_Prov_Label, 202, 2);
        SectionC.Rows.Add(CardModel_MakeRow(CardModel_Field_Kind, TEXT("Guard")));    // same value, other field
        Model.Sections.Add(SectionC);

        // Same (field, value) as SectionA but a DIFFERENT provider — merging is per-provider.
        auto SectionD = CardModel_MakeSection(CardModel_Prov_Team, 203, 3);
        SectionD.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Guard")));
        Model.Sections.Add(SectionD);

        const auto Merged = ck_debugoverlay::Prepare_FocusCardModel(Model, /*InMergeDuplicateRows=*/true);
        TestEqual(TEXT("nothing merges across field/value/provider"), CardModel_CountRows(Merged), 4);
        TestEqual(TEXT("all sections survive"),                       Merged.Sections.Num(), 4);
        for (const auto& Section : Merged.Sections)
        { TestEqual(TEXT("unmerged rows report a count of 1"), Section.Rows[0].MergedCount, 1); }
    }

    // (c) Merge gate off — duplicates stay as-is (the strip still runs).
    {
        FCk_DebugOverlay_EntityModel Model;
        for (auto Idx = 0; Idx < 3; ++Idx)
        {
            auto Section = CardModel_MakeSection(CardModel_Prov_Label, 300 + Idx, Idx);
            Section.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Guard")));
            Model.Sections.Add(Section);
        }

        const auto Unmerged = ck_debugoverlay::Prepare_FocusCardModel(Model, /*InMergeDuplicateRows=*/false);
        TestEqual(TEXT("every source keeps its section"), Unmerged.Sections.Num(), 3);
        TestEqual(TEXT("every duplicate row is kept"),    CardModel_CountRows(Unmerged), 3);
        TestEqual(TEXT("no merge count applied"),         Unmerged.Sections[0].Rows[0].MergedCount, 1);
    }

    // (d) Value-less rows are stripped BEFORE the budget, so budget slots go to real rows.
    //     Without the strip the two placeholders below would eat two of the three slots and
    //     displace the whole second section.
    {
        FCk_DebugOverlay_EntityModel Model;

        auto SectionA         = CardModel_MakeSection(CardModel_Prov_Label, 400, 0);
        SectionA.SortPriority = 1;
        SectionA.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("")));
        SectionA.Rows.Add(CardModel_MakeRow(CardModel_Field_Kind, TEXT("")));
        SectionA.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Guard")));
        Model.Sections.Add(SectionA);

        auto SectionB         = CardModel_MakeSection(CardModel_Prov_Timer, 401, 0);
        SectionB.SortPriority = 2;
        SectionB.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("2.5s")));
        SectionB.Rows.Add(CardModel_MakeRow(CardModel_Field_Kind, TEXT("Cooldown")));
        Model.Sections.Add(SectionB);

        const auto Prepared = ck_debugoverlay::Prepare_FocusCardModel(Model, /*InMergeDuplicateRows=*/true);
        TestEqual(TEXT("placeholders never reach the budget"), CardModel_CountRows(Prepared), 3);

        // A row carrying only explicit history still renders, so it must survive the strip.
        {
            auto WithHistory = FCk_DebugOverlay_Row{};
            WithHistory.FieldTag = CardModel_Field_Kind;
            WithHistory.ExplicitHistory.Add(FText::FromString(TEXT("Idle")));

            FCk_DebugOverlay_EntityModel HistoryModel;
            auto HistorySection = CardModel_MakeSection(CardModel_Prov_Label, 410, 0);
            HistorySection.Rows.Add(WithHistory);
            HistoryModel.Sections.Add(HistorySection);

            const auto PreparedHistory = ck_debugoverlay::Prepare_FocusCardModel(HistoryModel, true);
            TestEqual(TEXT("history-only row survives the strip"), CardModel_CountRows(PreparedHistory), 1);
        }

        const auto Budgeted = ck_debugoverlay::Apply_FocusCardBudget(Prepared, { 4, 3 });
        TestEqual(TEXT("budget spends all three slots on real rows"), CardModel_CountRows(Budgeted), 3);
        for (const auto& Section : Budgeted.Sections)
        {
            for (const auto& Row : Section.Rows)
            { TestFalse(TEXT("no value-less row survives into the card"), Row.Value.IsEmpty()); }
        }
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_FocusCardLegend_Test,
    "Ck.DebugOverlay.Presentation.FocusCardLegend",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_FocusCardLegend_Test::RunTest(const FString&)
{
    // (e) One legend entry per PROVIDER, however many sections it contributed.
    auto Sections = TArray<FCk_DebugOverlay_Section>{};

    for (auto Idx = 0; Idx < 3; ++Idx)
    {
        auto Section = CardModel_MakeSection(CardModel_Prov_Label, 500 + Idx, Idx);
        Section.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Guard")));
        Sections.Add(Section);
    }

    auto TeamSection = CardModel_MakeSection(CardModel_Prov_Team, 510, 0);
    TeamSection.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Red")));
    Sections.Add(TeamSection);

    // Draws nothing (no rows, no omission summary) — must not claim a legend slot.
    Sections.Add(CardModel_MakeSection(CardModel_Prov_Timer, 520, 0));

    const auto Entries = SCkDebugOverlay_FocusCard::Build_LegendEntries(Sections);

    TestEqual(TEXT("one entry per rendered provider"), Entries.Num(), 2);
    TestEqual(TEXT("first entry is the first rendered provider"), Entries[0].FullName, FString(TEXT("Label")));
    TestEqual(TEXT("repeated provider counted, not repeated"),    Entries[0].SectionCount, 3);
    TestEqual(TEXT("single-section provider counts once"),        Entries[1].SectionCount, 1);

    for (const auto& Entry : Entries)
    { TestFalse(TEXT("legend abbrev is never empty"), Entry.Abbrev.IsEmpty()); }

    // (f) LegendMode::PerSection — one entry per RENDERED SECTION, no dedup, but the same
    // draws-nothing skip. Fed the identical section list so the two modes stay comparable.
    const auto PerSection = SCkDebugOverlay_FocusCard::Build_LegendEntries_PerSection(Sections);

    TestEqual(TEXT("per-section legend lists every rendered section"), PerSection.Num(), 4);
    TestEqual(TEXT("per-section legend skips the section that draws nothing"),
        PerSection.Num(), Sections.Num() - 1);

    for (const auto& Entry : PerSection)
    {
        TestFalse(TEXT("per-section abbrev is never empty"), Entry.Abbrev.IsEmpty());
        // Nothing is aggregated in this mode, so the xN annotation must never render.
        TestEqual(TEXT("per-section entries never aggregate"), Entry.SectionCount, 1);
    }

    TestEqual(TEXT("per-section keeps section order"), PerSection[0].FullName, FString(TEXT("Label")));
    TestEqual(TEXT("per-section repeats the repeated provider"), PerSection[2].FullName, FString(TEXT("Label")));
    TestEqual(TEXT("per-section keeps the single-section provider last"),
        PerSection[3].FullName, FString(TEXT("Team")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
