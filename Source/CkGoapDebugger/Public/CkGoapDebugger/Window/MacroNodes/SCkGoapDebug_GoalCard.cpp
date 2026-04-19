#include "SCkGoapDebug_GoalCard.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

auto
	SCkGoapDebug_GoalCard::
	Construct(const FArguments& InArgs)
	-> void
{
	const auto& G = InArgs._Goal;

	auto Conds = FString{};
	for (auto i = 0; i < G.Conditions.Num(); ++i)
	{
		if (i > 0) { Conds += TEXT(", "); }
		Conds += G.Conditions[i].Key.ToString();
	}

	const auto IsActive = G.IsActiveGoal;
	const auto BorderColor = IsActive
		? FLinearColor(0.74f, 0.52f, 0.14f)
		: FLinearColor(0.30f, 0.26f, 0.12f);
	const auto FillColor = IsActive
		? FLinearColor(0.12f, 0.10f, 0.04f)
		: FLinearColor(0.08f, 0.07f, 0.03f);
	const auto TitleColor = IsActive
		? FLinearColor(0.96f, 0.78f, 0.18f)
		: FLinearColor(0.74f, 0.60f, 0.14f);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox")))
		.BorderBackgroundColor(FSlateColor(BorderColor))
		.Padding(FMargin(1.0f))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("GenericWhiteBox")))
			.BorderBackgroundColor(FSlateColor(FillColor))
			.Padding(FMargin(10.0f, 6.0f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(G.ClassName))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
						.ColorAndOpacity(FSlateColor(TitleColor))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SCkDebug_CountBadge)
						.ValueText(FText::AsNumber(G.Priority))
						.SuffixText(FText::FromString(TEXT("priority")))
						.ValueColor(FLinearColor(0.96f, 0.78f, 0.18f))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Conds))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.74f, 0.60f, 0.14f)))
					.AutoWrapText(true)
				]
			]
		]
	];
}

// ====================================================================================================================
