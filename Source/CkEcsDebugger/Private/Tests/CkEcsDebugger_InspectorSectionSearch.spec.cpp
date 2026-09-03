#include "Misc/AutomationTest.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDynamic/CkDynamic_FragmentDisplaySchema.h"
#include "CkDynamic/CkDynamic_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Snapshot/CkSnapshot_Posture.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Panels/CkDebuggerPanel_Inspector.h"

#include "StructUtils/InstancedStruct.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_ecs_debugger_inspector_section_search_tests
{
    auto CollectText(const TSharedRef<SWidget>& InWidget, TArray<FString>& OutText) -> void
    {
        if (InWidget->GetType() == FName(TEXT("STextBlock")))
        {
            OutText.Add(StaticCastSharedRef<STextBlock>(InWidget)->GetText().ToString());
        }

        auto* Children = InWidget->GetChildren();
        if (Children == nullptr)
        { return; }

        for (auto Index = 0; Index < Children->Num(); ++Index)
        { CollectText(Children->GetChildAt(Index), OutText); }
    }

    auto FindDualSearchBar(const TSharedRef<SWidget>& InWidget) -> TSharedPtr<SCkDebug_DualSearchBar>
    {
        if (InWidget->GetType() == FName(TEXT("SCkDebug_DualSearchBar")))
        { return StaticCastSharedRef<SCkDebug_DualSearchBar>(InWidget); }

        auto* Children = InWidget->GetChildren();
        if (Children == nullptr)
        { return {}; }

        for (auto Index = 0; Index < Children->Num(); ++Index)
        {
            if (const auto Found = FindDualSearchBar(Children->GetChildAt(Index)); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    struct FNativeDisplaySchemaScope
    {
        FString FragmentTypePath;
        ck::dynamic::FFragmentDisplaySchema PreviousSchema;
        bool HasPreviousSchema = false;
        bool Registered = false;

        FNativeDisplaySchemaScope(const UScriptStruct* InFragmentType, const FString& InDisplayName)
            : FragmentTypePath(InFragmentType->GetPathName())
            , HasPreviousSchema(ck::dynamic::TryGet_NativeFragmentDisplaySchema(FragmentTypePath, PreviousSchema))
        {
            auto Schema = ck::dynamic::FFragmentDisplaySchema{};
            Schema.FragmentDisplayName = InDisplayName;
            Registered = ck::dynamic::Register_NativeFragmentDisplaySchema(FragmentTypePath, MoveTemp(Schema));
        }

        ~FNativeDisplaySchemaScope()
        {
            if (NOT Registered)
            { return; }

            ck::dynamic::Unregister_NativeFragmentDisplaySchema(FragmentTypePath);
            if (HasPreviousSchema)
            {
                ck::dynamic::Register_NativeFragmentDisplaySchema(FragmentTypePath, MoveTemp(PreviousSchema));
            }
        }
    };
}

// --------------------------------------------------------------------------------------------------------------------
// The panel owns section inclusion, so exercise its public predicate directly.  In
// particular, Dynamic Fragments has one inspector name and many independently
// authored fragment section names: a subsection match must not be lost merely
// because the inspector name itself does not contain the query.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerInspectorSectionSearch_QueryNormalization_Test,
    "Ck.EcsDebugger.InspectorSectionSearch.QueryNormalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerInspectorSectionSearch_QueryNormalization_Test::RunTest(const FString&)
{
    using ck_debugger_panel_inspector::Matches_SectionQuery;

    TestTrue(TEXT("empty query includes an inspector"),
        Matches_SectionQuery(TEXT(""), TEXT("Dynamic Fragments")));
    TestTrue(TEXT("inspector-name matching ignores case"),
        Matches_SectionQuery(TEXT("dYnAmIc"), TEXT("Dynamic Fragments")));
    TestTrue(TEXT("section-name matching ignores spaces"),
        Matches_SectionQuery(TEXT("WorldItem"), TEXT("Dynamic Fragments"), TEXT("BB Fragment World Item")));
    TestTrue(TEXT("section-name matching ignores underscores and case"),
        Matches_SectionQuery(TEXT("bb_fragment_worlditem"), TEXT("Dynamic Fragments"), TEXT("BB Fragment World Item")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerInspectorSectionSearch_DynamicSubsections_Test,
    "Ck.EcsDebugger.InspectorSectionSearch.DynamicSubsections",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerInspectorSectionSearch_DynamicSubsections_Test::RunTest(const FString&)
{
    using ck_debugger_panel_inspector::Matches_SectionQuery;

    constexpr auto DynamicInspector = TEXT("Dynamic Fragments");

    // A precise fragment query keeps the matching section visible even though
    // "World Item" is not part of the Dynamic Fragments inspector label.
    TestTrue(TEXT("WorldItem finds its dynamic fragment subsection"),
        Matches_SectionQuery(TEXT("WorldItem"), DynamicInspector, TEXT("BB Fragment World Item")));
    TestFalse(TEXT("WorldItem excludes an unrelated dynamic fragment subsection"),
        Matches_SectionQuery(TEXT("WorldItem"), DynamicInspector, TEXT("BB Fragment Store Shelf")));

    // The broad inspector query intentionally includes every section emitted by
    // that inspector, preserving the existing inspector-level search behavior.
    TestTrue(TEXT("Dynamic includes the World Item subsection"),
        Matches_SectionQuery(TEXT("Dynamic"), DynamicInspector, TEXT("BB Fragment World Item")));
    TestTrue(TEXT("Dynamic includes other dynamic subsections"),
        Matches_SectionQuery(TEXT("Dynamic"), DynamicInspector, TEXT("BB Fragment Store Shelf")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerInspectorSectionSearch_PanelFilterWiring_Test,
    "Ck.EcsDebugger.InspectorSectionSearch.PanelFilterWiring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerInspectorSectionSearch_PanelFilterWiring_Test::RunTest(const FString&)
{
    using namespace ck_ecs_debugger_inspector_section_search_tests;

    // Real empty reflected payloads keep this a lightweight, non-UObject fixture.
    // The temporary schemas make their displayed Dynamic subsection names exact
    // test inputs and restore any pre-existing producer registrations on scope exit.
    const auto* WorldItemType = FCk_Snapshot_Session::StaticStruct();
    const auto* StorageShelfType = FCk_Snapshot_Durable::StaticStruct();
    TestNotNull(TEXT("World Item reflected fixture resolves"), WorldItemType);
    TestNotNull(TEXT("Storage Shelf reflected fixture resolves"), StorageShelfType);
    if (WorldItemType == nullptr || StorageShelfType == nullptr)
    { return false; }

    auto WorldItemSchema = FNativeDisplaySchemaScope{WorldItemType, TEXT("World Item")};
    auto StorageShelfSchema = FNativeDisplaySchemaScope{StorageShelfType, TEXT("Storage Shelf")};
    TestTrue(TEXT("World Item display schema installs"), WorldItemSchema.Registered);
    TestTrue(TEXT("Storage Shelf display schema installs"), StorageShelfSchema.Registered);
    if (NOT WorldItemSchema.Registered || NOT StorageShelfSchema.Registered)
    { return false; }

    auto World = ck::FEcsWorld{};
    const auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    TestTrue(TEXT("fixture creates a valid entity"), ck::IsValid(Entity));
    if (NOT ck::IsValid(Entity))
    { return false; }

    const auto WorldItemAdded = UCk_Utils_DynamicFragment_UE::Add_Fragment(
        Entity, FInstancedStruct::Make<FCk_Snapshot_Session>());
    const auto StorageShelfAdded = UCk_Utils_DynamicFragment_UE::Add_Fragment(
        Entity, FInstancedStruct::Make<FCk_Snapshot_Durable>());
    TestTrue(TEXT("fixture adds the World Item dynamic fragment"), ck::IsValid(WorldItemAdded));
    TestTrue(TEXT("fixture adds the Storage Shelf dynamic fragment"), ck::IsValid(StorageShelfAdded));
    if (NOT ck::IsValid(WorldItemAdded) || NOT ck::IsValid(StorageShelfAdded))
    { return false; }

    const auto Selection = MakeShared<FCkDebuggerModel_EntitySelection>();
    Selection->Set_SelectedEntities({Entity});
    const auto Panel = SNew(SCkDebuggerPanel_Inspector, Selection);

    const auto SearchBar = FindDualSearchBar(Panel);
    TestTrue(TEXT("panel contains its public dual search bar"), SearchBar.IsValid());
    if (NOT SearchBar.IsValid())
    { return false; }

    SearchBar->Set_FilterText(TEXT("WorldItem"));
    auto FilteredText = TArray<FString>{};
    CollectText(Panel, FilteredText);
    TestTrue(TEXT("WorldItem filter renders the matching Dynamic subsection"),
        FilteredText.Contains(TEXT("World Item")));
    TestFalse(TEXT("WorldItem filter omits the unrelated Dynamic subsection"),
        FilteredText.Contains(TEXT("Storage Shelf")));

    SearchBar->Set_FilterText(TEXT("Dynamic"));
    auto DynamicText = TArray<FString>{};
    CollectText(Panel, DynamicText);
    TestTrue(TEXT("Dynamic filter retains the World Item subsection"),
        DynamicText.Contains(TEXT("World Item")));
    TestTrue(TEXT("Dynamic filter retains all Dynamic subsections"),
        DynamicText.Contains(TEXT("Storage Shelf")));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
