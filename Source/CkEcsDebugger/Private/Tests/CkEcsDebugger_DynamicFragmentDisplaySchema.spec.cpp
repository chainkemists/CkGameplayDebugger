#include "Misc/AutomationTest.h"

#include "CkDynamic/CkDynamic_Fragment_Data.h"
#include "CkDynamic/CkDynamic_FragmentDisplaySchema.h"

#include "UObject/UnrealType.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerDynamicFragmentDisplaySchema_UsesCookedSchemaLabels,
    "Ck.EcsDebugger.DynamicFragments.UsesCookedSchemaLabels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

bool FCkEcsDebuggerDynamicFragmentDisplaySchema_UsesCookedSchemaLabels::RunTest(const FString&)
{
    const auto* FragmentType = FCk_Fragment_DynamicFragment_Data::StaticStruct();
    const auto* Property = FindFProperty<FProperty>(FragmentType, TEXT("_StructData"));
    const auto* Enum = StaticEnum<ECk_DestroyFilter>();

    TestNotNull(TEXT("Dynamic Fragment type is reflected"), FragmentType);
    TestNotNull(TEXT("Dynamic Fragment property is reflected"), Property);
    TestNotNull(TEXT("Dynamic Fragment enum is reflected"), Enum);
    if (FragmentType == nullptr || Property == nullptr || Enum == nullptr)
    { return false; }

    auto Schema = ck::dynamic::FFragmentDisplaySchema{};
    Schema.FragmentDisplayName = TEXT("QA / not a prettified fragment");
    Schema.PropertyDisplayNames.Add(Property->GetFName(), TEXT("Payload (raw, do not beautify)"));
    Schema.EnumValueDisplayNames.FindOrAdd(Enum->GetPathName()).Add(
        static_cast<int64>(ECk_DestroyFilter::Teardown), TEXT("Tear it down, now"));

    const auto FragmentTypePath = FragmentType->GetPathName();
    auto PreviousSchema = ck::dynamic::FFragmentDisplaySchema{};
    const auto HadPreviousSchema =
        ck::dynamic::TryGet_NativeFragmentDisplaySchema(FragmentTypePath, PreviousSchema);
    const auto Registered = ck::dynamic::Register_NativeFragmentDisplaySchema(FragmentTypePath, MoveTemp(Schema));
    TestTrue(TEXT("Custom Dynamic Fragment display schema registers"), Registered);
    if (NOT Registered)
    { return false; }

    TestEqual(TEXT("Fragment label keeps its authored punctuation"),
        ck::dynamic::Resolve_FragmentDisplayName(FragmentType), FString{TEXT("QA / not a prettified fragment")});
    TestEqual(TEXT("Property label keeps its authored punctuation"),
        ck::dynamic::Resolve_PropertyDisplayName(FragmentType, Property), FString{TEXT("Payload (raw, do not beautify)")});
    TestEqual(TEXT("Enum label keeps its authored punctuation"),
        ck::dynamic::Resolve_EnumValueDisplayName(FragmentType, Enum, static_cast<int64>(ECk_DestroyFilter::Teardown)),
        FString{TEXT("Tear it down, now")});

    const auto* FallbackType = FCk_DynamicFragment_SnapshotTransient::StaticStruct();
    TestNotNull(TEXT("Fallback Dynamic Fragment type is reflected"), FallbackType);
    if (FallbackType != nullptr)
    {
        const auto ExpectedFallback = FName::NameToDisplayString(FallbackType->GetName(), false);
        TestEqual(TEXT("Unregistered fragment fallback is deterministic"),
            ck::dynamic::Resolve_FragmentDisplayName(FallbackType), ExpectedFallback);
        TestEqual(TEXT("Unregistered fragment fallback remains deterministic"),
            ck::dynamic::Resolve_FragmentDisplayName(FallbackType), ExpectedFallback);
    }

    TestTrue(TEXT("Custom Dynamic Fragment display schema unregisters"),
        ck::dynamic::Unregister_NativeFragmentDisplaySchema(FragmentTypePath));
    if (HadPreviousSchema)
    {
        TestTrue(TEXT("Prior Dynamic Fragment display schema is restored"),
            ck::dynamic::Register_NativeFragmentDisplaySchema(FragmentTypePath, MoveTemp(PreviousSchema)));
    }

    return FallbackType != nullptr;
}

// --------------------------------------------------------------------------------------------------------------------
