#include "CkDebuggerAxes.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"

#include "CkCore/Format/CkFormat.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_debugger_axes
{
    constexpr auto ChipPaddingX  = CkStyle::SpaceM;
    constexpr auto ChipPaddingY  = CkStyle::SpaceXS;
    constexpr auto BadgePaddingX = CkStyle::SpaceS;
    constexpr auto BadgePaddingY = 1.0f;
    constexpr auto BorderWidth   = 1.0f;

    // Matches the tree fold affordance's existing button content padding, so the Chip option
    // keeps the current geometry when the tree starts composing through this function.
    constexpr auto FoldChipPaddingX = CkStyle::SpaceXS;

    constexpr auto AbbrevLength = 3;

    // --------------------------------------------------------------------------------------------------------------

    auto Make_Label(const FText& InText, const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(STextBlock)
            .Text(InText)
            .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
            .ColorAndOpacity(FSlateColor{InColor});
    }

    // Single rounded box: one fill, no border ring.
    auto Make_FilledBox(
        const FSlateBrush* InBrush,
        const FLinearColor& InFill,
        const FMargin& InPadding,
        TSharedRef<SWidget> InContent) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(InBrush)
            .BorderBackgroundColor(FSlateColor{InFill})
            .Padding(InPadding)
            [
                InContent
            ];
    }

    // Nested rounded boxes: outer tint reads as the border ring, inner fill is the body.
    auto Make_RingedBox(
        const FSlateBrush* InBrush,
        const FLinearColor& InRing,
        const FLinearColor& InFill,
        const FMargin& InPadding,
        TSharedRef<SWidget> InContent) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(InBrush)
            .BorderBackgroundColor(FSlateColor{InRing})
            .Padding(FMargin{BorderWidth})
            [
                Make_FilledBox(InBrush, InFill, InPadding, InContent)
            ];
    }

    // The look the entity tree ships today: a dim label in a bounded, lightly padded target.
    auto Make_FoldChipBody(const FText& InText) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .VAlign(VAlign_Center)
            .Padding(FMargin{FoldChipPaddingX, 0.0f})
            [
                Make_Label(InText, CkStyle::TextDim())
            ];
    }

    auto Get_Abbreviation(const FText& InText) -> FText
    {
        return FText::FromString(InText.ToString().Left(AbbrevLength).ToUpper());
    }

    // --------------------------------------------------------------------------------------------------------------

    // Ramps take an already-normalized value from the caller, but a debugger divides by
    // live data — a zero range yields inf/NaN. Fold that to the cold end rather than
    // painting an undefined color.
    auto Sanitize_Normalized(float InValue) -> float
    {
        if (FMath::IsNaN(InValue))
        { return 0.0f; }

        return FMath::Clamp(InValue, 0.0f, 1.0f);
    }

    // The categorical palette is rebuilt per call ON PURPOSE: CkStyle:: reads the settings CDO,
    // so a file-static TArray would both risk static-init order and freeze the palette against
    // later Editor Preferences edits.
    constexpr auto CategoricalPaletteSize = 8;

    auto Get_CategoricalEntry(int32 InSlot) -> FLinearColor
    {
        switch (InSlot)
        {
            case 0:  return CkStyle::CategoryGather();    // green
            case 1:  return CkStyle::CategoryBuild();     // amber
            case 2:  return CkStyle::CategoryResearch();  // blue
            case 3:  return CkStyle::CategoryTrain();     // red
            case 4:  return CkStyle::CategoryAge();       // purple
            case 5:  return CkStyle::Accent();            // cyan
            case 6:  return CkStyle::CategoryTrade();     // gold
            default: return CkStyle::Relationship();      // pink
        }
    }
}

// ====================================================================================================================
// RENDER

auto
    ck::debug_axes::
    Make_Chip(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone)
    -> TSharedRef<SWidget>
{
    using namespace ck_debugger_axes;

    const auto Tone    = CkStyle::GetToneColor(InTone);
    const auto ToneDim = CkStyle::GetToneDimColor(InTone);
    const auto Padding = FMargin{ChipPaddingX, ChipPaddingY};

    switch (InSelection.ChipStyle)
    {
        case ECkDebugAxis_ChipStyle::Tint:
            return Make_FilledBox(CkStyle::GetRoundedBrush(), ToneDim, Padding, Make_Label(InText, Tone));

        case ECkDebugAxis_ChipStyle::Solid:
            return Make_FilledBox(CkStyle::GetRoundedBrush(), Tone, Padding, Make_Label(InText, CkStyle::TextStrong()));

        case ECkDebugAxis_ChipStyle::Outline:
            return Make_RingedBox(CkStyle::GetRoundedBrush(), Tone, CkStyle::Bg2(), Padding, Make_Label(InText, Tone));

        case ECkDebugAxis_ChipStyle::TextOnly:
            return Make_Label(InText, Tone);
    }

    return Make_FilledBox(CkStyle::GetRoundedBrush(), ToneDim, Padding, Make_Label(InText, Tone));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_Badge(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone)
    -> TSharedRef<SWidget>
{
    using namespace ck_debugger_axes;

    const auto Tone    = CkStyle::GetToneColor(InTone);
    const auto Padding = FMargin{BadgePaddingX, BadgePaddingY};

    switch (InSelection.BadgeStyle)
    {
        case ECkDebugAxis_BadgeStyle::Solid:
            return Make_FilledBox(
                CkStyle::GetRoundedBrush_Small(), Tone, Padding, Make_Label(InText, CkStyle::TextStrong()));

        case ECkDebugAxis_BadgeStyle::Hollow:
            return Make_RingedBox(
                CkStyle::GetRoundedBrush_Small(), Tone, CkStyle::Bg2(), Padding, Make_Label(InText, Tone));

        case ECkDebugAxis_BadgeStyle::CountOnly:
            return Make_Label(InText, Tone);
    }

    return Make_FilledBox(CkStyle::GetRoundedBrush_Small(), Tone, Padding, Make_Label(InText, CkStyle::TextStrong()));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_FoldChip(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone)
    -> TSharedRef<SWidget>
{
    using namespace ck_debugger_axes;

    switch (InSelection.FoldChipStyle)
    {
        case ECkDebugAxis_FoldChipStyle::Chip:
            return Make_FoldChipBody(InText);

        case ECkDebugAxis_FoldChipStyle::Text:
            return Make_Label(InText, CkStyle::GetToneColor(InTone));

        case ECkDebugAxis_FoldChipStyle::Minimal:
            return SNew(STextBlock)
                .Text(InText)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()});
    }

    return Make_FoldChipBody(InText);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_ProviderChip(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone)
    -> TSharedRef<SWidget>
{
    using namespace ck_debugger_axes;

    const auto Tone    = CkStyle::GetToneColor(InTone);
    const auto ToneDim = CkStyle::GetToneDimColor(InTone);
    const auto Padding = FMargin{BadgePaddingX, BadgePaddingY};

    switch (InSelection.ProviderChipStyle)
    {
        case ECkDebugAxis_ProviderChipStyle::Tint:
            return Make_FilledBox(CkStyle::GetRoundedBrush_Small(), ToneDim, Padding, Make_Label(InText, Tone));

        case ECkDebugAxis_ProviderChipStyle::Solid:
            return Make_FilledBox(
                CkStyle::GetRoundedBrush_Small(), Tone, Padding, Make_Label(InText, CkStyle::TextStrong()));

        case ECkDebugAxis_ProviderChipStyle::AbbrevOnly:
            return Make_Label(Get_Abbreviation(InText), Tone);
    }

    return Make_FilledBox(CkStyle::GetRoundedBrush_Small(), ToneDim, Padding, Make_Label(InText, Tone));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_SectionHeader(
        const FCkDebuggerStyleSelection& InSelection,
        const FText& InText,
        ECk_Tone InTone)
    -> TSharedRef<SWidget>
{
    const auto Tone = CkStyle::GetToneColor(InTone);

    switch (InSelection.SectionHeaderStyle)
    {
        case ECkDebugAxis_SectionHeaderStyle::Uppercase:
            return SNew(STextBlock)
                .Text(InText)
                .Font(CkStyle::BoldFont(CkStyle::FontSizeH4()))
                .ColorAndOpacity(FSlateColor{Tone})
                .TransformPolicy(ETextTransformPolicy::ToUpper);

        case ECkDebugAxis_SectionHeaderStyle::Mixed:
            return SNew(STextBlock)
                .Text(InText)
                .Font(CkStyle::BoldFont(CkStyle::FontSizeH4()))
                .ColorAndOpacity(FSlateColor{Tone});

        case ECkDebugAxis_SectionHeaderStyle::Minimal:
            return SNew(STextBlock)
                .Text(InText)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor{CkStyle::TextMute()});
    }

    return SNew(STextBlock)
        .Text(InText)
        .Font(CkStyle::BoldFont(CkStyle::FontSizeH4()))
        .ColorAndOpacity(FSlateColor{Tone})
        .TransformPolicy(ETextTransformPolicy::ToUpper);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_EntityIdText(
        const FCkDebuggerStyleSelection& InSelection,
        const FString& InCleanName,
        const FString& InIdText)
    -> FText
{
    // A nameless entity has nothing to show but its id — the name-bearing options degrade to it
    // rather than rendering a lone separator.
    const auto NameAndId = InCleanName.IsEmpty()
        ? InIdText
        : ck::Format_UE(TEXT("{} | {}"), InCleanName, InIdText);

    const auto NameOrId = InCleanName.IsEmpty() ? InIdText : InCleanName;

    switch (InSelection.EntityIdStyle)
    {
        case ECkDebugAxis_EntityIdStyle::NameAndId:
            return FText::FromString(NameAndId);

        case ECkDebugAxis_EntityIdStyle::CompactId:
            return FText::FromString(InIdText);

        case ECkDebugAxis_EntityIdStyle::NameOnly:
            return FText::FromString(NameOrId);

        // The hash tint is a widget-level concern; the text stays the full identifier so the chip
        // remains readable without relying on color alone.
        case ECkDebugAxis_EntityIdStyle::HashTintedChip:
            return FText::FromString(NameAndId);
    }

    return FText::FromString(NameAndId);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_MergeCount(
        const FCkDebuggerStyleSelection& InSelection,
        int32 InCount)
    -> TSharedPtr<SWidget>
{
    using namespace ck_debugger_axes;

    if (InCount <= 1)
    { return nullptr; }

    switch (InSelection.MergeCountDisplay)
    {
        // "×" = MULTIPLICATION SIGN (U+00D7), matching the overlay focus card's existing literal.
        case ECkDebugAxis_MergeCountDisplay::SuffixText:
            return Make_Label(FText::FromString(ck::Format_UE(TEXT("×{}"), InCount)), CkStyle::TextMute());

        case ECkDebugAxis_MergeCountDisplay::CountBadge:
            return Make_Badge(InSelection, FText::AsNumber(InCount), ECk_Tone::Neutral);

        case ECkDebugAxis_MergeCountDisplay::Hidden:
            return nullptr;
    }

    return Make_Label(FText::FromString(ck::Format_UE(TEXT("×{}"), InCount)), CkStyle::TextMute());
}

// ====================================================================================================================
// METRICS

auto
    ck::debug_axes::
    Get_RowPadding(
        const FCkDebuggerStyleSelection& InSelection)
    -> FMargin
{
    switch (InSelection.RowDensity)
    {
        case ECkDebugAxis_RowDensity::Comfortable: return FMargin{CkStyle::SpaceM, CkStyle::SpaceXS};
        case ECkDebugAxis_RowDensity::Compact:     return FMargin{CkStyle::SpaceS, 0.0f};
        case ECkDebugAxis_RowDensity::Airy:        return FMargin{CkStyle::SpaceL, CkStyle::SpaceS};
    }

    return FMargin{CkStyle::SpaceM, CkStyle::SpaceXS};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Get_IconSize(
        const FCkDebuggerStyleSelection& InSelection)
    -> float
{
    switch (InSelection.IconSize)
    {
        case ECkDebugAxis_IconSize::Medium: return 16.0f;
        case ECkDebugAxis_IconSize::Small:  return 12.0f;
        case ECkDebugAxis_IconSize::Large:  return 20.0f;
    }

    return 16.0f;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Get_SeparatorThickness(
        const FCkDebuggerStyleSelection& InSelection)
    -> float
{
    switch (InSelection.SeparatorWeight)
    {
        case ECkDebugAxis_SeparatorWeight::Hairline: return 1.0f;
        case ECkDebugAxis_SeparatorWeight::None:     return 0.0f;
        case ECkDebugAxis_SeparatorWeight::Standard: return 2.0f;
        case ECkDebugAxis_SeparatorWeight::Heavy:    return 3.0f;
    }

    return 1.0f;
}

// ====================================================================================================================
// METRIC DELTAS

auto
    ck::debug_axes::
    Apply_RowDensity(
        const FMargin& InBase)
    -> FMargin
{
    const auto Baseline = Get_RowPadding(FCkDebuggerStyleSelection{});
    const auto Current  = Get_RowPadding(UCkDebuggerStyleSettings::Get_Selection());

    const auto DeltaX = Current.Left - Baseline.Left;
    const auto DeltaY = Current.Top  - Baseline.Top;

    return FMargin
    {
        FMath::Max(0.0f, InBase.Left   + DeltaX),
        FMath::Max(0.0f, InBase.Top    + DeltaY),
        FMath::Max(0.0f, InBase.Right  + DeltaX),
        FMath::Max(0.0f, InBase.Bottom + DeltaY)
    };
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Apply_IconSize(
        float InBase)
    -> float
{
    const auto Baseline = Get_IconSize(FCkDebuggerStyleSelection{});
    const auto Current  = Get_IconSize(UCkDebuggerStyleSettings::Get_Selection());

    return FMath::Max(1.0f, InBase + (Current - Baseline));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_AxisSeparator()
    -> TSharedRef<SWidget>
{
    const auto Get_Thickness = []()
    {
        return Get_SeparatorThickness(UCkDebuggerStyleSettings::Get_Selection());
    };

    return SNew(SBox)
        .HeightOverride_Lambda([Get_Thickness]() -> FOptionalSize
        {
            return FOptionalSize{Get_Thickness()};
        })
        .Visibility_Lambda([Get_Thickness]()
        {
            return Get_Thickness() > 0.0f ? EVisibility::Visible : EVisibility::Collapsed;
        })
        [
            SNew(SSeparator).Thickness(1.0f)
        ];
}

// ====================================================================================================================
// PREDICATES

auto
    ck::debug_axes::
    EditControls_AreVisible(
        const FCkDebuggerStyleSelection& InSelection)
    -> bool
{
    return InSelection.EditControlStyle != ECkDebugAxis_EditControlStyle::Hidden;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    EditControls_RevealOnHover(
        const FCkDebuggerStyleSelection& InSelection)
    -> bool
{
    return InSelection.EditControlStyle == ECkDebugAxis_EditControlStyle::OnHover;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Legend_IsVisible(
        const FCkDebuggerStyleSelection& InSelection)
    -> bool
{
    return InSelection.LegendMode != ECkDebugAxis_LegendMode::Off;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Legend_IsDeduped(
        const FCkDebuggerStyleSelection& InSelection)
    -> bool
{
    return InSelection.LegendMode == ECkDebugAxis_LegendMode::Deduped;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Values_UseAlignedColumns(
        const FCkDebuggerStyleSelection& InSelection)
    -> bool
{
    return InSelection.ValueAlignment == ECkDebugAxis_ValueAlignment::AlignedColumns;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Values_AlignRight(
        const FCkDebuggerStyleSelection& InSelection)
    -> bool
{
    return InSelection.ValueAlignment == ECkDebugAxis_ValueAlignment::Right;
}

// ====================================================================================================================
// TREE COMPLEXITY
// Modulators, not absolutes: every one of these applies ON TOP of the entity tree's own project
// settings, and every one of them is a no-op under the Normal default — which is what makes the
// default option a byte-identical regression bar.

auto
    ck::debug_axes::
    Tree_FoldThresholdMultiplier(
        const FCkDebuggerStyleSelection& InSelection)
    -> float
{
    switch (InSelection.TreeComplexity)
    {
        // Half the project's threshold — sibling runs coalesce at roughly half the size before
        // they earn individual rows.
        case ECkDebugAxis_TreeComplexity::Minimal: return 0.5f;

        // Full never groups at all (Tree_GroupsSiblings), so the threshold is moot; 1.0 keeps
        // the value meaningful for any caller that reads the multiplier on its own.
        case ECkDebugAxis_TreeComplexity::Full:    return 1.0f;

        case ECkDebugAxis_TreeComplexity::Normal:  return 1.0f;
    }

    return 1.0f;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Tree_ShowsInternalRows(
        const FCkDebuggerStyleSelection& InSelection)
    -> bool
{
    return InSelection.TreeComplexity == ECkDebugAxis_TreeComplexity::Full;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Tree_GroupsSiblings(
        const FCkDebuggerStyleSelection& InSelection)
    -> bool
{
    return InSelection.TreeComplexity != ECkDebugAxis_TreeComplexity::Full;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Tree_GroupsUnnamedRows(
        const FCkDebuggerStyleSelection& InSelection)
    -> bool
{
    return InSelection.TreeComplexity == ECkDebugAxis_TreeComplexity::Minimal;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Tree_BadgeCap(
        const FCkDebuggerStyleSelection& InSelection,
        int32 InBaseCap)
    -> int32
{
    // A caller that hands over a nonsense base still gets a renderable budget back.
    const auto Base = FMath::Max(1, InBaseCap);

    switch (InSelection.TreeComplexity)
    {
        case ECkDebugAxis_TreeComplexity::Minimal: return FMath::Max(1, Base / 2);
        case ECkDebugAxis_TreeComplexity::Full:    return Base * 2;
        case ECkDebugAxis_TreeComplexity::Normal:  return Base;
    }

    return Base;
}

// ====================================================================================================================
// DOMAIN RAMPS

auto
    ck::debug_axes::
    Get_HeatColor(
        float InNormalized)
    -> FLinearColor
{
    using namespace ck_debugger_axes;

    const auto T = Sanitize_Normalized(InNormalized);

    // Two segments so the ramp passes through Warn exactly at the halfway point — that is the
    // "at budget" reading every consumer wants to be able to point at.
    return T <= 0.5f
        ? FMath::Lerp(CkStyle::Ok(),   CkStyle::Warn(), T * 2.0f)
        : FMath::Lerp(CkStyle::Warn(), CkStyle::Err(),  (T - 0.5f) * 2.0f);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Get_ScoreColor(
        float InNormalized)
    -> FLinearColor
{
    using namespace ck_debugger_axes;

    return FMath::Lerp(CkStyle::Info(), CkStyle::Ok(), Sanitize_Normalized(InNormalized));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Get_CategoricalColor(
        int32 InIndex)
    -> FLinearColor
{
    using namespace ck_debugger_axes;

    // Modulo on a negative operand is negative in C++; fold it back so every int32 is valid.
    const auto Slot = ((InIndex % CategoricalPaletteSize) + CategoricalPaletteSize) % CategoricalPaletteSize;
    return Get_CategoricalEntry(Slot);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Get_CategoricalColor(
        FName InKey)
    -> FLinearColor
{
    using namespace ck_debugger_axes;

    const auto Hash = GetTypeHash(InKey);
    return Get_CategoricalEntry(static_cast<int32>(Hash % static_cast<uint32>(CategoricalPaletteSize)));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Get_CategoricalPaletteSize()
    -> int32
{
    return ck_debugger_axes::CategoricalPaletteSize;
}

// ====================================================================================================================
// PROFILES

auto
    ck::debug_axes::
    Get_StyleProfiles()
    -> const TArray<FCkDebuggerStyleProfile>&
{
    static const auto Profiles = []() -> TArray<FCkDebuggerStyleProfile>
    {
        auto Result = TArray<FCkDebuggerStyleProfile>{};

        Result.Add(FCkDebuggerStyleProfile{
            TEXT("Classic"),
            TEXT("Every axis at its default — the look the debuggers shipped with."),
            FCkDebuggerStyleSelection{}});

        auto Dense = FCkDebuggerStyleSelection{};
        Dense.RowDensity        = ECkDebugAxis_RowDensity::Compact;
        Dense.EntityIdStyle     = ECkDebugAxis_EntityIdStyle::CompactId;
        Dense.BadgeStyle        = ECkDebugAxis_BadgeStyle::CountOnly;
        Dense.SeparatorWeight   = ECkDebugAxis_SeparatorWeight::None;
        Dense.IconSize          = ECkDebugAxis_IconSize::Small;
        Dense.MergeCountDisplay = ECkDebugAxis_MergeCountDisplay::Hidden;

        Result.Add(FCkDebuggerStyleProfile{
            TEXT("Dense"),
            TEXT("Maximum rows on screen: tight padding, id-only entity refs, no separators."),
            Dense});

        auto Presentation = FCkDebuggerStyleSelection{};
        Presentation.RowDensity      = ECkDebugAxis_RowDensity::Airy;
        Presentation.EntityIdStyle   = ECkDebugAxis_EntityIdStyle::NameOnly;
        Presentation.ChipStyle       = ECkDebugAxis_ChipStyle::Solid;
        Presentation.SeparatorWeight = ECkDebugAxis_SeparatorWeight::Standard;
        Presentation.IconSize        = ECkDebugAxis_IconSize::Large;

        Result.Add(FCkDebuggerStyleProfile{
            TEXT("Presentation"),
            TEXT("Readable from across the room: loose rows, names over ids, solid chips, large icons."),
            Presentation});

        auto MinimalInk = FCkDebuggerStyleSelection{};
        MinimalInk.ChipStyle         = ECkDebugAxis_ChipStyle::TextOnly;
        MinimalInk.BadgeStyle        = ECkDebugAxis_BadgeStyle::Hollow;
        MinimalInk.SeparatorWeight   = ECkDebugAxis_SeparatorWeight::Hairline;
        MinimalInk.ProviderChipStyle = ECkDebugAxis_ProviderChipStyle::AbbrevOnly;
        MinimalInk.LegendMode        = ECkDebugAxis_LegendMode::Off;

        Result.Add(FCkDebuggerStyleProfile{
            TEXT("Minimal ink"),
            TEXT("Content over chrome: no chip fills, hollow badges, abbreviated providers, no legend."),
            MinimalInk});

        return Result;
    }();

    return Profiles;
}

// ====================================================================================================================
