#include "SCkDebug_NodePill.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

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

	auto DoStyleFor(ECkDebug_NodePillVariant InVariant) -> FPillStyle
	{
		switch (InVariant)
		{
			case ECkDebug_NodePillVariant::InPlan:
				return {
					CkDebugStyle::NodeFill_InPlan(),
					CkDebugStyle::NodeBorder_InPlan(),
					CkDebugStyle::NodeBorder_InPlan(),  // badge matches border
					CkDebugStyle::BgRoot(),
					1.0f
				};
			case ECkDebug_NodePillVariant::Pending:
				return {
					CkDebugStyle::PlanStep_Fill_Pending(),
					CkDebugStyle::PlanStep_Border_Pending(),
					CkDebugStyle::PlanStep_Badge_Pending(),
					CkDebugStyle::TextDim(),
					1.0f
				};
			case ECkDebug_NodePillVariant::Active:
				return {
					CkDebugStyle::PlanStep_Fill_Active(),
					CkDebugStyle::PlanStep_Border_Active(),
					CkDebugStyle::PlanStep_Badge_Active(),
					CkDebugStyle::BgRoot(),
					1.0f
				};
			case ECkDebug_NodePillVariant::Done:
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

	auto HasColor(const FLinearColor& InColor) -> bool
	{
		return InColor.A > 0.0f;
	}
}

// ====================================================================================================================

auto
	SCkDebug_NodePill::
	Construct(const FArguments& InArgs)
	-> void
{
	_OnClicked = InArgs._OnClicked;
	_Selected  = InArgs._Selected;

	const auto VariantStyle = DoStyleFor(InArgs._Variant);
	// Border color + opacity are evaluated live (per-frame) so flag-driven nodes
	// can recolor / fade without a widget rebuild. Callers passing a static value
	// get a constant attribute — identical behavior to the old SLATE_ARGUMENTs.
	const auto BorderAttr     = InArgs._BorderColorOverride;
	const auto OpacityAttr    = InArgs._OpacityOverride;
	const auto VariantBorder  = VariantStyle.Border;
	const auto VariantOpacity = VariantStyle.DefaultOpacity;
	const auto ShowBadge    = InArgs._StepIndex >= 0;
	const auto ShowAccent   = HasColor(InArgs._AccentColor);
	const auto AccentColor  = InArgs._AccentColor;
	const auto RoundedBrush = CkDebugStyle::GetRoundedBrush();

	// ---- Header presence: if none of badge / title / cost are populated, the
	//      whole header row is skipped. Callers that render their own title
	//      inside BodyContent pass Title(FText::GetEmpty()) + ShowCost(false)
	//      + StepIndex(-1) to get a body-only pill without empty-header padding.
	const auto ShowHeader = ShowBadge || !InArgs._Title.IsEmpty() || InArgs._ShowCost;

	// ---- Header row: [badge?] title [cost?] --------------------------------
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
					.BorderBackgroundColor(FSlateColor(VariantStyle.BadgeFill))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(FMargin(0.0f))
					[
						SNew(STextBlock)
						.Text(FText::AsNumber(InArgs._StepIndex + 1))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeSmall()))
						.ColorAndOpacity(FSlateColor(VariantStyle.BadgeText))
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

	// ---- Header + optional body stacked vertically --------------------------
	auto PillBody = SNew(SVerticalBox);

	if (ShowHeader)
	{
		PillBody->AddSlot()
			.AutoHeight()
			[
				HeaderRow
			];
	}

	if (InArgs._BodyContent.Widget != SNullWidget::NullWidget)
	{
		PillBody->AddSlot()
			.AutoHeight()
			.Padding(0.0f, ShowHeader ? CkDebugStyle::SpaceS : 0.0f, 0.0f, 0.0f)
			[
				InArgs._BodyContent.Widget
			];
	}

	// ---- Optional accent strip on the left edge ----------------------------
	TSharedRef<SWidget> ContentRow = PillBody;
	if (ShowAccent)
	{
		ContentRow = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Fill)
			.Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(4.0f)
				[
					SNew(SImage)
					.Image(CkDebugStyle::GetFilledBrush())
					.ColorAndOpacity(FSlateColor(AccentColor))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				PillBody
			];
	}

	// ---- Border + fill frame, dimmed via ColorAndOpacity when opacity < 1 ---
	auto Frame = SNew(SBorder)
		.BorderImage(RoundedBrush)
		.BorderBackgroundColor_Lambda([BorderAttr, VariantBorder]() -> FSlateColor
		{
			const auto Override = BorderAttr.Get();
			return FSlateColor(HasColor(Override) ? Override : VariantBorder);
		})
		.Padding(FMargin(CkDebugStyle::NodeBorderThickness()))
		.ColorAndOpacity_Lambda([OpacityAttr, VariantOpacity]() -> FLinearColor
		{
			const auto Override = OpacityAttr.Get();
			const auto Alpha = Override >= 0.0f ? Override : VariantOpacity;
			return FLinearColor(1.0f, 1.0f, 1.0f, Alpha);
		})
		[
			SNew(SBorder)
			.BorderImage(RoundedBrush)
			.BorderBackgroundColor(FSlateColor(VariantStyle.Fill))
			.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
			[
				ContentRow
			]
		];

	auto Sized = SNew(SBox);
	if (InArgs._MinDesiredWidth > 0.0f) { Sized->SetMinDesiredWidth(InArgs._MinDesiredWidth); }
	Sized->SetContent(Frame);

	// ---- Selection ring — a thicker border wrapping the pill ----------------
	auto Ringed = SAssignNew(_SelectionRing, SBorder)
		.BorderImage(RoundedBrush)
		.BorderBackgroundColor(FSlateColor(_Selected ? CkDebugStyle::Selection() : FLinearColor::Transparent))
		.Padding(FMargin(2.0f))
		[
			Sized
		];

	// ---- Optional click-button wrap -----------------------------------------
	TSharedRef<SWidget> Inner = Ringed;
	if (_OnClicked.IsBound())
	{
		Inner = SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ContentPadding(FMargin(0.0f))
			.OnClicked(this, &SCkDebug_NodePill::OnButtonClicked)
			[
				Ringed
			];
	}

	// Wrap with right-click → copy menu when CopyText is supplied. SButton ignores
	// non-left buttons so the right-click bubbles up to this SBorder.
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
		ChildSlot[ Inner ];
	}
}

auto
	SCkDebug_NodePill::
	Set_Selected(bool InSelected)
	-> void
{
	if (_Selected == InSelected) { return; }
	_Selected = InSelected;
	if (_SelectionRing.IsValid())
	{
		_SelectionRing->SetBorderBackgroundColor(
			FSlateColor(_Selected ? CkDebugStyle::Selection() : FLinearColor::Transparent));
	}
}

auto
	SCkDebug_NodePill::
	OnButtonClicked()
	-> FReply
{
	_OnClicked.ExecuteIfBound();
	return FReply::Handled();
}

// ====================================================================================================================
