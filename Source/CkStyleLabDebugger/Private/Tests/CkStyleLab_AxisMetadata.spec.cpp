#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkStyleLabDebugger/Styles/CkStyleLab_AxisMetadata.h"
#include "CkStyleLabDebugger/Widgets/SCkStyleLab_ControlsPane.h"
#include "CkStyleLabDebugger/Widgets/SCkStyleLab_InputHudControls.h"
#include "CkStyleLabDebugger/Widgets/SCkStyleLab_SamplePane.h"

#include "UObject/UnrealType.h"

namespace ck_style_lab_axis_metadata_tests
{
    auto Is_EnumAxis(const FProperty& InProperty) -> bool
    {
        return CastField<FEnumProperty>(&InProperty) != nullptr
            || CastField<FByteProperty>(&InProperty) != nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkStyleLab_AxisGroupMetadataIntegrity,
    "Ck.StyleLab.AxisGroupMetadataIntegrity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkStyleLab_AxisGroupMetadataIntegrity::RunTest(const FString&) -> bool
{
    const auto& Groups = ck::style_lab::Get_GroupMetadata();
    TestEqual(TEXT("Style Lab registers all seven ordered groups"), Groups.Num(), 7);

    auto SeenGroups = TSet<ECkStyleLab_Group>{};
    for (auto Index = 0; Index < Groups.Num(); ++Index)
    {
        const auto& Group = Groups[Index];
        TestEqual(TEXT("Style Lab group order follows its enum"),
            static_cast<uint8>(Group.Group), static_cast<uint8>(Index));
        TestTrue(TEXT("Style Lab group appears only once"), !SeenGroups.Contains(Group.Group));
        TestFalse(TEXT("Style Lab group has a label"), Group.DisplayName.IsEmpty());
        TestFalse(TEXT("Style Lab group explains its scope"), Group.Description.IsEmpty());
        SeenGroups.Add(Group.Group);
    }
    TestTrue(TEXT("Input HUD is the final feature-local group"),
        Groups.IsValidIndex(6) && Groups[6].Group == ECkStyleLab_Group::InputHud);

    auto GroupCounts = TMap<ECkStyleLab_Group, int32>{};
    auto ReflectedProperties = TSet<FName>{};
    auto MetadataCount = 0;
    for (auto PropertyIt = TFieldIterator<FProperty>{FCkDebuggerStyleSelection::StaticStruct()};
         PropertyIt;
         ++PropertyIt)
    {
        const auto* Property = *PropertyIt;
        if (Property == nullptr || !ck_style_lab_axis_metadata_tests::Is_EnumAxis(*Property))
        { continue; }

        const auto PropertyName = Property->GetFName();
        TestTrue(TEXT("Each reflected generic axis is unique"), !ReflectedProperties.Contains(PropertyName));
        ReflectedProperties.Add(PropertyName);

        const auto* Metadata = ck::style_lab::Find_AxisMetadata(PropertyName);
        TestNotNull(TEXT("Each reflected generic axis has exactly one reachable metadata record"), Metadata);
        if (Metadata == nullptr)
        { continue; }

        ++MetadataCount;
        TestTrue(TEXT("Generic axes never leak into the feature-local Input HUD group"),
            Metadata->Group != ECkStyleLab_Group::InputHud);
        GroupCounts.FindOrAdd(Metadata->Group)++;
    }

    TestEqual(TEXT("All 24 reflected generic axes have metadata"), ReflectedProperties.Num(), 24);
    TestEqual(TEXT("All 24 reflected generic axes resolve to metadata"), MetadataCount, 24);
    TestEqual(TEXT("Workbench surface axis count"), GroupCounts.FindRef(ECkStyleLab_Group::WorkbenchSurfaces), 6);
    TestEqual(TEXT("Token and legend axis count"), GroupCounts.FindRef(ECkStyleLab_Group::TokensLegend), 6);
    TestEqual(TEXT("Entity and value axis count"), GroupCounts.FindRef(ECkStyleLab_Group::EntityValues), 3);
    TestEqual(TEXT("Hierarchy and editing axis count"), GroupCounts.FindRef(ECkStyleLab_Group::HierarchyEditing), 4);
    TestEqual(TEXT("Icon axis count"), GroupCounts.FindRef(ECkStyleLab_Group::Icons), 2);
    TestEqual(TEXT("Graph telemetry axis count"), GroupCounts.FindRef(ECkStyleLab_Group::GraphTelemetry), 3);
    TestEqual(TEXT("Input HUD is allowed no generic axes"), GroupCounts.FindRef(ECkStyleLab_Group::InputHud), 0);

    const auto PaneTreatment = FName{TEXT("SurfaceElevation")};
    const auto* PaneTreatmentMetadata = ck::style_lab::Find_AxisMetadata(PaneTreatment);
    TestEqual(TEXT("Surface Elevation is named Pane Treatment"),
        PaneTreatmentMetadata ? PaneTreatmentMetadata->DisplayName.ToString() : FString{}, FString{TEXT("Pane Treatment")});

    const auto* Cards = ck::style_lab::Find_AxisOptionLabel(
        PaneTreatment, static_cast<int64>(ECkDebugAxis_SurfaceElevation::Layered));
    const auto* Workbench = ck::style_lab::Find_AxisOptionLabel(
        PaneTreatment, static_cast<int64>(ECkDebugAxis_SurfaceElevation::Flat));
    const auto* Outlined = ck::style_lab::Find_AxisOptionLabel(
        PaneTreatment, static_cast<int64>(ECkDebugAxis_SurfaceElevation::Outlined));
    TestEqual(TEXT("Pane Treatment labels Cards"), Cards ? Cards->ToString() : FString{}, FString{TEXT("Cards")});
    TestEqual(TEXT("Pane Treatment labels Workbench"), Workbench ? Workbench->ToString() : FString{}, FString{TEXT("Workbench")});
    TestNull(TEXT("Pane Treatment does not expose the retired Outlined value"), Outlined);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkStyleLab_GraphEventEmphasisMetadata,
    "Ck.StyleLab.GraphEventEmphasisMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkStyleLab_GraphEventEmphasisMetadata::RunTest(const FString&) -> bool
{
    const auto PropertyName = FName{TEXT("GraphEventEmphasis")};
    const auto* Axis = ck::style_lab::Find_AxisMetadata(PropertyName);
    TestNotNull(TEXT("Graph Event Emphasis has Style Lab metadata"), Axis);
    if (Axis == nullptr)
    {
        return false;
    }

    TestEqual(TEXT("Graph Event Emphasis has its authored display name"),
              Axis->DisplayName.ToString(),
              FString{TEXT("Graph Event Emphasis")});
    TestFalse(TEXT("Graph Event Emphasis explains its purpose"), Axis->ToolTip.IsEmpty());

    const auto* Subtle = ck::style_lab::Find_AxisOptionLabel(
        PropertyName, static_cast<int64>(ECkDebugAxis_GraphEventEmphasis::Subtle));
    const auto* Clear = ck::style_lab::Find_AxisOptionLabel(
        PropertyName, static_cast<int64>(ECkDebugAxis_GraphEventEmphasis::Clear));
    const auto* Bold = ck::style_lab::Find_AxisOptionLabel(
        PropertyName, static_cast<int64>(ECkDebugAxis_GraphEventEmphasis::Bold));
    TestEqual(TEXT("Style Lab labels Subtle"), Subtle ? Subtle->ToString() : FString{}, FString{TEXT("Subtle")});
    TestEqual(TEXT("Style Lab labels Clear"), Clear ? Clear->ToString() : FString{}, FString{TEXT("Clear")});
    TestEqual(TEXT("Style Lab labels Bold"), Bold ? Bold->ToString() : FString{}, FString{TEXT("Bold")});
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkStyleLab_InputHudPreviewConstruction,
    "Ck.StyleLab.InputHudPreviewConstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkStyleLab_InputHudPreviewConstruction::RunTest(const FString&) -> bool
{
    const auto Pane = SNew(SCkStyleLab_SamplePane);
    Pane->SlatePrepass();

    TestTrue(TEXT("Style Lab sample composes the real Input HUD preview"), Pane->GetDesiredSize().Y > 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkStyleLab_InputHudControlsConstruction,
    "Ck.StyleLab.InputHudControlsConstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkStyleLab_InputHudControlsConstruction::RunTest(const FString&) -> bool
{
    const auto Controls = SNew(SCkStyleLab_InputHudControls);
    Controls->SlatePrepass();

    TestTrue(TEXT("Style Lab composes the feature-local Input HUD tuner panel"),
        Controls->GetDesiredSize().Y > 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkStyleLab_GroupedControlsConstruction,
    "Ck.StyleLab.GroupedControlsConstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkStyleLab_GroupedControlsConstruction::RunTest(const FString&) -> bool
{
    const auto Controls = SNew(SCkStyleLab_ControlsPane);
    Controls->SlatePrepass();

    TestEqual(TEXT("Style Lab exposes every reflected generic axis"), Controls->Get_AxisCount(), 24);
    TestEqual(TEXT("Every Style Lab group owns an immediate preview"), Controls->Get_GroupPreviewCount(), 7);
    TestTrue(TEXT("Grouped Style Lab controls produce a visible single-column document"),
        Controls->GetDesiredSize().Y > 0.0f);
    return true;
}

#endif
