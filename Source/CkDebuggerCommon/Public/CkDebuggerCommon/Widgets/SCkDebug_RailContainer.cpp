#include "SCkDebug_RailContainer.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
	SCkDebug_RailContainer::
	Construct(const FArguments& InArgs)
	-> void
{
	auto HeaderRow = SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._Title)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::PaneHeadingFontSize()))
			.ColorAndOpacity(FSlateColor(CkStyle::PaneHeadingColor()))
			.TransformPolicy(ETextTransformPolicy::ToUpper)
		];

	if (!InArgs._CountText.IsEmpty())
	{
		HeaderRow->AddSlot().FillWidth(1.0f);
		HeaderRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(_CountBadge, STextBlock)
				.Text(InArgs._CountText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeMicro()))
				.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
			];
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(CkStyle::GetFilledBrush())
			.BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
			.Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM))
			[
				HeaderRow
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(_Body, SVerticalBox)
			]
		]
	];
}

auto
	SCkDebug_RailContainer::
	AddChild(TSharedRef<SWidget> InWidget)
	-> void
{
	if (!_Body.IsValid()) { return; }
	_Body->AddSlot()
		.AutoHeight()
		[
			InWidget
		];
}

auto
	SCkDebug_RailContainer::
	ClearChildren()
	-> void
{
	if (_Body.IsValid()) { _Body->ClearChildren(); }
}

auto
	SCkDebug_RailContainer::
	Set_CountText(const FText& InText)
	-> void
{
	if (_CountBadge.IsValid()) { _CountBadge->SetText(InText); }
}

// ====================================================================================================================
