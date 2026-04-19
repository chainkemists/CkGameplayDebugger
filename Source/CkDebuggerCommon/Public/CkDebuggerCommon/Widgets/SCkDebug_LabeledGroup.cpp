#include "SCkDebug_LabeledGroup.h"

#include "SCkDebug_CategoryDot.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

auto
	SCkDebug_LabeledGroup::
	Construct(const FArguments& InArgs)
	-> void
{
	auto Header = SNew(SHorizontalBox);

	const auto HasDot = InArgs._DotColor.A > 0.0f || InArgs._DotColor != FLinearColor::Transparent;
	if (HasDot)
	{
		Header->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SCkDebug_CategoryDot)
				.Color(InArgs._DotColor)
				.Diameter(8.0f)
			];
	}

	Header->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._Label)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.ColorAndOpacity(FSlateColor(InArgs._LabelColor))
		];

	if (!InArgs._CountText.IsEmpty())
	{
		Header->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InArgs._CountText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.48f, 0.55f)))
			];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox")))
		.BorderBackgroundColor(FSlateColor(InArgs._BorderColor))
		.Padding(FMargin(1.0f))
		[
			SNew(SVerticalBox)

			// Header strip
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox")))
				.BorderBackgroundColor(FSlateColor(InArgs._HeaderBackgroundColor))
				.Padding(FMargin(8.0f, 4.0f))
				[
					Header
				]
			]

			// Body
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox")))
				.BorderBackgroundColor(FSlateColor(InArgs._BackgroundColor))
				.Padding(FMargin(6.0f, 4.0f))
				[
					SAssignNew(_Body, SVerticalBox)
				]
			]
		]
	];
}

auto
	SCkDebug_LabeledGroup::
	AddChild(TSharedRef<SWidget> InWidget)
	-> void
{
	if (!_Body.IsValid()) { return; }
	_Body->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 1.0f)
		[
			InWidget
		];
}

auto
	SCkDebug_LabeledGroup::
	ClearChildren()
	-> void
{
	if (_Body.IsValid()) { _Body->ClearChildren(); }
}

// ====================================================================================================================
