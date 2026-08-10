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

    // EditControlStyle — the axis the interactive inspector rows compose through. Both predicates are
    // total over the enum, and the two of them must never both be true (a hidden control cannot also
    // be a hover-revealed one).
    const auto EditStyles = Get_AllOptions<ECkDebugAxis_EditControlStyle>();
    TestEqual(TEXT("EditControlStyle option count"), EditStyles.Num(), 3);

    for (const auto Option : EditStyles)
    {
        auto Selection = FCkDebuggerStyleSelection{};
        Selection.EditControlStyle = Option;

        const auto AreVisible   = ck::debug_axes::EditControls_AreVisible(Selection);
        const auto RevealOnHover = ck::debug_axes::EditControls_RevealOnHover(Selection);
        const auto Name = Get_OptionName(
            static_cast<int64>(Option), StaticEnum<ECkDebugAxis_EditControlStyle>());

        TestTrue(
            *ck::Format_UE(TEXT("'{}' is visible unless it is Hidden"), Name),
            AreVisible == (Option != ECkDebugAxis_EditControlStyle::Hidden));

        TestTrue(
            *ck::Format_UE(TEXT("'{}' reveals on hover only in OnHover"), Name),
            RevealOnHover == (Option == ECkDebugAxis_EditControlStyle::OnHover));

        TestFalse(
            *ck::Format_UE(TEXT("'{}' cannot both hide and hover-reveal"), Name),
            RevealOnHover && NOT AreVisible);
    }

    // Inline is the default: an inspector with no user configuration is interactive out of the box.
    TestTrue(TEXT("the default selection shows edit controls inline"),
        ck::debug_axes::EditControls_AreVisible(FCkDebuggerStyleSelection{}) &&
        NOT ck::debug_axes::EditControls_RevealOnHover(FCkDebuggerStyleSelection{}));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerStyle_TreeComplexityIsTotal,
    "Ck.DebuggerCommon.Style.TreeComplexityIsTotal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_TreeComplexityIsTotal::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_style_tests;

    const auto* ComplexityEnum = StaticEnum<ECkDebugAxis_TreeComplexity>();
    const auto Levels = Get_AllOptions<ECkDebugAxis_TreeComplexity>();

    TestEqual(TEXT("TreeComplexity option count"), Levels.Num(), 3);

    // Every level must yield a usable modulation — a zero/negative multiplier or a zero badge cap
    // would silently erase rows or badges instead of decluttering them.
    constexpr auto BaseCap = 6;

    for (const auto Option : Levels)
    {
        auto Selection = FCkDebuggerStyleSelection{};
        Selection.TreeComplexity = Option;

        const auto Name       = Get_OptionName(static_cast<int64>(Option), ComplexityEnum);
        const auto Multiplier = ck::debug_axes::Tree_FoldThresholdMultiplier(Selection);
        const auto BadgeCap   = ck::debug_axes::Tree_BadgeCap(Selection, BaseCap);

        TestTrue(
            *ck::Format_UE(TEXT("TreeComplexity '{}' yields a positive threshold multiplier"), Name),
            Multiplier > 0.0f);

        TestTrue(
            *ck::Format_UE(TEXT("TreeComplexity '{}' yields a badge cap of at least one"), Name),
            BadgeCap >= 1);

        // Grouping nameless rows IS grouping — the two predicates can never disagree.
        TestTrue(
            *ck::Format_UE(TEXT("TreeComplexity '{}' only groups unnamed rows when it groups at all"), Name),
            NOT ck::debug_axes::Tree_GroupsUnnamedRows(Selection)
                || ck::debug_axes::Tree_GroupsSiblings(Selection));
    }

    // A nonsense base cap still resolves to something renderable at every level.
    for (const auto Option : Levels)
    {
        auto Selection = FCkDebuggerStyleSelection{};
        Selection.TreeComplexity = Option;

        TestTrue(TEXT("A zero base cap still yields at least one badge slot"),
            ck::debug_axes::Tree_BadgeCap(Selection, 0) >= 1);
        TestTrue(TEXT("A negative base cap still yields at least one badge slot"),
            ck::debug_axes::Tree_BadgeCap(Selection, -4) >= 1);
    }

    // THE REGRESSION BAR: Normal is the default and must be a no-op on every modulator, so an
    // untouched axis leaves the entity tree exactly as it shipped.
    const auto Defaults = FCkDebuggerStyleSelection{};

    TestTrue(TEXT("Normal is the default level"),
        Defaults.TreeComplexity == ECkDebugAxis_TreeComplexity::Normal);
    TestEqual(TEXT("Normal leaves the project's fold threshold alone"),
        ck::debug_axes::Tree_FoldThresholdMultiplier(Defaults), 1.0f);
    TestEqual(TEXT("Normal leaves the caller's badge cap alone"),
        ck::debug_axes::Tree_BadgeCap(Defaults, BaseCap), BaseCap);
    TestFalse(TEXT("Normal leaves internal rows to the project's fold setting"),
        ck::debug_axes::Tree_ShowsInternalRows(Defaults));
    TestTrue(TEXT("Normal leaves sibling grouping to the project's setting"),
        ck::debug_axes::Tree_GroupsSiblings(Defaults));
    TestFalse(TEXT("Normal does not regroup unnamed rows"),
        ck::debug_axes::Tree_GroupsUnnamedRows(Defaults));

    // Full is the "hide nothing" end of the dial: no folding, no grouping.
    auto Full = FCkDebuggerStyleSelection{};
    Full.TreeComplexity = ECkDebugAxis_TreeComplexity::Full;

    TestTrue(TEXT("Full presents internal rows plainly"), ck::debug_axes::Tree_ShowsInternalRows(Full));
    TestFalse(TEXT("Full never coalesces siblings"), ck::debug_axes::Tree_GroupsSiblings(Full));
    TestTrue(TEXT("Full never reduces the badge budget"),
        ck::debug_axes::Tree_BadgeCap(Full, BaseCap) >= BaseCap);

    // Minimal is the "show less" end: folds sooner, fewer badges, still nothing outright removed.
    auto Minimal = FCkDebuggerStyleSelection{};
    Minimal.TreeComplexity = ECkDebugAxis_TreeComplexity::Minimal;

    TestTrue(TEXT("Minimal coalesces sooner than the project threshold"),
        ck::debug_axes::Tree_FoldThresholdMultiplier(Minimal) < 1.0f);
    TestTrue(TEXT("Minimal shows no more badges than Normal"),
        ck::debug_axes::Tree_BadgeCap(Minimal, BaseCap) <= BaseCap);
    TestFalse(TEXT("Minimal still folds internals rather than showing them plainly"),
        ck::debug_axes::Tree_ShowsInternalRows(Minimal));
    TestTrue(TEXT("Minimal groups unnamed rows"), ck::debug_axes::Tree_GroupsUnnamedRows(Minimal));

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
