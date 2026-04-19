#include "SCkDebug_StatusPill.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace
{
	auto DoGetToneColor(ECkDebug_Tone InTone) -> FLinearColor
	{
		switch (InTone)
		{
			case ECkDebug_Tone::Info:   return CkDebugStyle::Info;
			case ECkDebug_Tone::Ok:     return CkDebugStyle::Ok;
			case ECkDebug_Tone::Warn:   return CkDebugStyle::Warn;
			case ECkDebug_Tone::Err:    return CkDebugStyle::Err;
			case ECkDebug_Tone::Accent: return CkDebugStyle::Accent;
			default:                    return CkDebugStyle::TextDim;
		}
	}
}

// ====================================================================================================================

auto
	SCkDebug_StatusPill::
	Construct(const FArguments& InArgs)
	-> void
{
	const auto ToneColor = DoGetToneColor(InArgs._Tone);
	auto BgColor = ToneColor; BgColor.A = 0.16f;
	auto BorderColor = ToneColor; BorderColor.A = 0.38f;

	auto Row = SNew(SHorizontalBox);

	if (InArgs._ShowDot)
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(7.0f)
				.HeightOverride(7.0f)
				[
					SNew(SImage)
					.Image(CkDebugStyle::GetFilledBrush())
					.ColorAndOpacity(FSlateColor(ToneColor))
				]
			];
	}

	Row->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._Text)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeSmall))
			.ColorAndOpacity(FSlateColor(ToneColor))
		];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(CkDebugStyle::GetRoundedBrush())
		.BorderBackgroundColor(FSlateColor(BorderColor))
		.Padding(FMargin(1.0f))
		[
			SNew(SBorder)
			.BorderImage(CkDebugStyle::GetRoundedBrush())
			.BorderBackgroundColor(FSlateColor(BgColor))
			.Padding(FMargin(CkDebugStyle::SpaceM, 2.0f))
			[
				Row
			]
		]
	];
}

// ====================================================================================================================
