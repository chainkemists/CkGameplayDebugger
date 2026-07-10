#include "SCkDebug_SectionHeader.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
	SCkDebug_SectionHeader::
	Construct(const FArguments& InArgs)
	-> void
{
	auto Row = SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._Label)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeH4()))
			.ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
			.TransformPolicy(ETextTransformPolicy::ToUpper)
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
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeSmall()))
				.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
			];
	}

	ChildSlot
	.Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceS))
	[
		Row
	];
}

// ====================================================================================================================
