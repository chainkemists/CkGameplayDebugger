#include "SCkDebug_LabeledGroup.h"

#include "SCkDebug_CategoryDot.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

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
	// Backgrounds / text color / fonts all pull from CkDebugStyle — the
	// FLinearColor args on this widget are ignored so the macro tab can't
	// drift from the shared palette.

	auto Header = SNew(SHorizontalBox);

	const auto HasDot = InArgs._DotColor.A > 0.0f;
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
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::PaneHeadingFontSize()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::PaneHeadingColor()))
			.TransformPolicy(ETextTransformPolicy::ToUpper)
		];

	if (!InArgs._CountText.IsEmpty())
	{
		Header->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(InArgs._CountText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeMicro()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()))
			];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(CkDebugStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(CkDebugStyle::Border()))
		.Padding(FMargin(1.0f))
		[
			SNew(SVerticalBox)

			// Header strip
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(CkDebugStyle::GetFilledBrush())
				.BorderBackgroundColor(FSlateColor(CkDebugStyle::Bg1()))
				.Padding(FMargin(CkDebugStyle::SpaceM, CkDebugStyle::SpaceS))
				[
					Header
				]
			]

			// Body
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.BorderImage(CkDebugStyle::GetFilledBrush())
				.BorderBackgroundColor(FSlateColor(CkDebugStyle::Bg2()))
				.Padding(FMargin(CkDebugStyle::SpaceS, CkDebugStyle::SpaceS))
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
