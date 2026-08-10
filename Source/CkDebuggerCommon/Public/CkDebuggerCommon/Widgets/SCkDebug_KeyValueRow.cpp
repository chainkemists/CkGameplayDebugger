#include "SCkDebug_KeyValueRow.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_debug_key_value_row
{
	// THE inspector row: 26 consumer sites compose through it, so both of its axes are bound as
	// attributes rather than read at construction — an axis flip has to move rows that were built
	// when the entity was selected, not only rows built after the flip.
	//
	// Both axes are DELTAS on this row's own shipped geometry, matching the metric-delta contract
	// in CkDebuggerAxes.h: the default option (Comfortable + Left) yields exactly the margins and
	// the value treatment the row has always drawn.
	constexpr auto MarkerGapBase    = CkStyle::SpaceM;
	constexpr auto ValueGapBase     = CkStyle::SpaceM;
	constexpr auto BodyPaddingXBase = CkStyle::SpaceM;
	constexpr auto BodyPaddingYBase = 3.0f;

	// Wide enough for the numeric values these rows carry; matches the Style Lab's own preview
	// column so the Lab does not lie about what AlignedColumns does.
	constexpr auto AlignedValueColumnWidth = 96.0f;

	auto Get_MarkerGap() -> FMargin
	{
		return ck::debug_axes::Apply_RowDensity(FMargin{0.0f, 0.0f, MarkerGapBase, 0.0f});
	}

	auto Get_ValueGap() -> FMargin
	{
		return ck::debug_axes::Apply_RowDensity(FMargin{ValueGapBase, 0.0f, 0.0f, 0.0f});
	}

	auto Get_BodyPadding() -> FMargin
	{
		return ck::debug_axes::Apply_RowDensity(FMargin{BodyPaddingXBase, BodyPaddingYBase});
	}

	auto Get_ValueJustification() -> ETextJustify::Type
	{
		const auto& Selection = UCkDebuggerStyleSettings::Get_Selection();

		return ck::debug_axes::Values_AlignRight(Selection) || ck::debug_axes::Values_UseAlignedColumns(Selection)
			? ETextJustify::Right
			: ETextJustify::Left;
	}

	// Only AlignedColumns needs geometry: a fixed-width right-aligned value is what makes decimal
	// points line up down a column. Left and Right leave the value hugging its own text, which is
	// the width the row has always given it.
	auto Get_ValueMinWidth() -> float
	{
		return ck::debug_axes::Values_UseAlignedColumns(UCkDebuggerStyleSettings::Get_Selection())
			? AlignedValueColumnWidth
			: 0.0f;
	}
}

// ====================================================================================================================

auto
	SCkDebug_KeyValueRow::
	Construct(const FArguments& InArgs)
	-> void
{
	// Resolve value color. Bool tone only makes sense for static text; fall back to Text() if ambiguous.
	auto ValueColorAttr = TAttribute<FSlateColor>{};
	if (InArgs._Tone == ECkDebug_KeyValueTone::Bool)
	{
		const auto ValStr = InArgs._ValueText.Get().ToString();
		const auto BoolColor = ValStr == TEXT("true") ? CkStyle::Ok() : CkStyle::Err();
		ValueColorAttr = FSlateColor(BoolColor);
	}
	else
	{
		const auto CustomAttr = InArgs._CustomValueColor;
		ValueColorAttr = TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda(
			[CustomAttr]() -> FSlateColor
			{
				return FSlateColor(CustomAttr.Get());
			}));
	}

	const auto MonoFont = FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeSmall());
	const auto BoldFont = FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeSmall());

	auto Row = SNew(SHorizontalBox);

	if (InArgs._ShowMarker)
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(TAttribute<FMargin>::CreateStatic(&ck_debug_key_value_row::Get_MarkerGap))
			[
				SNew(SBox)
				.WidthOverride(7.0f)
				.HeightOverride(7.0f)
				[
					SNew(SImage)
					.Image(CkStyle::GetFilledBrush())
					.ColorAndOpacity(FSlateColor(InArgs._MarkerColor))
				]
			];
	}

	// ---- Key column: button if OnKeyClicked is bound, otherwise dim-colored label. ----
	TSharedRef<SWidget> KeyWidget = SNullWidget::NullWidget;
	if (InArgs._OnKeyClicked.IsBound())
	{
		const auto OnClicked = InArgs._OnKeyClicked;
		KeyWidget = SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(0.0f)
			.ToolTipText(InArgs._KeyText)
			.OnClicked_Lambda([OnClicked]()
			{
				OnClicked.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(InArgs._KeyText)
				.Font(MonoFont)
				.ColorAndOpacity(FSlateColor(CkStyle::Selection()))
			];
	}
	else
	{
		// SEditableText (read-only) lets users select/copy the key text via Ctrl+C
		// or right-click context menu while still rendering like a label.
		KeyWidget = SNew(SEditableText)
			.Text(InArgs._KeyText)
			.Font(MonoFont)
			.ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
			.IsReadOnly(true);
	}

	Row->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			KeyWidget
		];

	// ---- Value column: custom widget slot if supplied, otherwise dynamic text. ----
	const auto HasCustomValueSlot = InArgs._ValueWidget.Widget != SNullWidget::NullWidget;
	if (HasCustomValueSlot)
	{
		// A caller-supplied value widget owns its own alignment — the row only contributes the gap.
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(TAttribute<FMargin>::CreateStatic(&ck_debug_key_value_row::Get_ValueGap))
			[
				InArgs._ValueWidget.Widget
			];
	}
	else
	{
		const auto ValueAttr = InArgs._ValueText;
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(TAttribute<FMargin>::CreateStatic(&ck_debug_key_value_row::Get_ValueGap))
			[
				SNew(SEditableText)
				.Text(ValueAttr)
				.Font(BoldFont)
				.ColorAndOpacity(ValueColorAttr)
				.IsReadOnly(true)
				.Justification_Static(&ck_debug_key_value_row::Get_ValueJustification)
				.MinDesiredWidth_Static(&ck_debug_key_value_row::Get_ValueMinWidth)
				.Visibility_Lambda([ValueAttr]()
				{
					return ValueAttr.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
				})
			];
	}

	auto Body = SNew(SBorder)
		.BorderImage(CkStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(InArgs._BackgroundColor))
		.Padding_Static(&ck_debug_key_value_row::Get_BodyPadding)
		[
			Row
		];

	ChildSlot
	[
		Body
	];
}

// ====================================================================================================================
