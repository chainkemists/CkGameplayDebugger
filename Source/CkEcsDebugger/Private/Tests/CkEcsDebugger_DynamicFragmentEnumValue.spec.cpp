#include "Misc/AutomationTest.h"

#include "CkEcsDebugger/Inspectors/CkInspector_DynamicFragments.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDynamic/CkDynamic_Utils.h"
#include "CkDynamic/CkDynamic_FragmentDisplaySchema.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkJolt/Query/CkJoltQuery_Data.h"
#include "CkTimer/CkTimer_Fragment_Data.h"

#include "Engine/EngineTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UnrealType.h"
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
    auto Sections = Inspector.Get_InspectorSections(Entity);

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

    Sections = Inspector.Get_InspectorSections(Entity);

    const auto ExpectedByteDisplayName = ck::dynamic::Resolve_EnumValueDisplayName(
        ByteFragmentType, ByteProperty->Enum, ByteValue);
    const auto ExpectedBytePropertyName = ck::dynamic::Resolve_PropertyDisplayName(ByteFragmentType, ByteProperty);
    TestTrue(TEXT("the Channel row retains FByteProperty enum rendering"),
        AnySectionHasKeyValuePair(Sections, ExpectedBytePropertyName, ExpectedByteDisplayName));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
