#include "CkDebuggerAxes.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
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

    // ONE shape for every chip / badge option: a ring box around a fill box around a label. An
    // option that draws no ring (or no fill, or no box at all) makes that layer transparent and
    // zero-padded rather than producing a different widget tree — the SCkDebug_CountBadge idiom.
    // That is what makes every option reachable through attributes, and it is why a caller can
    // build once and never rebuild. A collapsed-to-transparent layer contributes no geometry, so
    // Classic renders byte-for-byte what the three-branch version rendered.
    auto Make_ToneBox(
        const FSlateBrush*         InBrush,
        TAttribute<FSlateColor>    InRing,
        TAttribute<FMargin>        InRingPadding,
        TAttribute<FSlateColor>    InFill,
        TAttribute<FMargin>        InFillPadding,
        TAttribute<FText>          InText,
        TAttribute<FSlateFontInfo> InFont,
        TAttribute<FSlateColor>    InInk) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(InBrush)
            .BorderBackgroundColor(MoveTemp(InRing))
            .Padding(MoveTemp(InRingPadding))
            [
                SNew(SBorder)
                .BorderImage(InBrush)
                .BorderBackgroundColor(MoveTemp(InFill))
                .Padding(MoveTemp(InFillPadding))
                [
                    SNew(STextBlock)
                    .Text(MoveTemp(InText))
                    .Font(MoveTemp(InFont))
                    .ColorAndOpacity(MoveTemp(InInk))
                ]
            ];
    }

    auto Get_LiveSelection() -> const FCkDebuggerStyleSelection&
    {
        return UCkDebuggerStyleSettings::Get_Selection();
    }

    auto Get_LabelFont(const TOptional<int32>& InFontSize) -> FSlateFontInfo
    {
        // Resolved per read rather than captured: FontSizeSmall goes through the style settings, so
        // an Editor Preferences font edit moves the label too.
        const auto Size = InFontSize.IsSet() ? InFontSize.GetValue() : CkStyle::FontSizeSmall();
        return CkStyle::RegularFont(Size);
    }

    auto Get_Transparent() -> FSlateColor
    {
        return FSlateColor{FLinearColor::Transparent};
    }

    // ----- ChipStyle resolvers ------------------------------------------------

    auto Chip_RingPadding() -> FMargin
    {
        switch (Get_LiveSelection().ChipStyle)
        {
            case ECkDebugAxis_ChipStyle::Tint:     return FMargin{0.0f};
            case ECkDebugAxis_ChipStyle::Solid:    return FMargin{0.0f};
            case ECkDebugAxis_ChipStyle::Outline:  return FMargin{BorderWidth};
            case ECkDebugAxis_ChipStyle::TextOnly: return FMargin{0.0f};
        }

        return FMargin{0.0f};
    }

    auto Chip_FillPadding() -> FMargin
    {
        switch (Get_LiveSelection().ChipStyle)
        {
            case ECkDebugAxis_ChipStyle::Tint:     return FMargin{ChipPaddingX, ChipPaddingY};
            case ECkDebugAxis_ChipStyle::Solid:    return FMargin{ChipPaddingX, ChipPaddingY};
            case ECkDebugAxis_ChipStyle::Outline:  return FMargin{ChipPaddingX, ChipPaddingY};
            case ECkDebugAxis_ChipStyle::TextOnly: return FMargin{0.0f};
        }

        return FMargin{ChipPaddingX, ChipPaddingY};
    }

    auto Chip_Ring(const FLinearColor& InInk) -> FSlateColor
    {
        switch (Get_LiveSelection().ChipStyle)
        {
            case ECkDebugAxis_ChipStyle::Tint:     return Get_Transparent();
            case ECkDebugAxis_ChipStyle::Solid:    return Get_Transparent();
            case ECkDebugAxis_ChipStyle::Outline:  return FSlateColor{InInk};
            case ECkDebugAxis_ChipStyle::TextOnly: return Get_Transparent();
        }

        return Get_Transparent();
    }

    auto Chip_Fill(const FLinearColor& InInk, const FLinearColor& InFill) -> FSlateColor
    {
        switch (Get_LiveSelection().ChipStyle)
        {
            case ECkDebugAxis_ChipStyle::Tint:     return FSlateColor{InFill};
            case ECkDebugAxis_ChipStyle::Solid:    return FSlateColor{InInk};
            case ECkDebugAxis_ChipStyle::Outline:  return FSlateColor{CkStyle::Bg2()};
            case ECkDebugAxis_ChipStyle::TextOnly: return Get_Transparent();
        }

        return FSlateColor{InFill};
    }

    auto Chip_Ink(const FLinearColor& InInk) -> FSlateColor
    {
        switch (Get_LiveSelection().ChipStyle)
        {
            case ECkDebugAxis_ChipStyle::Tint:     return FSlateColor{InInk};
            case ECkDebugAxis_ChipStyle::Solid:    return FSlateColor{CkStyle::TextStrong()};
            case ECkDebugAxis_ChipStyle::Outline:  return FSlateColor{InInk};
            case ECkDebugAxis_ChipStyle::TextOnly: return FSlateColor{InInk};
        }

        return FSlateColor{InInk};
    }

    // ----- BadgeStyle resolvers -----------------------------------------------

    auto Badge_RingPadding() -> FMargin
    {
        switch (Get_LiveSelection().BadgeStyle)
        {
            case ECkDebugAxis_BadgeStyle::Solid:     return FMargin{0.0f};
            case ECkDebugAxis_BadgeStyle::Hollow:    return FMargin{BorderWidth};
            case ECkDebugAxis_BadgeStyle::CountOnly: return FMargin{0.0f};
        }

        return FMargin{0.0f};
    }

    auto Badge_FillPadding() -> FMargin
    {
        switch (Get_LiveSelection().BadgeStyle)
        {
            case ECkDebugAxis_BadgeStyle::Solid:     return FMargin{BadgePaddingX, BadgePaddingY};
            case ECkDebugAxis_BadgeStyle::Hollow:    return FMargin{BadgePaddingX, BadgePaddingY};
            case ECkDebugAxis_BadgeStyle::CountOnly: return FMargin{0.0f};
        }

        return FMargin{BadgePaddingX, BadgePaddingY};
    }

    auto Badge_Ring(const FLinearColor& InInk) -> FSlateColor
    {
        switch (Get_LiveSelection().BadgeStyle)
        {
            case ECkDebugAxis_BadgeStyle::Solid:     return Get_Transparent();
            case ECkDebugAxis_BadgeStyle::Hollow:    return FSlateColor{InInk};
            case ECkDebugAxis_BadgeStyle::CountOnly: return Get_Transparent();
        }

        return Get_Transparent();
    }

    auto Badge_Fill(const FLinearColor& InFill) -> FSlateColor
    {
        switch (Get_LiveSelection().BadgeStyle)
        {
            case ECkDebugAxis_BadgeStyle::Solid:     return FSlateColor{InFill};
            case ECkDebugAxis_BadgeStyle::Hollow:    return FSlateColor{CkStyle::Bg2()};
            case ECkDebugAxis_BadgeStyle::CountOnly: return Get_Transparent();
        }

        return FSlateColor{InFill};
    }

    auto Badge_Ink(const FLinearColor& InInk) -> FSlateColor
    {
        switch (Get_LiveSelection().BadgeStyle)
        {
            case ECkDebugAxis_BadgeStyle::Solid:     return FSlateColor{CkStyle::TextStrong()};
            case ECkDebugAxis_BadgeStyle::Hollow:    return FSlateColor{InInk};
            case ECkDebugAxis_BadgeStyle::CountOnly: return FSlateColor{InInk};
        }

        return FSlateColor{CkStyle::TextStrong()};
    }

    // ----- FoldChipStyle resolvers --------------------------------------------

    auto FoldChip_Padding() -> FMargin
    {
        switch (Get_LiveSelection().FoldChipStyle)
        {
            // The look the entity tree ships today: a dim label in a bounded, lightly padded target.
            case ECkDebugAxis_FoldChipStyle::Chip:    return FMargin{FoldChipPaddingX, 0.0f};
            case ECkDebugAxis_FoldChipStyle::Text:    return FMargin{0.0f};
            case ECkDebugAxis_FoldChipStyle::Minimal: return FMargin{0.0f};
        }

        return FMargin{FoldChipPaddingX, 0.0f};
    }

    auto FoldChip_Font() -> FSlateFontInfo
    {
        switch (Get_LiveSelection().FoldChipStyle)
        {
            case ECkDebugAxis_FoldChipStyle::Chip:    return CkStyle::RegularFont(CkStyle::FontSizeSmall());
            case ECkDebugAxis_FoldChipStyle::Text:    return CkStyle::RegularFont(CkStyle::FontSizeSmall());
            case ECkDebugAxis_FoldChipStyle::Minimal: return CkStyle::RegularFont(CkStyle::FontSizeMicro());
        }

        return CkStyle::RegularFont(CkStyle::FontSizeSmall());
    }

    auto FoldChip_Ink(const FLinearColor& InTone) -> FSlateColor
    {
        switch (Get_LiveSelection().FoldChipStyle)
        {
            case ECkDebugAxis_FoldChipStyle::Chip:    return FSlateColor{CkStyle::TextDim()};
            case ECkDebugAxis_FoldChipStyle::Text:    return FSlateColor{InTone};
            case ECkDebugAxis_FoldChipStyle::Minimal: return FSlateColor{CkStyle::TextMute()};
        }

        return FSlateColor{CkStyle::TextDim()};
    }

    // ----- ProviderChipStyle resolvers ----------------------------------------

    auto ProviderChip_FillPadding() -> FMargin
    {
        switch (Get_LiveSelection().ProviderChipStyle)
        {
            case ECkDebugAxis_ProviderChipStyle::Tint:       return FMargin{BadgePaddingX, BadgePaddingY};
            case ECkDebugAxis_ProviderChipStyle::Solid:      return FMargin{BadgePaddingX, BadgePaddingY};
            case ECkDebugAxis_ProviderChipStyle::AbbrevOnly: return FMargin{0.0f};
        }

        return FMargin{BadgePaddingX, BadgePaddingY};
    }

    auto ProviderChip_Fill(const FLinearColor& InTone, const FLinearColor& InToneDim) -> FSlateColor
    {
        switch (Get_LiveSelection().ProviderChipStyle)
        {
            case ECkDebugAxis_ProviderChipStyle::Tint:       return FSlateColor{InToneDim};
            case ECkDebugAxis_ProviderChipStyle::Solid:      return FSlateColor{InTone};
            case ECkDebugAxis_ProviderChipStyle::AbbrevOnly: return Get_Transparent();
        }

        return FSlateColor{InToneDim};
    }

    auto ProviderChip_Ink(const FLinearColor& InTone) -> FSlateColor
    {
        switch (Get_LiveSelection().ProviderChipStyle)
        {
            case ECkDebugAxis_ProviderChipStyle::Tint:       return FSlateColor{InTone};
            case ECkDebugAxis_ProviderChipStyle::Solid:      return FSlateColor{CkStyle::TextStrong()};
            case ECkDebugAxis_ProviderChipStyle::AbbrevOnly: return FSlateColor{InTone};
        }

        return FSlateColor{InTone};
    }

    // ----- SectionHeaderStyle resolvers ---------------------------------------
    // Mirrors SCkDebug_SectionHeader's own resolvers; the two must stay in lock-step because the
    // widget and this helper are the same header rendered through different entry points.

    auto SectionHeader_Font() -> FSlateFontInfo
    {
        switch (Get_LiveSelection().SectionHeaderStyle)
        {
            case ECkDebugAxis_SectionHeaderStyle::Uppercase: return CkStyle::BoldFont(CkStyle::FontSizeH4());
            case ECkDebugAxis_SectionHeaderStyle::Mixed:     return CkStyle::BoldFont(CkStyle::FontSizeH4());
            case ECkDebugAxis_SectionHeaderStyle::Minimal:   return CkStyle::RegularFont(CkStyle::FontSizeSmall());
        }

        return CkStyle::BoldFont(CkStyle::FontSizeH4());
    }

    auto SectionHeader_Ink(const FLinearColor& InTone) -> FSlateColor
    {
        switch (Get_LiveSelection().SectionHeaderStyle)
        {
            case ECkDebugAxis_SectionHeaderStyle::Uppercase: return FSlateColor{InTone};
            case ECkDebugAxis_SectionHeaderStyle::Mixed:     return FSlateColor{InTone};
            case ECkDebugAxis_SectionHeaderStyle::Minimal:   return FSlateColor{CkStyle::TextMute()};
        }

        return FSlateColor{InTone};
    }

    auto SectionHeader_Transform() -> ETextTransformPolicy
    {
        switch (Get_LiveSelection().SectionHeaderStyle)
        {
            case ECkDebugAxis_SectionHeaderStyle::Uppercase: return ETextTransformPolicy::ToUpper;
            case ECkDebugAxis_SectionHeaderStyle::Mixed:     return ETextTransformPolicy::None;
            case ECkDebugAxis_SectionHeaderStyle::Minimal:   return ETextTransformPolicy::None;
        }

        return ETextTransformPolicy::ToUpper;
    }

    // ----- MergeCountDisplay resolvers ----------------------------------------

    auto MergeCount_Visibility(ECkDebugAxis_MergeCountDisplay InOption) -> EVisibility
    {
        return Get_LiveSelection().MergeCountDisplay == InOption ? EVisibility::Visible : EVisibility::Collapsed;
    }

    // The container collapses too, so Hidden costs the caller's slot padding nothing — the old
    // null return and the new collapsed widget occupy exactly the same amount of layout.
    auto MergeCount_ContainerVisibility() -> EVisibility
    {
        switch (Get_LiveSelection().MergeCountDisplay)
        {
            case ECkDebugAxis_MergeCountDisplay::SuffixText: return EVisibility::Visible;
            case ECkDebugAxis_MergeCountDisplay::CountBadge: return EVisibility::Visible;
            case ECkDebugAxis_MergeCountDisplay::Hidden:     return EVisibility::Collapsed;
        }

        return EVisibility::Visible;
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
        const FText& InText,
        FLinearColor InInk,
        FLinearColor InFill,
        TOptional<int32> InFontSize)
    -> TSharedRef<SWidget>
{
    using namespace ck_debugger_axes;

    return Make_ToneBox(
        CkStyle::GetRoundedBrush(),
        TAttribute<FSlateColor>::CreateLambda([InInk]() { return Chip_Ring(InInk); }),
        TAttribute<FMargin>::CreateStatic(&Chip_RingPadding),
        TAttribute<FSlateColor>::CreateLambda([InInk, InFill]() { return Chip_Fill(InInk, InFill); }),
        TAttribute<FMargin>::CreateStatic(&Chip_FillPadding),
        InText,
        TAttribute<FSlateFontInfo>::CreateLambda([InFontSize]() { return Get_LabelFont(InFontSize); }),
        TAttribute<FSlateColor>::CreateLambda([InInk]() { return Chip_Ink(InInk); }));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_Chip(
        const FCkDebuggerStyleSelection& /*InSelection*/,
        const FText& InText,
        ECk_Tone InTone)
    -> TSharedRef<SWidget>
{
    return Make_Chip(InText, CkStyle::GetToneColor(InTone), CkStyle::GetToneDimColor(InTone));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_Badge(
        const FText& InText,
        FLinearColor InInk,
        FLinearColor InFill,
        TOptional<int32> InFontSize)
    -> TSharedRef<SWidget>
{
    using namespace ck_debugger_axes;

    return Make_ToneBox(
        CkStyle::GetRoundedBrush_Small(),
        TAttribute<FSlateColor>::CreateLambda([InInk]() { return Badge_Ring(InInk); }),
        TAttribute<FMargin>::CreateStatic(&Badge_RingPadding),
        TAttribute<FSlateColor>::CreateLambda([InFill]() { return Badge_Fill(InFill); }),
        TAttribute<FMargin>::CreateStatic(&Badge_FillPadding),
        InText,
        TAttribute<FSlateFontInfo>::CreateLambda([InFontSize]() { return Get_LabelFont(InFontSize); }),
        TAttribute<FSlateColor>::CreateLambda([InInk]() { return Badge_Ink(InInk); }));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_Badge(
        const FCkDebuggerStyleSelection& /*InSelection*/,
        const FText& InText,
        ECk_Tone InTone)
    -> TSharedRef<SWidget>
{
    // Ink and Fill are the same tone: a badge has no tint wash, so Solid's body and Hollow's ring
    // are both the tone color — exactly what the three-branch version rendered.
    return Make_Badge(InText, CkStyle::GetToneColor(InTone), CkStyle::GetToneColor(InTone));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_FoldChip(
        const FCkDebuggerStyleSelection& /*InSelection*/,
        const FText& InText,
        ECk_Tone InTone)
    -> TSharedRef<SWidget>
{
    using namespace ck_debugger_axes;

    const auto Tone = CkStyle::GetToneColor(InTone);

    // The SBox is unconditional so the axis never changes the tree: Text and Minimal simply zero
    // its padding. VAlign_Center now applies to every option (it previously only reached Chip) —
    // the fold affordance is always vertically centred in its row, which is what the tree wants.
    return SNew(SBox)
        .VAlign(VAlign_Center)
        .Padding_Static(&FoldChip_Padding)
        [
            SNew(STextBlock)
            .Text(InText)
            .Font_Static(&FoldChip_Font)
            .ColorAndOpacity_Lambda([Tone]() { return FoldChip_Ink(Tone); })
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_ProviderChip(
        const FCkDebuggerStyleSelection& /*InSelection*/,
        const FText& InText,
        ECk_Tone InTone)
    -> TSharedRef<SWidget>
{
    using namespace ck_debugger_axes;

    const auto Tone    = CkStyle::GetToneColor(InTone);
    const auto ToneDim = CkStyle::GetToneDimColor(InTone);

    // AbbrevOnly is the one option that changes the TEXT rather than the box, so the text itself
    // is an attribute too — abbreviating live instead of at construction.
    const auto Full   = InText;
    const auto Abbrev = Get_Abbreviation(InText);

    return Make_ToneBox(
        CkStyle::GetRoundedBrush_Small(),
        Get_Transparent(),
        FMargin{0.0f},
        TAttribute<FSlateColor>::CreateLambda([Tone, ToneDim]() { return ProviderChip_Fill(Tone, ToneDim); }),
        TAttribute<FMargin>::CreateStatic(&ProviderChip_FillPadding),
        TAttribute<FText>::CreateLambda([Full, Abbrev]()
        {
            return Get_LiveSelection().ProviderChipStyle == ECkDebugAxis_ProviderChipStyle::AbbrevOnly
                ? Abbrev
                : Full;
        }),
        CkStyle::RegularFont(CkStyle::FontSizeSmall()),
        TAttribute<FSlateColor>::CreateLambda([Tone]() { return ProviderChip_Ink(Tone); }));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::debug_axes::
    Make_SectionHeader(
        const FCkDebuggerStyleSelection& /*InSelection*/,
        const FText& InText,
        ECk_Tone InTone,
        const FText& InToolTip)
    -> TSharedRef<SWidget>
{
    using namespace ck_debugger_axes;

    const auto Tone = CkStyle::GetToneColor(InTone);

    auto Header = SNew(STextBlock)
        .Text(InText)
        .Font_Static(&SectionHeader_Font)
        .ColorAndOpacity_Lambda([Tone]() { return SectionHeader_Ink(Tone); })
        .TransformPolicy_Static(&SectionHeader_Transform);

    // Only attach a tooltip when the caller asked for one — an empty one would still register a
    // tooltip widget, and "no tooltip" is what every existing three-argument call expects.
    if (NOT InToolTip.IsEmpty())
    { Header->SetToolTipText(InToolTip); }

    return Header;
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

    // Nothing merged is DATA, not an axis — the caller genuinely has no slot to fill.
    if (InCount <= 1)
    { return nullptr; }

    // Both affordances are composed once and switched by visibility, so MergeCountDisplay (Hidden
    // included) applies without the caller ever revisiting the slot. A collapsed child costs no
    // geometry and no slot padding, which is what makes Hidden equivalent to the old null return.
    //
    // "×" = MULTIPLICATION SIGN (U+00D7), matching the overlay focus card's existing literal.
    auto Suffix = Make_Label(FText::FromString(ck::Format_UE(TEXT("×{}"), InCount)), CkStyle::TextMute());
    auto Badge  = Make_Badge(InSelection, FText::AsNumber(InCount), ECk_Tone::Neutral);

    Suffix->SetVisibility(TAttribute<EVisibility>::CreateStatic(
        &MergeCount_Visibility, ECkDebugAxis_MergeCountDisplay::SuffixText));
    Badge->SetVisibility(TAttribute<EVisibility>::CreateStatic(
        &MergeCount_Visibility, ECkDebugAxis_MergeCountDisplay::CountBadge));

    auto Row = SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            Suffix
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            Badge
        ];

    Row->SetVisibility(TAttribute<EVisibility>::CreateStatic(&MergeCount_ContainerVisibility));

    return Row;
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
