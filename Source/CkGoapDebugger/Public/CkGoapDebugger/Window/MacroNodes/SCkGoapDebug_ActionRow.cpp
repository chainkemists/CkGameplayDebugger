#include "SCkGoapDebug_ActionRow.h"

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
				.BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox")))
				.BorderBackgroundColor(FSlateColor(FLinearColor(0.09f, 0.11f, 0.15f)))
				.Padding(FMargin(8.0f, 4.0f))
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
						.Text(FText::FromString(InArgs._Action.ClassName))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.88f, 0.92f)))
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
						.ValueColor(FLinearColor(0.96f, 0.62f, 0.04f))
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
