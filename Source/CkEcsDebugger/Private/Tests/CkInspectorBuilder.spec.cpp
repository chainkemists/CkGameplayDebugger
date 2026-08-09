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
