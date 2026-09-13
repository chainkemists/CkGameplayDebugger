#include "Misc/AutomationTest.h"

#include "CkEcsDebugger/Inspectors/CkInspector_DynamicFragments.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDynamic/CkDynamic_Fragment.h"
#include "CkDynamic/CkDynamic_Utils.h"
#include "CkDynamic/CkDynamic_FragmentDisplaySchema.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkJolt/Query/CkJoltQuery_Data.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkTimer/CkTimer_Fragment_Data.h"

#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_ecs_debugger_dynamic_fragment_enum_value_tests
{
    auto HasKeyValuePair(
        const TSharedRef<SWidget>& InWidget,
        const FString& InExpectedKey,
        const FString& InExpectedValue) -> bool
    {
        auto* Children = InWidget->GetChildren();
        if (Children == nullptr)
        { return false; }

        if (InWidget->GetType() == FName(TEXT("SGridPanel")))
        {
            for (auto ChildIndex = 0; ChildIndex + 1 < Children->Num(); ChildIndex += 2)
            {
                const auto KeyWidget = Children->GetChildAt(ChildIndex);
                const auto ValueWidget = Children->GetChildAt(ChildIndex + 1);
                if (KeyWidget->GetType() != FName(TEXT("STextBlock")) ||
                    ValueWidget->GetType() != FName(TEXT("STextBlock")))
                { continue; }

                const auto KeyText = StaticCastSharedRef<STextBlock>(KeyWidget)->GetText().ToString();
                const auto ValueText = StaticCastSharedRef<STextBlock>(ValueWidget)->GetText().ToString();
                if (KeyText == InExpectedKey && ValueText == InExpectedValue)
                { return true; }
            }
        }

        for (auto ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
        {
            if (HasKeyValuePair(Children->GetChildAt(ChildIndex), InExpectedKey, InExpectedValue))
            { return true; }
        }

        return false;
    }

    auto AnySectionHasKeyValuePair(
        const TArray<ICkDebuggerComponentInspector_Base::FInspectorSection>& InSections,
        const FString& InExpectedKey,
        const FString& InExpectedValue) -> bool
    {
        return InSections.ContainsByPredicate([&InExpectedKey, &InExpectedValue](const auto& InSection)
        { return HasKeyValuePair(InSection.Widget, InExpectedKey, InExpectedValue); });
    }

    auto FindButtonWithTag(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SButton> Found = FindButtonWithTag(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
            if (Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto CollectionHasTextPair(
        const TSharedPtr<FCkUiCollection>& InCollection,
        const FString& InExpectedName,
        const FString& InExpectedValue) -> bool
    {
        if (NOT InCollection.IsValid())
        { return false; }
        for (const TSharedPtr<const FCkUiRecord>& Record : InCollection->GetRecords())
        {
            if (NOT Record.IsValid())
            { continue; }
            const FCkUiFieldValue* Name = Record->FindField(TEXT("property-name"));
            const FCkUiFieldValue* Value = Record->FindField(TEXT("property-value"));
            if (Name != nullptr && Value != nullptr
                && Name->Text.ToString() == InExpectedName
                && Value->Text.ToString() == InExpectedValue)
            { return true; }
        }
        return false;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerDynamicFragmentEnumValue_UsesFieldAddress,
    "Ck.EcsDebugger.DynamicFragments.EnumValueUsesFieldAddress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerDynamicFragmentEnumValue_UsesFieldAddress::RunTest(const FString&)
{
    using namespace ck_ecs_debugger_dynamic_fragment_enum_value_tests;

    // The timer's Duration precedes CountDirection. Its default zero duration reproduces the old
    // container-relative read while CountDirection is authored to a distinct nonzero enum value.
    auto TimerFragment = FInstancedStruct::Make<FCk_Fragment_Timer_ParamsData>();
    const auto* TimerType = FCk_Fragment_Timer_ParamsData::StaticStruct();
    const auto* EnumProperty = CastField<FEnumProperty>(TimerType->FindPropertyByName(TEXT("_CountDirection")));
    TestNotNull(TEXT("fixture exposes the FEnumProperty"), EnumProperty);
    if (EnumProperty == nullptr)
    { return false; }

    const auto* Enum = EnumProperty->GetEnum();
    const auto* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
    TestNotNull(TEXT("fixture enum descriptor resolves"), Enum);
    TestNotNull(TEXT("fixture enum has an underlying property"), UnderlyingProperty);
    if (Enum == nullptr || UnderlyingProperty == nullptr)
    { return false; }

    // Explicit runtime values keep the fixture independent of stripped UEnum metadata.
    const auto ExpectedValue = static_cast<int64>(ECk_Timer_CountDirection::CountDown);
    TestTrue(TEXT("fixture uses an explicit nonzero CountDirection value"), ExpectedValue != 0);
    TestTrue(TEXT("fixture enum field is not at the container start"), EnumProperty->GetOffset_ForInternal() > 0);
    if (ExpectedValue == 0 || EnumProperty->GetOffset_ForInternal() <= 0)
    { return false; }

    UnderlyingProperty->SetIntPropertyValue(
        EnumProperty->ContainerPtrToValuePtr<void>(TimerFragment.GetMutableMemory()), ExpectedValue);

    int64 ContainerRelativeValue = 0;
    UnderlyingProperty->GetValue_InContainer(TimerFragment.GetMemory(), &ContainerRelativeValue);
    TestNotEqual(
        TEXT("the old container-relative read observes the distinct prefix byte"),
        ContainerRelativeValue,
        ExpectedValue);

    auto World = ck::FEcsWorld{};
    auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    TestTrue(TEXT("fixture creates a valid entity for the inspector action rows"), ck::IsValid(Entity));
    if (NOT ck::IsValid(Entity))
    { return false; }

    TestTrue(TEXT("fixture admits the FEnumProperty dynamic fragment"),
        ck::IsValid(UCk_Utils_DynamicFragment_UE::Add_Fragment(Entity, TimerFragment)));

    auto Inspector = FCkInspector_DynamicFragments{};
    auto Sections = TArray<ICkDebuggerComponentInspector_Base::FInspectorSection>{};
    {
        const auto Capture = FCkInspector_RowCaptureScope{};
        Sections = Inspector.Get_InspectorSections(Entity);
    }

    const auto ExpectedDisplayName = ck::dynamic::Resolve_EnumValueDisplayName(TimerType, Enum, ExpectedValue);
    const auto ExpectedPropertyName = ck::dynamic::Resolve_PropertyDisplayName(TimerType, EnumProperty);
    TestTrue(TEXT("the CountDirection section row renders the enum value at its field address"),
        AnySectionHasKeyValuePair(Sections, ExpectedPropertyName, ExpectedDisplayName));

    // Keep the FByteProperty-with-enum branch covered by the same formatter/widget path.
    auto ByteFragment = FCk_Jolt_QueryFilter{};
    const auto* ByteFragmentType = FCk_Jolt_QueryFilter::StaticStruct();
    const auto* ByteProperty = CastField<FByteProperty>(ByteFragmentType->FindPropertyByName(TEXT("_Channel")));
    TestNotNull(TEXT("byte-enum fixture exposes the FByteProperty"), ByteProperty);
    TestNotNull(TEXT("byte-enum fixture exposes an enum descriptor"), ByteProperty != nullptr ? ByteProperty->Enum.Get() : nullptr);
    if (ByteProperty == nullptr || ByteProperty->Enum == nullptr)
    { return false; }

    const auto ByteValue = static_cast<int64>(ECC_WorldDynamic);
    TestTrue(TEXT("byte-enum fixture uses an explicit nonzero collision channel"), ByteValue != 0);
    if (ByteValue == 0)
    { return false; }

    ByteProperty->SetIntPropertyValue(ByteProperty->ContainerPtrToValuePtr<void>(&ByteFragment), ByteValue);

    TestTrue(TEXT("fixture admits the FByteProperty dynamic fragment"),
        ck::IsValid(UCk_Utils_DynamicFragment_UE::Add_Fragment(
            Entity,
            FInstancedStruct::Make<FCk_Jolt_QueryFilter>(ByteFragment))));

    {
        const auto Capture = FCkInspector_RowCaptureScope{};
        Sections = Inspector.Get_InspectorSections(Entity);
    }

    const auto ExpectedByteDisplayName = ck::dynamic::Resolve_EnumValueDisplayName(
        ByteFragmentType, ByteProperty->Enum, ByteValue);
    const auto ExpectedBytePropertyName = ck::dynamic::Resolve_PropertyDisplayName(ByteFragmentType, ByteProperty);
    TestTrue(TEXT("the Channel row retains FByteProperty enum rendering"),
        AnySectionHasKeyValuePair(Sections, ExpectedBytePropertyName, ExpectedByteDisplayName));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerDynamicFragments_AuthoredComposition,
    "Ck.UiAuthoring.EcsDebugger.DynamicFragmentsInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkEcsDebuggerDynamicFragments_AuthoredComposition::RunTest(const FString&)
{
    using namespace ck_ecs_debugger_dynamic_fragment_enum_value_tests;

    auto World = ck::FEcsWorld{};
    auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    UWorld* const TestWorld = GWorld;
    if (NOT TestTrue(TEXT("fixture creates a live entity in the automation world"),
        ck::IsValid(Entity) && TestWorld != nullptr))
    { return false; }
    Entity.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    UCk_Utils_Net_UE::Add(Entity, FCk_Net_ConnectionSettings{
        ECk_Replication::DoesNotReplicate, ECk_Net_NetModeType::Host, ECk_Net_EntityNetRole::Authority});

    auto TimerFragment = FInstancedStruct::Make<FCk_Fragment_Timer_ParamsData>();
    const UScriptStruct* TimerType = TimerFragment.GetScriptStruct();
    auto JoltFragment = FInstancedStruct::Make<FCk_Jolt_QueryFilter>();
    const UScriptStruct* JoltType = JoltFragment.GetScriptStruct();
    if (NOT TestTrue(TEXT("fixture adds independent non-replicated and replicated dynamic fragments"),
        ck::IsValid(UCk_Utils_DynamicFragment_UE::Add_Fragment(Entity, TimerFragment))
            && ck::IsValid(UCk_Utils_DynamicFragment_UE::Add_Fragment(
                Entity, JoltFragment, ECk_Replication::Replicates))))
    { return false; }

    const auto* TimerEnum = CastField<FEnumProperty>(TimerType->FindPropertyByName(TEXT("_CountDirection")));
    const auto* JoltEnum = CastField<FByteProperty>(JoltType->FindPropertyByName(TEXT("_Channel")));
    if (NOT TestTrue(TEXT("fixture resolves both enum-bearing properties"),
        TimerEnum != nullptr && JoltEnum != nullptr && JoltEnum->Enum != nullptr))
    { return false; }

    const FString TimerName = ck::dynamic::Resolve_PropertyDisplayName(TimerType, TimerEnum);
    const int64 TimerNumericValue = TimerEnum->GetUnderlyingProperty()->GetSignedIntPropertyValue(
        TimerEnum->ContainerPtrToValuePtr<void>(TimerFragment.GetMemory()));
    const FString TimerValue = ck::dynamic::Resolve_EnumValueDisplayName(
        TimerType, TimerEnum->GetEnum(), TimerNumericValue);
    const FString JoltName = ck::dynamic::Resolve_PropertyDisplayName(JoltType, JoltEnum);
    const int64 JoltNumericValue = JoltEnum->GetSignedIntPropertyValue(
        JoltEnum->ContainerPtrToValuePtr<void>(JoltFragment.GetMemory()));
    const FString JoltValue = ck::dynamic::Resolve_EnumValueDisplayName(
        JoltType, JoltEnum->Enum, JoltNumericValue);

    auto Inspector = FCkInspector_DynamicFragments{};
    auto NativeSections = TArray<ICkDebuggerComponentInspector_Base::FInspectorSection>{};
    {
        const auto Capture = FCkInspector_RowCaptureScope{};
        NativeSections = Inspector.Get_InspectorSections(Entity);
    }
    TestTrue(TEXT("native multi-section capture remains the enum formatting authority"),
        NativeSections.Num() == 2
            && AnySectionHasKeyValuePair(NativeSections, TimerName, TimerValue)
            && AnySectionHasKeyValuePair(NativeSections, JoltName, JoltValue));

    const auto Sections = Inspector.Get_InspectorSections(Entity);
    if (NOT TestTrue(TEXT("production multi-section path authors both dynamic fragment bodies atomically"),
        Sections.Num() == 2
            && Sections[0].Widget->GetTypeAsString() == TEXT("SCkInspector_DynamicFragmentAuthored")
            && Sections[1].Widget->GetTypeAsString() == TEXT("SCkInspector_DynamicFragmentAuthored")))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }

    const auto AuthoredA = StaticCastSharedRef<SCkInspector_DynamicFragmentAuthored>(Sections[0].Widget);
    const auto AuthoredB = StaticCastSharedRef<SCkInspector_DynamicFragmentAuthored>(Sections[1].Widget);
    const TSharedRef<SCkInspector_DynamicFragmentAuthored> TimerAuthored =
        CollectionHasTextPair(AuthoredA->Get_Properties(), TimerName, TimerValue) ? AuthoredA : AuthoredB;
    const TSharedRef<SCkInspector_DynamicFragmentAuthored> JoltAuthored =
        TimerAuthored == AuthoredA ? AuthoredB : AuthoredA;
    TSharedPtr<FCkUiView> TimerView = TimerAuthored->Get_View();
    TSharedPtr<FCkUiView> JoltView = JoltAuthored->Get_View();
    TSharedPtr<FCkUiCollection> TimerProperties = TimerAuthored->Get_Properties();
    TSharedPtr<FCkUiCollection> JoltProperties = JoltAuthored->Get_Properties();
    if (NOT TestTrue(TEXT("each fragment section owns an independent authored view and live property collection"),
        TimerAuthored->Is_Mounted() && JoltAuthored->Is_Mounted()
            && TimerView.IsValid() && JoltView.IsValid() && TimerView != JoltView
            && TimerProperties.IsValid() && JoltProperties.IsValid() && TimerProperties != JoltProperties
            && CollectionHasTextPair(TimerProperties, TimerName, TimerValue)
            && CollectionHasTextPair(JoltProperties, JoltName, JoltValue)))
    { return false; }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Stylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Dynamic Fragments resources are readable"),
        Plugin.IsValid()
            && FFileHelper::LoadFileToString(
                Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorDynamicFragments.ui.html")))
            && FFileHelper::LoadFileToString(
                Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorDynamicFragments.ui.css")))))
    { return false; }
    TestTrue(TEXT("resource owns property repetition, entity references, and both action placements"),
        Markup.Contains(TEXT("bind=\"dynamic-fragment-properties\""))
            && Markup.Contains(TEXT("<debug-entity-ref"))
            && Markup.Contains(TEXT("action=\"dynamic-fragment-remove\""))
            && Markup.Contains(TEXT("action=\"dynamic-fragment-mark-rep-dirty\"")));

    const TSharedPtr<SButton> HeldTimerRemove = FindButtonWithTag(
        TimerAuthored, TEXT("dynamic-fragment-remove"));
    const TSharedPtr<SButton> HeldJoltMark = FindButtonWithTag(
        JoltAuthored, TEXT("dynamic-fragment-mark-rep-dirty"));
    if (NOT TestTrue(TEXT("authored sections mount physical Remove and replication-dirty actions"),
        HeldTimerRemove.IsValid() && HeldTimerRemove->IsEnabled()
            && HeldJoltMark.IsValid() && HeldJoltMark->IsEnabled()))
    { return false; }

    const int64 TimerRevision = TimerView->GetRevision();
    const int64 JoltRevision = JoltView->GetRevision();
    const TSharedRef<SWidget> JoltMain = JoltView->GetRegion(TEXT("main"));
    TestTrue(TEXT("compatible reload retains the Timer view and physical action"),
        TimerView->TryReload(Markup, Stylesheet, TEXT("Dynamic Fragments compatible candidate")).Succeeded
            && TimerAuthored->Get_View() == TimerView && TimerView->GetRevision() > TimerRevision
            && FindButtonWithTag(TimerAuthored, TEXT("dynamic-fragment-remove")) == HeldTimerRemove
            && JoltView->GetRevision() == JoltRevision);
    TestFalse(TEXT("missing property collection binding is rejected atomically"),
        JoltView->TryReload(
            Markup.Replace(TEXT("bind=\"dynamic-fragment-properties\""),
                TEXT("bind=\"dynamic-fragment-missing-properties\"")),
            Stylesheet, TEXT("Dynamic Fragments rejected collection candidate")).Succeeded);
    TestFalse(TEXT("missing Remove action binding is rejected atomically"),
        JoltView->TryReload(
            Markup.Replace(TEXT("action=\"dynamic-fragment-remove\""),
                TEXT("action=\"dynamic-fragment-missing-remove\"")),
            Stylesheet, TEXT("Dynamic Fragments rejected action candidate")).Succeeded);
    TestTrue(TEXT("rejected reloads retain the Jolt tree and revision"),
        &JoltView->GetRegion(TEXT("main")).Get() == &JoltMain.Get()
            && JoltView->GetRevision() == JoltRevision);

    Entity.Try_Remove<ck::FTag_DynamicFragment_MayRequireReplication>();
    HeldJoltMark->SimulateClick();
    TestTrue(TEXT("physical replication-dirty action routes the authority-only public mutation"),
        Entity.Has<ck::FTag_DynamicFragment_MayRequireReplication>()
            && UCk_Utils_DynamicFragment_UE::Has_Fragment(Entity, JoltType)
            && UCk_Utils_DynamicFragment_UE::Has_Fragment(Entity, TimerType));

    Entity.Try_Remove<ck::FTag_DynamicFragment_MayRequireReplication>();
    Entity.Replace<TWeakObjectPtr<UWorld>>();
    HeldJoltMark->SlatePrepass();
    TestFalse(TEXT("worldless entity disables the physical authority-only action fail-closed"),
        HeldJoltMark->IsEnabled());
    HeldJoltMark->SimulateClick();
    TestFalse(TEXT("worldless physical action cannot publish replication-dirty mutation"),
        Entity.Has<ck::FTag_DynamicFragment_MayRequireReplication>());
    Entity.Replace<TWeakObjectPtr<UWorld>>(TestWorld);
    HeldJoltMark->SlatePrepass();
    TestTrue(TEXT("restoring the standalone world re-enables the physical authority-only action"),
        HeldJoltMark->IsEnabled());

    HeldTimerRemove->SimulateClick();
    TestTrue(TEXT("physical Remove targets only its exact dynamic fragment type synchronously"),
        NOT UCk_Utils_DynamicFragment_UE::Has_Fragment(Entity, TimerType)
            && UCk_Utils_DynamicFragment_UE::Has_Fragment(Entity, JoltType));
    HeldTimerRemove->SlatePrepass();
    TestFalse(TEXT("retained Remove action is disabled after its fragment disappears"),
        HeldTimerRemove->IsEnabled());
    HeldTimerRemove->SimulateClick();
    TestTrue(TEXT("stale Remove cannot affect an independent surviving fragment"),
        UCk_Utils_DynamicFragment_UE::Has_Fragment(Entity, JoltType));
    Inspector.Tick(Entity, 0.0f);
    TestTrue(TEXT("fragment membership change requests structural inspector rebuild"), Inspector.NeedsRebuild());

    TSharedPtr<SCkInspector_DynamicFragmentAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_DynamicFragments>();
        const auto DestructorSections = DestructorInspector->Get_InspectorSections(Entity);
        if (NOT TestTrue(TEXT("destructor fixture mounts the surviving authored fragment section"),
            DestructorSections.Num() == 1
                && DestructorSections[0].Widget->GetTypeAsString() == TEXT("SCkInspector_DynamicFragmentAuthored")))
        { return false; }
        DestructorAuthored = StaticCastSharedRef<SCkInspector_DynamicFragmentAuthored>(DestructorSections[0].Widget);
    }
    TestTrue(TEXT("inspector destruction releases its retained authored fragment section"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert()
            && NOT DestructorAuthored->Is_Mounted()
            && NOT DestructorAuthored->Get_View().IsValid()
            && NOT DestructorAuthored->Get_Properties().IsValid());

    Entity.AddOrGet<ck::FTag_DestroyEntity_Initiate>();
    Entity.Try_Remove<ck::FTag_DynamicFragment_MayRequireReplication>();
    HeldJoltMark->SlatePrepass();
    TestFalse(TEXT("pending destruction disables the retained replication action"), HeldJoltMark->IsEnabled());
    HeldJoltMark->SimulateClick();
    TestFalse(TEXT("pending destruction prevents retained action mutation"),
        Entity.Has<ck::FTag_DynamicFragment_MayRequireReplication>());

    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation releases every retained authored fragment section"),
        TimerAuthored->Is_Inert() && JoltAuthored->Is_Inert()
            && NOT TimerAuthored->Is_Mounted() && NOT JoltAuthored->Is_Mounted()
            && NOT TimerAuthored->Get_View().IsValid() && NOT JoltAuthored->Get_View().IsValid()
            && NOT TimerAuthored->Get_Properties().IsValid() && NOT JoltAuthored->Get_Properties().IsValid());
    Entity.Try_Remove<ck::FTag_DestroyEntity_Initiate>();
    HeldTimerRemove->SimulateClick();
    HeldJoltMark->SimulateClick();
    TestTrue(TEXT("held physical actions remain inert after deactivation even when destruction clears"),
        UCk_Utils_DynamicFragment_UE::Has_Fragment(Entity, JoltType)
            && NOT Entity.Has<ck::FTag_DynamicFragment_MayRequireReplication>());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
