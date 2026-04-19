#include "SCkDebug_CountBadge.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

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
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
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
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(CkDebugStyle::GetRoundedBrush())
		.BorderBackgroundColor(FSlateColor(InArgs._BorderColor))
		.Padding(FMargin(1.0f))
		[
			SNew(SBorder)
			.BorderImage(CkDebugStyle::GetRoundedBrush())
			.BorderBackgroundColor(FSlateColor(InArgs._BackgroundColor))
			.Padding(FMargin(6.0f, 1.0f))
			[
				Row
			]
		]
	];
}

// ====================================================================================================================
