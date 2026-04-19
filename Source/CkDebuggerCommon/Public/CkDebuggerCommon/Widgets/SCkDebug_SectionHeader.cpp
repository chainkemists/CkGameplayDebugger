#include "SCkDebug_SectionHeader.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

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
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeH4))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim))
			.TransformPolicy(ETextTransformPolicy::ToUpper)
		];

	if (!InArgs._CountText.IsEmpty())
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(InArgs._CountText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute))
			];
	}

	ChildSlot
	.Padding(FMargin(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS))
	[
		Row
	];
}

// ====================================================================================================================
