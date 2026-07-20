#include "SCkDebug_SectionHeader.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
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
			.Font(CkStyle::BoldFont(CkStyle::FontSizeH4()))
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
				.HeightOverride(1.0f)
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
