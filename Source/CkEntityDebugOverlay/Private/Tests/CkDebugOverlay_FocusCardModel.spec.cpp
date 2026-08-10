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

    auto CardModel_MakeRow(FGameplayTag InFieldTag, const TCHAR* InValue, ECk_DebugOverlay_Severity InSeverity)
        -> FCk_DebugOverlay_Row
    {
        auto Row     = CardModel_MakeRow(InFieldTag, InValue);
        Row.Severity = InSeverity;
        return Row;
    }

    // A row whose trail is the evidence — identical values with different trails are different
    // facts, so these must survive both merge passes.
    auto CardModel_MakeHistoryRow(FGameplayTag InFieldTag, const TCHAR* InValue, const TCHAR* InTrail)
        -> FCk_DebugOverlay_Row
    {
        auto Row = CardModel_MakeRow(InFieldTag, InValue);
        Row.ExplicitHistory.Add(FText::FromString(FString{ InTrail }));
        return Row;
    }

    // One section per SOURCE, as subtree aggregation emits them (same provider, distinct
    // SourceEntityId — the history bucket a surviving merged row must keep). MergeBehavior is
    // stamped from the provider in Build_EntityModel; specs hand-set it.
    auto CardModel_MakeSection(
        FGameplayTag                   InProviderTag,
        uint32                         InSourceEntityId,
        int32                          InSourceOrder,
        ECk_DebugOverlay_MergeBehavior InMergeBehavior = ECk_DebugOverlay_MergeBehavior::MergeWithinSource)
        -> FCk_DebugOverlay_Section
    {
        auto Section           = FCk_DebugOverlay_Section{};
        Section.ProviderTag    = InProviderTag;
        Section.SourceEntityId = InSourceEntityId;
        Section.SourceOrder    = InSourceOrder;
        Section.MergeBehavior  = InMergeBehavior;
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
    // (a) MergeAcrossSources: rows identical by (FieldTag, Value) inside one provider collapse
    //     to one row even when they came from different sub-entities (Label / Team semantics).
    {
        FCk_DebugOverlay_EntityModel Model;
        for (auto Idx = 0; Idx < 3; ++Idx)
        {
            auto Section = CardModel_MakeSection(CardModel_Prov_Label, 100 + Idx, Idx,
                ECk_DebugOverlay_MergeBehavior::MergeAcrossSources);
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

    // (a1) MergeWithinSource is the default and the whole point of the policy: identical rows
    //      on DIFFERENT sources are distinct facts and every one of them keeps rendering.
    {
        FCk_DebugOverlay_EntityModel Model;
        for (auto Idx = 0; Idx < 3; ++Idx)
        {
            auto Section = CardModel_MakeSection(CardModel_Prov_Timer, 110 + Idx, Idx);
            Section.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("2.5s")));
            Model.Sections.Add(Section);
        }

        const auto Merged = ck_debugoverlay::Prepare_FocusCardModel(Model, /*InMergeDuplicateRows=*/true);
        TestEqual(TEXT("every source keeps its own section"), Merged.Sections.Num(), 3);
        TestEqual(TEXT("no cross-source row is lost"),        CardModel_CountRows(Merged), 3);
        for (const auto& Section : Merged.Sections)
        { TestEqual(TEXT("cross-source rows never carry a merge count"), Section.Rows[0].MergedCount, 1); }
    }

    // (a2) …but duplicates INSIDE one source are genuine redundancy and collapse under every
    //      behavior, including the MergeWithinSource default.
    {
        FCk_DebugOverlay_EntityModel Model;

        auto Section = CardModel_MakeSection(CardModel_Prov_Timer, 120, 0);
        Section.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("2.5s")));
        Section.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("2.5s")));
        Section.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("2.5s")));
        Section.Rows.Add(CardModel_MakeRow(CardModel_Field_Kind, TEXT("Cooldown")));
        Model.Sections.Add(Section);

        const auto Merged = ck_debugoverlay::Prepare_FocusCardModel(Model, /*InMergeDuplicateRows=*/true);
        TestEqual(TEXT("within-source duplicates collapse"), CardModel_CountRows(Merged), 2);
        TestEqual(TEXT("survivor counts every duplicate"),   Merged.Sections[0].Rows[0].MergedCount, 3);
        TestEqual(TEXT("the distinct row is untouched"),     Merged.Sections[0].Rows[1].MergedCount, 1);
    }

    // (a3) A merge must never quieten the card: the survivor keeps the loudest severity, in
    //      both the within-section and the cross-section pass.
    {
        FCk_DebugOverlay_EntityModel Model;

        auto WithinSection = CardModel_MakeSection(CardModel_Prov_Timer, 130, 0);
        WithinSection.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("2.5s"), ECk_DebugOverlay_Severity::Normal));
        WithinSection.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("2.5s"), ECk_DebugOverlay_Severity::Bad));
        Model.Sections.Add(WithinSection);

        auto AcrossA = CardModel_MakeSection(CardModel_Prov_Label, 131, 0,
            ECk_DebugOverlay_MergeBehavior::MergeAcrossSources);
        AcrossA.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Guard"), ECk_DebugOverlay_Severity::Good));
        Model.Sections.Add(AcrossA);

        auto AcrossB = CardModel_MakeSection(CardModel_Prov_Label, 132, 1,
            ECk_DebugOverlay_MergeBehavior::MergeAcrossSources);
        AcrossB.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Guard"), ECk_DebugOverlay_Severity::Warn));
        Model.Sections.Add(AcrossB);

        const auto Merged = ck_debugoverlay::Prepare_FocusCardModel(Model, /*InMergeDuplicateRows=*/true);
        TestEqual(TEXT("both merges leave one row each"), CardModel_CountRows(Merged), 2);
        TestEqual(TEXT("within-section merge keeps the loudest severity"),
            static_cast<int32>(Merged.Sections[0].Rows[0].Severity),
            static_cast<int32>(ECk_DebugOverlay_Severity::Bad));
        TestEqual(TEXT("cross-section merge keeps the loudest severity"),
            static_cast<int32>(Merged.Sections[1].Rows[0].Severity),
            static_cast<int32>(ECk_DebugOverlay_Severity::Warn));
    }

    // (a4) Rows carrying a trail never merge — identical values with different histories are
    //      different facts, and only the survivor's trail would have rendered.
    {
        FCk_DebugOverlay_EntityModel Model;

        // Same section, same (field, value), different trails.
        auto WithinSection = CardModel_MakeSection(CardModel_Prov_Timer, 140, 0);
        WithinSection.Rows.Add(CardModel_MakeHistoryRow(CardModel_Field_Name, TEXT("(2 entries)"), TEXT("Idle -> Chase")));
        WithinSection.Rows.Add(CardModel_MakeHistoryRow(CardModel_Field_Name, TEXT("(2 entries)"), TEXT("Idle -> Flee")));
        Model.Sections.Add(WithinSection);

        // Different sections of a MergeAcrossSources provider — the trail still wins.
        auto AcrossA = CardModel_MakeSection(CardModel_Prov_Label, 141, 0,
            ECk_DebugOverlay_MergeBehavior::MergeAcrossSources);
        AcrossA.Rows.Add(CardModel_MakeHistoryRow(CardModel_Field_Kind, TEXT("(2 entries)"), TEXT("A -> B")));
        Model.Sections.Add(AcrossA);

        auto AcrossB = CardModel_MakeSection(CardModel_Prov_Label, 142, 1,
            ECk_DebugOverlay_MergeBehavior::MergeAcrossSources);
        AcrossB.Rows.Add(CardModel_MakeHistoryRow(CardModel_Field_Kind, TEXT("(2 entries)"), TEXT("C -> D")));
        Model.Sections.Add(AcrossB);

        const auto Merged = ck_debugoverlay::Prepare_FocusCardModel(Model, /*InMergeDuplicateRows=*/true);
        TestEqual(TEXT("no trail-carrying row is merged away"), CardModel_CountRows(Merged), 4);
        TestEqual(TEXT("every trail-carrying section survives"), Merged.Sections.Num(), 3);
        for (const auto& Section : Merged.Sections)
        {
            for (const auto& Row : Section.Rows)
            { TestEqual(TEXT("trail rows never carry a merge count"), Row.MergedCount, 1); }
        }
    }

    // (b) Differing FieldTag or Value must NOT merge, even under MergeAcrossSources.
    {
        FCk_DebugOverlay_EntityModel Model;

        auto SectionA = CardModel_MakeSection(CardModel_Prov_Label, 200, 0,
            ECk_DebugOverlay_MergeBehavior::MergeAcrossSources);
        SectionA.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Guard")));
        Model.Sections.Add(SectionA);

        auto SectionB = CardModel_MakeSection(CardModel_Prov_Label, 201, 1,
            ECk_DebugOverlay_MergeBehavior::MergeAcrossSources);
        SectionB.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Sentry")));   // same field, other value
        Model.Sections.Add(SectionB);

        auto SectionC = CardModel_MakeSection(CardModel_Prov_Label, 202, 2,
            ECk_DebugOverlay_MergeBehavior::MergeAcrossSources);
        SectionC.Rows.Add(CardModel_MakeRow(CardModel_Field_Kind, TEXT("Guard")));    // same value, other field
        Model.Sections.Add(SectionC);

        // Same (field, value) as SectionA but a DIFFERENT provider — merging is per-provider.
        auto SectionD = CardModel_MakeSection(CardModel_Prov_Team, 203, 3,
            ECk_DebugOverlay_MergeBehavior::MergeAcrossSources);
        SectionD.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Guard")));
        Model.Sections.Add(SectionD);

        const auto Merged = ck_debugoverlay::Prepare_FocusCardModel(Model, /*InMergeDuplicateRows=*/true);
        TestEqual(TEXT("nothing merges across field/value/provider"), CardModel_CountRows(Merged), 4);
        TestEqual(TEXT("all sections survive"),                       Merged.Sections.Num(), 4);
        for (const auto& Section : Merged.Sections)
        { TestEqual(TEXT("unmerged rows report a count of 1"), Section.Rows[0].MergedCount, 1); }
    }

    // (c) Merge gate off — duplicates stay as-is (the strip still runs). The sections declare
    //     MergeAcrossSources, so the gate is the ONLY reason they survive.
    {
        FCk_DebugOverlay_EntityModel Model;
        for (auto Idx = 0; Idx < 3; ++Idx)
        {
            auto Section = CardModel_MakeSection(CardModel_Prov_Label, 300 + Idx, Idx,
                ECk_DebugOverlay_MergeBehavior::MergeAcrossSources);
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

    // (g) CondensePerSource, focus section present: it is the provider's FIRST section, so it
    //     stays full and every section after it collapses into ONE condensed section holding a
    //     summary row per source. Every other behavior passes straight through.
    {
        auto Sections = TArray<FCk_DebugOverlay_Section>{};

        // Focus section (SourceOrder 0, and first in order) — full detail, must survive verbatim.
        auto Focus = CardModel_MakeSection(CardModel_Prov_Label, 900, 0,
            ECk_DebugOverlay_MergeBehavior::CondensePerSource);
        Focus.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Chase")));
        Focus.Rows.Add(CardModel_MakeRow(CardModel_Field_Kind, TEXT("(4 entries)")));
        Sections.Add(Focus);

        // Three sub-planners / sub-SMs, discovered after the focus entity.
        for (auto Idx = 1; Idx <= 3; ++Idx)
        {
            auto Sub = CardModel_MakeSection(CardModel_Prov_Label, 900 + Idx, Idx,
                ECk_DebugOverlay_MergeBehavior::CondensePerSource);
            Sub.SourceName = FText::FromString(FString{ TEXT("Sub") } + FString::FromInt(Idx));
            Sub.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Idle")));
            Sub.Rows.Add(CardModel_MakeRow(CardModel_Field_Kind, TEXT("(0 entries)")));
            Sections.Add(Sub);
        }

        // A sub-source section that collected nothing contributes no line.
        Sections.Add(CardModel_MakeSection(CardModel_Prov_Label, 904, 4,
            ECk_DebugOverlay_MergeBehavior::CondensePerSource));

        // Another provider's sub-source section — a different behavior, so untouched.
        auto Other = CardModel_MakeSection(CardModel_Prov_Timer, 950, 1);
        Other.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("2.5s")));
        Sections.Add(Other);

        const auto Condensed = ck_debugoverlay::Condense_ProviderSections(Sections, /*InFocusEntityId=*/900,
            [](const FGameplayTag&                 /*InProviderTag*/,
               const FText&                        InSourceName,
               const TArray<FCk_DebugOverlay_Row>& InRows) -> FCk_DebugOverlay_Row
            {
                auto Row     = FCk_DebugOverlay_Row{};
                Row.FieldTag = InRows.IsEmpty() ? FGameplayTag{} : InRows[0].FieldTag;
                Row.Value    = FText::FromString(InSourceName.ToString() + TEXT(": summarised"));
                return Row;
            });

        TestEqual(TEXT("primary + one condensed + the other provider"), Condensed.Num(), 3);

        TestEqual(TEXT("the focus section is the primary and is untouched"),
            static_cast<int32>(Condensed[0].SourceEntityId), 900);
        TestEqual(TEXT("primary section keeps every row"), Condensed[0].Rows.Num(), 2);
        TestTrue(TEXT("primary section keeps its empty focus source chip"),
            Condensed[0].SourceName.IsEmpty());

        const auto& Collapsed = Condensed[1];
        TestEqual(TEXT("one summary row per collapsed sub-entity"),  Collapsed.Rows.Num(), 3);
        TestEqual(TEXT("collapsed section buckets history on the focus entity"),
            static_cast<int32>(Collapsed.SourceEntityId), 900);
        TestEqual(TEXT("collapsed section takes the lowest source order"), Collapsed.SourceOrder, 1);
        TestFalse(TEXT("collapsed section names its source chip"), Collapsed.SourceName.IsEmpty());
        TestEqual(TEXT("summary rows are the provider's, in discovery order"),
            Collapsed.Rows[0].Value.ToString(), FString(TEXT("Sub1: summarised")));
        TestEqual(TEXT("last collapsed source is the last summary row"),
            Collapsed.Rows[2].Value.ToString(), FString(TEXT("Sub3: summarised")));

        TestEqual(TEXT("another behavior's sub-source section passes through"),
            static_cast<int32>(Condensed[2].SourceEntityId), 950);
    }

    // (g2) CondensePerSource with NO focus section — the realistic topology, since a GOAP
    //      planner and an SM root are both CHILD entities and never provide on the focus entity.
    //      The FIRST section (nearest source = the NPC's primary) still stays full.
    {
        const auto Make_Summary =
            [](const FGameplayTag&                 /*InProviderTag*/,
               const FText&                        InSourceName,
               const TArray<FCk_DebugOverlay_Row>& /*InRows*/) -> FCk_DebugOverlay_Row
            {
                auto Row  = FCk_DebugOverlay_Row{};
                Row.Value = FText::FromString(InSourceName.ToString() + TEXT(": summarised"));
                return Row;
            };

        // Three sub-source sections, no SourceOrder 0 anywhere.
        auto Sections = TArray<FCk_DebugOverlay_Section>{};
        for (auto Idx = 1; Idx <= 3; ++Idx)
        {
            auto Sub = CardModel_MakeSection(CardModel_Prov_Label, 900 + Idx, Idx,
                ECk_DebugOverlay_MergeBehavior::CondensePerSource);
            Sub.SourceName = FText::FromString(FString{ TEXT("Sm") } + FString::FromInt(Idx));
            Sub.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Chase")));
            Sub.Rows.Add(CardModel_MakeRow(CardModel_Field_Kind, TEXT("(4 entries)")));
            Sections.Add(Sub);
        }

        const auto Condensed = ck_debugoverlay::Condense_ProviderSections(
            Sections, /*InFocusEntityId=*/900, Make_Summary);

        TestEqual(TEXT("primary section plus one condensed section"), Condensed.Num(), 2);

        TestEqual(TEXT("the first sub-source section is the primary"),
            static_cast<int32>(Condensed[0].SourceEntityId), 901);
        TestEqual(TEXT("the primary keeps every row"), Condensed[0].Rows.Num(), 2);
        TestEqual(TEXT("the primary keeps its own source chip"),
            Condensed[0].SourceName.ToString(), FString(TEXT("Sm1")));
        TestEqual(TEXT("the primary keeps its own source order"), Condensed[0].SourceOrder, 1);

        const auto& Collapsed = Condensed[1];
        TestEqual(TEXT("only the sections BEHIND the primary collapse"), Collapsed.Rows.Num(), 2);
        TestEqual(TEXT("condensed section buckets history on the focus entity"),
            static_cast<int32>(Collapsed.SourceEntityId), 900);
        TestEqual(TEXT("condensed section takes the lowest COLLAPSED source order"),
            Collapsed.SourceOrder, 2);
        TestEqual(TEXT("the primary is never summarised"),
            Collapsed.Rows[0].Value.ToString(), FString(TEXT("Sm2: summarised")));

        // A provider that contributed exactly one section has nothing to condense: it passes
        // through whole, keeping its own source chip (no "N sub-entities" rename).
        {
            auto Solo = CardModel_MakeSection(CardModel_Prov_Label, 960, 1,
                ECk_DebugOverlay_MergeBehavior::CondensePerSource);
            Solo.SourceName = FText::FromString(FString{ TEXT("Solo") });
            Solo.Rows.Add(CardModel_MakeRow(CardModel_Field_Name, TEXT("Chase")));

            const auto Untouched = ck_debugoverlay::Condense_ProviderSections(
                TArray<FCk_DebugOverlay_Section>{ Solo }, /*InFocusEntityId=*/900, Make_Summary);

            TestEqual(TEXT("a lone section is not condensed"),        Untouched.Num(), 1);
            TestEqual(TEXT("a lone section keeps its rows"),          Untouched[0].Rows.Num(), 1);
            TestEqual(TEXT("a lone section keeps its source chip"),
                Untouched[0].SourceName.ToString(), FString(TEXT("Solo")));
            TestEqual(TEXT("a lone section keeps its history bucket"),
                static_cast<int32>(Untouched[0].SourceEntityId), 960);
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
