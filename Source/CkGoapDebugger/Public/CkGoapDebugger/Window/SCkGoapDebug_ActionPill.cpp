#include "SCkGoapDebug_ActionPill.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

namespace
{
	struct FPillStyle
	{
		FLinearColor Fill;
		FLinearColor Border;
		FLinearColor BadgeFill;
		FLinearColor BadgeText;
		float DefaultOpacity;
	};

	auto DoStyleFor(ECkGoapDebug_ActionPillVariant InVariant) -> FPillStyle
	{
		switch (InVariant)
		{
			case ECkGoapDebug_ActionPillVariant::InPlan:
				return {
					CkDebugStyle::NodeFill_InPlan(),
					CkDebugStyle::NodeBorder_InPlan(),
					CkDebugStyle::NodeBorder_InPlan(),  // badge matches border
					CkDebugStyle::BgRoot(),
					1.0f
				};
			case ECkGoapDebug_ActionPillVariant::Pending:
				return {
					CkDebugStyle::PlanStep_Fill_Pending(),
					CkDebugStyle::PlanStep_Border_Pending(),
					CkDebugStyle::PlanStep_Badge_Pending(),
					CkDebugStyle::TextDim(),
					1.0f
				};
			case ECkGoapDebug_ActionPillVariant::Active:
				return {
					CkDebugStyle::PlanStep_Fill_Active(),
					CkDebugStyle::PlanStep_Border_Active(),
					CkDebugStyle::PlanStep_Badge_Active(),
					CkDebugStyle::BgRoot(),
					1.0f
				};
			case ECkGoapDebug_ActionPillVariant::Done:
				return {
					CkDebugStyle::PlanStep_Fill_Done(),
					CkDebugStyle::PlanStep_Border_Done(),
					CkDebugStyle::PlanStep_Badge_Done(),
					CkDebugStyle::BgRoot(),
					1.0f
				};
			default: // Inactive
				return {
					CkDebugStyle::NodeFill_Inactive(),
					CkDebugStyle::NodeBorder_Inactive(),
					CkDebugStyle::Bg3(),
					CkDebugStyle::TextDim(),
					CkDebugStyle::NodeInactiveOpacity()
				};
		}
	}
}

// ====================================================================================================================

auto
	SCkGoapDebug_ActionPill::
	Construct(const FArguments& InArgs)
	-> void
{
	_OnClicked = InArgs._OnClicked;

	const auto Style = DoStyleFor(InArgs._Variant);
	const auto Alpha = InArgs._OpacityOverride >= 0.0f ? InArgs._OpacityOverride : Style.DefaultOpacity;
	const auto ShowBadge = InArgs._StepIndex >= 0;
	const auto RoundedBrush = CkDebugStyle::GetRoundedBrush();

	// Header row: [badge?] title [cost?]
	auto HeaderRow = SNew(SHorizontalBox);

	if (ShowBadge)
	{
		HeaderRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(18.0f)
				.HeightOverride(18.0f)
				[
					SNew(SBorder)
					.BorderImage(RoundedBrush)
					.BorderBackgroundColor(FSlateColor(Style.BadgeFill))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(FMargin(0.0f))
					[
						SNew(STextBlock)
						.Text(FText::AsNumber(InArgs._StepIndex + 1))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeSmall()))
						.ColorAndOpacity(FSlateColor(Style.BadgeText))
					]
				]
			];
	}

	HeaderRow->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._Title)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::NodeTitleFontSize()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
		];

	if (InArgs._ShowCost)
	{
		HeaderRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("$%.0f"), InArgs._CostValue)))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::NodeCostFontSize()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent()))
			];
	}

	auto PillBody = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			HeaderRow
		];

	if (InArgs._BodyContent.Widget != SNullWidget::NullWidget)
	{
		PillBody->AddSlot()
			.AutoHeight()
			.Padding(0.0f, CkDebugStyle::SpaceS, 0.0f, 0.0f)
			[
				InArgs._BodyContent.Widget
			];
	}

	// Border + fill frame, dimmed via ColorAndOpacity when opacity < 1.
	auto Frame = SNew(SBorder)
		.BorderImage(RoundedBrush)
		.BorderBackgroundColor(FSlateColor(Style.Border))
		.Padding(FMargin(CkDebugStyle::NodeBorderThickness()))
		.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, Alpha))
		[
			SNew(SBorder)
			.BorderImage(RoundedBrush)
			.BorderBackgroundColor(FSlateColor(Style.Fill))
			.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
			[
				PillBody
			]
		];

	auto Sized = SNew(SBox);
	if (InArgs._MinDesiredWidth > 0.0f) { Sized->SetMinDesiredWidth(InArgs._MinDesiredWidth); }
	Sized->SetContent(Frame);

	// Wrap in a click-button only when a handler is provided so graph nodes
	// (no click) don't get an unnecessary button layer.
	if (_OnClicked.IsBound())
	{
		ChildSlot
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ContentPadding(FMargin(0.0f))
			.OnClicked(this, &SCkGoapDebug_ActionPill::OnButtonClicked)
			[
				Sized
			]
		];
	}
	else
	{
		ChildSlot[ Sized ];
	}
}

auto
	SCkGoapDebug_ActionPill::
	OnButtonClicked()
	-> FReply
{
	_OnClicked.ExecuteIfBound();
	return FReply::Handled();
}

// ====================================================================================================================
