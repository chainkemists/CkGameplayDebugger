#include "SCkDebug_KeyValueRow.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

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
		const auto BoolColor = ValStr == TEXT("true") ? CkDebugStyle::Ok() : CkDebugStyle::Err();
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

	const auto MonoFont = FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall());
	const auto BoldFont = FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeSmall());

	auto Row = SNew(SHorizontalBox);

	if (InArgs._ShowMarker)
	{
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(7.0f)
				.HeightOverride(7.0f)
				[
					SNew(SImage)
					.Image(CkDebugStyle::GetFilledBrush())
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
				.ColorAndOpacity(FSlateColor(CkDebugStyle::Selection()))
			];
	}
	else
	{
		// SEditableText (read-only) lets users select/copy the key text via Ctrl+C
		// or right-click context menu while still rendering like a label.
		KeyWidget = SNew(SEditableText)
			.Text(InArgs._KeyText)
			.Font(MonoFont)
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
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
		Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
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
			.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SNew(SEditableText)
				.Text(ValueAttr)
				.Font(BoldFont)
				.ColorAndOpacity(ValueColorAttr)
				.IsReadOnly(true)
				.Visibility_Lambda([ValueAttr]()
				{
					return ValueAttr.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
				})
			];
	}

	auto Body = SNew(SBorder)
		.BorderImage(CkDebugStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(InArgs._BackgroundColor))
		.Padding(FMargin(CkDebugStyle::SpaceM, 3.0f))
		[
			Row
		];

	ChildSlot
	[
		Body
	];
}

// ====================================================================================================================
