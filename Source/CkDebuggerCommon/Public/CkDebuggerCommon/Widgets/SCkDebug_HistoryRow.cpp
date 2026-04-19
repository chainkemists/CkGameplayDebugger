#include "SCkDebug_HistoryRow.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

auto
	SCkDebug_HistoryRow::
	Construct(const FArguments& InArgs)
	-> void
{
	_OnClicked = InArgs._OnClicked;

	const auto ToneColor = CkDebugStyle::GetToneColor(InArgs._Tone);
	const auto BgColor = InArgs._IsSelected
		? CkDebugStyle::OverlayOf(CkDebugStyle::Info, 0.10f)
		: FLinearColor::Transparent;
	const auto LeftBorderColor = InArgs._IsSelected ? CkDebugStyle::Info : FLinearColor::Transparent;

	auto TopRow = SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(8.0f)
			.HeightOverride(8.0f)
			[
				SNew(SImage)
				.Image(CkDebugStyle::GetFilledBrush())
				.ColorAndOpacity(FSlateColor(ToneColor))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._TitleText)
			.Font(FCoreStyle::GetDefaultFontStyle(InArgs._IsSelected ? "Bold" : "Regular", CkDebugStyle::FontSizeBody))
			.ColorAndOpacity(FSlateColor(InArgs._IsSelected ? CkDebugStyle::Text : CkDebugStyle::TextDim))
		];

	if (!InArgs._RightText.IsEmpty())
	{
		TopRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(InArgs._RightText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeMicro))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute))
			];
	}

	auto Body = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			TopRow
		];

	if (!InArgs._SubtitleText.IsEmpty())
	{
		Body->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 3.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(InArgs._SubtitleText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeMicro))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute))
				.AutoWrapText(true)
			];
	}

	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
		.ContentPadding(FMargin(0.0f))
		.OnClicked(this, &SCkDebug_HistoryRow::OnClicked)
		[
			SNew(SHorizontalBox)

			// Left selection accent
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(3.0f)
				[
					SNew(SBorder)
					.BorderImage(CkDebugStyle::GetFilledBrush())
					.BorderBackgroundColor(FSlateColor(LeftBorderColor))
					.Padding(FMargin(0.0f))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBorder)
				.BorderImage(CkDebugStyle::GetFilledBrush())
				.BorderBackgroundColor(FSlateColor(BgColor))
				.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
				[
					Body
				]
			]
		]
	];
}

auto
	SCkDebug_HistoryRow::
	OnClicked()
	-> FReply
{
	_OnClicked.ExecuteIfBound();
	return FReply::Handled();
}

// ====================================================================================================================
