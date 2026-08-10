#include "SCkDebug_SectionHeader.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_debug_section_header
{
	// The SectionHeaderStyle axis, expressed as three attributes on ONE text block instead of
	// three alternative text blocks: font, ink and transform are all TAttributes, so the axis
	// applies live and the header never rebuilds. The per-option values mirror
	// ck::debug_axes::Make_SectionHeader exactly — Uppercase is today's look, byte for byte
	// (GetToneColor(Neutral) resolves to TextDim()).
	auto Get_HeaderStyle() -> ECkDebugAxis_SectionHeaderStyle
	{
		return UCkDebuggerStyleSettings::Get_Selection().SectionHeaderStyle;
	}

	auto Get_LabelFont() -> FSlateFontInfo
	{
		return Get_HeaderStyle() == ECkDebugAxis_SectionHeaderStyle::Minimal
			? CkStyle::RegularFont(CkStyle::FontSizeSmall())
			: CkStyle::BoldFont(CkStyle::FontSizeH4());
	}

	auto Get_LabelColor() -> FSlateColor
	{
		return Get_HeaderStyle() == ECkDebugAxis_SectionHeaderStyle::Minimal
			? FSlateColor(CkStyle::TextMute())
			: FSlateColor(CkStyle::TextDim());
	}

	auto Get_LabelTransform() -> ETextTransformPolicy
	{
		return Get_HeaderStyle() == ECkDebugAxis_SectionHeaderStyle::Uppercase
			? ETextTransformPolicy::ToUpper
			: ETextTransformPolicy::None;
	}

	// Minimal drops the decoration; every other option keeps the panel-header hairline at the
	// SeparatorWeight thickness (Hairline == today's 1px). Zero thickness collapses the slot.
	auto Get_UnderlineThickness() -> float
	{
		if (Get_HeaderStyle() == ECkDebugAxis_SectionHeaderStyle::Minimal)
		{ return 0.0f; }

		return ck::debug_axes::Get_SeparatorThickness(UCkDebuggerStyleSettings::Get_Selection());
	}
}

// ====================================================================================================================

auto
	SCkDebug_SectionHeader::
	Construct(const FArguments& InArgs)
	-> void
{
	auto Label = SNew(STextBlock)
		.Text(InArgs._Label)
		.Font_Static(&ck_debug_section_header::Get_LabelFont)
		.ColorAndOpacity_Static(&ck_debug_section_header::Get_LabelColor)
		.TransformPolicy_Static(&ck_debug_section_header::Get_LabelTransform);

	// Only attach a tooltip when the caller asked for one — an empty one would still register a
	// tooltip widget, and "no tooltip" is what every existing call expects.
	if (!InArgs._ToolTip.IsEmpty())
	{ Label->SetToolTipText(InArgs._ToolTip); }

	auto Row = SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			Label
		];

	if (!InArgs._CountText.IsEmpty())
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(InArgs._CountText)
				.Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
				.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
			];
	}

	if (InArgs._SubContent.Widget != SNullWidget::NullWidget)
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				InArgs._SubContent.Widget
			];
	}
	else if (!InArgs._SubText.IsEmpty())
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(InArgs._SubText)
				.Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
				.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
			];
	}

	Row->AddSlot()
		.FillWidth(1.0f)
		[
			SNew(SBox)
		];

	if (InArgs._RightContent.Widget != SNullWidget::NullWidget)
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				InArgs._RightContent.Widget
			];
	}

	if (InArgs._Underline)
	{
		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM, CkStyle::SpaceL, CkStyle::SpaceM))
			[
				Row
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride_Lambda([]() -> FOptionalSize
				{
					return FOptionalSize{ck_debug_section_header::Get_UnderlineThickness()};
				})
				.Visibility_Lambda([]()
				{
					return ck_debug_section_header::Get_UnderlineThickness() > 0.0f
						? EVisibility::Visible
						: EVisibility::Collapsed;
				})
				[
					SNew(SImage)
					.Image(CkStyle::GetFilledBrush())
					.ColorAndOpacity(FSlateColor(CkStyle::Border()))
				]
			]
		];
		return;
	}

	ChildSlot
	.Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceS))
	[
		Row
	];
}

// ====================================================================================================================
