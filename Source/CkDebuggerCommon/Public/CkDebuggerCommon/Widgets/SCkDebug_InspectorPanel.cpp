#include "SCkDebug_InspectorPanel.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

auto
	SCkDebug_InspectorPanel::
	Construct(const FArguments& InArgs)
	-> void
{
	_IsExpanded = InArgs._StartExpanded;
	_OnToggled = InArgs._OnToggled;
	_Title = InArgs._Title;
	_CountText = InArgs._CountText;
	_StatusPillText = InArgs._StatusPillText;
	_StatusPillTone = InArgs._StatusPillTone;

	SAssignNew(_HeaderRow, SHorizontalBox);

	auto HeaderButton = SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
		.ContentPadding(FMargin(0.0f))
		.OnClicked(this, &SCkDebug_InspectorPanel::OnHeaderClicked)
		[
			SNew(SBorder)
			.BorderImage(CkStyle::GetFilledBrush())
			.BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
			.Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM))
			[
				_HeaderRow.ToSharedRef()
			]
		];

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			HeaderButton
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			// Body uses the NoBorder brush so the editor panel background
			// shows through — layered tints on top of editor chrome read as
			// washed-out gray.
			SAssignNew(_BodyBorder, SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
			.Padding(FMargin(0.0f))
			.Visibility(_IsExpanded ? EVisibility::Visible : EVisibility::Collapsed)
			[
				InArgs._Body.Widget
			]
		]
	];

	RebuildHeader();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkDebug_InspectorPanel::
	RebuildHeader()
	-> void
{
	if (!_HeaderRow.IsValid()) { return; }
	_HeaderRow->ClearChildren();

	_HeaderRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(_Title)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::PaneHeadingFontSize()))
			.ColorAndOpacity(FSlateColor(CkStyle::PaneHeadingColor()))
			.TransformPolicy(ETextTransformPolicy::ToUpper)
		];

	if (!_CountText.IsEmpty())
	{
		_HeaderRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(_CountBadge, STextBlock)
				.Text(_CountText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeSmall()))
				.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
			];
	}

	if (!_StatusPillText.IsEmpty())
	{
		_HeaderRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(_StatusPill, SCkDebug_StatusPill)
				.Text(_StatusPillText)
				.Tone(_StatusPillTone)
			];
	}

	_HeaderRow->AddSlot().FillWidth(1.0f);

	_HeaderRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SAssignNew(_ChevronText, STextBlock)
			.Text(FText::FromString(_IsExpanded ? TEXT("\u25BE") : TEXT("\u25B8")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeH4()))
			.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkDebug_InspectorPanel::
	Set_Expanded(bool InExpanded)
	-> void
{
	if (_IsExpanded == InExpanded) { return; }
	_IsExpanded = InExpanded;
	if (_BodyBorder.IsValid())
	{
		_BodyBorder->SetVisibility(_IsExpanded ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (_ChevronText.IsValid())
	{
		_ChevronText->SetText(FText::FromString(_IsExpanded ? TEXT("\u25BE") : TEXT("\u25B8")));
	}
	_OnToggled.ExecuteIfBound(_IsExpanded);
}

auto
	SCkDebug_InspectorPanel::
	Set_CountText(const FText& InText)
	-> void
{
	_CountText = InText;
	if (_CountBadge.IsValid()) { _CountBadge->SetText(InText); }
	else { RebuildHeader(); }
}

auto
	SCkDebug_InspectorPanel::
	Set_StatusPill(const FText& InText, ECk_Tone InTone)
	-> void
{
	_StatusPillText = InText;
	_StatusPillTone = InTone;
	RebuildHeader();
}

auto
	SCkDebug_InspectorPanel::
	OnHeaderClicked()
	-> FReply
{
	Set_Expanded(!_IsExpanded);
	return FReply::Handled();
}

// ====================================================================================================================
