#include "SCkGoapDebug_ActionRow.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CategoryDot.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

auto
	SCkGoapDebug_ActionRow::
	Construct(const FArguments& InArgs)
	-> void
{
	_ClassName = InArgs._Action.ClassName;
	_OnClicked = InArgs._OnClicked;

	const auto CatColor = FCkGoapDebug_ActionCategorizer::CategoryColor(InArgs._Category);

	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
		.ContentPadding(FMargin(0.0f))
		.OnClicked(this, &SCkGoapDebug_ActionRow::OnButtonClicked)
		[
			SNew(SHorizontalBox)

			// Category stripe on the left
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(3.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox")))
					.BorderBackgroundColor(FSlateColor(CatColor))
					.Padding(FMargin(0.0f))
				]
			]

			// Row body
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBorder)
				.BorderImage(CkDebugStyle::GetFilledBrush())
				.BorderBackgroundColor(FSlateColor(CkDebugStyle::Bg2()))
				.Padding(FMargin(CkDebugStyle::SpaceM, CkDebugStyle::SpaceS))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SCkDebug_CategoryDot)
						.Color(CatColor)
						.Diameter(8.0f)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(InArgs._DisplayName.IsEmpty() ? InArgs._Action.ClassName : InArgs._DisplayName))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeBody()))
						.ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SCkDebug_CountBadge)
						.ValueText(FText::AsNumber(InArgs._Action.Preconditions.Num()))
						.SuffixText(FText::FromString(TEXT("pre")))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SCkDebug_CountBadge)
						.ValueText(FText::FromString(FString::Printf(TEXT("$%.0f"), InArgs._Action.Cost)))
						.ValueColor(CkDebugStyle::Accent())
					]
				]
			]
		]
	];
}

auto
	SCkGoapDebug_ActionRow::
	OnButtonClicked()
	-> FReply
{
	_OnClicked.ExecuteIfBound(_ClassName);
	return FReply::Handled();
}

// ====================================================================================================================
