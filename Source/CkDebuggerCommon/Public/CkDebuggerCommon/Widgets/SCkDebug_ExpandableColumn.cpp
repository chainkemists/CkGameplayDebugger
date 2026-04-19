#include "SCkDebug_ExpandableColumn.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

auto
	SCkDebug_ExpandableColumn::
	Construct(const FArguments& InArgs)
	-> void
{
	_IsExpanded = InArgs._StartExpanded;
	_OnExpansionChanged = InArgs._OnExpansionChanged;

	auto HeaderRow = SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._Title)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			.ColorAndOpacity(FSlateColor(InArgs._TitleColor))
		];

	if (!InArgs._PillText.IsEmpty())
	{
		HeaderRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("RoundedSelection_16x")))
				.BorderBackgroundColor(FSlateColor(FLinearColor(0.16f, 0.19f, 0.25f)))
				.Padding(FMargin(6.0f, 1.0f))
				[
					SNew(STextBlock)
					.Text(InArgs._PillText)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.54f, 0.57f, 0.65f)))
				]
			];
	}

	HeaderRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SAssignNew(_ChevronText, STextBlock)
			.Text(FText::FromString(_IsExpanded ? TEXT("\u25BE") : TEXT("\u25B8")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.42f, 0.44f, 0.50f)))
		];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox")))
		.BorderBackgroundColor(FSlateColor(InArgs._BorderColor))
		.Padding(FMargin(1.0f))
		[
			SNew(SVerticalBox)

			// Clickable header
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.ContentPadding(FMargin(0.0f))
				.OnClicked(this, &SCkDebug_ExpandableColumn::OnHeaderClicked)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox")))
					.BorderBackgroundColor(FSlateColor(InArgs._HeaderBackgroundColor))
					.Padding(FMargin(12.0f, 8.0f))
					[
						HeaderRow
					]
				]
			]

			// Body (switched between expanded + collapsed)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox")))
				.BorderBackgroundColor(FSlateColor(InArgs._BackgroundColor))
				.Padding(FMargin(8.0f, 6.0f))
				[
					SAssignNew(_BodySwitcher, SWidgetSwitcher)
					.WidgetIndex(_IsExpanded ? 0 : 1)

					+ SWidgetSwitcher::Slot()
					[
						InArgs._ExpandedBody.Widget
					]

					+ SWidgetSwitcher::Slot()
					[
						InArgs._CollapsedSummary.Widget
					]
				]
			]
		]
	];
}

auto
	SCkDebug_ExpandableColumn::
	Set_Expanded(bool InExpanded)
	-> void
{
	if (_IsExpanded == InExpanded) { return; }
	_IsExpanded = InExpanded;
	if (_BodySwitcher.IsValid()) { _BodySwitcher->SetActiveWidgetIndex(_IsExpanded ? 0 : 1); }
	if (_ChevronText.IsValid())  { _ChevronText->SetText(FText::FromString(_IsExpanded ? TEXT("\u25BE") : TEXT("\u25B8"))); }
	_OnExpansionChanged.ExecuteIfBound(_IsExpanded);
}

auto
	SCkDebug_ExpandableColumn::
	OnHeaderClicked()
	-> FReply
{
	Set_Expanded(!_IsExpanded);
	return FReply::Handled();
}

// ====================================================================================================================
