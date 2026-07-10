#include "SCkDebug_ExpandableColumn.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
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

	// Backgrounds / colors / fonts pull from CkStyle — FLinearColor
	// args on this widget are ignored so the macro tab stays in sync with
	// the shared palette.

	// Title row: title fills, chevron on the right.
	auto TitleRow = SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._Title)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::PaneHeadingFontSize()))
			.ColorAndOpacity(FSlateColor(CkStyle::PaneHeadingColor()))
			.TransformPolicy(ETextTransformPolicy::ToUpper)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			// Engine SVG chevrons \u2014 the old text glyphs (\u25BE/\u25B8) render as boxes in
			// the editor font on some systems.
			SAssignNew(_ChevronIcon, SImage)
			.Image(FAppStyle::GetBrush(_IsExpanded ? TEXT("Icons.ChevronDown") : TEXT("Icons.ChevronRight")))
			.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
			.DesiredSizeOverride(FVector2D(12.0f, 12.0f))
		];

	// Pill (gate tag, etc.) drops to its own row below the title so long
	// pill text can't push the title into ellipsis.
	auto HeaderStack = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			TitleRow
		];

	if (!InArgs._PillText.IsEmpty())
	{
		HeaderStack->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(CkStyle::GetRoundedBrush())
				.BorderBackgroundColor(FSlateColor(CkStyle::Bg3()))
				.Padding(FMargin(CkStyle::SpaceM, 1.0f))
				[
					SNew(STextBlock)
					.Text(InArgs._PillText)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeMicro()))
					.ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
				]
			];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(CkStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(CkStyle::Border()))
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
					.BorderImage(CkStyle::GetFilledBrush())
					.BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
					.Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM))
					[
						HeaderStack
					]
				]
			]

			// Body (switched between expanded + collapsed)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.BorderImage(CkStyle::GetFilledBrush())
				.BorderBackgroundColor(FSlateColor(CkStyle::Bg2()))
				.Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
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
	if (_ChevronIcon.IsValid())  { _ChevronIcon->SetImage(FAppStyle::GetBrush(_IsExpanded ? TEXT("Icons.ChevronDown") : TEXT("Icons.ChevronRight"))); }
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
