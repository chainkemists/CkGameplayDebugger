#include "SCkDebug_ExpandableColumn.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

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

	// Backgrounds / colors / fonts pull from CkDebugStyle — FLinearColor
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
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::PaneHeadingFontSize()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::PaneHeadingColor()))
			.TransformPolicy(ETextTransformPolicy::ToUpper)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SAssignNew(_ChevronText, STextBlock)
			.Text(FText::FromString(_IsExpanded ? TEXT("\u25BE") : TEXT("\u25B8")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeBody()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()))
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
			.Padding(0.0f, CkDebugStyle::SpaceS, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(CkDebugStyle::GetRoundedBrush())
				.BorderBackgroundColor(FSlateColor(CkDebugStyle::Bg3()))
				.Padding(FMargin(CkDebugStyle::SpaceM, 1.0f))
				[
					SNew(STextBlock)
					.Text(InArgs._PillText)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeMicro()))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
				]
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
					.BorderImage(CkDebugStyle::GetFilledBrush())
					.BorderBackgroundColor(FSlateColor(CkDebugStyle::Bg1()))
					.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
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
				.BorderImage(CkDebugStyle::GetFilledBrush())
				.BorderBackgroundColor(FSlateColor(CkDebugStyle::Bg2()))
				.Padding(FMargin(CkDebugStyle::SpaceM, CkDebugStyle::SpaceS))
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
