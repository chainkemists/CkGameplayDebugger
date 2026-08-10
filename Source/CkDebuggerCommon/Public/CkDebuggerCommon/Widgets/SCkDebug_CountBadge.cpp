#include "SCkDebug_CountBadge.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_debug_count_badge
{
	// The badge is a ring around a fill. The BadgeStyle axis is expressed by making both
	// layers transparent-and-zero-padded on demand rather than by building three different
	// widget trees — every option is then live, and Solid is today's construction untouched.
	//
	// NOTE: the axis' render helper (ck::debug_axes::Make_Badge) is not usable here — it
	// resolves its ink from an ECk_Tone, while this widget takes four caller-supplied colors
	// and an optional suffix label. See the U3 report.
	constexpr auto RingPadding  = 1.0f;
	constexpr auto InnerPaddingX = 6.0f;
	constexpr auto InnerPaddingY = 1.0f;

	auto Get_BadgeStyle() -> ECkDebugAxis_BadgeStyle
	{
		return UCkDebuggerStyleSettings::Get_Selection().BadgeStyle;
	}

	auto Has_Box() -> bool
	{
		return Get_BadgeStyle() != ECkDebugAxis_BadgeStyle::CountOnly;
	}

	auto Get_RingPadding() -> FMargin
	{
		return Has_Box() ? FMargin{RingPadding} : FMargin{0.0f};
	}

	auto Get_InnerPadding() -> FMargin
	{
		return Has_Box() ? FMargin{InnerPaddingX, InnerPaddingY} : FMargin{0.0f};
	}

	auto Get_ValueFont() -> FSlateFontInfo
	{
		return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody());
	}

	auto Get_SuffixFont() -> FSlateFontInfo
	{
		return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro());
	}
}

// ====================================================================================================================

auto
	SCkDebug_CountBadge::
	Construct(const FArguments& InArgs)
	-> void
{
	auto Row = SNew(SHorizontalBox);

	Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._ValueText)
			.ColorAndOpacity(FSlateColor(InArgs._ValueColor))
			.Font_Static(&ck_debug_count_badge::Get_ValueFont)
		];

	if (!InArgs._SuffixText.IsEmpty())
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(InArgs._SuffixText)
				.ColorAndOpacity(FSlateColor(InArgs._SuffixColor))
				.Font_Static(&ck_debug_count_badge::Get_SuffixFont)
			];
	}

	const auto BorderColor = InArgs._BorderColor;
	const auto BackgroundColor = InArgs._BackgroundColor;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage_Static(&ck::debug_axes::Get_ChipBrush)
		.BorderBackgroundColor_Lambda([BorderColor]() -> FSlateColor
		{
			// Solid + Hollow both keep the ring; CountOnly drops all chrome.
			return FSlateColor(ck_debug_count_badge::Has_Box() ? BorderColor : FLinearColor::Transparent);
		})
		.Padding_Static(&ck_debug_count_badge::Get_RingPadding)
		[
			SNew(SBorder)
			.BorderImage_Static(&ck::debug_axes::Get_ChipBrush)
			.BorderBackgroundColor_Lambda([BackgroundColor]() -> FSlateColor
			{
				// Hollow is the ring without its body — the outline reading.
				return FSlateColor(
					ck_debug_count_badge::Get_BadgeStyle() == ECkDebugAxis_BadgeStyle::Solid
						? BackgroundColor
						: FLinearColor::Transparent);
			})
			.Padding_Static(&ck_debug_count_badge::Get_InnerPadding)
			[
				Row
			]
		]
	];
}

// ====================================================================================================================
