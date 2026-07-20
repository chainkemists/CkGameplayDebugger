#include "SCkDebug_StatusPill.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
	SCkDebug_StatusPill::
	Construct(const FArguments& InArgs)
	-> void
{
	const auto Tone = InArgs._Tone;

	const auto ToneColor = [Tone]() -> FSlateColor
	{
		return FSlateColor(CkStyle::GetToneColor(Tone.Get(ECk_Tone::Neutral)));
	};
	const auto DimColor = [Tone]() -> FSlateColor
	{
		return FSlateColor(CkStyle::GetToneDimColor(Tone.Get(ECk_Tone::Neutral)));
	};
	const auto BorderColor = [Tone]() -> FSlateColor
	{
		auto Color = CkStyle::GetToneColor(Tone.Get(ECk_Tone::Neutral));
		Color.A = 0.5f;
		return FSlateColor(Color);
	};

	auto Row = SNew(SHorizontalBox);

	if (InArgs._ShowDot)
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(6.0f)
				.HeightOverride(6.0f)
				[
					SNew(SImage)
					.Image(CkStyle::GetRoundedBrush_Pill())
					.ColorAndOpacity_Lambda(ToneColor)
				]
			];
	}

	Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._Text)
			.Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
			.ColorAndOpacity_Lambda(ToneColor)
		];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(CkStyle::GetRoundedBrush_Pill())
		.BorderBackgroundColor_Lambda(BorderColor)
		.Padding(FMargin(1.0f))
		[
			SNew(SBorder)
			.BorderImage(CkStyle::GetRoundedBrush_Pill())
			.BorderBackgroundColor_Lambda(DimColor)
			.Padding(FMargin(CkStyle::SpaceM, 2.0f))
			[
				Row
			]
		]
	];
}

// ====================================================================================================================
