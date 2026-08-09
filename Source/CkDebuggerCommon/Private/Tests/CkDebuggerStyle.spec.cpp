#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugger_style_tests
{
    // Every enumerator of InEnumType, minus the hidden UHT-generated _MAX entry.
    template <typename T_Axis>
    auto Get_AllOptions() -> TArray<T_Axis>
    {
        const auto* Enum = StaticEnum<T_Axis>();

        auto Options = TArray<T_Axis>{};
        if (Enum == nullptr)
        { return Options; }

        for (auto Index = 0; Index < Enum->NumEnums() - 1; ++Index)
        {
            Options.Add(static_cast<T_Axis>(Enum->GetValueByIndex(Index)));
        }
        return Options;
    }

    auto Get_OptionName(int64 InValue, const UEnum* InEnum) -> FString
    {
        return InEnum == nullptr ? FString{TEXT("?")} : InEnum->GetNameStringByValue(InValue);
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerStyle_ProfileRegistryIntegrity,
    "Ck.DebuggerCommon.Style.ProfileRegistryIntegrity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_ProfileRegistryIntegrity::RunTest(const FString& Parameters)
{
    const auto& Profiles = ck::debug_axes::Get_StyleProfiles();

    TestEqual(TEXT("Four curated profiles are registered"), Profiles.Num(), 4);

    if (Profiles.Num() == 0)
    { return false; }

    TestEqual(TEXT("Classic is first"), Profiles[0].Name, FString{TEXT("Classic")});

    auto SeenNames = TSet<FString>{};
    for (const auto& Profile : Profiles)
    {
        TestFalse(
            *ck::Format_UE(TEXT("Profile name '{}' is unique"), Profile.Name),
            SeenNames.Contains(Profile.Name));
        SeenNames.Add(Profile.Name);

        TestFalse(*ck::Format_UE(TEXT("Profile '{}' has a blurb"), Profile.Name), Profile.Blurb.IsEmpty());
    }

    // The identity that makes "reset to Classic" and "restore defaults" the same operation. Compared
    // through reflection so a newly added axis is covered without touching this test.
    const auto Defaults = FCkDebuggerStyleSelection{};
    const auto* SelectionStruct = FCkDebuggerStyleSelection::StaticStruct();

    auto ComparedAxes = 0;
    for (TFieldIterator<FProperty> It{SelectionStruct}; It; ++It)
    {
        const auto* Property = *It;
        const auto* Lhs = Property->ContainerPtrToValuePtr<void>(&Defaults);
        const auto* Rhs = Property->ContainerPtrToValuePtr<void>(&Profiles[0].Selection);

        TestTrue(
            *ck::Format_UE(TEXT("Classic matches the default selection on axis '{}'"), Property->GetName()),
            Property->Identical(Lhs, Rhs));

        ++ComparedAxes;
    }

    TestTrue(TEXT("The reflection walk actually visited axes"), ComparedAxes > 0);

    // Every non-Classic profile must actually differ, otherwise the registry entry is dead weight.
    for (auto Index = 1; Index < Profiles.Num(); ++Index)
    {
        auto AnyDifference = false;
        for (TFieldIterator<FProperty> It{SelectionStruct}; It; ++It)
        {
            const auto* Property = *It;
            if (NOT Property->Identical(
                Property->ContainerPtrToValuePtr<void>(&Defaults),
                Property->ContainerPtrToValuePtr<void>(&Profiles[Index].Selection)))
            { AnyDifference = true; break; }
        }

        TestTrue(
            *ck::Format_UE(TEXT("Profile '{}' differs from Classic"), Profiles[Index].Name),
            AnyDifference);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerStyle_MetricsAndPredicatesAreTotal,
    "Ck.DebuggerCommon.Style.MetricsAndPredicatesAreTotal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_MetricsAndPredicatesAreTotal::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_style_tests;

    const auto* DensityEnum   = StaticEnum<ECkDebugAxis_RowDensity>();
    const auto* IconEnum      = StaticEnum<ECkDebugAxis_IconSize>();
    const auto* SeparatorEnum = StaticEnum<ECkDebugAxis_SeparatorWeight>();

    const auto Densities   = Get_AllOptions<ECkDebugAxis_RowDensity>();
    const auto IconSizes   = Get_AllOptions<ECkDebugAxis_IconSize>();
    const auto Separators  = Get_AllOptions<ECkDebugAxis_SeparatorWeight>();

    TestEqual(TEXT("RowDensity option count"), Densities.Num(), 3);
    TestEqual(TEXT("IconSize option count"), IconSizes.Num(), 3);
    TestEqual(TEXT("SeparatorWeight option count"), Separators.Num(), 4);

    for (const auto Option : Densities)
    {
        auto Selection = FCkDebuggerStyleSelection{};
        Selection.RowDensity = Option;

        const auto Padding = ck::debug_axes::Get_RowPadding(Selection);
        const auto Name    = Get_OptionName(static_cast<int64>(Option), DensityEnum);

        TestTrue(
            *ck::Format_UE(TEXT("RowDensity '{}' yields non-negative padding"), Name),
            Padding.Left >= 0.0f && Padding.Top >= 0.0f && Padding.Right >= 0.0f && Padding.Bottom >= 0.0f);
    }

    const auto ValidIconSizes = TArray<float>{12.0f, 16.0f, 20.0f};
    for (const auto Option : IconSizes)
    {
        auto Selection = FCkDebuggerStyleSelection{};
        Selection.IconSize = Option;

        const auto Size = ck::debug_axes::Get_IconSize(Selection);
        const auto Name = Get_OptionName(static_cast<int64>(Option), IconEnum);

        TestTrue(
            *ck::Format_UE(TEXT("IconSize '{}' resolves to a catalog size"), Name),
            ValidIconSizes.Contains(Size));
    }

    const auto ValidThicknesses = TArray<float>{0.0f, 1.0f, 2.0f, 3.0f};
    for (const auto Option : Separators)
    {
        auto Selection = FCkDebuggerStyleSelection{};
        Selection.SeparatorWeight = Option;

        const auto Thickness = ck::debug_axes::Get_SeparatorThickness(Selection);
        const auto Name      = Get_OptionName(static_cast<int64>(Option), SeparatorEnum);

        TestTrue(
            *ck::Format_UE(TEXT("SeparatorWeight '{}' resolves to a catalog thickness"), Name),
            ValidThicknesses.Contains(Thickness));
    }

    auto NoSeparator = FCkDebuggerStyleSelection{};
    NoSeparator.SeparatorWeight = ECkDebugAxis_SeparatorWeight::None;
    TestEqual(TEXT("None separator draws nothing"), ck::debug_axes::Get_SeparatorThickness(NoSeparator), 0.0f);

    for (const auto Option : Get_AllOptions<ECkDebugAxis_LegendMode>())
    {
        auto Selection = FCkDebuggerStyleSelection{};
        Selection.LegendMode = Option;

        const auto IsVisible = ck::debug_axes::Legend_IsVisible(Selection);
        const auto IsDeduped = ck::debug_axes::Legend_IsDeduped(Selection);

        TestTrue(TEXT("A deduped legend is by definition a visible legend"), NOT IsDeduped || IsVisible);
        TestTrue(TEXT("Off is the only invisible legend mode"),
            IsVisible == (Option != ECkDebugAxis_LegendMode::Off));
    }

    for (const auto Option : Get_AllOptions<ECkDebugAxis_ValueAlignment>())
    {
        auto Selection = FCkDebuggerStyleSelection{};
        Selection.ValueAlignment = Option;

        const auto Aligned = ck::debug_axes::Values_UseAlignedColumns(Selection);
        const auto Right   = ck::debug_axes::Values_AlignRight(Selection);

        TestFalse(TEXT("Aligned columns and plain right-align are mutually exclusive"), Aligned && Right);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerStyle_RevisionBumpsOnNotifyChanged,
    "Ck.DebuggerCommon.Style.RevisionBumpsOnNotifyChanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_RevisionBumpsOnNotifyChanged::RunTest(const FString& Parameters)
{
    auto* Settings = UCkDebuggerStyleSettings::Get_Mutable();

    if (Settings == nullptr)
    {
        AddError(TEXT("UCkDebuggerStyleSettings default object is unavailable"));
        return false;
    }

    TestTrue(TEXT("Get and Get_Mutable resolve to the same object"),
        Settings == UCkDebuggerStyleSettings::Get());

    const auto Before = Settings->Get_Revision();

    Settings->NotifyChanged();
    TestTrue(TEXT("NotifyChanged bumps the revision"), Settings->Get_Revision() == Before + 1);

    Settings->NotifyChanged();
    Settings->NotifyChanged();
    TestTrue(TEXT("Every call bumps"), Settings->Get_Revision() == Before + 3);

    TestEqual(TEXT("Schema version is the current catalog version"),
        Settings->SchemaVersion, UCkDebuggerStyleSettings::CurrentSchemaVersion);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerStyle_MergeCountVisibility,
    "Ck.DebuggerCommon.Style.MergeCountVisibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_MergeCountVisibility::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_style_tests;

    for (const auto Option : Get_AllOptions<ECkDebugAxis_MergeCountDisplay>())
    {
        auto Selection = FCkDebuggerStyleSelection{};
        Selection.MergeCountDisplay = Option;

        const auto Name = Get_OptionName(
            static_cast<int64>(Option), StaticEnum<ECkDebugAxis_MergeCountDisplay>());

        TestFalse(
            *ck::Format_UE(TEXT("'{}' renders nothing for an unmerged row"), Name),
            ck::debug_axes::Make_MergeCount(Selection, 1).IsValid());

        TestFalse(
            *ck::Format_UE(TEXT("'{}' renders nothing for a zero count"), Name),
            ck::debug_axes::Make_MergeCount(Selection, 0).IsValid());

        const auto Merged = ck::debug_axes::Make_MergeCount(Selection, 4);
        const auto ExpectVisible = Option != ECkDebugAxis_MergeCountDisplay::Hidden;

        TestTrue(
            *ck::Format_UE(TEXT("'{}' visibility for a merged row"), Name),
            Merged.IsValid() == ExpectVisible);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
