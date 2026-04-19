#include "SCkDebug_KeyValueRow.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
	SCkDebug_KeyValueRow::
	Construct(const FArguments& InArgs)
	-> void
{
	auto ValueColor = CkDebugStyle::Text;
	if (InArgs._Tone == ECkDebug_KeyValueTone::Bool)
	{
		const auto ValStr = InArgs._ValueText.ToString();
		ValueColor = ValStr == TEXT("true") ? CkDebugStyle::Ok : CkDebugStyle::Err;
	}
	else
	{
		ValueColor = InArgs._CustomValueColor;
	}

	const auto MonoFont = FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall);

	auto Row = SNew(SHorizontalBox);

	if (InArgs._ShowMarker)
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(7.0f)
				.HeightOverride(7.0f)
				[
					SNew(SImage)
					.Image(CkDebugStyle::GetFilledBrush())
					.ColorAndOpacity(FSlateColor(InArgs._MarkerColor))
				]
			];
	}

	Row->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._KeyText)
			.Font(MonoFont)
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim))
		];

	if (!InArgs._ValueText.IsEmpty())
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(InArgs._ValueText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeSmall))
				.ColorAndOpacity(FSlateColor(ValueColor))
			];
	}

	auto Body = SNew(SBorder)
		.BorderImage(CkDebugStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(InArgs._BackgroundColor))
		.Padding(FMargin(CkDebugStyle::SpaceM, 3.0f))
		[
			Row
		];

	ChildSlot
	[
		Body
	];
}

// ====================================================================================================================
