#include "SCkDebug_RailContainer.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

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
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::PaneHeadingFontSize()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::PaneHeadingColor()))
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
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeMicro()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()))
			];
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(CkDebugStyle::GetFilledBrush())
			.BorderBackgroundColor(FSlateColor(CkDebugStyle::Bg1()))
			.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
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
