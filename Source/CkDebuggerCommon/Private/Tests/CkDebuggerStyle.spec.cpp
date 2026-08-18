#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Styling/AppStyle.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#include "Widgets/SWidget.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugger_style_tests
{
    // Specs must never leak style state — a later spec (or the user's editor session) would inherit
    // whatever axis this one was mid-way through flipping. The WHOLE selection plus the profile name
    // are captured and put back, so an early `return false` cannot poison the rest of the run.
    struct FScopedStyleSelection
    {
        FScopedStyleSelection()
        {
            if (const auto* Settings = UCkDebuggerStyleSettings::Get())
            {
                _Selection   = Settings->Selection;
                _ProfileName = Settings->ActiveProfileName;
            }
        }

        ~FScopedStyleSelection()
        {
            if (auto* Settings = UCkDebuggerStyleSettings::Get_Mutable())
            {
                Settings->Selection          = _Selection;
                Settings->ActiveProfileName  = _ProfileName;
                Settings->NotifyChanged();
            }
        }

    private:
        FCkDebuggerStyleSelection _Selection;
        FString                   _ProfileName;
    };

    // The live-ness probe. The widget is NEVER recreated between calls — only the settings CDO
    // moves — so a size change is proof the widget re-read the axis through its own attributes.
    auto Repass(const TSharedRef<SWidget>& InWidget) -> FVector2D
    {
        InWidget->MarkPrepassAsDirty();
        InWidget->SlatePrepass();

        const auto Size = InWidget->GetDesiredSize();
        return FVector2D{Size.X, Size.Y};
    }

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

    if (Profiles.Num() != 4)
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

    // The catalog size is asserted explicitly so an accidental axis drop is caught here rather than
    // silently narrowing every reflection-driven surface (the Style Lab's controls pane included).
    TestEqual(TEXT("The axis catalog holds every declared axis"), ComparedAxes, 24);

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
    FCkDebuggerStyle_GraphMotionSelection,
    "Ck.DebuggerCommon.Style.GraphMotionSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_GraphMotionSelection::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_style_tests;

    const auto Defaults = FCkDebuggerStyleSelection{};
    TestEqual(TEXT("Graph Motion defaults to Measured"),
        Defaults.GraphMotion, ECkDebugAxis_GraphMotion::Measured);

    const auto Options = Get_AllOptions<ECkDebugAxis_GraphMotion>();
    TestEqual(TEXT("Graph Motion exposes three pacing choices"), Options.Num(), 3);
    TestTrue(TEXT("Graph Motion includes Quick"), Options.Contains(ECkDebugAxis_GraphMotion::Quick));
    TestTrue(TEXT("Graph Motion includes Measured"), Options.Contains(ECkDebugAxis_GraphMotion::Measured));
    TestTrue(TEXT("Graph Motion includes Deliberate"), Options.Contains(ECkDebugAxis_GraphMotion::Deliberate));

    TestEqual(TEXT("Current style schema retains the retired Input HUD axis migration"),
        UCkDebuggerStyleSettings::CurrentSchemaVersion, 9);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerStyle_GraphEventEmphasisSelection,
    "Ck.DebuggerCommon.Style.GraphEventEmphasisSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_GraphEventEmphasisSelection::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_style_tests;

    const auto Defaults = FCkDebuggerStyleSelection{};
    TestEqual(TEXT("Graph Event Emphasis defaults to Clear"),
              Defaults.GraphEventEmphasis,
              ECkDebugAxis_GraphEventEmphasis::Clear);

    const auto Options = Get_AllOptions<ECkDebugAxis_GraphEventEmphasis>();
    TestEqual(TEXT("Graph Event Emphasis exposes three choices"), Options.Num(), 3);
    TestTrue(TEXT("Graph Event Emphasis includes Subtle"),
             Options.Contains(ECkDebugAxis_GraphEventEmphasis::Subtle));
    TestTrue(TEXT("Graph Event Emphasis includes Clear"),
             Options.Contains(ECkDebugAxis_GraphEventEmphasis::Clear));
    TestTrue(TEXT("Graph Event Emphasis includes Bold"),
             Options.Contains(ECkDebugAxis_GraphEventEmphasis::Bold));

    const auto* Property = FindFProperty<FProperty>(
        FCkDebuggerStyleSelection::StaticStruct(), TEXT("GraphEventEmphasis"));
    TestNotNull(TEXT("Graph Event Emphasis remains a reflected config property"), Property);
    if (Property == nullptr)
    {
        return false;
    }

    auto Source = FCkDebuggerStyleSelection{};
    Source.GraphEventEmphasis = ECkDebugAxis_GraphEventEmphasis::Bold;
    auto Serialized = FString{};
    Property->ExportTextItem_Direct(
        Serialized,
        Property->ContainerPtrToValuePtr<void>(&Source),
        nullptr,
        nullptr,
        PPF_None);
    TestEqual(TEXT("Graph Event Emphasis serializes by stable enum name"),
              Serialized,
              FString{TEXT("Bold")});

    auto RoundTripped = FCkDebuggerStyleSelection{};
    const auto* Remainder = Property->ImportText_Direct(
        *Serialized,
        Property->ContainerPtrToValuePtr<void>(&RoundTripped),
        nullptr,
        PPF_None);
    TestNotNull(TEXT("Graph Event Emphasis deserializes from its enum name"), Remainder);
    TestEqual(TEXT("Graph Event Emphasis survives config serialization"),
              RoundTripped.GraphEventEmphasis,
              ECkDebugAxis_GraphEventEmphasis::Bold);
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

    const auto Guard = FScopedStyleSelection{};

    auto* Settings = UCkDebuggerStyleSettings::Get_Mutable();

    if (Settings == nullptr)
    {
        AddError(TEXT("UCkDebuggerStyleSettings default object is unavailable"));
        return false;
    }

    for (const auto Option : Get_AllOptions<ECkDebugAxis_MergeCountDisplay>())
    {
        auto Selection = FCkDebuggerStyleSelection{};
        Selection.MergeCountDisplay = Option;

        const auto Name = Get_OptionName(
            static_cast<int64>(Option), StaticEnum<ECkDebugAxis_MergeCountDisplay>());

        // Null is DATA-driven only — an unmerged row genuinely has no affordance to place.
        TestFalse(
            *ck::Format_UE(TEXT("'{}' renders nothing for an unmerged row"), Name),
            ck::debug_axes::Make_MergeCount(Selection, 1).IsValid());

        TestFalse(
            *ck::Format_UE(TEXT("'{}' renders nothing for a zero count"), Name),
            ck::debug_axes::Make_MergeCount(Selection, 0).IsValid());

        // A merged row ALWAYS gets a widget now, whatever the caller's snapshot says — the axis
        // lives on the widget, not in the call. Hidden is a collapsed widget, not a null one.
        TestTrue(
            *ck::Format_UE(TEXT("'{}' always emits a widget for a merged row"), Name),
            ck::debug_axes::Make_MergeCount(Selection, 4).IsValid());
    }

    // ---- Live-ness: one widget, three axis states, no rebuild ----------------
    Settings->Selection.MergeCountDisplay = ECkDebugAxis_MergeCountDisplay::SuffixText;

    const auto MergeCount = ck::debug_axes::Make_MergeCount(UCkDebuggerStyleSettings::Get_Selection(), 4);

    if (NOT MergeCount.IsValid())
    {
        AddError(TEXT("Make_MergeCount returned nothing for a merged row"));
        return false;
    }

    const auto AsSuffix = Repass(MergeCount.ToSharedRef());
    TestTrue(TEXT("SuffixText occupies space"), AsSuffix.X > 0.0f);

    Settings->Selection.MergeCountDisplay = ECkDebugAxis_MergeCountDisplay::Hidden;
    const auto AsHidden = Repass(MergeCount.ToSharedRef());

    TestTrue(
        TEXT("Flipping to Hidden collapses the SAME widget — the option is live, not baked"),
        AsHidden.X == 0.0f && AsHidden.Y == 0.0f);

    Settings->Selection.MergeCountDisplay = ECkDebugAxis_MergeCountDisplay::CountBadge;
    const auto AsBadge = Repass(MergeCount.ToSharedRef());

    TestTrue(TEXT("Flipping back to CountBadge re-expands the same widget"), AsBadge.X > 0.0f);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerStyle_HelpersAreLive,
    "Ck.DebuggerCommon.Style.HelpersAreLive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_HelpersAreLive::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_style_tests;

    // R1's regression bar, expressed as a test: a widget built under one axis option must FOLLOW a
    // later flip. Every case below builds ONCE, prepasses, flips the settings CDO, and prepasses
    // the same widget again — a size change can only come from the widget re-reading the axis.
    const auto Guard = FScopedStyleSelection{};

    auto* Settings = UCkDebuggerStyleSettings::Get_Mutable();

    if (Settings == nullptr)
    {
        AddError(TEXT("UCkDebuggerStyleSettings default object is unavailable"));
        return false;
    }

    Settings->Selection = FCkDebuggerStyleSelection{};

    const auto Label = FText::FromString(TEXT("transform"));

    // ---- Make_Chip: Tint (boxed) -> TextOnly (no box) ------------------------
    const auto Chip = ck::debug_axes::Make_Chip(
        UCkDebuggerStyleSettings::Get_Selection(), Label, ECk_Tone::Info);

    const auto ChipTint = Repass(Chip);

    Settings->Selection.ChipStyle = ECkDebugAxis_ChipStyle::TextOnly;
    const auto ChipTextOnly = Repass(Chip);

    TestTrue(
        TEXT("ChipStyle TextOnly drops the box padding off an ALREADY-BUILT chip"),
        ChipTextOnly.X < ChipTint.X && ChipTextOnly.Y < ChipTint.Y);

    Settings->Selection.ChipStyle = ECkDebugAxis_ChipStyle::Tint;
    TestTrue(
        TEXT("Flipping back restores the chip's original geometry"),
        FMath::IsNearlyEqual(Repass(Chip).X, ChipTint.X));

    // ---- Make_Badge: Solid (boxed) -> CountOnly (no box) ---------------------
    const auto Badge = ck::debug_axes::Make_Badge(
        UCkDebuggerStyleSettings::Get_Selection(), FText::AsNumber(12), ECk_Tone::Neutral);

    const auto BadgeSolid = Repass(Badge);

    Settings->Selection.BadgeStyle = ECkDebugAxis_BadgeStyle::CountOnly;
    const auto BadgeCountOnly = Repass(Badge);

    TestTrue(
        TEXT("BadgeStyle CountOnly drops the box padding off an ALREADY-BUILT badge"),
        BadgeCountOnly.X < BadgeSolid.X);

    Settings->Selection.BadgeStyle = ECkDebugAxis_BadgeStyle::Solid;

    // ---- Make_SectionHeader: Uppercase (bold, ToUpper) -> Minimal ------------
    // The label is deliberately lowercase: ToUpper makes it measurably wider in any proportional
    // font, and Minimal drops both the transform and the bold face.
    const auto Header = ck::debug_axes::Make_SectionHeader(
        UCkDebuggerStyleSettings::Get_Selection(), Label, ECk_Tone::Info);

    const auto HeaderUppercase = Repass(Header);

    Settings->Selection.SectionHeaderStyle = ECkDebugAxis_SectionHeaderStyle::Minimal;
    const auto HeaderMinimal = Repass(Header);

    TestTrue(
        TEXT("SectionHeaderStyle Minimal restyles an ALREADY-BUILT header"),
        HeaderMinimal.X < HeaderUppercase.X);

    Settings->Selection.SectionHeaderStyle = ECkDebugAxis_SectionHeaderStyle::Uppercase;

    // ---- Make_FoldChip: Chip (padded) -> Text (bare) -------------------------
    const auto FoldChip = ck::debug_axes::Make_FoldChip(
        UCkDebuggerStyleSettings::Get_Selection(), Label, ECk_Tone::Neutral);

    const auto FoldChipBoxed = Repass(FoldChip);

    Settings->Selection.FoldChipStyle = ECkDebugAxis_FoldChipStyle::Text;
    const auto FoldChipBare = Repass(FoldChip);

    TestTrue(
        TEXT("FoldChipStyle Text drops the chip padding off an ALREADY-BUILT fold chip"),
        FoldChipBare.X < FoldChipBoxed.X);

    Settings->Selection.FoldChipStyle = ECkDebugAxis_FoldChipStyle::Chip;

    // ---- Make_ProviderChip: Tint (padded) -> AbbrevOnly (bare, 3 chars) ------
    const auto ProviderChip = ck::debug_axes::Make_ProviderChip(
        UCkDebuggerStyleSettings::Get_Selection(), Label, ECk_Tone::Accent);

    const auto ProviderTint = Repass(ProviderChip);

    Settings->Selection.ProviderChipStyle = ECkDebugAxis_ProviderChipStyle::AbbrevOnly;
    const auto ProviderAbbrev = Repass(ProviderChip);

    TestTrue(
        TEXT("ProviderChipStyle AbbrevOnly reshapes AND abbreviates an ALREADY-BUILT chip"),
        ProviderAbbrev.X < ProviderTint.X);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerStyle_CommonWidgetsAreLive,
    "Ck.DebuggerCommon.Style.CommonWidgetsAreLive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_CommonWidgetsAreLive::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_style_tests;

    const auto Guard = FScopedStyleSelection{};

    auto* Settings = UCkDebuggerStyleSettings::Get_Mutable();

    if (Settings == nullptr)
    {
        AddError(TEXT("UCkDebuggerStyleSettings default object is unavailable"));
        return false;
    }

    Settings->Selection = FCkDebuggerStyleSelection{};

    // ---- SCkDebug_KeyValueRow: RowDensity on THE inspector row ---------------
    // Airy adds (SpaceS - SpaceXS) to the row's vertical padding on both edges. Padding-driven, so
    // the assertion does not depend on font metrics.
    const auto Row = SNew(SCkDebug_KeyValueRow)
        .KeyText(FText::FromString(TEXT("Location")))
        .ValueText(FText::FromString(TEXT("0, 0, 0")))
        .Tone(ECkDebug_KeyValueTone::Custom);

    const auto RowComfortable = Repass(Row);

    Settings->Selection.RowDensity = ECkDebugAxis_RowDensity::Airy;
    const auto RowAiry = Repass(Row);

    TestTrue(
        TEXT("RowDensity Airy loosens an ALREADY-BUILT inspector row"),
        RowAiry.Y > RowComfortable.Y);

    Settings->Selection.RowDensity = ECkDebugAxis_RowDensity::Compact;
    const auto RowCompact = Repass(Row);

    TestTrue(
        TEXT("RowDensity Compact tightens the same row"),
        RowCompact.Y < RowAiry.Y);

    Settings->Selection.RowDensity = ECkDebugAxis_RowDensity::Comfortable;
    TestTrue(
        TEXT("Returning to Comfortable restores the row's shipped height"),
        FMath::IsNearlyEqual(Repass(Row).Y, RowComfortable.Y));

    // ---- SCkDebug_IconToggle: IconSize on the glyph box ----------------------
    const auto Toggle = SNew(SCkDebug_IconToggle)
        .IconId(ECk_Icon::Grid)
        .Label(FText::FromString(TEXT("Live icon")))
        .ShowLabel(false)
        .IsOn(false)
        .OnStateChanged(FOnCkDebug_IconToggleChanged::CreateLambda([](bool) {}));

    const auto ToggleMedium = Repass(Toggle);

    Settings->Selection.IconSize = ECkDebugAxis_IconSize::Large;
    const auto ToggleLarge = Repass(Toggle);

    TestTrue(
        TEXT("IconSize Large grows an ALREADY-BUILT icon toggle"),
        ToggleLarge.X > ToggleMedium.X);

    Settings->Selection.IconSize = ECkDebugAxis_IconSize::Small;
    const auto ToggleSmall = Repass(Toggle);

    TestTrue(
        TEXT("IconSize Small shrinks the same toggle"),
        ToggleSmall.X < ToggleMedium.X);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerStyle_SchemaV4AxesResolveDistinctly,
    "Ck.DebuggerCommon.Style.SchemaV4AxesResolveDistinctly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_SchemaV4AxesResolveDistinctly::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_style_tests;

    // Every resolver added with schema v4 reads the LIVE selection, so the settings CDO is the only
    // knob these cases turn. The guard puts the user's whole selection back afterwards.
    const auto Guard = FScopedStyleSelection{};

    auto* Settings = UCkDebuggerStyleSettings::Get_Mutable();

    if (Settings == nullptr)
    {
        AddError(TEXT("UCkDebuggerStyleSettings default object is unavailable"));
        return false;
    }

    Settings->Selection = FCkDebuggerStyleSelection{};

    // v5 split EntityIdStyle (composition) from EntityRefStyle (treatment); v6 added GraphMotion;
    // v7 added GraphEventEmphasis; v8 temporarily added InputHudStyle; v9 retires it because Signal Strip is
    // feature-local rather than a shared debugger axis.
    // The catalog schema check makes either accidental regression a test failure rather than a silent revert.
    TestEqual(TEXT("Schema version tracks the current axis catalog"),
        UCkDebuggerStyleSettings::CurrentSchemaVersion, 9);

    // ---- TextScale -----------------------------------------------------------
    const auto RoleSize = CkStyle::FontSizeSmall();

    TestEqual(TEXT("Normal is a byte-identical pass-through of the role size"),
        ck::debug_axes::Get_ScaledFontSize(RoleSize), RoleSize);
    TestEqual(TEXT("Normal is a unit multiplier"), ck::debug_axes::Get_TextScale(), 1.0f);

    Settings->Selection.TextScale = ECkDebugAxis_TextScale::Small;
    const auto SmallSize = ck::debug_axes::Get_ScaledFontSize(RoleSize);

    Settings->Selection.TextScale = ECkDebugAxis_TextScale::Large;
    const auto LargeSize = ck::debug_axes::Get_ScaledFontSize(RoleSize);

    TestTrue(TEXT("TextScale orders Small < Normal < Large at the palette's own role size"),
        SmallSize < RoleSize && RoleSize < LargeSize);

    Settings->Selection.TextScale = ECkDebugAxis_TextScale::Large;
    TestEqual(TEXT("ScaledFont carries the scaled size through to the font"),
        ck::debug_axes::ScaledFont("Regular", RoleSize).Size, static_cast<float>(LargeSize));

    Settings->Selection.TextScale = ECkDebugAxis_TextScale::Normal;

    // A nonsense role size still resolves to something renderable.
    TestTrue(TEXT("A zero role size still yields a drawable point size"),
        ck::debug_axes::Get_ScaledFontSize(0) >= 1);

    // ---- CornerStyle ---------------------------------------------------------
    // Rounded must be exactly the brushes the chip / badge / card drew before the axis existed.
    TestTrue(TEXT("Rounded chips keep CkStyle's 6px brush"),
        ck::debug_axes::Get_ChipBrush() == CkStyle::GetRoundedBrush());
    TestTrue(TEXT("Rounded badges keep CkStyle's 3px brush"),
        ck::debug_axes::Get_BadgeBrush() == CkStyle::GetRoundedBrush_Small());
    TestTrue(TEXT("Rounded cards keep CkStyle's 8px brush"),
        ck::debug_axes::Get_CardBrush() == CkStyle::GetRoundedBrush_Large());

    auto CornerBrushes = TSet<const FSlateBrush*>{};
    for (const auto Option : Get_AllOptions<ECkDebugAxis_CornerStyle>())
    {
        Settings->Selection.CornerStyle = Option;

        const auto* Brush = ck::debug_axes::Get_ChipBrush();
        const auto  Name  = Get_OptionName(
            static_cast<int64>(Option), StaticEnum<ECkDebugAxis_CornerStyle>());

        TestNotNull(*ck::Format_UE(TEXT("CornerStyle '{}' resolves to a registered brush"), Name), Brush);
        TestFalse(
            *ck::Format_UE(TEXT("CornerStyle '{}' is a distinct shape"), Name),
            CornerBrushes.Contains(Brush));

        CornerBrushes.Add(Brush);
    }

    Settings->Selection.CornerStyle = ECkDebugAxis_CornerStyle::Rounded;

    // ---- SurfaceElevation ----------------------------------------------------
    // Layered is the ladder every common surface widget drew by hand before the axis existed.
    TestTrue(TEXT("Depth 0 is the window ground"), ck::debug_axes::Get_SurfaceTint(0) == CkStyle::BgRoot());
    TestTrue(TEXT("Layered depth 1 is the strip tier"), ck::debug_axes::Get_SurfaceTint(1) == CkStyle::Bg1());
    TestTrue(TEXT("Layered depth 2 is the body tier"), ck::debug_axes::Get_SurfaceTint(2) == CkStyle::Bg2());
    TestTrue(TEXT("Layered depth 3 is the inset tier"), ck::debug_axes::Get_SurfaceTint(3) == CkStyle::Bg3());
    TestTrue(TEXT("Layered surfaces are plain fills"),
        ck::debug_axes::Get_SurfaceBrush(2) == CkStyle::GetFilledBrush());

    Settings->Selection.SurfaceElevation = ECkDebugAxis_SurfaceElevation::Flat;
    TestTrue(TEXT("Flat collapses the nested tiers onto one fill"),
        ck::debug_axes::Get_SurfaceTint(2) == ck::debug_axes::Get_SurfaceTint(3));

    Settings->Selection.SurfaceElevation = ECkDebugAxis_SurfaceElevation::Outlined;
    TestTrue(TEXT("Outlined swaps the fill brush for the ring brush"),
        ck::debug_axes::Get_SurfaceBrush(2) == FCkDebuggerStyle::Get_SurfaceOutlineBrush());
    TestTrue(TEXT("Outlined leaves the ground opaque so editor chrome cannot read through"),
        ck::debug_axes::Get_SurfaceBrush(0) == CkStyle::GetFilledBrush());

    // Every option is total over any depth a caller could hand over.
    for (const auto Option : Get_AllOptions<ECkDebugAxis_SurfaceElevation>())
    {
        Settings->Selection.SurfaceElevation = Option;

        TestNotNull(TEXT("A negative depth still resolves to a brush"), ck::debug_axes::Get_SurfaceBrush(-3));
        TestNotNull(TEXT("A very deep surface still resolves to a brush"), ck::debug_axes::Get_SurfaceBrush(99));
    }

    Settings->Selection.SurfaceElevation = ECkDebugAxis_SurfaceElevation::Layered;

    // ---- RowBanding ----------------------------------------------------------
    TestEqual(TEXT("Off never draws a rule"), ck::debug_axes::Get_RowBandingRuleThickness(), 0.0f);

    Settings->Selection.RowBanding = ECkDebugAxis_RowBanding::Zebra;
    TestTrue(TEXT("Zebra alternates between two band brushes"),
        ck::debug_axes::Get_RowBandingBrush(0) != ck::debug_axes::Get_RowBandingBrush(1));
    TestTrue(TEXT("Zebra bands repeat every other row"),
        ck::debug_axes::Get_RowBandingBrush(0) == ck::debug_axes::Get_RowBandingBrush(2));
    TestTrue(TEXT("A negative row index still alternates"),
        ck::debug_axes::Get_RowBandingBrush(-1) == ck::debug_axes::Get_RowBandingBrush(1));
    TestEqual(TEXT("Zebra separates by fill, not by a rule"),
        ck::debug_axes::Get_RowBandingRuleThickness(), 0.0f);

    Settings->Selection.RowBanding = ECkDebugAxis_RowBanding::Hairline;
    TestTrue(TEXT("Hairline draws a rule at the separator weight"),
        ck::debug_axes::Get_RowBandingRuleThickness() > 0.0f);

    // Silencing separators has to silence the per-row rule too, or the axis smuggles the line back.
    Settings->Selection.SeparatorWeight = ECkDebugAxis_SeparatorWeight::None;
    TestEqual(TEXT("SeparatorWeight None silences the hairline banding"),
        ck::debug_axes::Get_RowBandingRuleThickness(), 0.0f);

    Settings->Selection.SeparatorWeight = ECkDebugAxis_SeparatorWeight::Hairline;
    Settings->Selection.RowBanding      = ECkDebugAxis_RowBanding::Off;

    // ---- GraphNodeStyle ------------------------------------------------------
    TestEqual(TEXT("Card passes the node border role through untouched"),
        ck::debug_axes::Get_NodeBorderThickness(), CkStyle::NodeBorderThickness());
    TestEqual(TEXT("Card passes the inactive opacity role through untouched"),
        ck::debug_axes::Get_NodeInactiveOpacity(), CkStyle::NodeInactiveOpacity());

    Settings->Selection.GraphNodeStyle = ECkDebugAxis_GraphNodeStyle::Minimal;
    const auto MinimalBorder  = ck::debug_axes::Get_NodeBorderThickness();
    const auto MinimalOpacity = ck::debug_axes::Get_NodeInactiveOpacity();

    Settings->Selection.GraphNodeStyle = ECkDebugAxis_GraphNodeStyle::Dense;
    const auto DenseBorder  = ck::debug_axes::Get_NodeBorderThickness();
    const auto DenseOpacity = ck::debug_axes::Get_NodeInactiveOpacity();

    TestTrue(TEXT("GraphNodeStyle orders Minimal < Card < Dense on border weight"),
        MinimalBorder < CkStyle::NodeBorderThickness() && CkStyle::NodeBorderThickness() < DenseBorder);
    TestTrue(TEXT("Minimal fades inactive nodes harder than Dense"), MinimalOpacity < DenseOpacity);

    for (const auto Option : Get_AllOptions<ECkDebugAxis_GraphNodeStyle>())
    {
        Settings->Selection.GraphNodeStyle = Option;

        TestTrue(TEXT("Every graph node style keeps a drawable border"),
            ck::debug_axes::Get_NodeBorderThickness() > 0.0f);
        TestTrue(TEXT("Every graph node style keeps an inactive node visible"),
            ck::debug_axes::Get_NodeInactiveOpacity() > 0.0f
                && ck::debug_axes::Get_NodeInactiveOpacity() <= 1.0f);
    }

    Settings->Selection.GraphNodeStyle = ECkDebugAxis_GraphNodeStyle::Card;

    // ---- IconTreatment -------------------------------------------------------
    const auto Accent = CkStyle::Accent();

    TestEqual(TEXT("Plain costs the glyph no horizontal padding"),
        ck::debug_axes::Get_IconBackdropPadding().Left, 0.0f);
    TestEqual(TEXT("Plain costs the glyph no vertical padding"),
        ck::debug_axes::Get_IconBackdropPadding().Top, 0.0f);
    TestEqual(TEXT("Plain paints no backdrop"),
        ck::debug_axes::Get_IconBackdropTint(Accent).A, 0.0f);

    Settings->Selection.IconTreatment = ECkDebugAxis_IconTreatment::Well;
    TestEqual(TEXT("Well tints itself from the accent at the icon-well alpha"),
        ck::debug_axes::Get_IconBackdropTint(Accent).A, CkStyle::IconWellAlpha());
    TestTrue(TEXT("Well draws the registered well brush"),
        ck::debug_axes::Get_IconBackdropBrush() == FCkDebuggerStyle::Get_IconWellBrush());

    Settings->Selection.IconTreatment = ECkDebugAxis_IconTreatment::Ring;
    TestTrue(TEXT("Ring draws the registered ring brush"),
        ck::debug_axes::Get_IconBackdropBrush() == FCkDebuggerStyle::Get_IconRingBrush());

    // An unset accent still has to produce a readable backdrop rather than an invisible one.
    TestTrue(TEXT("An unset accent falls back instead of vanishing"),
        ck::debug_axes::Get_IconBackdropTint(FLinearColor::Transparent).A > 0.0f);

    Settings->Selection.IconTreatment = ECkDebugAxis_IconTreatment::Plain;

    // ---- EntityRefStyle ------------------------------------------------------
    TestEqual(TEXT("Flat boxes nothing"), ck::debug_axes::Get_EntityRefPadding().Left, 0.0f);
    TestTrue(TEXT("Flat keeps the caller's accent as its ink"),
        ck::debug_axes::Get_EntityRefInk(Accent) == Accent);

    Settings->Selection.EntityRefStyle = ECkDebugAxis_EntityRefStyle::Pill;
    const auto* PillBrush = ck::debug_axes::Get_EntityRefBrush();

    TestTrue(TEXT("Pill boxes the reference"),
        ck::debug_axes::Get_EntityRefPadding().Left > 0.0f);
    TestTrue(TEXT("Pill washes the box with the accent"),
        ck::debug_axes::Get_EntityRefFill(Accent).A > 0.0f);

    Settings->Selection.EntityRefStyle = ECkDebugAxis_EntityRefStyle::OutlinePill;
    TestTrue(TEXT("OutlinePill rings instead of filling"),
        ck::debug_axes::Get_EntityRefBrush() != PillBrush);

    Settings->Selection.EntityRefStyle = ECkDebugAxis_EntityRefStyle::Monochrome;
    TestTrue(TEXT("Monochrome drops the accent for the dim text role"),
        ck::debug_axes::Get_EntityRefInk(Accent) == CkStyle::TextDim());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerStyle_SchemaV4AxesAreLive,
    "Ck.DebuggerCommon.Style.SchemaV4AxesAreLive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_SchemaV4AxesAreLive::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_style_tests;

    const auto Guard = FScopedStyleSelection{};

    auto* Settings = UCkDebuggerStyleSettings::Get_Mutable();

    if (Settings == nullptr)
    {
        AddError(TEXT("UCkDebuggerStyleSettings default object is unavailable"));
        return false;
    }

    Settings->Selection = FCkDebuggerStyleSelection{};

    // ---- IconTreatment on an ALREADY-BUILT glyph -----------------------------
    const auto Icon = SNew(SCkDebug_Icon)
        .Brush(FAppStyle::GetBrush("Icons.FilledCircle"))
        .Meaning(FText::FromString(TEXT("Live icon treatment")))
        .Accent(CkStyle::Accent());

    const auto IconPlain = Repass(Icon);
    TestTrue(TEXT("A Plain glyph occupies its own box and nothing more"), IconPlain.X > 0.0f);

    Settings->Selection.IconTreatment = ECkDebugAxis_IconTreatment::Well;
    const auto IconWell = Repass(Icon);

    TestTrue(
        TEXT("IconTreatment Well grows the SAME glyph — the treatment is live, not baked"),
        IconWell.X > IconPlain.X && IconWell.Y > IconPlain.Y);

    Settings->Selection.IconTreatment = ECkDebugAxis_IconTreatment::Plain;
    TestTrue(
        TEXT("Returning to Plain restores the glyph's shipped footprint"),
        FMath::IsNearlyEqual(Repass(Icon).X, IconPlain.X));

    // ---- TextScale on an ALREADY-BUILT chip ----------------------------------
    const auto Chip = ck::debug_axes::Make_Chip(
        UCkDebuggerStyleSettings::Get_Selection(),
        FText::FromString(TEXT("transform")),
        ECk_Tone::Info);

    const auto ChipNormal = Repass(Chip);

    Settings->Selection.TextScale = ECkDebugAxis_TextScale::Large;
    const auto ChipLarge = Repass(Chip);

    TestTrue(
        TEXT("TextScale Large grows an ALREADY-BUILT chip's label"),
        ChipLarge.X > ChipNormal.X);

    Settings->Selection.TextScale = ECkDebugAxis_TextScale::Small;
    TestTrue(
        TEXT("TextScale Small shrinks the same chip"),
        Repass(Chip).X < ChipLarge.X);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebuggerStyle_EntityRefComposesAndTreatsSeparately,
    "Ck.DebuggerCommon.Style.EntityRefComposesAndTreatsSeparately",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebuggerStyle_EntityRefComposesAndTreatsSeparately::RunTest(const FString& Parameters)
{
    using namespace ck_debugger_style_tests;

    const auto Guard = FScopedStyleSelection{};

    auto* Settings = UCkDebuggerStyleSettings::Get_Mutable();

    if (Settings == nullptr)
    {
        AddError(TEXT("UCkDebuggerStyleSettings default object is unavailable"));
        return false;
    }

    Settings->Selection = FCkDebuggerStyleSelection{};

    const auto SampleName = FString{TEXT("Guard_Patrol_01")};
    const auto SampleId   = FString{TEXT("412|3(198)")};

    // ---- EntityIdStyle is COMPOSITION, and nothing else -----------------------
    const auto Compositions = Get_AllOptions<ECkDebugAxis_EntityIdStyle>();
    TestEqual(TEXT("EntityIdStyle carries three pure compositions"), Compositions.Num(), 3);

    auto ComposedTexts = TSet<FString>{};
    for (const auto Option : Compositions)
    {
        auto Selection = FCkDebuggerStyleSelection{};
        Selection.EntityIdStyle = Option;

        const auto Text = ck::debug_axes::Make_EntityIdText(Selection, SampleName, SampleId).ToString();
        const auto Name = Get_OptionName(
            static_cast<int64>(Option), StaticEnum<ECkDebugAxis_EntityIdStyle>());

        TestFalse(
            *ck::Format_UE(TEXT("Composition '{}' reads differently from every other"), Name),
            ComposedTexts.Contains(Text));

        ComposedTexts.Add(Text);
    }

    // ---- EntityRefStyle is TREATMENT, and its four options are distinguishable --
    // Flat and Monochrome share a brush and a padding, so neither alone separates them; the tuple
    // the widget actually composes with does.
    const auto Treatments = Get_AllOptions<ECkDebugAxis_EntityRefStyle>();
    TestEqual(TEXT("EntityRefStyle carries four treatments"), Treatments.Num(), 4);

    auto Signatures = TSet<FString>{};
    for (const auto Option : Treatments)
    {
        Settings->Selection.EntityRefStyle = Option;

        const auto Accent = ck::debug_axes::EntityRef_UsesHashTint()
            ? FLinearColor{0.9f, 0.3f, 0.1f, 1.0f}
            : CkStyle::EntityId();

        const auto Signature = ck::Format_UE(TEXT("{}|{}|{}"),
            static_cast<uint64>(reinterpret_cast<UPTRINT>(ck::debug_axes::Get_EntityRefBrush())),
            ck::debug_axes::Get_EntityRefPadding().Left,
            ck::debug_axes::Get_EntityRefInk(Accent).ToString());

        const auto Name = Get_OptionName(
            static_cast<int64>(Option), StaticEnum<ECkDebugAxis_EntityRefStyle>());

        TestFalse(
            *ck::Format_UE(TEXT("Treatment '{}' draws differently from every other"), Name),
            Signatures.Contains(Signature));

        Signatures.Add(Signature);
    }

    // Only the boxed treatments earn a hash hue — a wall of Flat references stays one column.
    Settings->Selection.EntityRefStyle = ECkDebugAxis_EntityRefStyle::Flat;
    TestFalse(TEXT("Flat keeps the one EntityId role"), ck::debug_axes::EntityRef_UsesHashTint());

    Settings->Selection.EntityRefStyle = ECkDebugAxis_EntityRefStyle::Pill;
    TestTrue(TEXT("Pill tints from the entity's own hash"), ck::debug_axes::EntityRef_UsesHashTint());

    Settings->Selection.EntityRefStyle = ECkDebugAxis_EntityRefStyle::OutlinePill;
    TestTrue(TEXT("OutlinePill rings in that same hash hue"), ck::debug_axes::EntityRef_UsesHashTint());

    Settings->Selection.EntityRefStyle = ECkDebugAxis_EntityRefStyle::Monochrome;
    TestFalse(TEXT("Monochrome is the deliberate absence of an accent"),
        ck::debug_axes::EntityRef_UsesHashTint());

    // ---- Both axes on ONE already-built widget, never recreated ---------------
    // Preview mode is what makes this assertable with no world: an invalid handle would collapse
    // every treatment onto the same muted "None".
    Settings->Selection = FCkDebuggerStyleSelection{};

    const auto Ref = SNew(SCkDebug_EntityRef)
        .PreviewName(SampleName)
        .PreviewIdText(SampleId);

    const auto AsNameAndId = Repass(Ref);
    TestTrue(TEXT("A previewed reference occupies real space"), AsNameAndId.X > 0.0f);

    Settings->Selection.EntityIdStyle = ECkDebugAxis_EntityIdStyle::CompactId;
    const auto AsCompactId = Repass(Ref);

    TestTrue(
        TEXT("EntityIdStyle CompactId drops the name off an ALREADY-BUILT reference"),
        AsCompactId.X < AsNameAndId.X);

    Settings->Selection.EntityIdStyle = ECkDebugAxis_EntityIdStyle::NameAndId;
    TestTrue(
        TEXT("Returning to NameAndId restores the reference's width"),
        FMath::IsNearlyEqual(Repass(Ref).X, AsNameAndId.X));

    Settings->Selection.EntityRefStyle = ECkDebugAxis_EntityRefStyle::Pill;
    const auto AsPill = Repass(Ref);

    TestTrue(
        TEXT("EntityRefStyle Pill boxes the SAME reference — the treatment is live, not baked"),
        AsPill.X > AsNameAndId.X && AsPill.Y > AsNameAndId.Y);

    Settings->Selection.EntityRefStyle = ECkDebugAxis_EntityRefStyle::OutlinePill;
    TestTrue(TEXT("OutlinePill keeps the boxed footprint"), Repass(Ref).X > AsNameAndId.X);

    Settings->Selection.EntityRefStyle = ECkDebugAxis_EntityRefStyle::Flat;
    TestTrue(
        TEXT("Returning to Flat restores the reference's shipped footprint"),
        FMath::IsNearlyEqual(Repass(Ref).X, AsNameAndId.X));

    // ---- Hover ink ------------------------------------------------------------
    // Slate hover cannot be produced from an automation test, so the assertion lands on the pure
    // resolver the widget's color attribute calls once IsHovered() and Is_Clickable() both hold.
    const auto RestInk  = CkStyle::EntityId();
    const auto HoverInk = ck::debug_axes::Get_EntityRefHoverInk(RestInk);

    TestTrue(TEXT("A hover only ever lifts the ink, never darkens it"),
        HoverInk.R >= RestInk.R && HoverInk.G >= RestInk.G && HoverInk.B >= RestInk.B);
    TestEqual(TEXT("A hover never changes the ink's opacity"), HoverInk.A, RestInk.A);

    if (CkStyle::HoverOverlayAlpha() > 0.0f)
    {
        TestTrue(TEXT("A hovered clickable reference reads differently from a resting one"),
            HoverInk != RestInk);
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
