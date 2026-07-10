#include "SCkDebug_HistoryRow.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

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

	const auto ToneColor = CkStyle::GetToneColor(InArgs._Tone);
	const auto BgColor = InArgs._IsSelected
		? CkStyle::OverlayOf(CkStyle::Info(), 0.10f)
		: FLinearColor::Transparent;
	const auto LeftBorderColor = InArgs._IsSelected ? CkStyle::Info() : FLinearColor::Transparent;

	auto TopRow = SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(8.0f)
			.HeightOverride(8.0f)
			[
				SNew(SImage)
				.Image(CkStyle::GetFilledBrush())
				.ColorAndOpacity(FSlateColor(ToneColor))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._TitleText)
			.Font(FCoreStyle::GetDefaultFontStyle(InArgs._IsSelected ? "Bold" : "Regular", CkStyle::FontSizeBody()))
			.ColorAndOpacity(FSlateColor(InArgs._IsSelected ? CkStyle::Text() : CkStyle::TextDim()))
		];

	if (!InArgs._RightText.IsEmpty())
	{
		TopRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(InArgs._RightText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeMicro()))
				.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
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
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeMicro()))
				.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
				.AutoWrapText(true)
			];
	}

	auto Inner = SNew(SButton)
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
					.BorderImage(CkStyle::GetFilledBrush())
					.BorderBackgroundColor(FSlateColor(LeftBorderColor))
					.Padding(FMargin(0.0f))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBorder)
				.BorderImage(CkStyle::GetFilledBrush())
				.BorderBackgroundColor(FSlateColor(BgColor))
				.Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM))
				[
					Body
				]
			]
		];

	// Wrap in a transparent right-click catcher only when CopyText is supplied.
	// SButton ignores non-left-mouse-buttons so right-click bubbles to the wrapper.
	if (NOT InArgs._CopyText.IsEmpty())
	{
		const auto CopyText = InArgs._CopyText;
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			.Padding(FMargin(0.0f))
			.OnMouseButtonDown_Lambda([WeakThis = TWeakPtr<SWidget>(AsShared()), CopyText](const FGeometry&, const FPointerEvent& Evt) -> FReply
			{
				const auto Self = WeakThis.Pin();
				if (NOT Self.IsValid())
				{ return FReply::Unhandled(); }
				return ck::DebugCopyMenu::Handle_RightClickToCopy(Self.ToSharedRef(), Evt, CopyText);
			})
			[
				Inner
			]
		];
	}
	else
	{
		ChildSlot
		[
			Inner
		];
	}
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
