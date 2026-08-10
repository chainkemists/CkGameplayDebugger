#include "Misc/AutomationTest.h"

#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

// --------------------------------------------------------------------------------------------------------------------
// Pure parts of the wave-1 builder vocabulary: the label-OR-value filter predicate, the aligned
// numeric row's column math / degrade-mode selection, and the axis color mapping. No Slate widget
// is constructed here — the row composition itself is [EDITOR-VERIFY].

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorBuilder_Filter_Test,
    "Ck.EcsDebugger.InspectorBuilder.FilterMatchesLabelOrValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInspectorBuilder_Filter_Test::RunTest(const FString&)
{
    const auto Match = [](const TCHAR* InFilter, const TCHAR* InLabel, const TCHAR* InValue)
    {
        return FCkInspectorWidgetBuilder::Matches_Filter(InFilter, InLabel, InValue);
    };

    TestTrue(TEXT("empty filter keeps every row"),
        Match(TEXT(""), TEXT("Health"), TEXT("42.5")));

    TestTrue(TEXT("label match survives"),
        Match(TEXT("hea"), TEXT("Health"), TEXT("42.5")));

    TestTrue(TEXT("value match survives when the label misses"),
        Match(TEXT("42"), TEXT("Health"), TEXT("42.5")));

    TestFalse(TEXT("neither matching drops the row"),
        Match(TEXT("zzz"), TEXT("Health"), TEXT("42.5")));

    TestFalse(TEXT("a valueless row still filters on its label alone"),
        Match(TEXT("42"), TEXT("Health"), TEXT("")));

    TestTrue(TEXT("label matching stays primary for a valueless row"),
        Match(TEXT("hlt"), TEXT("Health"), TEXT("")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorBuilder_NumericLayout_Test,
    "Ck.EcsDebugger.InspectorBuilder.AlignedNumericLayoutAndColumns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInspectorBuilder_NumericLayout_Test::RunTest(const FString&)
{
    using FBuilder = FCkInspectorWidgetBuilder;

    // Degrade-mode selection — every ValueAlignment enumerator is covered.
    const auto Layout = [](ECkDebugAxis_ValueAlignment InAlignment)
    {
        return static_cast<int32>(FCkInspectorWidgetBuilder::Get_NumericLayout(InAlignment));
    };

    TestEqual(TEXT("AlignedColumns keeps the column layout"),
        Layout(ECkDebugAxis_ValueAlignment::AlignedColumns),
        static_cast<int32>(ECkInspector_NumericLayout::AlignedColumns));

    TestEqual(TEXT("Left degrades to one left-aligned line"),
        Layout(ECkDebugAxis_ValueAlignment::Left),
        static_cast<int32>(ECkInspector_NumericLayout::SingleLine_Left));

    TestEqual(TEXT("Right degrades to one right-aligned line"),
        Layout(ECkDebugAxis_ValueAlignment::Right),
        static_cast<int32>(ECkInspector_NumericLayout::SingleLine_Right));

    // Column math: constant while the components fit the budget, so a 3-component Location row
    // lines up with a 3-component Scale row; wider rows share the budget down to the floor.
    const auto Base = FBuilder::Get_AlignedColumnWidth(3);

    TestEqual(TEXT("one component uses the base width"),   FBuilder::Get_AlignedColumnWidth(1), Base);
    TestEqual(TEXT("two components use the base width"),   FBuilder::Get_AlignedColumnWidth(2), Base);
    TestEqual(TEXT("degenerate count uses the base width"), FBuilder::Get_AlignedColumnWidth(0), Base);

    const auto Four = FBuilder::Get_AlignedColumnWidth(4);
    TestTrue(TEXT("four components narrow the columns"), Four < Base);

    const auto Ten = FBuilder::Get_AlignedColumnWidth(10);
    TestTrue(TEXT("many components clamp to a legible floor"), Ten > 0.0f && Ten < Four);
    TestEqual(TEXT("the floor is a floor"), FBuilder::Get_AlignedColumnWidth(64), Ten);

    // Joining — the Left/Right degrade payload and the filter value for an aligned row.
    TestTrue(TEXT("no components joins to nothing"),
        FBuilder::Join_NumericComponents({}).IsEmpty());

    TestEqual(TEXT("one component joins to itself"),
        FBuilder::Join_NumericComponents({FText::FromString(TEXT("1.00"))}).ToString(),
        FString{TEXT("1.00")});

    const auto Joined = FBuilder::Join_NumericComponents(
        {FText::FromString(TEXT("1.00")), FText::FromString(TEXT("-2.00")), FText::FromString(TEXT("3.00"))});

    TestTrue(TEXT("joined text carries every component"),
        Joined.ToString().Contains(TEXT("1.00")) &&
        Joined.ToString().Contains(TEXT("-2.00")) &&
        Joined.ToString().Contains(TEXT("3.00")));

    TestTrue(TEXT("joined components are separated"),
        Joined.ToString().Len() > FString{TEXT("1.00-2.003.00")}.Len());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorBuilder_CountBadge_Test,
    "Ck.EcsDebugger.InspectorBuilder.CountBadgeFormatting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInspectorBuilder_CountBadge_Test::RunTest(const FString&)
{
    using FBuilder = FCkInspectorWidgetBuilder;

    const auto Format = [](int32 InCount) { return FBuilder::Format_Count(InCount).ToString(); };

    TestEqual(TEXT("zero is a plain zero, never blank"), Format(0), FString{TEXT("0")});
    TestEqual(TEXT("a small count is the bare number"),  Format(7), FString{TEXT("7")});

    // Grouping is what keeps a five-digit body/edge count readable at badge size. The separator
    // itself is locale-owned, so assert its PRESENCE and the digits, not a specific glyph.
    const auto Digits = [](const FString& InText)
    {
        auto Out = FString{};
        for (const auto Char : InText)
        {
            if (FChar::IsDigit(Char)) { Out.AppendChar(Char); }
        }
        return Out;
    };

    const auto Large = Format(1024);
    TestTrue(TEXT("a four-digit count is grouped"), Large.Len() > FString{TEXT("1024")}.Len());
    TestEqual(TEXT("grouping keeps every digit"), Digits(Large), FString{TEXT("1024")});

    // A negative count means the caller computed a size wrong — surface it rather than clamp it.
    TestTrue(TEXT("a negative count renders as negative"), Format(-3).Contains(TEXT("3")));
    TestFalse(TEXT("a negative count is not silently zeroed"), Format(-3) == FString{TEXT("0")});

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorBuilder_AxisColor_Test,
    "Ck.EcsDebugger.InspectorBuilder.AxisColorByComponentIndex",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInspectorBuilder_AxisColor_Test::RunTest(const FString&)
{
    using FBuilder = FCkInspectorWidgetBuilder;

    TestTrue(TEXT("component 0 is the X axis role"), FBuilder::Get_AxisColor(0) == CkStyle::AxisX());
    TestTrue(TEXT("component 1 is the Y axis role"), FBuilder::Get_AxisColor(1) == CkStyle::AxisY());
    TestTrue(TEXT("component 2 is the Z axis role"), FBuilder::Get_AxisColor(2) == CkStyle::AxisZ());

    TestTrue(TEXT("further components are neutral text"), FBuilder::Get_AxisColor(3) == CkStyle::Text());
    TestTrue(TEXT("a negative index is neutral text"),     FBuilder::Get_AxisColor(-1) == CkStyle::Text());

    TestFalse(TEXT("the three axis roles are distinguishable"),
        FBuilder::Get_AxisColor(0) == FBuilder::Get_AxisColor(1) ||
        FBuilder::Get_AxisColor(1) == FBuilder::Get_AxisColor(2) ||
        FBuilder::Get_AxisColor(0) == FBuilder::Get_AxisColor(2));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
// The FlashOnChange state machine. The COLOUR it drives is [EDITOR-VERIFY]; the seed / change /
// decay rules are not, and they are what decides whether a rebuild lights the whole panel up.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorBuilder_Flash_Test,
    "Ck.EcsDebugger.InspectorBuilder.FlashOnChangeStateMachine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInspectorBuilder_Flash_Test::RunTest(const FString&)
{
    using FBuilder = FCkInspectorWidgetBuilder;

    constexpr auto Duration = FBuilder::FlashDurationSeconds;

    auto State = FCkInspector_FlashState{};

    TestEqual(TEXT("an unobserved row is not flashing"), FBuilder::Get_FlashAlpha(State, 100.0), 0.0f);

    // Seeding: a fresh row widget observes its value for the first time on its first paint. That must
    // NOT read as a change, or every rebuild would flash every row at once.
    FBuilder::Observe_FlashChange(State, TEXT("42"), 100.0);
    TestEqual(TEXT("the first observation only seeds"), FBuilder::Get_FlashAlpha(State, 100.0), 0.0f);

    // Idempotence: attributes evaluate several times per paint (desired size, then paint).
    FBuilder::Observe_FlashChange(State, TEXT("42"), 100.1);
    FBuilder::Observe_FlashChange(State, TEXT("42"), 100.2);
    TestEqual(TEXT("re-observing the same value never starts a flash"), FBuilder::Get_FlashAlpha(State, 100.2), 0.0f);

    // The change itself.
    FBuilder::Observe_FlashChange(State, TEXT("43"), 200.0);
    TestEqual(TEXT("a change flashes at full strength"), FBuilder::Get_FlashAlpha(State, 200.0), 1.0f);

    TestTrue(TEXT("the flash decays inside the window"),
        FBuilder::Get_FlashAlpha(State, 200.0 + Duration * 0.5) < 1.0f &&
        FBuilder::Get_FlashAlpha(State, 200.0 + Duration * 0.5) > 0.0f);

    TestTrue(TEXT("the decay is monotonic"),
        FBuilder::Get_FlashAlpha(State, 200.0 + Duration * 0.25) >
        FBuilder::Get_FlashAlpha(State, 200.0 + Duration * 0.75));

    TestEqual(TEXT("the window closes exactly at the duration"),
        FBuilder::Get_FlashAlpha(State, 200.0 + Duration), 0.0f);

    TestEqual(TEXT("a settled row stays settled"),
        FBuilder::Get_FlashAlpha(State, 200.0 + Duration * 10.0), 0.0f);

    // Re-observing the SAME value after the window does not re-arm — only a genuine change does.
    FBuilder::Observe_FlashChange(State, TEXT("43"), 300.0);
    TestEqual(TEXT("a settled repeat does not re-arm"), FBuilder::Get_FlashAlpha(State, 300.0), 0.0f);

    FBuilder::Observe_FlashChange(State, TEXT("44"), 300.0);
    TestEqual(TEXT("a second change re-arms"), FBuilder::Get_FlashAlpha(State, 300.0), 1.0f);

    // A clock that ran backwards (new Slate app / new PIE session) must read as settled, never as a
    // row that is permanently lit.
    TestEqual(TEXT("a backwards clock reads as settled"), FBuilder::Get_FlashAlpha(State, 10.0), 0.0f);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
// Multi-select diff: the pure comparison the inspector panel tints rows from.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorBuilder_Diff_Test,
    "Ck.EcsDebugger.InspectorBuilder.DiffComparesLabelValueMaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkInspectorBuilder_Diff_Test::RunTest(const FString&)
{
    using FBuilder = FCkInspectorWidgetBuilder;
    using FRows    = TMap<FString, FString>;

    const auto MakeRows = [](const FString& InName, const TCHAR* InHealth)
    {
        auto Rows = FRows{};
        Rows.Add(TEXT("Name:"), InName);

        // A null health is the "this inspector emitted no such row for this entity" shape.
        if (InHealth != nullptr)
        { Rows.Add(TEXT("Health:"), InHealth); }

        return Rows;
    };

    const auto A = MakeRows(TEXT("Goblin"), TEXT("10"));

    // Degenerate arities compare to nothing — there is no "differs" without something to differ from.
    TestEqual(TEXT("no entities compare to nothing"),
        FBuilder::Compute_DifferingLabels({}).Num(), 0);

    TestEqual(TEXT("one entity compares to nothing"),
        FBuilder::Compute_DifferingLabels({A}).Num(), 0);

    // Identical maps.
    TestEqual(TEXT("identical rows produce no marks"),
        FBuilder::Compute_DifferingLabels({A, A}).Num(), 0);

    // One value moved.
    const auto B = MakeRows(TEXT("Goblin"), TEXT("7"));
    const auto OneChanged = FBuilder::Compute_DifferingLabels({A, B});

    TestEqual(TEXT("only the changed label is marked"), OneChanged.Num(), 1);
    TestTrue(TEXT("the changed label is the one that moved"), OneChanged.Contains(TEXT("Health:")));
    TestFalse(TEXT("the matching label stays unmarked"), OneChanged.Contains(TEXT("Name:")));

    // A row set that differs in SHAPE — the dynamic-per-entity inspector case. A missing label is a
    // difference, not a skip.
    const auto C = MakeRows(TEXT("Goblin"), nullptr);
    const auto MissingLabel = FBuilder::Compute_DifferingLabels({A, C});

    TestTrue(TEXT("a label missing from one entity counts as differing"),
        MissingLabel.Contains(TEXT("Health:")));
    TestFalse(TEXT("the shared matching label is still unmarked"),
        MissingLabel.Contains(TEXT("Name:")));

    // An entity the inspector cannot inspect contributes an empty map: everything differs.
    const auto AgainstEmpty = FBuilder::Compute_DifferingLabels({A, FRows{}});
    TestEqual(TEXT("an empty participant marks every label"), AgainstEmpty.Num(), 2);

    // Three-way: a value that matches on two entities but not the third is still a difference.
    const auto ThreeWay = FBuilder::Compute_DifferingLabels({A, A, B});
    TestEqual(TEXT("a three-way disagreement marks the label once"), ThreeWay.Num(), 1);
    TestTrue(TEXT("the three-way mark is the disagreeing label"), ThreeWay.Contains(TEXT("Health:")));

    // Empty-string values are values, not absences: two rows that both read blank AGREE.
    const auto Blank = MakeRows(TEXT(""), TEXT(""));
    TestEqual(TEXT("two blank values agree"),
        FBuilder::Compute_DifferingLabels({Blank, Blank}).Num(), 0);

    TestEqual(TEXT("a blank value still differs from a set one"),
        FBuilder::Compute_DifferingLabels({Blank, A}).Num(), 2);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
