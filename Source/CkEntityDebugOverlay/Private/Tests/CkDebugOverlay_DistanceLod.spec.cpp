#include <limits>

#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEntityDebugOverlay/Model/CkDebugOverlay_Model.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_DistanceLod.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_FocusCardBudget.h"

// --------------------------------------------------------------------------------------------------------------------
// Test-only tags. UE_DEFINE_GAMEPLAY_TAG_STATIC registers at DLL load, before the runner fires;
// distinct local names keep this TU self-contained (see CkDebugOverlay_FocusCardModel.spec.cpp).
// Goap is a BUDGET-PROTECTED provider and FloatAttributes is a generic attribute flood — the
// LOD trim must rank them exactly as the budget does.

UE_DEFINE_GAMEPLAY_TAG_STATIC(Lod_Prov_Goap,  "Ck.OnScreenDebugger.Provider.Goap")
UE_DEFINE_GAMEPLAY_TAG_STATIC(Lod_Prov_Label, "Ck.OnScreenDebugger.Provider.Label")
UE_DEFINE_GAMEPLAY_TAG_STATIC(Lod_Prov_Timer, "Ck.OnScreenDebugger.Provider.Timer")
UE_DEFINE_GAMEPLAY_TAG_STATIC(Lod_Prov_Attr,  "Ck.OnScreenDebugger.Provider.FloatAttributes")
UE_DEFINE_GAMEPLAY_TAG_STATIC(Lod_Field_Name, "Ck.OnScreenDebugger.Provider.Label.Name")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    auto Lod_MakeRow(const TCHAR* InValue, ECk_DebugOverlay_Severity InSeverity = ECk_DebugOverlay_Severity::Normal)
        -> FCk_DebugOverlay_Row
    {
        auto Row     = FCk_DebugOverlay_Row{};
        Row.FieldTag = Lod_Field_Name;
        Row.Value    = FText::FromString(FString{ InValue });
        Row.Severity = InSeverity;
        return Row;
    }

    auto Lod_MakeSection(FGameplayTag InProviderTag, int32 InSortPriority, int32 InRowCount)
        -> FCk_DebugOverlay_Section
    {
        auto Section         = FCk_DebugOverlay_Section{};
        Section.ProviderTag  = InProviderTag;
        Section.SortPriority = InSortPriority;

        for (auto Idx = 0; Idx < InRowCount; ++Idx)
        { Section.Rows.Add(Lod_MakeRow(TEXT("value"))); }

        return Section;
    }

    auto Lod_CountRows(const FCk_DebugOverlay_EntityModel& InModel) -> int32
    {
        auto Count = 0;
        for (const auto& Section : InModel.Sections)
        { Count += Section.Rows.Num(); }
        return Count;
    }

    auto Lod_CountOmitted(const FCk_DebugOverlay_EntityModel& InModel) -> int32
    {
        auto Count = InModel.LodOmittedRowCount;
        for (const auto& Section : InModel.Sections)
        { Count += Section.OmittedRowCount; }
        return Count;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_DistanceLodTrim_Test,
    "Ck.DebugOverlay.Presentation.DistanceLodTrim",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_DistanceLodTrim_Test::RunTest(const FString&)
{
    // Four sections, deliberately in the WRONG order for the card: the generic attribute flood
    // first, the protected AI provider last.
    const auto Make_Model = []() -> FCk_DebugOverlay_EntityModel
    {
        auto Model = FCk_DebugOverlay_EntityModel{};
        Model.Sections.Add(Lod_MakeSection(Lod_Prov_Attr,  1, 4));
        Model.Sections.Add(Lod_MakeSection(Lod_Prov_Label, 2, 2));
        Model.Sections.Add(Lod_MakeSection(Lod_Prov_Timer, 3, 3));
        Model.Sections.Add(Lod_MakeSection(Lod_Prov_Goap, 50, 3));
        return Model;
    };

    const auto Source     = Make_Model();
    const auto SourceRows = Lod_CountRows(Source);   // 12

    // (a) Full — the tier that preserves today's behavior exactly. Nothing is reordered,
    //     nothing is trimmed; only the tier stamp changes.
    {
        const auto Full = ck_debugoverlay::Apply_DistanceLod(Source, ECk_DebugOverlay_LodTier::Full);

        TestEqual(TEXT("full tier keeps every section"), Full.Sections.Num(), Source.Sections.Num());
        TestEqual(TEXT("full tier keeps every row"),     Lod_CountRows(Full), SourceRows);
        TestEqual(TEXT("full tier omits nothing"),       Lod_CountOmitted(Full), 0);
        TestTrue (TEXT("full tier is stamped on the model"),
            Full.LodTier == ECk_DebugOverlay_LodTier::Full);
        TestTrue (TEXT("full tier does not reorder sections"),
            Full.Sections[0].ProviderTag == Lod_Prov_Attr);
    }

    // (b) Summary — header + the top N sections of the CANONICAL budget ordering, so the
    //     protected provider survives even though it was collected last. Every row the trim
    //     dropped is counted, never silently gone.
    {
        auto Trim = FCk_DebugOverlay_LodTrim{};
        Trim.SummaryMaxSections = 2;

        const auto Summary = ck_debugoverlay::Apply_DistanceLod(
            Source, ECk_DebugOverlay_LodTier::Summary, Trim);

        TestEqual(TEXT("summary keeps exactly the section cap"), Summary.Sections.Num(), 2);
        TestTrue (TEXT("the protected provider is promoted into the summary"),
            Summary.Sections[0].ProviderTag == Lod_Prov_Goap);
        TestTrue (TEXT("the generic attribute flood is NOT in the summary"),
            Summary.Sections[1].ProviderTag != Lod_Prov_Attr);

        TestEqual(TEXT("surviving sections keep every row"), Lod_CountRows(Summary), 3 + 2);
        TestEqual(TEXT("dropped sections are counted, not hidden"),
            Summary.LodOmittedRowCount, SourceRows - Lod_CountRows(Summary));
        TestEqual(TEXT("no row is unaccounted for"),
            Lod_CountRows(Summary) + Lod_CountOmitted(Summary), SourceRows);
        TestTrue (TEXT("summary tier is stamped on the model"),
            Summary.LodTier == ECk_DebugOverlay_LodTier::Summary);
    }

    // (b2) A cap wider than the model is a no-op trim (but still reorders + stamps).
    {
        auto Trim = FCk_DebugOverlay_LodTrim{};
        Trim.SummaryMaxSections = 99;

        const auto Summary = ck_debugoverlay::Apply_DistanceLod(
            Source, ECk_DebugOverlay_LodTier::Summary, Trim);

        TestEqual(TEXT("nothing to drop when the cap exceeds the section count"),
            Summary.Sections.Num(), Source.Sections.Num());
        TestEqual(TEXT("an over-wide cap omits nothing"), Lod_CountOmitted(Summary), 0);
    }

    // (c) Pill — one token per provider, each carrying that section's LOUDEST severity, with
    //     the shed rows on the section's own "+N more" counter.
    {
        auto Model   = Make_Model();
        auto& Loudest = Model.Sections[0];               // the attribute flood
        Loudest.Rows[0].Severity = ECk_DebugOverlay_Severity::Normal;
        Loudest.Rows[2].Severity = ECk_DebugOverlay_Severity::Bad;

        const auto Pill = ck_debugoverlay::Apply_DistanceLod(Model, ECk_DebugOverlay_LodTier::Pill);

        TestEqual(TEXT("pill keeps every provider"), Pill.Sections.Num(), Model.Sections.Num());
        TestEqual(TEXT("pill keeps exactly one row per provider"),
            Lod_CountRows(Pill), Pill.Sections.Num());
        TestEqual(TEXT("no row is unaccounted for"),
            Lod_CountRows(Pill) + Lod_CountOmitted(Pill), SourceRows);
        TestTrue (TEXT("pill tier is stamped on the model"),
            Pill.LodTier == ECk_DebugOverlay_LodTier::Pill);
        TestTrue (TEXT("pill orders by the protected budget ranking"),
            Pill.Sections[0].ProviderTag == Lod_Prov_Goap);

        const auto* AttrSection = Pill.Sections.FindByPredicate(
            [](const FCk_DebugOverlay_Section& InSection){ return InSection.ProviderTag == Lod_Prov_Attr; });

        TestTrue(TEXT("the attribute section survives as a token"), AttrSection != nullptr);
        if (AttrSection != nullptr)
        {
            TestEqual(TEXT("its shed rows are visible as an explicit omission"),
                AttrSection->OmittedRowCount, 3);
            // A collapse must never quieten the card — same rule the duplicate merge follows.
            TestEqual(TEXT("the surviving token carries the loudest severity"),
                static_cast<int32>(AttrSection->Rows[0].Severity),
                static_cast<int32>(ECk_DebugOverlay_Severity::Bad));
        }
    }

    // (d) The budget runs AFTER the LOD trim, so it must ADD to the omission counts rather
    //     than reset them — otherwise the pill tier's "+N more" silently disappears.
    {
        const auto Pill = ck_debugoverlay::Apply_DistanceLod(Source, ECk_DebugOverlay_LodTier::Pill);
        const auto PillOmitted = Lod_CountOmitted(Pill);

        const auto Budgeted = ck_debugoverlay::Apply_FocusCardBudget(Pill, { 4, 18 });

        TestEqual(TEXT("the budget leaves the pill tokens alone"),
            Lod_CountRows(Budgeted), Lod_CountRows(Pill));
        TestEqual(TEXT("the budget preserves LOD omissions instead of resetting them"),
            Lod_CountOmitted(Budgeted), PillOmitted);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_DistanceLodHysteresis_Test,
    "Ck.DebugOverlay.Presentation.DistanceLodHysteresis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_DistanceLodHysteresis_Test::RunTest(const FString&)
{
    auto Thresholds = FCk_DebugOverlay_LodThresholds{};
    Thresholds.SummaryDistance = 1000.0f;
    Thresholds.PillDistance    = 2000.0f;
    Thresholds.Hysteresis      = 100.0f;

    const auto Tier = [&Thresholds](float InDistance, ECk_DebugOverlay_LodTier InCurrent)
    { return ck_debugoverlay::Resolve_LodTier(InDistance, Thresholds, InCurrent); };

    // Coarsening needs Threshold + gap: sitting ON the boundary must not flip the card.
    TestTrue(TEXT("inside the gap above the summary threshold the card stays full"),
        Tier(1099.0f, ECk_DebugOverlay_LodTier::Full) == ECk_DebugOverlay_LodTier::Full);
    TestTrue(TEXT("past the gap the card drops to summary"),
        Tier(1101.0f, ECk_DebugOverlay_LodTier::Full) == ECk_DebugOverlay_LodTier::Summary);

    // Refining needs Threshold - gap, so the same walk backwards does NOT snap back early.
    TestTrue(TEXT("a summary card holds through the gap below its threshold"),
        Tier(901.0f, ECk_DebugOverlay_LodTier::Summary) == ECk_DebugOverlay_LodTier::Summary);
    TestTrue(TEXT("below the gap the card returns to full"),
        Tier(899.0f, ECk_DebugOverlay_LodTier::Summary) == ECk_DebugOverlay_LodTier::Full);

    // Same contract on the summary/pill boundary.
    TestTrue(TEXT("a summary card holds through the gap above the pill threshold"),
        Tier(2099.0f, ECk_DebugOverlay_LodTier::Summary) == ECk_DebugOverlay_LodTier::Summary);
    TestTrue(TEXT("past the gap the card drops to pill"),
        Tier(2101.0f, ECk_DebugOverlay_LodTier::Summary) == ECk_DebugOverlay_LodTier::Pill);
    TestTrue(TEXT("a pill card holds through the gap below its threshold"),
        Tier(1901.0f, ECk_DebugOverlay_LodTier::Pill) == ECk_DebugOverlay_LodTier::Pill);
    TestTrue(TEXT("below the gap the pill card returns to summary"),
        Tier(1899.0f, ECk_DebugOverlay_LodTier::Pill) == ECk_DebugOverlay_LodTier::Summary);

    // The boundary a card did NOT just cross keeps its normal position: a pill card that
    // teleports inside the near threshold goes straight to full.
    TestTrue(TEXT("a large step skips a tier"),
        Tier(10.0f, ECk_DebugOverlay_LodTier::Pill) == ECk_DebugOverlay_LodTier::Full);
    TestTrue(TEXT("a large step coarsens by two tiers as well"),
        Tier(9000.0f, ECk_DebugOverlay_LodTier::Full) == ECk_DebugOverlay_LodTier::Pill);

    // An absurd gap is clamped to half the band between the thresholds (500 here), so the
    // adjusted boundaries can never cross and invert the tiers.
    {
        auto Wide = Thresholds;
        Wide.Hysteresis = 5000.0f;

        TestTrue(TEXT("clamped gap still coarsens eventually"),
            ck_debugoverlay::Resolve_LodTier(1499.0f, Wide, ECk_DebugOverlay_LodTier::Full)
                == ECk_DebugOverlay_LodTier::Full);
        TestTrue(TEXT("clamped gap coarsens at threshold + half the band"),
            ck_debugoverlay::Resolve_LodTier(1501.0f, Wide, ECk_DebugOverlay_LodTier::Full)
                == ECk_DebugOverlay_LodTier::Summary);
    }

    // Degenerate configs are total, not undefined behavior.
    {
        auto Inverted = Thresholds;
        Inverted.PillDistance = 10.0f;   // below the summary threshold

        TestTrue(TEXT("an inverted pill threshold is lifted to the summary threshold"),
            ck_debugoverlay::Resolve_LodTier(5000.0f, Inverted, ECk_DebugOverlay_LodTier::Full)
                == ECk_DebugOverlay_LodTier::Pill);
    }

    TestTrue(TEXT("a negative distance never trims"),
        Tier(-1.0f, ECk_DebugOverlay_LodTier::Pill) == ECk_DebugOverlay_LodTier::Full);
    TestTrue(TEXT("a non-finite distance never trims"),
        Tier(std::numeric_limits<float>::quiet_NaN(), ECk_DebugOverlay_LodTier::Pill)
            == ECk_DebugOverlay_LodTier::Full);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
